/******************************************************************************
    Copyright (C) 2017 by Quinn Damerell <qdamere@microsoft.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <obs-module.h>
#include <obs-avc.h>
#include <util/platform.h>
#include <util/circlebuf.h>
#include <util/dstr.h>
#include <util/threading.h>
#include <inttypes.h>
#include "ftl.h"
#include "flv-mux.h"
#include "net-if.h"

#ifdef _WIN32
#include <Iphlpapi.h>
#else
#include <sys/ioctl.h>
#define INFINITE 0xFFFFFFFF
#endif

#define do_log(level, format, ...)                \
	blog(level, "[ftl stream: '%s'] " format, \
	     obs_output_get_name(stream->output), ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)

#define OPT_DROP_THRESHOLD "drop_threshold_ms"
#define OPT_MAX_SHUTDOWN_TIME_SEC "max_shutdown_time_sec"
#define OPT_BIND_IP "bind_ip"

#define FTL_URL_PROTOCOL "ftl://"

typedef struct _nalu_t {
	int len;
	int dts_usec;
	int send_marker_bit;
	uint8_t *data;
} nalu_t;

typedef struct _frame_of_nalus_t {
	nalu_t nalus[100];
	int total;
	int complete_frame;
} frame_of_nalus_t;

struct ftl_stream {
	obs_output_t *output;

	pthread_mutex_t packets_mutex;
	struct circlebuf packets;
	bool sent_headers;
	int64_t frames_sent;

	volatile bool connecting;
	pthread_t connect_thread;
	pthread_t status_thread;

	volatile bool active;
	volatile bool disconnected;
	volatile bool encode_error;
	pthread_t send_thread;

	int max_shutdown_time_sec;

	os_sem_t *send_sem;
	os_event_t *stop_event;
	uint64_t stop_ts;
	uint64_t shutdown_timeout_ts;

	struct dstr path;
	uint32_t channel_id;
	struct dstr username, password;
	struct dstr encoder_name;
	struct dstr bind_ip;

	/* frame drop variables */
	int64_t drop_threshold_usec;
	int64_t pframe_drop_threshold_usec;
	int min_priority;
	float congestion;

	int64_t last_dts_usec;

	uint64_t total_bytes_sent;
	uint64_t dropped_frames;
	uint64_t last_nack_count;

	ftl_handle_t ftl_handle;
	ftl_ingest_params_t params;
	int peak_kbps;
	uint32_t scale_width, scale_height, width, height;
	frame_of_nalus_t coded_pic_buffer;
};

static void log_libftl_messages(ftl_log_severity_t log_level,
				const char *message);
static int init_connect(struct ftl_stream *stream);
static void *connect_thread(void *data);
static void *status_thread(void *data);
static int _ftl_error_to_obs_error(int status);

static const char *ftl_stream_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("FTLStream");
}

static void log_ftl(int level, const char *format, va_list args)
{
	blogva(LOG_INFO, format, args);
	UNUSED_PARAMETER(level);
}

static inline size_t num_buffered_packets(struct ftl_stream *stream);

static inline void free_packets(struct ftl_stream *stream)
{
	size_t num_packets;

	pthread_mutex_lock(&stream->packets_mutex);

	num_packets = num_buffered_packets(stream);
	if (num_packets)
		info("Freeing %d remaining packets", (int)num_packets);

	while (stream->packets.size) {
		struct encoder_packet packet;
		circlebuf_pop_front(&stream->packets, &packet, sizeof(packet));
		obs_encoder_packet_release(&packet);
	}
	pthread_mutex_unlock(&stream->packets_mutex);
}

static inline bool stopping(struct ftl_stream *stream)
{
	return os_event_try(stream->stop_event) != EAGAIN;
}

static inline bool connecting(struct ftl_stream *stream)
{
	return os_atomic_load_bool(&stream->connecting);
}

static inline bool active(struct ftl_stream *stream)
{
	return os_atomic_load_bool(&stream->active);
}

static inline bool disconnected(struct ftl_stream *stream)
{
	return os_atomic_load_bool(&stream->disconnected);
}

