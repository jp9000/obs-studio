#include "obs-ffmpeg-mux.h"

#define do_log(level, format, ...)                      \
	blog(level, "[ffmpeg hls muxer: '%s'] " format, \
	     obs_output_get_name(stream->output), ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)

const char *ffmpeg_hls_mux_getname(void *type)
{
	UNUSED_PARAMETER(type);
	return obs_module_text("FFmpegHlsMuxer");
}

int hls_stream_dropped_frames(void *data)
{
	struct ffmpeg_muxer *stream = data;
	return stream->dropped_frames;
}

void ffmpeg_hls_mux_destroy(void *data)
{
	struct ffmpeg_muxer *stream = data;

	if (stream) {
		deactivate(stream, 0);

		pthread_mutex_destroy(&stream->write_mutex);
		os_sem_destroy(stream->write_sem);
		os_event_destroy(stream->stop_event);

		da_free(stream->mux_packets);
		circlebuf_free(&stream->packets);

		os_process_pipe_destroy(stream->pipe);
		dstr_free(&stream->path);
		dstr_free(&stream->printable_path);
		dstr_free(&stream->stream_key);
		dstr_free(&stream->muxer_settings);
		bfree(data);
	}
}

void *ffmpeg_hls_mux_create(obs_data_t *settings, obs_output_t *output)
{
	struct ffmpeg_muxer *stream = bzalloc(sizeof(*stream));
	pthread_mutex_init_value(&stream->write_mutex);
	stream->output = output;

	/* init mutex, semaphore and event */
	if (pthread_mutex_init(&stream->write_mutex, NULL) != 0)
		goto fail;
	if (os_event_init(&stream->stop_event, OS_EVENT_TYPE_AUTO) != 0)
		goto fail;
	if (os_sem_init(&stream->write_sem, 0) != 0)
		goto fail;

	UNUSED_PARAMETER(settings);
	return stream;

fail:
	ffmpeg_hls_mux_destroy(stream);
	return NULL;
}

static bool process_packet(struct ffmpeg_muxer *stream)
{
	struct encoder_packet packet;
	bool has_packet = false;
	bool ret = true;

	pthread_mutex_lock(&stream->write_mutex);

	if (stream->packets.size) {
		circlebuf_pop_front(&stream->packets, &packet, sizeof(packet));
		has_packet = true;
	}

	pthread_mutex_unlock(&stream->write_mutex);

	if (has_packet) {
		ret = write_packet(stream, &packet);
		obs_encoder_packet_release(&packet);
	}
	return ret;
}

static void *write_thread(void *data)
{
	struct ffmpeg_muxer *stream = data;

	while (os_sem_wait(stream->write_sem) == 0) {
		if (os_event_try(stream->stop_event) == 0)
			return NULL;

		if (!process_packet(stream))
			break;
	}

	obs_output_signal_stop(stream->output, OBS_OUTPUT_ERROR);
	deactivate(stream, 0);
	return NULL;
}

bool ffmpeg_hls_mux_start(void *data)
{
	struct ffmpeg_muxer *stream = data;
	obs_service_t *service;
	const char *path_str;
	const char *stream_key;
	struct dstr path = {0};
	obs_encoder_t *vencoder;
	obs_data_t *settings;
	int keyint_sec;

	if (!obs_output_can_begin_data_capture(stream->output, 0))
		return false;
	if (!obs_output_initialize_encoders(stream->output, 0))
		return false;

	service = obs_output_get_service(stream->output);
	if (!service)
		return false;
	path_str = obs_service_get_url(service);
	stream_key = obs_service_get_key(service);
	dstr_copy(&stream->stream_key, stream_key);
	dstr_copy(&path, path_str);
	dstr_replace(&path, "{stream_key}", stream_key);
	dstr_copy(&stream->muxer_settings,
		  "method=PUT http_persistent=1 ignore_io_errors=1 ");
	dstr_catf(&stream->muxer_settings, "http_user_agent=libobs/%s",
		  OBS_VERSION);

	vencoder = obs_output_get_video_encoder(stream->output);
	settings = obs_encoder_get_settings(vencoder);
	keyint_sec = (int)obs_data_get_int(settings, "keyint_sec");
	if (keyint_sec) {
		dstr_catf(&stream->muxer_settings, " hls_time=%d", keyint_sec);
		stream->keyint_sec = keyint_sec;
	}

	obs_data_release(settings);

	start_pipe(stream, path.array);
	dstr_free(&path);

	if (!stream->pipe) {
		obs_output_set_last_error(
			stream->output, obs_module_text("HelperProcessFailed"));
		warn("Failed to create process pipe");
		return false;
	}
	stream->mux_thread_joinable = pthread_create(&stream->mux_thread, NULL,
						     write_thread, stream) == 0;
	if (!stream->mux_thread_joinable)
		return false;

	/* write headers and start capture */
	os_atomic_set_bool(&stream->active, true);
	os_atomic_set_bool(&stream->capturing, true);
	stream->is_hls = true;
	stream->total_bytes = 0;
	stream->dropped_frames = 0;
	stream->min_priority = 0;

	obs_output_begin_data_capture(stream->output, 0);

	dstr_copy(&stream->printable_path, path_str);
	info("Writing to path '%s'...", stream->printable_path.array);
	return true;
}

