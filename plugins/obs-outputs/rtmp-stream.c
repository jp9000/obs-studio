/******************************************************************************
    Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

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

#include "rtmp-stream.h"

#ifndef SEC_TO_NSEC
#define SEC_TO_NSEC 1000000000ULL
#endif

#ifndef MSEC_TO_USEC
#define MSEC_TO_USEC 1000ULL
#endif

#ifndef MSEC_TO_NSEC
#define MSEC_TO_NSEC 1000000ULL
#endif

/* dynamic bitrate coefficients */
#define DBR_INC_TIMER (30ULL * SEC_TO_NSEC)
#define DBR_TRIGGER_USEC (200ULL * MSEC_TO_USEC)
#define MIN_ESTIMATE_DURATION_MS 1000
#define MAX_ESTIMATE_DURATION_MS 2000

static const char *rtmp_stream_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("RTMPStream");
}

static void log_rtmp(int level, const char *format, va_list args)
{
	if (level > RTMP_LOGWARNING)
		return;

	blogva(LOG_INFO, format, args);
}

static inline size_t num_buffered_packets(struct rtmp_stream *stream);

static inline void free_packets(struct rtmp_stream *stream)
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

static inline bool stopping(struct rtmp_stream *stream)
{
	return os_event_try(stream->stop_event) != EAGAIN;
}

static inline bool connecting(struct rtmp_stream *stream)
{
	return os_atomic_load_bool(&stream->connecting);
}

static inline bool active(struct rtmp_stream *stream)
{
	return os_atomic_load_bool(&stream->active);
}

static inline bool disconnected(struct rtmp_stream *stream)
{
	return os_atomic_load_bool(&stream->disconnected);
}

static void rtmp_stream_destroy(void *data)
{
	struct rtmp_stream *stream = data;

	if (stopping(stream) && !connecting(stream)) {
		pthread_join(stream->send_thread, NULL);

	} else if (connecting(stream) || active(stream)) {
		if (stream->connecting)
			pthread_join(stream->connect_thread, NULL);

		stream->stop_ts = 0;
		os_event_signal(stream->stop_event);

		if (active(stream)) {
			os_sem_post(stream->send_sem);
			obs_output_end_data_capture(stream->output);
			pthread_join(stream->send_thread, NULL);
		}
	}

	RTMP_TLS_Free(&stream->rtmp);
	free_packets(stream);
	dstr_free(&stream->path);
	dstr_free(&stream->key);
	dstr_free(&stream->username);
	dstr_free(&stream->password);
	dstr_free(&stream->encoder_name);
	dstr_free(&stream->bind_ip);
	os_event_destroy(stream->stop_event);
	os_sem_destroy(stream->send_sem);
	pthread_mutex_destroy(&stream->packets_mutex);
	circlebuf_free(&stream->packets);
#ifdef TEST_FRAMEDROPS
	circlebuf_free(&stream->droptest_info);
#endif
	circlebuf_free(&stream->dbr_frames);
	pthread_mutex_destroy(&stream->dbr_mutex);

	os_event_destroy(stream->buffer_space_available_event);
	os_event_destroy(stream->buffer_has_data_event);
	os_event_destroy(stream->socket_available_event);
	os_event_destroy(stream->send_thread_signaled_exit);
	pthread_mutex_destroy(&stream->write_buf_mutex);

	if (stream->write_buf)
		bfree(stream->write_buf);
	bfree(stream);
}

static void *rtmp_stream_create(obs_data_t *settings, obs_output_t *output)
{
	struct rtmp_stream *stream = bzalloc(sizeof(struct rtmp_stream));
	stream->output = output;
	pthread_mutex_init_value(&stream->packets_mutex);

	RTMP_LogSetCallback(log_rtmp);
	RTMP_Init(&stream->rtmp);
	RTMP_LogSetLevel(RTMP_LOGWARNING);

	if (pthread_mutex_init(&stream->packets_mutex, NULL) != 0)
		goto fail;
	if (os_event_init(&stream->stop_event, OS_EVENT_TYPE_MANUAL) != 0)
		goto fail;

	if (pthread_mutex_init(&stream->write_buf_mutex, NULL) != 0) {
		warn("Failed to initialize write buffer mutex");
		goto fail;
	}

	if (pthread_mutex_init(&stream->dbr_mutex, NULL) != 0) {
		warn("Failed to initialize dbr mutex");
		goto fail;
	}

	if (os_event_init(&stream->buffer_space_available_event,
			  OS_EVENT_TYPE_AUTO) != 0) {
		warn("Failed to initialize write buffer event");
		goto fail;
	}
	if (os_event_init(&stream->buffer_has_data_event, OS_EVENT_TYPE_AUTO) !=
	    0) {
		warn("Failed to initialize data buffer event");
		goto fail;
	}
	if (os_event_init(&stream->socket_available_event,
			  OS_EVENT_TYPE_AUTO) != 0) {
		warn("Failed to initialize socket buffer event");
		goto fail;
	}
	if (os_event_init(&stream->send_thread_signaled_exit,
			  OS_EVENT_TYPE_MANUAL) != 0) {
		warn("Failed to initialize socket exit event");
		goto fail;
	}

	UNUSED_PARAMETER(settings);
	return stream;

fail:
	rtmp_stream_destroy(stream);
	return NULL;
}