static void ftl_stream_destroy(void *data)
{
	struct ftl_stream *stream = data;
	ftl_status_t status_code;

	info("ftl_stream_destroy");

	if (stopping(stream) && !connecting(stream)) {
		pthread_join(stream->send_thread, NULL);

	} else if (connecting(stream) || active(stream)) {
		if (stream->connecting) {
			info("wait for connect_thread to terminate");
			pthread_join(stream->status_thread, NULL);
			pthread_join(stream->connect_thread, NULL);
			info("wait for connect_thread to terminate: done");
		}

		stream->stop_ts = 0;
		os_event_signal(stream->stop_event);

		if (active(stream)) {
			os_sem_post(stream->send_sem);
			obs_output_end_data_capture(stream->output);
			pthread_join(stream->send_thread, NULL);
		}
	}

	info("ingest destroy");

	status_code = ftl_ingest_destroy(&stream->ftl_handle);
	if (status_code != FTL_SUCCESS) {
		info("Failed to destroy from ingest %d", status_code);
	}

	if (stream) {
		free_packets(stream);
		dstr_free(&stream->path);
		dstr_free(&stream->username);
		dstr_free(&stream->password);
		dstr_free(&stream->encoder_name);
		dstr_free(&stream->bind_ip);
		os_event_destroy(stream->stop_event);
		os_sem_destroy(stream->send_sem);
		pthread_mutex_destroy(&stream->packets_mutex);
		circlebuf_free(&stream->packets);
		bfree(stream);
	}
}

static void *ftl_stream_create(obs_data_t *settings, obs_output_t *output)
{
	struct ftl_stream *stream = bzalloc(sizeof(struct ftl_stream));
	info("ftl_stream_create");

	stream->output = output;
	pthread_mutex_init_value(&stream->packets_mutex);

	stream->peak_kbps = -1;
	ftl_init();

	if (pthread_mutex_init(&stream->packets_mutex, NULL) != 0) {
		goto fail;
	}
	if (os_event_init(&stream->stop_event, OS_EVENT_TYPE_MANUAL) != 0) {
		goto fail;
	}

	stream->coded_pic_buffer.total = 0;
	stream->coded_pic_buffer.complete_frame = 0;

	UNUSED_PARAMETER(settings);
	return stream;

fail:
	return NULL;
}

static void ftl_stream_stop(void *data, uint64_t ts)
{
	struct ftl_stream *stream = data;
	info("ftl_stream_stop");

	if (stopping(stream) && ts != 0) {
		return;
	}

	if (connecting(stream)) {
		pthread_join(stream->status_thread, NULL);
		pthread_join(stream->connect_thread, NULL);
	}

	stream->stop_ts = ts / 1000ULL;

	if (ts) {
		stream->shutdown_timeout_ts =
			ts +
			(uint64_t)stream->max_shutdown_time_sec * 1000000000ULL;
	}

	if (active(stream)) {
		os_event_signal(stream->stop_event);
		if (stream->stop_ts == 0)
			os_sem_post(stream->send_sem);
	} else {
		obs_output_signal_stop(stream->output, OBS_OUTPUT_SUCCESS);
	}
}

static inline bool get_next_packet(struct ftl_stream *stream,
				   struct encoder_packet *packet)
{
	bool new_packet = false;

	pthread_mutex_lock(&stream->packets_mutex);
	if (stream->packets.size) {
		circlebuf_pop_front(&stream->packets, packet,
				    sizeof(struct encoder_packet));
		new_packet = true;
	}
	pthread_mutex_unlock(&stream->packets_mutex);

	return new_packet;
}

static int avc_get_video_frame(struct ftl_stream *stream,
			       struct encoder_packet *packet, bool is_header)
{
	int consumed = 0;
	int len = (int)packet->size;
	nalu_t *nalu;

	unsigned char *video_stream = packet->data;

	while ((size_t)consumed < packet->size) {
		size_t total_max = sizeof(stream->coded_pic_buffer.nalus) /
				   sizeof(stream->coded_pic_buffer.nalus[0]);

		if ((size_t)stream->coded_pic_buffer.total >= total_max) {
			warn("ERROR: cannot continue, nalu buffers are full");
			return -1;
		}

		nalu = &stream->coded_pic_buffer
				.nalus[stream->coded_pic_buffer.total];

		if (is_header) {
			if (consumed == 0) {
				//first 6 bytes are some obs header with part
				//of the sps
				video_stream += 6;
				consumed += 6;
			} else {
				//another spacer byte of 0x1
				video_stream += 1;
				consumed += 1;
			}

			len = video_stream[0] << 8 | video_stream[1];
			video_stream += 2;
			consumed += 2;
		} else {
			len = video_stream[0] << 24 | video_stream[1] << 16 |
			      video_stream[2] << 8 | video_stream[3];

			if ((size_t)len > (packet->size - (size_t)consumed)) {
				warn("ERROR: got len of %d but packet only "
				     "has %d left",
				     len, (int)(packet->size - consumed));
			}

			consumed += 4;
			video_stream += 4;
		}

		consumed += len;

		uint8_t nalu_type = video_stream[0] & 0x1F;
		uint8_t nri = (video_stream[0] >> 5) & 0x3;

		if ((nalu_type != 12 && nalu_type != 6 && nalu_type != 9) ||
		    nri) {
			nalu->data = video_stream;
			nalu->len = len;
			nalu->send_marker_bit = 0;
			stream->coded_pic_buffer.total++;
		}

		video_stream += len;
	}