static bool write_packet_to_buf(struct ffmpeg_muxer *stream,
				struct encoder_packet *packet)
{
	circlebuf_push_back(&stream->packets, packet,
			    sizeof(struct encoder_packet));
	return true;
}

static void drop_frames(struct ffmpeg_muxer *stream, int highest_priority)
{
	struct circlebuf new_buf = {0};
	int num_frames_dropped = 0;

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

	stream->dropped_frames += num_frames_dropped;
}

static bool find_first_video_packet(struct ffmpeg_muxer *stream,
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

void check_to_drop_frames(struct ffmpeg_muxer *stream, bool pframes)
{
	struct encoder_packet first;
	int64_t buffer_duration_usec;
	int priority = pframes ? OBS_NAL_PRIORITY_HIGHEST
			       : OBS_NAL_PRIORITY_HIGH;
	int keyint_sec = stream->keyint_sec;
	int64_t drop_threshold_sec = keyint_sec ? 2 * keyint_sec : 10;

	if (!find_first_video_packet(stream, &first))
		return;

	buffer_duration_usec = stream->last_dts_usec - first.dts_usec;

	if (buffer_duration_usec > drop_threshold_sec * 1000000)
		drop_frames(stream, priority);
}

static bool add_video_packet(struct ffmpeg_muxer *stream,
			     struct encoder_packet *packet)
{
	check_to_drop_frames(stream, false);
	check_to_drop_frames(stream, true);

	/* if currently dropping frames, drop packets until it reaches the
	 * desired priority */
	if (packet->drop_priority < stream->min_priority) {
		stream->dropped_frames++;
		return false;
	} else {
		stream->min_priority = 0;
	}

	stream->last_dts_usec = packet->dts_usec;
	return write_packet_to_buf(stream, packet);
}

void ffmpeg_hls_mux_data(void *data, struct encoder_packet *packet)
{
	struct ffmpeg_muxer *stream = data;
	struct encoder_packet new_packet;
	struct encoder_packet tmp_packet;
	bool added_packet = false;

	if (!active(stream))
		return;

	/* encoder failure */
	if (!packet) {
		deactivate(stream, OBS_OUTPUT_ENCODE_ERROR);
		return;
	}

	if (!stream->sent_headers) {
		if (!send_headers(stream))
			return;
		stream->sent_headers = true;
	}

	if (stopping(stream)) {
		if (packet->sys_dts_usec >= stream->stop_ts) {
			deactivate(stream, 0);
			return;
		}
	}

	if (packet->type == OBS_ENCODER_VIDEO) {
		obs_parse_avc_packet(&tmp_packet, packet);
		packet->drop_priority = tmp_packet.priority;
		obs_encoder_packet_release(&tmp_packet);
	}
	obs_encoder_packet_ref(&new_packet, packet);

	pthread_mutex_lock(&stream->write_mutex);

	if (active(stream)) {
		added_packet =
			(packet->type == OBS_ENCODER_VIDEO)
				? add_video_packet(stream, &new_packet)
				: write_packet_to_buf(stream, &new_packet);
	}

	pthread_mutex_unlock(&stream->write_mutex);

	if (added_packet)
		os_sem_post(stream->write_sem);
	else
		obs_encoder_packet_release(&new_packet);
}

struct obs_output_info ffmpeg_hls_muxer = {
	.id = "ffmpeg_hls_muxer",
	.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED | OBS_OUTPUT_MULTI_TRACK |
		 OBS_OUTPUT_SERVICE,
	.encoded_video_codecs = "h264",
	.encoded_audio_codecs = "aac",
	.get_name = ffmpeg_hls_mux_getname,
	.create = ffmpeg_hls_mux_create,
	.destroy = ffmpeg_hls_mux_destroy,
	.start = ffmpeg_hls_mux_start,
	.stop = ffmpeg_mux_stop,
	.encoded_packet = ffmpeg_hls_mux_data,
	.get_total_bytes = ffmpeg_mux_total_bytes,
	.get_dropped_frames = hls_stream_dropped_frames,
};