static void rtmp_stream_stop(void *data, uint64_t ts)
{
	struct rtmp_stream *stream = data;

	if (stopping(stream) && ts != 0)
		return;

	if (connecting(stream))
		pthread_join(stream->connect_thread, NULL);

	stream->stop_ts = ts / 1000ULL;

	if (ts)
		stream->shutdown_timeout_ts =
			ts +
			(uint64_t)stream->max_shutdown_time_sec * 1000000000ULL;

	if (active(stream)) {
		os_event_signal(stream->stop_event);
		if (stream->stop_ts == 0)
			os_sem_post(stream->send_sem);
	} else {
		obs_output_signal_stop(stream->output, OBS_OUTPUT_SUCCESS);
	}
}

static inline void set_rtmp_str(AVal *val, const char *str)
{
	bool valid = (str && *str);
	val->av_val = valid ? (char *)str : NULL;
	val->av_len = valid ? (int)strlen(str) : 0;
}

static inline void set_rtmp_dstr(AVal *val, struct dstr *str)
{
	bool valid = !dstr_is_empty(str);
	val->av_val = valid ? str->array : NULL;
	val->av_len = valid ? (int)str->len : 0;
}

static inline bool get_next_packet(struct rtmp_stream *stream,
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

static bool discard_recv_data(struct rtmp_stream *stream, size_t size)
{
	RTMP *rtmp = &stream->rtmp;
	uint8_t buf[512];
#ifdef _WIN32
	int ret;
#else
	ssize_t ret;
#endif

	do {
		size_t bytes = size > 512 ? 512 : size;
		size -= bytes;

#ifdef _WIN32
		ret = recv(rtmp->m_sb.sb_socket, buf, (int)bytes, 0);
#else
		ret = recv(rtmp->m_sb.sb_socket, buf, bytes, 0);
#endif

		if (ret <= 0) {
#ifdef _WIN32
			int error = WSAGetLastError();
#else
			int error = errno;
#endif
			if (ret < 0) {
				do_log(LOG_ERROR, "recv error: %d (%d bytes)",
				       error, (int)size);
			}
			return false;
		}
	} while (size > 0);

	return true;
}

#ifdef TEST_FRAMEDROPS
static void droptest_cap_data_rate(struct rtmp_stream *stream, size_t size)
{
	uint64_t ts = os_gettime_ns();
	struct droptest_info info;

#if defined(_WIN32) && defined(TEST_FRAMEDROPS_WITH_BITRATE_SHORTCUTS)
	uint64_t check_elapsed = ts - stream->droptest_last_key_check;

	if (check_elapsed > (200ULL * MSEC_TO_NSEC)) {
		size_t bitrate = 0;

		stream->droptest_last_key_check = ts;

		if (GetAsyncKeyState(VK_NUMPAD0) & 0x8000) {
			stream->droptest_max = 0;
		} else if (GetAsyncKeyState(VK_NUMPAD1) & 0x8000) {
			bitrate = 1000;
		} else if (GetAsyncKeyState(VK_NUMPAD2) & 0x8000) {
			bitrate = 2000;
		} else if (GetAsyncKeyState(VK_NUMPAD3) & 0x8000) {
			bitrate = 3000;
		} else if (GetAsyncKeyState(VK_NUMPAD4) & 0x8000) {
			bitrate = 4000;
		} else if (GetAsyncKeyState(VK_NUMPAD5) & 0x8000) {
			bitrate = 5000;
		} else if (GetAsyncKeyState(VK_NUMPAD6) & 0x8000) {
			bitrate = 6000;
		} else if (GetAsyncKeyState(VK_NUMPAD7) & 0x8000) {
			bitrate = 7000;
		} else if (GetAsyncKeyState(VK_NUMPAD8) & 0x8000) {
			bitrate = 8000;
		} else if (GetAsyncKeyState(VK_NUMPAD9) & 0x8000) {
			bitrate = 9000;
		}

		if (bitrate) {
			stream->droptest_max = (bitrate * 1000 / 8);
		}
	}
	if (!stream->droptest_max) {
		return;
	}
#else
	if (!stream->droptest_max) {
		stream->droptest_max = DROPTEST_MAX_BYTES;
	}
#endif

	info.ts = ts;
	info.size = size;

	circlebuf_push_back(&stream->droptest_info, &info, sizeof(info));
	stream->droptest_size += size;

	if (stream->droptest_info.size) {
		circlebuf_peek_front(&stream->droptest_info, &info,
				     sizeof(info));

		if (stream->droptest_size > stream->droptest_max) {
			uint64_t elapsed = ts - info.ts;

			if (elapsed < 1000000000ULL) {
				elapsed = 1000000000ULL - elapsed;
				os_sleepto_ns(ts + elapsed);
			}

			while (stream->droptest_size > stream->droptest_max) {
				circlebuf_pop_front(&stream->droptest_info,
						    &info, sizeof(info));
				stream->droptest_size -= info.size;
			}
		}
	}
}
#endif

static int socket_queue_data(RTMPSockBuf *sb, const char *data, int len,
			     void *arg)
{
	UNUSED_PARAMETER(sb);

	struct rtmp_stream *stream = arg;

retry_send:

	if (!RTMP_IsConnected(&stream->rtmp))
		return 0;

	pthread_mutex_lock(&stream->write_buf_mutex);

	if (stream->write_buf_len + len > stream->write_buf_size) {

		pthread_mutex_unlock(&stream->write_buf_mutex);

		if (os_event_wait(stream->buffer_space_available_event)) {
			return 0;
		}

		goto retry_send;
	}

	memcpy(stream->write_buf + stream->write_buf_len, data, len);
	stream->write_buf_len += len;

	pthread_mutex_unlock(&stream->write_buf_mutex);

	os_event_signal(stream->buffer_has_data_event);

	return len;
}

static int send_packet(struct rtmp_stream *stream,
		       struct encoder_packet *packet, bool is_header,
		       size_t idx)
{
	uint8_t *data;
	size_t size;
	int recv_size = 0;
	int ret = 0;

	assert(idx < RTMP_MAX_STREAMS);

	if (!stream->new_socket_loop) {
#ifdef _WIN32
		ret = ioctlsocket(stream->rtmp.m_sb.sb_socket, FIONREAD,
				  (u_long *)&recv_size);
#else
		ret = ioctl(stream->rtmp.m_sb.sb_socket, FIONREAD, &recv_size);
#endif

		if (ret >= 0 && recv_size > 0) {
			if (!discard_recv_data(stream, (size_t)recv_size))
				return -1;
		}
	}

	if (idx > 0) {
		flv_additional_packet_mux(
			packet, is_header ? 0 : stream->start_dts_offset, &data,
			&size, is_header, idx);
	} else {
		flv_packet_mux(packet, is_header ? 0 : stream->start_dts_offset,
			       &data, &size, is_header);
	}

#ifdef TEST_FRAMEDROPS
	droptest_cap_data_rate(stream, size);
#endif

	ret = RTMP_Write(&stream->rtmp, (char *)data, (int)size, 0);
	bfree(data);

	if (is_header)
		bfree(packet->data);
	else
		obs_encoder_packet_release(packet);

	stream->total_bytes_sent += size;
	return ret;
}

static inline bool send_headers(struct rtmp_stream *stream);

static inline bool can_shutdown_stream(struct rtmp_stream *stream,
				       struct encoder_packet *packet)
{
	uint64_t cur_time = os_gettime_ns();
	bool timeout = cur_time >= stream->shutdown_timeout_ts;

	if (timeout)
		info("Stream shutdown timeout reached (%d second(s))",
		     stream->max_shutdown_time_sec);

	return timeout || packet->sys_dts_usec >= (int64_t)stream->stop_ts;
}

static void set_output_error(struct rtmp_stream *stream)
{
	const char *msg = NULL;
#ifdef _WIN32
	switch (stream->rtmp.last_error_code) {
	case WSAETIMEDOUT:
		msg = obs_module_text("ConnectionTimedOut");
		break;
	case WSAEACCES:
		msg = obs_module_text("PermissionDenied");
		break;
	case WSAECONNABORTED:
		msg = obs_module_text("ConnectionAborted");
		break;
	case WSAECONNRESET:
		msg = obs_module_text("ConnectionReset");
		break;
	case WSAHOST_NOT_FOUND:
		msg = obs_module_text("HostNotFound");
		break;
	case WSANO_DATA:
		msg = obs_module_text("NoData");
		break;
	case WSAEADDRNOTAVAIL:
		msg = obs_module_text("AddressNotAvailable");
		break;
	}
#else
	switch (stream->rtmp.last_error_code) {
	case ETIMEDOUT:
		msg = obs_module_text("ConnectionTimedOut");
		break;
	case EACCES:
		msg = obs_module_text("PermissionDenied");
		break;
	case ECONNABORTED:
		msg = obs_module_text("ConnectionAborted");
		break;
	case ECONNRESET:
		msg = obs_module_text("ConnectionReset");
		break;
	case HOST_NOT_FOUND:
		msg = obs_module_text("HostNotFound");
		break;
	case NO_DATA:
		msg = obs_module_text("NoData");
		break;
	case EADDRNOTAVAIL:
		msg = obs_module_text("AddressNotAvailable");
		break;
	}
#endif

	// non platform-specific errors
	if (!msg) {
		switch (stream->rtmp.last_error_code) {
		case -0x2700:
			msg = obs_module_text("SSLCertVerifyFailed");
			break;
		case -0x7680:
			msg = "Failed to load root certificates for a secure TLS connection."
#if defined(__linux__)
			      " Check you have an up to date root certificate bundle in /etc/ssl/certs."
#endif
				;
			break;
		}
	}

	if (msg)
		obs_output_set_last_error(stream->output, msg);
}

static void dbr_add_frame(struct rtmp_stream *stream, struct dbr_frame *back)
{
	struct dbr_frame front;
	uint64_t dur;

	circlebuf_push_back(&stream->dbr_frames, back, sizeof(*back));
	circlebuf_peek_front(&stream->dbr_frames, &front, sizeof(front));

	stream->dbr_data_size += back->size;

	dur = (back->send_end - front.send_beg) / 1000000;

	if (dur >= MAX_ESTIMATE_DURATION_MS) {
		stream->dbr_data_size -= front.size;
		circlebuf_pop_front(&stream->dbr_frames, NULL, sizeof(front));
	}

	stream->dbr_est_bitrate =
		(dur >= MIN_ESTIMATE_DURATION_MS)
			? (long)(stream->dbr_data_size * 1000 / dur)
			: 0;
	stream->dbr_est_bitrate *= 8;
	stream->dbr_est_bitrate /= 1000;

	if (stream->dbr_est_bitrate) {
		stream->dbr_est_bitrate -= stream->audio_bitrate;
		if (stream->dbr_est_bitrate < 50)
			stream->dbr_est_bitrate = 50;
	}
}

static void dbr_set_bitrate(struct rtmp_stream *stream);

static void *send_thread(void *data)
{
	struct rtmp_stream *stream = data;

	os_set_thread_name("rtmp-stream: send_thread");

	while (os_sem_wait(stream->send_sem) == 0) {
		struct encoder_packet packet;
		struct dbr_frame dbr_frame;

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

		if (!stream->sent_headers) {
			if (!send_headers(stream)) {
				os_atomic_set_bool(&stream->disconnected, true);
				break;
			}
		}

		if (stream->dbr_enabled) {
			dbr_frame.send_beg = os_gettime_ns();
			dbr_frame.size = packet.size;
		}

		if (send_packet(stream, &packet, false, packet.track_idx) < 0) {
			os_atomic_set_bool(&stream->disconnected, true);
			break;
		}

		if (stream->dbr_enabled) {
			dbr_frame.send_end = os_gettime_ns();

			pthread_mutex_lock(&stream->dbr_mutex);
			dbr_add_frame(stream, &dbr_frame);
			pthread_mutex_unlock(&stream->dbr_mutex);
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

	if (stream->new_socket_loop) {
		os_event_signal(stream->send_thread_signaled_exit);
		os_event_signal(stream->buffer_has_data_event);
		pthread_join(stream->socket_thread, NULL);
		stream->socket_thread_active = false;
		stream->rtmp.m_bCustomSend = false;
	}

	set_output_error(stream);
	RTMP_Close(&stream->rtmp);

	if (!stopping(stream)) {
		pthread_detach(stream->send_thread);
		obs_output_signal_stop(stream->output, OBS_OUTPUT_DISCONNECTED);
	} else if (encode_error) {
		obs_output_signal_stop(stream->output, OBS_OUTPUT_ENCODE_ERROR);
	} else {
		obs_output_end_data_capture(stream->output);
	}

	free_packets(stream);
	os_event_reset(stream->stop_event);
	os_atomic_set_bool(&stream->active, false);
	stream->sent_headers = false;

	/* reset bitrate on stop */
	if (stream->dbr_enabled) {
		if (stream->dbr_cur_bitrate != stream->dbr_orig_bitrate) {
			stream->dbr_cur_bitrate = stream->dbr_orig_bitrate;
			dbr_set_bitrate(stream);
		}
	}

	return NULL;
}

static bool send_additional_meta_data(struct rtmp_stream *stream)
{
	uint8_t *meta_data;
	size_t meta_data_size;
	bool success = true;

	flv_additional_meta_data(stream->output, &meta_data, &meta_data_size);
	success = RTMP_Write(&stream->rtmp, (char *)meta_data,
			     (int)meta_data_size, 0) >= 0;
	bfree(meta_data);

	return success;
}

static bool send_meta_data(struct rtmp_stream *stream)
{
	uint8_t *meta_data;
	size_t meta_data_size;
	bool success = true;

	flv_meta_data(stream->output, &meta_data, &meta_data_size, false);
	success = RTMP_Write(&stream->rtmp, (char *)meta_data,
			     (int)meta_data_size, 0) >= 0;
	bfree(meta_data);

	return success;
}

static bool send_audio_header(struct rtmp_stream *stream, size_t idx,
			      bool *next)
{
	obs_output_t *context = stream->output;
	obs_encoder_t *aencoder = obs_output_get_audio_encoder(context, idx);
	uint8_t *header;

	struct encoder_packet packet = {.type = OBS_ENCODER_AUDIO,
					.timebase_den = 1};

	if (!aencoder) {
		*next = false;
		return true;
	}

	obs_encoder_get_extra_data(aencoder, &header, &packet.size);
	packet.data = bmemdup(header, packet.size);
	return send_packet(stream, &packet, true, idx) >= 0;
}

static bool send_video_header(struct rtmp_stream *stream)
{
	obs_output_t *context = stream->output;
	obs_encoder_t *vencoder = obs_output_get_video_encoder(context);
	uint8_t *header;
	size_t size;

	struct encoder_packet packet = {
		.type = OBS_ENCODER_VIDEO, .timebase_den = 1, .keyframe = true};

	obs_encoder_get_extra_data(vencoder, &header, &size);
	packet.size = obs_parse_avc_header(&packet.data, header, size);
	return send_packet(stream, &packet, true, 0) >= 0;
}

static inline bool send_headers(struct rtmp_stream *stream)
{
	stream->sent_headers = true;
	size_t i = 0;
	bool next = true;

	if (!send_audio_header(stream, i++, &next))
		return false;
	if (!send_video_header(stream))
		return false;

	while (next) {
		if (!send_audio_header(stream, i++, &next))
			return false;
	}

	return true;
}

static inline bool reset_semaphore(struct rtmp_stream *stream)
{
	os_sem_destroy(stream->send_sem);
	return os_sem_init(&stream->send_sem, 0) == 0;
}

#ifdef _WIN32
#define socklen_t int
#endif

#define MIN_SENDBUF_SIZE 65535

static void adjust_sndbuf_size(struct rtmp_stream *stream, int new_size)
{
	int cur_sendbuf_size = new_size;
	socklen_t int_size = sizeof(int);

	getsockopt(stream->rtmp.m_sb.sb_socket, SOL_SOCKET, SO_SNDBUF,
		   (char *)&cur_sendbuf_size, &int_size);

	if (cur_sendbuf_size < new_size) {
		cur_sendbuf_size = new_size;
		setsockopt(stream->rtmp.m_sb.sb_socket, SOL_SOCKET, SO_SNDBUF,
			   (const char *)&cur_sendbuf_size, int_size);
	}
}

static int init_send(struct rtmp_stream *stream)
{
	int ret;
	obs_output_t *context = stream->output;

#if defined(_WIN32)
	adjust_sndbuf_size(stream, MIN_SENDBUF_SIZE);
#endif

	reset_semaphore(stream);

	ret = pthread_create(&stream->send_thread, NULL, send_thread, stream);
	if (ret != 0) {
		RTMP_Close(&stream->rtmp);
		warn("Failed to create send thread");
		return OBS_OUTPUT_ERROR;
	}

	if (stream->new_socket_loop) {
		int one = 1;
#ifdef _WIN32
		if (ioctlsocket(stream->rtmp.m_sb.sb_socket, FIONBIO, &one)) {
			stream->rtmp.last_error_code = WSAGetLastError();
#else
		if (ioctl(stream->rtmp.m_sb.sb_socket, FIONBIO, &one)) {
			stream->rtmp.last_error_code = errno;
#endif
			warn("Failed to set non-blocking socket");
			return OBS_OUTPUT_ERROR;
		}

		os_event_reset(stream->send_thread_signaled_exit);

		info("New socket loop enabled by user");
		if (stream->low_latency_mode)
			info("Low latency mode enabled by user");

		if (stream->write_buf)
			bfree(stream->write_buf);

		int total_bitrate = 0;

		obs_encoder_t *vencoder = obs_output_get_video_encoder(context);
		if (vencoder) {
			obs_data_t *params = obs_encoder_get_settings(vencoder);
			if (params) {
				int bitrate =
					obs_data_get_int(params, "bitrate");
				if (!bitrate) {
					warn("Video encoder didn't return a "
					     "valid bitrate, new network "
					     "code may function poorly. "
					     "Low latency mode disabled.");
					stream->low_latency_mode = false;
					bitrate = 10000;
				}
				total_bitrate += bitrate;
				obs_data_release(params);
			}
		}

		obs_encoder_t *aencoder =
			obs_output_get_audio_encoder(context, 0);
		if (aencoder) {
			obs_data_t *params = obs_encoder_get_settings(aencoder);
			if (params) {
				int bitrate =
					obs_data_get_int(params, "bitrate");
				if (!bitrate)
					bitrate = 160;
				total_bitrate += bitrate;
				obs_data_release(params);
			}
		}

		// to bytes/sec
		int ideal_buffer_size = total_bitrate * 128;

		if (ideal_buffer_size < 131072)
			ideal_buffer_size = 131072;

		stream->write_buf_size = ideal_buffer_size;
		stream->write_buf = bmalloc(ideal_buffer_size);

#ifdef _WIN32
		ret = pthread_create(&stream->socket_thread, NULL,
				     socket_thread_windows, stream);
#else
		warn("New socket loop not supported on this platform");
		return OBS_OUTPUT_ERROR;
#endif

		if (ret != 0) {
			RTMP_Close(&stream->rtmp);
			warn("Failed to create socket thread");
			return OBS_OUTPUT_ERROR;
		}

		stream->socket_thread_active = true;
		stream->rtmp.m_bCustomSend = true;
		stream->rtmp.m_customSendFunc = socket_queue_data;
		stream->rtmp.m_customSendParam = stream;
	}

	os_atomic_set_bool(&stream->active, true);

	if (!send_meta_data(stream)) {
		warn("Disconnected while attempting to send metadata");
		set_output_error(stream);
		return OBS_OUTPUT_DISCONNECTED;
	}

	obs_encoder_t *aencoder = obs_output_get_audio_encoder(context, 1);
	if (aencoder && !send_additional_meta_data(stream)) {
		warn("Disconnected while attempting to send additional "
		     "metadata");
		return OBS_OUTPUT_DISCONNECTED;
	}

	if (obs_output_get_audio_encoder(context, 2) != NULL) {
		warn("Additional audio streams not supported");
		return OBS_OUTPUT_DISCONNECTED;
	}

	obs_output_begin_data_capture(stream->output, 0);

	return OBS_OUTPUT_SUCCESS;
}

#ifdef _WIN32
static void win32_log_interface_type(struct rtmp_stream *stream)
{
	RTMP *rtmp = &stream->rtmp;
	MIB_IPFORWARDROW route;
	uint32_t dest_addr, source_addr;
	char hostname[256];
	HOSTENT *h;

	if (rtmp->Link.hostname.av_len >= sizeof(hostname) - 1)
		return;

	strncpy(hostname, rtmp->Link.hostname.av_val, sizeof(hostname));
	hostname[rtmp->Link.hostname.av_len] = 0;

	h = gethostbyname(hostname);
	if (!h)
		return;

	dest_addr = *(uint32_t *)h->h_addr_list[0];

	if (rtmp->m_bindIP.addrLen == 0)
		source_addr = 0;
	else if (rtmp->m_bindIP.addr.ss_family == AF_INET)
		source_addr = (*(struct sockaddr_in *)&rtmp->m_bindIP.addr)
				      .sin_addr.S_un.S_addr;
	else
		return;

	if (!GetBestRoute(dest_addr, source_addr, &route)) {
		MIB_IFROW row;
		memset(&row, 0, sizeof(row));
		row.dwIndex = route.dwForwardIfIndex;

		if (!GetIfEntry(&row)) {
			uint32_t speed = row.dwSpeed / 1000000;
			char *type;
			struct dstr other = {0};

			if (row.dwType == IF_TYPE_ETHERNET_CSMACD) {
				type = "ethernet";
			} else if (row.dwType == IF_TYPE_IEEE80211) {
				type = "802.11";
			} else {
				dstr_printf(&other, "type %lu", row.dwType);
				type = other.array;
			}

			info("Interface: %s (%s, %lu mbps)", row.bDescr, type,
			     speed);

			dstr_free(&other);
		}
	}
}
#endif

static int try_connect(struct rtmp_stream *stream)
{
	if (dstr_is_empty(&stream->path)) {
		warn("URL is empty");
		return OBS_OUTPUT_BAD_PATH;
	}

	info("Connecting to RTMP URL %s...", stream->path.array);

	// on reconnect we need to reset the internal variables of librtmp
	// otherwise the data sent/received will not parse correctly on the other end
	RTMP_Reset(&stream->rtmp);

	// since we don't call RTMP_Init above, there's no other good place
	// to reset this as doing it in RTMP_Close breaks the ugly RTMP
	// authentication system
	memset(&stream->rtmp.Link, 0, sizeof(stream->rtmp.Link));
	stream->rtmp.last_error_code = 0;

	if (!RTMP_SetupURL(&stream->rtmp, stream->path.array))
		return OBS_OUTPUT_BAD_PATH;

	RTMP_EnableWrite(&stream->rtmp);

	dstr_copy(&stream->encoder_name, "FMLE/3.0 (compatible; FMSc/1.0)");

	set_rtmp_dstr(&stream->rtmp.Link.pubUser, &stream->username);
	set_rtmp_dstr(&stream->rtmp.Link.pubPasswd, &stream->password);
	set_rtmp_dstr(&stream->rtmp.Link.flashVer, &stream->encoder_name);
	stream->rtmp.Link.swfUrl = stream->rtmp.Link.tcUrl;

	if (dstr_is_empty(&stream->bind_ip) ||
	    dstr_cmp(&stream->bind_ip, "default") == 0) {
		memset(&stream->rtmp.m_bindIP, 0,
		       sizeof(stream->rtmp.m_bindIP));
	} else {
		bool success = netif_str_to_addr(&stream->rtmp.m_bindIP.addr,
						 &stream->rtmp.m_bindIP.addrLen,
						 stream->bind_ip.array);
		if (success) {
			int len = stream->rtmp.m_bindIP.addrLen;
			bool ipv6 = len == sizeof(struct sockaddr_in6);
			info("Binding to IPv%d", ipv6 ? 6 : 4);
		}
	}

	RTMP_AddStream(&stream->rtmp, stream->key.array);

	stream->rtmp.m_outChunkSize = 4096;
	stream->rtmp.m_bSendChunkSizeInfo = true;
	stream->rtmp.m_bUseNagle = true;

#ifdef _WIN32
	win32_log_interface_type(stream);
#endif

	if (!RTMP_Connect(&stream->rtmp, NULL)) {
		set_output_error(stream);
		return OBS_OUTPUT_CONNECT_FAILED;
	}

	if (!RTMP_ConnectStream(&stream->rtmp, 0))
		return OBS_OUTPUT_INVALID_STREAM;

	info("Connection to %s successful", stream->path.array);

	return init_send(stream);
}

static bool init_connect(struct rtmp_stream *stream)
{
	obs_service_t *service;
	obs_data_t *settings;
	const char *bind_ip;
	int64_t drop_p;
	int64_t drop_b;
	uint32_t caps;

	if (stopping(stream)) {
		pthread_join(stream->send_thread, NULL);
	}

	free_packets(stream);

	service = obs_output_get_service(stream->output);
	if (!service)
		return false;

	os_atomic_set_bool(&stream->disconnected, false);
	os_atomic_set_bool(&stream->encode_error, false);
	stream->total_bytes_sent = 0;
	stream->dropped_frames = 0;
	stream->min_priority = 0;
	stream->got_first_video = false;

	settings = obs_output_get_settings(stream->output);
	dstr_copy(&stream->path, obs_service_get_url(service));
	dstr_copy(&stream->key, obs_service_get_key(service));
	dstr_copy(&stream->username, obs_service_get_username(service));
	dstr_copy(&stream->password, obs_service_get_password(service));
	dstr_depad(&stream->path);
	dstr_depad(&stream->key);
	drop_b = (int64_t)obs_data_get_int(settings, OPT_DROP_THRESHOLD);
	drop_p = (int64_t)obs_data_get_int(settings, OPT_PFRAME_DROP_THRESHOLD);
	stream->max_shutdown_time_sec =
		(int)obs_data_get_int(settings, OPT_MAX_SHUTDOWN_TIME_SEC);

	obs_encoder_t *venc = obs_output_get_video_encoder(stream->output);
	obs_encoder_t *aenc = obs_output_get_audio_encoder(stream->output, 0);
	obs_data_t *vsettings = obs_encoder_get_settings(venc);
	obs_data_t *asettings = obs_encoder_get_settings(aenc);

	circlebuf_free(&stream->dbr_frames);
	stream->audio_bitrate = (long)obs_data_get_int(asettings, "bitrate");
	stream->dbr_data_size = 0;
	stream->dbr_orig_bitrate = (long)obs_data_get_int(vsettings, "bitrate");
	stream->dbr_cur_bitrate = stream->dbr_orig_bitrate;
	stream->dbr_est_bitrate = 0;
	stream->dbr_inc_bitrate = stream->dbr_orig_bitrate / 10;
	stream->dbr_inc_timeout = 0;
	stream->dbr_enabled = obs_data_get_bool(settings, OPT_DYN_BITRATE);

	caps = obs_encoder_get_caps(venc);
	if ((caps & OBS_ENCODER_CAP_DYN_BITRATE) == 0) {
		stream->dbr_enabled = false;
	}

	if (obs_output_get_delay(stream->output) != 0) {
		stream->dbr_enabled = false;
	}

	if (stream->dbr_enabled) {
		info("Dynamic bitrate enabled.  Dropped frames begone!");
	}

	obs_data_release(vsettings);
	obs_data_release(asettings);

	if (drop_p < (drop_b + 200))
		drop_p = drop_b + 200;

	stream->drop_threshold_usec = 1000 * drop_b;
	stream->pframe_drop_threshold_usec = 1000 * drop_p;

	bind_ip = obs_data_get_string(settings, OPT_BIND_IP);
	dstr_copy(&stream->bind_ip, bind_ip);

	stream->new_socket_loop =
		obs_data_get_bool(settings, OPT_NEWSOCKETLOOP_ENABLED);
	stream->low_latency_mode =
		obs_data_get_bool(settings, OPT_LOWLATENCY_ENABLED);

	obs_data_release(settings);
	return true;
}

static void *connect_thread(void *data)
{
	struct rtmp_stream *stream = data;
	int ret;

	os_set_thread_name("rtmp-stream: connect_thread");

	if (!init_connect(stream)) {
		obs_output_signal_stop(stream->output, OBS_OUTPUT_BAD_PATH);
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

static bool rtmp_stream_start(void *data)
{
	struct rtmp_stream *stream = data;

	if (!obs_output_can_begin_data_capture(stream->output, 0))
		return false;
	if (!obs_output_initialize_encoders(stream->output, 0))
		return false;

	os_atomic_set_bool(&stream->connecting, true);
	return pthread_create(&stream->connect_thread, NULL, connect_thread,
			      stream) == 0;
}

static inline bool add_packet(struct rtmp_stream *stream,
			      struct encoder_packet *packet)
{
	circlebuf_push_back(&stream->packets, packet,
			    sizeof(struct encoder_packet));
	return true;
}

static inline size_t num_buffered_packets(struct rtmp_stream *stream)
{
	return stream->packets.size / sizeof(struct encoder_packet);
}

static void drop_frames(struct rtmp_stream *stream, const char *name,
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

static bool find_first_video_packet(struct rtmp_stream *stream,
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

static bool dbr_bitrate_lowered(struct rtmp_stream *stream)
{
	long prev_bitrate = stream->dbr_prev_bitrate;
	long est_bitrate = 0;
	long new_bitrate;

	if (stream->dbr_est_bitrate &&
	    stream->dbr_est_bitrate < stream->dbr_cur_bitrate) {
		stream->dbr_data_size = 0;
		circlebuf_pop_front(&stream->dbr_frames, NULL,
				    stream->dbr_frames.size);
		est_bitrate = stream->dbr_est_bitrate / 100 * 100;
		if (est_bitrate < 50) {
			est_bitrate = 50;
		}
	}

#if 0
	if (prev_bitrate && est_bitrate) {
		if (prev_bitrate < est_bitrate) {
			blog(LOG_INFO, "going back to prev bitrate: "
					"prev_bitrate (%d) < est_bitrate (%d)",
					prev_bitrate,
					est_bitrate);
			new_bitrate = prev_bitrate;
		} else {
			new_bitrate = est_bitrate;
		}
		new_bitrate = est_bitrate;
	} else if (prev_bitrate) {
		new_bitrate = prev_bitrate;
		info("going back to prev bitrate");

	} else if (est_bitrate) {
		new_bitrate = est_bitrate;

	} else {
		return false;
	}
#else
	if (est_bitrate) {
		new_bitrate = est_bitrate;

	} else if (prev_bitrate) {
		new_bitrate = prev_bitrate;
		info("going back to prev bitrate");

	} else {
		return false;
	}

	if (new_bitrate == stream->dbr_cur_bitrate) {
		return false;
	}
#endif

	stream->dbr_prev_bitrate = 0;
	stream->dbr_cur_bitrate = new_bitrate;
	stream->dbr_inc_timeout = os_gettime_ns() + DBR_INC_TIMER;
	info("bitrate decreased to: %ld", stream->dbr_cur_bitrate);
	return true;
}

static void dbr_set_bitrate(struct rtmp_stream *stream)
{
	obs_encoder_t *vencoder = obs_output_get_video_encoder(stream->output);
	obs_data_t *settings = obs_encoder_get_settings(vencoder);

	obs_data_set_int(settings, "bitrate", stream->dbr_cur_bitrate);
	obs_encoder_update(vencoder, settings);

	obs_data_release(settings);
}

static void dbr_inc_bitrate(struct rtmp_stream *stream)
{
	stream->dbr_prev_bitrate = stream->dbr_cur_bitrate;
	stream->dbr_cur_bitrate += stream->dbr_inc_bitrate;

	if (stream->dbr_cur_bitrate >= stream->dbr_orig_bitrate) {
		stream->dbr_cur_bitrate = stream->dbr_orig_bitrate;
		info("bitrate increased to: %ld, done",
		     stream->dbr_cur_bitrate);
	} else if (stream->dbr_cur_bitrate < stream->dbr_orig_bitrate) {
		stream->dbr_inc_timeout = os_gettime_ns() + DBR_INC_TIMER;
		info("bitrate increased to: %ld, waiting",
		     stream->dbr_cur_bitrate);
	}
}

static void check_to_drop_frames(struct rtmp_stream *stream, bool pframes)
{
	struct encoder_packet first;
	int64_t buffer_duration_usec;
	size_t num_packets = num_buffered_packets(stream);
	const char *name = pframes ? "p-frames" : "b-frames";
	int priority = pframes ? OBS_NAL_PRIORITY_HIGHEST
			       : OBS_NAL_PRIORITY_HIGH;
	int64_t drop_threshold = pframes ? stream->pframe_drop_threshold_usec
					 : stream->drop_threshold_usec;

	if (!pframes && stream->dbr_enabled) {
		if (stream->dbr_inc_timeout) {
			uint64_t t = os_gettime_ns();

			if (t >= stream->dbr_inc_timeout) {
				stream->dbr_inc_timeout = 0;
				dbr_inc_bitrate(stream);
				dbr_set_bitrate(stream);
			}
		}
	}

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

	/* alternatively, drop only pframes:
	 * (!pframes && stream->dbr_enabled)
	 * but let's test without dropping frames
	 * at all first */
	if (stream->dbr_enabled) {
		bool bitrate_changed = false;

		if (pframes) {
			return;
		}

		if ((uint64_t)buffer_duration_usec >= DBR_TRIGGER_USEC) {
			pthread_mutex_lock(&stream->dbr_mutex);
			bitrate_changed = dbr_bitrate_lowered(stream);
			pthread_mutex_unlock(&stream->dbr_mutex);
		}

		if (bitrate_changed) {
			debug("buffer_duration_msec: %" PRId64,
			      buffer_duration_usec / 1000);
			dbr_set_bitrate(stream);
		}
		return;
	}

	if (buffer_duration_usec > drop_threshold) {
		debug("buffer_duration_usec: %" PRId64, buffer_duration_usec);
		drop_frames(stream, name, priority, pframes);
	}
}

static bool add_video_packet(struct rtmp_stream *stream,
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
	return add_packet(stream, packet);
}

static void rtmp_stream_data(void *data, struct encoder_packet *packet)
{
	struct rtmp_stream *stream = data;
	struct encoder_packet new_packet;
	bool added_packet = false;

	if (disconnected(stream) || !active(stream))
		return;

	/* encoder fail */
	if (!packet) {
		os_atomic_set_bool(&stream->encode_error, true);
		os_sem_post(stream->send_sem);
		return;
	}

	if (packet->type == OBS_ENCODER_VIDEO) {
		if (!stream->got_first_video) {
			stream->start_dts_offset =
				get_ms_time(packet, packet->dts);
			stream->got_first_video = true;
		}

		obs_parse_avc_packet(&new_packet, packet);
	} else {
		obs_encoder_packet_ref(&new_packet, packet);
	}

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

static void rtmp_stream_defaults(obs_data_t *defaults)
{
	obs_data_set_default_int(defaults, OPT_DROP_THRESHOLD, 700);
	obs_data_set_default_int(defaults, OPT_PFRAME_DROP_THRESHOLD, 900);
	obs_data_set_default_int(defaults, OPT_MAX_SHUTDOWN_TIME_SEC, 30);
	obs_data_set_default_string(defaults, OPT_BIND_IP, "default");
	obs_data_set_default_bool(defaults, OPT_NEWSOCKETLOOP_ENABLED, false);
	obs_data_set_default_bool(defaults, OPT_LOWLATENCY_ENABLED, false);
}

static obs_properties_t *rtmp_stream_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();
	struct netif_saddr_data addrs = {0};
	obs_property_t *p;

	obs_properties_add_int(props, OPT_DROP_THRESHOLD,
			       obs_module_text("RTMPStream.DropThreshold"), 200,
			       10000, 100);

	p = obs_properties_add_list(props, OPT_BIND_IP,
				    obs_module_text("RTMPStream.BindIP"),
				    OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);

	obs_property_list_add_string(p, obs_module_text("Default"), "default");

	netif_get_addrs(&addrs);
	for (size_t i = 0; i < addrs.addrs.num; i++) {
		struct netif_saddr_item item = addrs.addrs.array[i];
		obs_property_list_add_string(p, item.name, item.addr);
	}
	netif_saddr_data_free(&addrs);

	obs_properties_add_bool(props, OPT_NEWSOCKETLOOP_ENABLED,
				obs_module_text("RTMPStream.NewSocketLoop"));
	obs_properties_add_bool(props, OPT_LOWLATENCY_ENABLED,
				obs_module_text("RTMPStream.LowLatencyMode"));

	return props;
}

static uint64_t rtmp_stream_total_bytes_sent(void *data)
{
	struct rtmp_stream *stream = data;
	return stream->total_bytes_sent;
}

static int rtmp_stream_dropped_frames(void *data)
{
	struct rtmp_stream *stream = data;
	return stream->dropped_frames;
}

static float rtmp_stream_congestion(void *data)
{
	struct rtmp_stream *stream = data;

	if (stream->new_socket_loop)
		return (float)stream->write_buf_len /
		       (float)stream->write_buf_size;
	else
		return stream->min_priority > 0 ? 1.0f : stream->congestion;
}

static int rtmp_stream_connect_time(void *data)
{
	struct rtmp_stream *stream = data;
	return stream->rtmp.connect_time_ms;
}

struct obs_output_info rtmp_output_info = {
	.id = "rtmp_output",
	.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED | OBS_OUTPUT_SERVICE |
		 OBS_OUTPUT_MULTI_TRACK,
	.encoded_video_codecs = "h264",
	.encoded_audio_codecs = "aac",
	.get_name = rtmp_stream_getname,
	.create = rtmp_stream_create,
	.destroy = rtmp_stream_destroy,
	.start = rtmp_stream_start,
	.stop = rtmp_stream_stop,
	.encoded_packet = rtmp_stream_data,
	.get_defaults = rtmp_stream_defaults,
	.get_properties = rtmp_stream_properties,
	.get_total_bytes = rtmp_stream_total_bytes_sent,
	.get_congestion = rtmp_stream_congestion,
	.get_connect_time_ms = rtmp_stream_connect_time,
	.get_dropped_frames = rtmp_stream_dropped_frames,
};