	if (!is_header) {
		size_t idx = stream->coded_pic_buffer.total - 1;
		stream->coded_pic_buffer.nalus[idx].send_marker_bit = 1;
	}

	return 0;
}

static int send_packet(struct ftl_stream *stream, struct encoder_packet *packet,
		       bool is_header)
{
	int bytes_sent = 0;
	int ret = 0;

	if (packet->type == OBS_ENCODER_VIDEO) {
		stream->coded_pic_buffer.total = 0;
		avc_get_video_frame(stream, packet, is_header);

		int i;
		for (i = 0; i < stream->coded_pic_buffer.total; i++) {
			nalu_t *nalu = &stream->coded_pic_buffer.nalus[i];
			bytes_sent += ftl_ingest_send_media_dts(
				&stream->ftl_handle, FTL_VIDEO_DATA,
				packet->dts_usec, nalu->data, nalu->len,
				nalu->send_marker_bit);

			if (nalu->send_marker_bit) {
				stream->frames_sent++;
			}
		}

	} else if (packet->type == OBS_ENCODER_AUDIO) {
		bytes_sent += ftl_ingest_send_media_dts(
			&stream->ftl_handle, FTL_AUDIO_DATA, packet->dts_usec,
			packet->data, (int)packet->size, 0);
	} else {
		warn("Got packet type %d", packet->type);
	}

	if (is_header) {
		bfree(packet->data);
	} else {
		obs_encoder_packet_release(packet);
	}

	stream->total_bytes_sent += bytes_sent;
	return ret;
}

static void set_peak_bitrate(struct ftl_stream *stream)
{
	int speedtest_kbps = 15000;
	int speedtest_duration = 1000;
	speed_test_t results;
	ftl_status_t status_code;

	status_code = ftl_ingest_speed_test_ex(&stream->ftl_handle,
					       speedtest_kbps,
					       speedtest_duration, &results);

	float percent_lost = 0;

	if (status_code == FTL_SUCCESS) {
		percent_lost = (float)results.lost_pkts * 100.f /
			       (float)results.pkts_sent;
	} else {
		warn("Speed test failed with: %s",
		     ftl_status_code_to_string(status_code));
	}

	// Get what the user set the encoding bitrate to.
	obs_encoder_t *video_encoder =
		obs_output_get_video_encoder(stream->output);
	obs_data_t *video_settings = obs_encoder_get_settings(video_encoder);
	int user_desired_bitrate =
		(int)obs_data_get_int(video_settings, "bitrate");
	obs_data_release(video_settings);

	// Report the results.
	info("Speed test completed: User desired bitrate %d, Peak kbps %d, "
	     "initial rtt %d, "
	     "final rtt %d, %3.2f lost packets",
	     user_desired_bitrate, results.peak_kbps, results.starting_rtt,
	     results.ending_rtt, percent_lost);

	// We still want to set the peak to about 1.2x what the target bitrate is,
	// even if the speed test reported it should be lower. If we don't, FTL
	// will queue data on the client and start adding latency. If the internet
	// connection really can't handle the bitrate the user will see either lost frame
	// and recovered frame counts go up, which is reflect in the dropped_frames count.
	stream->peak_kbps = stream->params.peak_kbps =
		user_desired_bitrate * 12 / 10;
	ftl_ingest_update_params(&stream->ftl_handle, &stream->params);
}

static inline bool send_headers(struct ftl_stream *stream, int64_t dts_usec);

static inline bool can_shutdown_stream(struct ftl_stream *stream,
				       struct encoder_packet *packet)
{
	uint64_t cur_time = os_gettime_ns();
	bool timeout = cur_time >= stream->shutdown_timeout_ts;

	if (timeout)
		info("Stream shutdown timeout reached (%d second(s))",
		     stream->max_shutdown_time_sec);

	return timeout || packet->sys_dts_usec >= (int64_t)stream->stop_ts;
}

static void *send_thread(void *data)
{
	struct ftl_stream *stream = data;
	ftl_status_t status_code;

	os_set_thread_name("ftl-stream: send_thread");

	while (os_sem_wait(stream->send_sem) == 0) {
		struct encoder_packet packet;

		if (stopping(stream) && stream->stop_ts == 0) {
			break;
		}

		if (!get_next_packet(stream, &packet))
			continue;

		if (stopping(stream)) {
			if (can_shutdown_stream(stream, &packet)) {
				obs_encoder_packet_release(&packet);
				break;
			}
		}

		/* sends sps/pps on every key frame as this is typically
		 * required for webrtc */
		if (packet.keyframe) {
			if (!send_headers(stream, packet.dts_usec)) {
				os_atomic_set_bool(&stream->disconnected, true);
				break;
			}
		}

		if (send_packet(stream, &packet, false) < 0) {
			os_atomic_set_bool(&stream->disconnected, true);
			break;
		}
	}

	bool encode_error = os_atomic_load_bool(&stream->encode_error);

	if (disconnected(stream)) {
		info("Disconnected from %s", stream->path.array);
	} else if (encode_error) {
		info("Encoder error, disconnecting");
	} else {
		info("User stopped the stream");
	}

	if (!stopping(stream)) {
		pthread_detach(stream->send_thread);
		obs_output_signal_stop(stream->output, OBS_OUTPUT_DISCONNECTED);
	} else if (encode_error) {
		obs_output_signal_stop(stream->output, OBS_OUTPUT_ENCODE_ERROR);
	} else {
		obs_output_end_data_capture(stream->output);
	}

	info("ingest disconnect");

	status_code = ftl_ingest_disconnect(&stream->ftl_handle);
	if (status_code != FTL_SUCCESS) {
		printf("Failed to disconnect from ingest %d", status_code);
	}

	free_packets(stream);
	os_event_reset(stream->stop_event);
	os_atomic_set_bool(&stream->active, false);
	stream->sent_headers = false;
	return NULL;
}

static bool send_video_header(struct ftl_stream *stream, int64_t dts_usec)
{
	obs_output_t *context = stream->output;
	obs_encoder_t *vencoder = obs_output_get_video_encoder(context);
	uint8_t *header;
	size_t size;

	struct encoder_packet packet = {.type = OBS_ENCODER_VIDEO,
					.timebase_den = 1,
					.keyframe = true,
					.dts_usec = dts_usec};

	obs_encoder_get_extra_data(vencoder, &header, &size);
	packet.size = obs_parse_avc_header(&packet.data, header, size);
	return send_packet(stream, &packet, true) >= 0;
}

static inline bool send_headers(struct ftl_stream *stream, int64_t dts_usec)
{
	stream->sent_headers = true;

	if (!send_video_header(stream, dts_usec))
		return false;

	return true;
}

static inline bool reset_semaphore(struct ftl_stream *stream)
{
	os_sem_destroy(stream->send_sem);
	return os_sem_init(&stream->send_sem, 0) == 0;
}

#ifdef _WIN32
#define socklen_t int
#endif

static int init_send(struct ftl_stream *stream)
{
	int ret;

	reset_semaphore(stream);

	ret = pthread_create(&stream->send_thread, NULL, send_thread, stream);
	if (ret != 0) {
		warn("Failed to create send thread");
		return OBS_OUTPUT_ERROR;
	}

	os_atomic_set_bool(&stream->active, true);

	obs_output_begin_data_capture(stream->output, 0);

	return OBS_OUTPUT_SUCCESS;
}

static int try_connect(struct ftl_stream *stream)
{
	ftl_status_t status_code;

	if (dstr_is_empty(&stream->path)) {
		warn("URL is empty");
		return OBS_OUTPUT_BAD_PATH;
	}

	info("Connecting to FTL Ingest URL %s...", stream->path.array);

	stream->width = (int)obs_output_get_width(stream->output);
	stream->height = (int)obs_output_get_height(stream->output);

	status_code = ftl_ingest_connect(&stream->ftl_handle);
	if (status_code != FTL_SUCCESS) {
		if (status_code == FTL_BAD_OR_INVALID_STREAM_KEY) {
			blog(LOG_ERROR, "Invalid Key (%s)",
			     ftl_status_code_to_string(status_code));
			return OBS_OUTPUT_INVALID_STREAM;
		} else {
			warn("Ingest connect failed with: %s (%d)",
			     ftl_status_code_to_string(status_code),
			     status_code);
			return _ftl_error_to_obs_error(status_code);
		}
	}

	info("Connection to %s successful", stream->path.array);

	// Always get the peak bitrate when we are starting.
	set_peak_bitrate(stream);

	pthread_create(&stream->status_thread, NULL, status_thread, stream);

	return init_send(stream);
}

static bool ftl_stream_start(void *data)
{
	struct ftl_stream *stream = data;

	info("ftl_stream_start");

	// Mixer doesn't support bframes. So force them off.
	obs_encoder_t *video_encoder =
		obs_output_get_video_encoder(stream->output);
	obs_data_t *video_settings = obs_encoder_get_settings(video_encoder);
	obs_data_set_int(video_settings, "bf", 0);
	obs_data_release(video_settings);

	if (!obs_output_can_begin_data_capture(stream->output, 0)) {
		return false;
	}
	if (!obs_output_initialize_encoders(stream->output, 0)) {
		return false;
	}

	stream->frames_sent = 0;
	os_atomic_set_bool(&stream->connecting, true);

	return pthread_create(&stream->connect_thread, NULL, connect_thread,
			      stream) == 0;
}

static inline bool add_packet(struct ftl_stream *stream,
			      struct encoder_packet *packet)
{
	circlebuf_push_back(&stream->packets, packet,
			    sizeof(struct encoder_packet));
	return true;
}

static inline size_t num_buffered_packets(struct ftl_stream *stream)
{
	return stream->packets.size / sizeof(struct encoder_packet);
}

static void drop_frames(struct ftl_stream *stream, const char *name,
			int highest_priority, bool pframes)
{
	UNUSED_PARAMETER(pframes);

	struct circlebuf new_buf = {0};
	int num_frames_dropped = 0;

#ifdef _DEBUG
	int start_packets = (int)num_buffered_packets(stream);
#else
	UNUSED_PARAMETER(name);
#endif

	circlebuf_reserve(&new_buf, sizeof(struct encoder_packet) * 8);

	while (stream->packets.size) {
		struct encoder_packet packet;
		circlebuf_pop_front(&stream->packets, &packet, sizeof(packet));

		/* do not drop audio data or video keyframes */
		if (packet.type == OBS_ENCODER_AUDIO ||
		    packet.drop_priority >= highest_priority) {
			circlebuf_push_back(&new_buf, &packet, sizeof(packet));

		} else {
			num_frames_dropped++;
			obs_encoder_packet_release(&packet);
		}
	}

	circlebuf_free(&stream->packets);
	stream->packets = new_buf;

	if (stream->min_priority < highest_priority)
		stream->min_priority = highest_priority;
	if (!num_frames_dropped)
		return;

	stream->dropped_frames += num_frames_dropped;
#ifdef _DEBUG
	debug("Dropped %s, prev packet count: %d, new packet count: %d", name,
	      start_packets, (int)num_buffered_packets(stream));
#endif
}

static bool find_first_video_packet(struct ftl_stream *stream,
				    struct encoder_packet *first)
{
	size_t count = stream->packets.size / sizeof(*first);

	for (size_t i = 0; i < count; i++) {
		struct encoder_packet *cur =
			circlebuf_data(&stream->packets, i * sizeof(*first));
		if (cur->type == OBS_ENCODER_VIDEO && !cur->keyframe) {
			*first = *cur;
			return true;
		}
	}

	return false;
}

static void check_to_drop_frames(struct ftl_stream *stream, bool pframes)
{
	struct encoder_packet first;
	int64_t buffer_duration_usec;
	size_t num_packets = num_buffered_packets(stream);
	const char *name = pframes ? "p-frames" : "b-frames";
	int priority = pframes ? OBS_NAL_PRIORITY_HIGHEST
			       : OBS_NAL_PRIORITY_HIGH;
	int64_t drop_threshold = pframes ? stream->pframe_drop_threshold_usec
					 : stream->drop_threshold_usec;

	if (num_packets < 5) {
		if (!pframes)
			stream->congestion = 0.0f;
		return;
	}

	if (!find_first_video_packet(stream, &first))
		return;

	/* if the amount of time stored in the buffered packets waiting to be
	 * sent is higher than threshold, drop frames */
	buffer_duration_usec = stream->last_dts_usec - first.dts_usec;

	if (!pframes) {
		stream->congestion =
			(float)buffer_duration_usec / (float)drop_threshold;
	}

	if (buffer_duration_usec > drop_threshold) {
		debug("buffer_duration_usec: %" PRId64, buffer_duration_usec);
		drop_frames(stream, name, priority, pframes);
	}
}

static bool add_video_packet(struct ftl_stream *stream,
			     struct encoder_packet *packet)
{
	check_to_drop_frames(stream, false);
	check_to_drop_frames(stream, true);

	/* if currently dropping frames, drop packets until it reaches the
	 * desired priority */
	if (packet->priority < stream->min_priority) {
		stream->dropped_frames++;
		return false;
	} else {
		stream->min_priority = 0;
	}

	stream->last_dts_usec = packet->dts_usec;
	return add_packet(stream, packet);
}

static void ftl_stream_data(void *data, struct encoder_packet *packet)
{
	struct ftl_stream *stream = data;

	struct encoder_packet new_packet;
	bool added_packet = false;

	if (disconnected(stream) || !active(stream))
		return;

	/* encoder failure */
	if (!packet) {
		os_atomic_set_bool(&stream->encode_error, true);
		os_sem_post(stream->send_sem);
		return;
	}

	if (packet->type == OBS_ENCODER_VIDEO)
		obs_parse_avc_packet(&new_packet, packet);
	else
		obs_encoder_packet_ref(&new_packet, packet);

	pthread_mutex_lock(&stream->packets_mutex);

	if (!disconnected(stream)) {
		added_packet = (packet->type == OBS_ENCODER_VIDEO)
				       ? add_video_packet(stream, &new_packet)
				       : add_packet(stream, &new_packet);
	}

	pthread_mutex_unlock(&stream->packets_mutex);

	if (added_packet)
		os_sem_post(stream->send_sem);
	else
		obs_encoder_packet_release(&new_packet);
}

static void ftl_stream_defaults(obs_data_t *defaults)
{
	UNUSED_PARAMETER(defaults);
}

static obs_properties_t *ftl_stream_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();
	obs_properties_add_int(props, "peak_bitrate_kbps",
			       obs_module_text("FTLStream.PeakBitrate"), 1000,
			       10000, 500);

	return props;
}

static uint64_t ftl_stream_total_bytes_sent(void *data)
{
	struct ftl_stream *stream = data;

	return stream->total_bytes_sent;
}

static int ftl_stream_dropped_frames(void *data)
{
	struct ftl_stream *stream = data;
	return (int)stream->dropped_frames;
}

static float ftl_stream_congestion(void *data)
{
	struct ftl_stream *stream = data;
	return stream->min_priority > 0 ? 1.0f : stream->congestion;
}

enum ret_type {
	RET_CONTINUE,
	RET_BREAK,
	RET_EXIT,
};

static enum ret_type ftl_event(struct ftl_stream *stream,
			       ftl_status_msg_t status)
{
	if (status.msg.event.type != FTL_STATUS_EVENT_TYPE_DISCONNECTED)
		return RET_CONTINUE;

	info("Disconnected from ingest with reason: %s",
	     ftl_status_code_to_string(status.msg.event.error_code));

	if (status.msg.event.reason == FTL_STATUS_EVENT_REASON_API_REQUEST) {
		return RET_BREAK;
	}

	//tell OBS and it will trigger a reconnection
	blog(LOG_WARNING, "Reconnecting to Ingest");
	obs_output_signal_stop(stream->output, OBS_OUTPUT_DISCONNECTED);
	reset_semaphore(stream);
	return RET_EXIT;
}

static void *status_thread(void *data)
{
	struct ftl_stream *stream = data;

	ftl_status_msg_t status;
	ftl_status_t status_code;

	while (!disconnected(stream)) {
		status_code = ftl_ingest_get_status(&stream->ftl_handle,
						    &status, 1000);

		if (status_code == FTL_STATUS_TIMEOUT ||
		    status_code == FTL_QUEUE_EMPTY) {
			continue;
		} else if (status_code == FTL_NOT_INITIALIZED) {
			break;
		}

		if (status.type == FTL_STATUS_EVENT) {
			enum ret_type ret_type = ftl_event(stream, status);
			if (ret_type == RET_EXIT)
				return NULL;
			else if (ret_type == RET_BREAK)
				break;

		} else if (status.type == FTL_STATUS_LOG) {
			blog(LOG_INFO, "[%d] %s", status.msg.log.log_level,
			     status.msg.log.string);

		} else if (status.type == FTL_STATUS_VIDEO_PACKETS) {
			ftl_packet_stats_msg_t *p = &status.msg.pkt_stats;

			// Report nack requests as dropped frames
			stream->dropped_frames +=
				p->nack_reqs - stream->last_nack_count;
			stream->last_nack_count = p->nack_reqs;

			int log_level = p->nack_reqs > 0 ? LOG_INFO : LOG_DEBUG;

			blog(log_level,
			     "Avg packet send per second %3.1f, "
			     "total nack requests %d",
			     (float)p->sent * 1000.f / p->period,
			     (int)p->nack_reqs);

		} else if (status.type == FTL_STATUS_VIDEO_PACKETS_INSTANT) {
			ftl_packet_stats_instant_msg_t *p =
				&status.msg.ipkt_stats;

			int log_level = p->avg_rtt > 20 ? LOG_INFO : LOG_DEBUG;

			blog(log_level,
			     "avg transmit delay %dms "
			     "(min: %d, max: %d), "
			     "avg rtt %dms (min: %d, max: %d)",
			     p->avg_xmit_delay, p->min_xmit_delay,
			     p->max_xmit_delay, p->avg_rtt, p->min_rtt,
			     p->max_rtt);

		} else if (status.type == FTL_STATUS_VIDEO) {
			ftl_video_frame_stats_msg_t *v =
				&status.msg.video_stats;

			int log_level = v->queue_fullness > 0 ? LOG_INFO
							      : LOG_DEBUG;

			blog(log_level,
			     "Queue an average of %3.2f fps "
			     "(%3.1f kbps), "
			     "sent an average of %3.2f fps "
			     "(%3.1f kbps), "
			     "queue fullness %d, "
			     "max frame size %d",
			     (float)v->frames_queued * 1000.f / v->period,
			     (float)v->bytes_queued / v->period * 8,
			     (float)v->frames_sent * 1000.f / v->period,
			     (float)v->bytes_sent / v->period * 8,
			     v->queue_fullness, v->max_frame_size);
		} else {
			blog(LOG_DEBUG,
			     "Status:  Got Status message of type "
			     "%d",
			     status.type);
		}
	}

	blog(LOG_DEBUG, "status_thread:  Exited");
	pthread_detach(stream->status_thread);
	return NULL;
}

static void *connect_thread(void *data)
{
	struct ftl_stream *stream = data;
	int ret;

	os_set_thread_name("ftl-stream: connect_thread");

	blog(LOG_WARNING, "ftl-stream: connect thread");

	ret = init_connect(stream);
	if (ret != OBS_OUTPUT_SUCCESS) {
		obs_output_signal_stop(stream->output, ret);
		return NULL;
	}

	ret = try_connect(stream);
	if (ret != OBS_OUTPUT_SUCCESS) {
		obs_output_signal_stop(stream->output, ret);
		info("Connection to %s failed: %d", stream->path.array, ret);
	}

	if (!stopping(stream))
		pthread_detach(stream->connect_thread);

	os_atomic_set_bool(&stream->connecting, false);
	return NULL;
}

static void log_libftl_messages(ftl_log_severity_t log_level,
				const char *message)
{
	UNUSED_PARAMETER(log_level);
	blog(LOG_WARNING, "[libftl] %s", message);
}

static int init_connect(struct ftl_stream *stream)
{
	obs_service_t *service;
	obs_data_t *settings;
	const char *bind_ip, *key, *ingest_url;
	ftl_status_t status_code;

	info("init_connect");

	if (stopping(stream))
		pthread_join(stream->send_thread, NULL);

	free_packets(stream);

	service = obs_output_get_service(stream->output);
	if (!service) {
		return OBS_OUTPUT_ERROR;
	}

	os_atomic_set_bool(&stream->disconnected, false);
	os_atomic_set_bool(&stream->encode_error, false);
	stream->total_bytes_sent = 0;
	stream->dropped_frames = 0;
	stream->min_priority = 0;

	settings = obs_output_get_settings(stream->output);
	obs_encoder_t *video_encoder =
		obs_output_get_video_encoder(stream->output);
	obs_data_t *video_settings = obs_encoder_get_settings(video_encoder);

	ingest_url = obs_service_get_url(service);
	if (strncmp(ingest_url, FTL_URL_PROTOCOL, strlen(FTL_URL_PROTOCOL)) ==
	    0) {
		dstr_copy(&stream->path, ingest_url + strlen(FTL_URL_PROTOCOL));
	} else {
		dstr_copy(&stream->path, ingest_url);
	}

	key = obs_service_get_key(service);

	int target_bitrate = (int)obs_data_get_int(video_settings, "bitrate");
	int peak_bitrate = (int)((float)target_bitrate * 1.1f);

	//minimum overshoot tolerance of 10%
	if (peak_bitrate < target_bitrate) {
		peak_bitrate = target_bitrate;
	}

	stream->params.stream_key = (char *)key;
	stream->params.video_codec = FTL_VIDEO_H264;
	stream->params.audio_codec = FTL_AUDIO_OPUS;
	stream->params.ingest_hostname = stream->path.array;
	stream->params.vendor_name = "OBS Studio";
	stream->params.vendor_version = OBS_VERSION;
	stream->params.peak_kbps = stream->peak_kbps < 0 ? 0
							 : stream->peak_kbps;

	//not required when using ftl_ingest_send_media_dts
	stream->params.fps_num = 0;
	stream->params.fps_den = 0;

	status_code = ftl_ingest_create(&stream->ftl_handle, &stream->params);
	if (status_code != FTL_SUCCESS) {
		if (status_code == FTL_BAD_OR_INVALID_STREAM_KEY) {
			blog(LOG_ERROR, "Invalid Key (%s)",
			     ftl_status_code_to_string(status_code));
			return OBS_OUTPUT_INVALID_STREAM;
		} else {
			blog(LOG_ERROR, "Failed to create ingest handle (%s)",
			     ftl_status_code_to_string(status_code));
			return OBS_OUTPUT_ERROR;
		}
	}

	dstr_copy(&stream->username, obs_service_get_username(service));
	dstr_copy(&stream->password, obs_service_get_password(service));
	dstr_depad(&stream->path);

	stream->drop_threshold_usec =
		(int64_t)obs_data_get_int(settings, OPT_DROP_THRESHOLD) * 1000;
	stream->max_shutdown_time_sec =
		(int)obs_data_get_int(settings, OPT_MAX_SHUTDOWN_TIME_SEC);

	bind_ip = obs_data_get_string(settings, OPT_BIND_IP);
	dstr_copy(&stream->bind_ip, bind_ip);

	obs_data_release(settings);
	obs_data_release(video_settings);
	return OBS_OUTPUT_SUCCESS;
}

// Returns 0 on success
static int _ftl_error_to_obs_error(int status)
{
	/* Map FTL errors to OBS errors */

	switch (status) {
	case FTL_SUCCESS:
		return OBS_OUTPUT_SUCCESS;
	case FTL_SOCKET_NOT_CONNECTED:
	case FTL_MALLOC_FAILURE:
	case FTL_INTERNAL_ERROR:
	case FTL_CONFIG_ERROR:
	case FTL_NOT_ACTIVE_STREAM:
	case FTL_NOT_CONNECTED:
	case FTL_ALREADY_CONNECTED:
	case FTL_STATUS_TIMEOUT:
	case FTL_QUEUE_FULL:
	case FTL_STATUS_WAITING_FOR_KEY_FRAME:
	case FTL_QUEUE_EMPTY:
	case FTL_NOT_INITIALIZED:
		return OBS_OUTPUT_ERROR;
	case FTL_BAD_REQUEST:
	case FTL_DNS_FAILURE:
	case FTL_CONNECT_ERROR:
	case FTL_UNSUPPORTED_MEDIA_TYPE:
	case FTL_OLD_VERSION:
	case FTL_UNAUTHORIZED:
	case FTL_AUDIO_SSRC_COLLISION:
	case FTL_VIDEO_SSRC_COLLISION:
	case FTL_STREAM_REJECTED:
	case FTL_BAD_OR_INVALID_STREAM_KEY:
	case FTL_CHANNEL_IN_USE:
	case FTL_REGION_UNSUPPORTED:
	case FTL_GAME_BLOCKED:
		return OBS_OUTPUT_CONNECT_FAILED;
	case FTL_NO_MEDIA_TIMEOUT:
		return OBS_OUTPUT_DISCONNECTED;
	case FTL_USER_DISCONNECT:
		return OBS_OUTPUT_SUCCESS;
	case FTL_UNKNOWN_ERROR_CODE:
	default:
		/* Unknown FTL error */
		return OBS_OUTPUT_ERROR;
	}
}

struct obs_output_info ftl_output_info = {
	.id = "ftl_output",
	.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED | OBS_OUTPUT_SERVICE,
	.encoded_video_codecs = "h264",
	.encoded_audio_codecs = "opus",
	.get_name = ftl_stream_getname,
	.create = ftl_stream_create,
	.destroy = ftl_stream_destroy,
	.start = ftl_stream_start,
	.stop = ftl_stream_stop,
	.encoded_packet = ftl_stream_data,
	.get_defaults = ftl_stream_defaults,
	.get_properties = ftl_stream_properties,
	.get_total_bytes = ftl_stream_total_bytes_sent,
	.get_congestion = ftl_stream_congestion,
	.get_dropped_frames = ftl_stream_dropped_frames,
};
