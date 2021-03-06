/*
 * Copyright (c) 2017 Hugh Bailey <obs.jim@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <obs.h>
#include <util/platform.h>

#include <assert.h>

#include "media.h"
#include "closest-format.h"

#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>

static int64_t base_sys_ts = 0;

static inline enum video_format convert_pixel_format(int f)
{
	switch (f) {
	case AV_PIX_FMT_NONE:
		return VIDEO_FORMAT_NONE;
	case AV_PIX_FMT_YUV420P:
		return VIDEO_FORMAT_I420;
	case AV_PIX_FMT_NV12:
		return VIDEO_FORMAT_NV12;
	case AV_PIX_FMT_YUYV422:
		return VIDEO_FORMAT_YUY2;
	case AV_PIX_FMT_YUV444P:
		return VIDEO_FORMAT_I444;
	case AV_PIX_FMT_UYVY422:
		return VIDEO_FORMAT_UYVY;
	case AV_PIX_FMT_RGBA:
		return VIDEO_FORMAT_RGBA;
	case AV_PIX_FMT_BGRA:
		return VIDEO_FORMAT_BGRA;
	case AV_PIX_FMT_BGR0:
		return VIDEO_FORMAT_BGRX;
	case AV_PIX_FMT_YUVA420P:
		return VIDEO_FORMAT_I40A;
	case AV_PIX_FMT_YUVA422P:
		return VIDEO_FORMAT_I42A;
	case AV_PIX_FMT_YUVA444P:
		return VIDEO_FORMAT_YUVA;
	default:;
	}

	return VIDEO_FORMAT_NONE;
}

static inline enum audio_format convert_sample_format(int f)
{
	switch (f) {
	case AV_SAMPLE_FMT_U8:
		return AUDIO_FORMAT_U8BIT;
	case AV_SAMPLE_FMT_S16:
		return AUDIO_FORMAT_16BIT;
	case AV_SAMPLE_FMT_S32:
		return AUDIO_FORMAT_32BIT;
	case AV_SAMPLE_FMT_FLT:
		return AUDIO_FORMAT_FLOAT;
	case AV_SAMPLE_FMT_U8P:
		return AUDIO_FORMAT_U8BIT_PLANAR;
	case AV_SAMPLE_FMT_S16P:
		return AUDIO_FORMAT_16BIT_PLANAR;
	case AV_SAMPLE_FMT_S32P:
		return AUDIO_FORMAT_32BIT_PLANAR;
	case AV_SAMPLE_FMT_FLTP:
		return AUDIO_FORMAT_FLOAT_PLANAR;
	default:;
	}

	return AUDIO_FORMAT_UNKNOWN;
}

static inline enum speaker_layout convert_speaker_layout(uint8_t channels)
{
	switch (channels) {
	case 0:
		return SPEAKERS_UNKNOWN;
	case 1:
		return SPEAKERS_MONO;
	case 2:
		return SPEAKERS_STEREO;
	case 3:
		return SPEAKERS_2POINT1;
	case 4:
		return SPEAKERS_4POINT0;
	case 5:
		return SPEAKERS_4POINT1;
	case 6:
		return SPEAKERS_5POINT1;
	case 8:
		return SPEAKERS_7POINT1;
	default:
		return SPEAKERS_UNKNOWN;
	}
}

static inline enum video_colorspace
convert_color_space(enum AVColorSpace s, enum AVColorTransferCharacteristic trc)
{
	switch (s) {
	case AVCOL_SPC_BT709:
		return (trc == AVCOL_TRC_IEC61966_2_1) ? VIDEO_CS_SRGB
						       : VIDEO_CS_709;
	case AVCOL_SPC_FCC:
	case AVCOL_SPC_BT470BG:
	case AVCOL_SPC_SMPTE170M:
	case AVCOL_SPC_SMPTE240M:
		return VIDEO_CS_601;
	default:
		return VIDEO_CS_DEFAULT;
	}
}

static inline enum video_range_type convert_color_range(enum AVColorRange r)
{
	return r == AVCOL_RANGE_JPEG ? VIDEO_RANGE_FULL : VIDEO_RANGE_DEFAULT;
}

static inline struct mp_decode *get_packet_decoder(mp_media_t *media,
						   AVPacket *pkt)
{
	if (media->has_audio && pkt->stream_index == media->a.stream->index)
		return &media->a;
	if (media->has_video && pkt->stream_index == media->v.stream->index)
		return &media->v;

	return NULL;
}

static int mp_media_next_packet(mp_media_t *media)
{
	AVPacket new_pkt;
	AVPacket pkt;
	av_init_packet(&pkt);
	new_pkt = pkt;

	int ret = av_read_frame(media->fmt, &pkt);
	if (ret < 0) {
		if (ret != AVERROR_EOF && ret != AVERROR_EXIT)
			blog(LOG_WARNING, "MP: av_read_frame failed: %s (%d)",
			     av_err2str(ret), ret);
		return ret;
	}

	struct mp_decode *d = get_packet_decoder(media, &pkt);
	if (d && pkt.size) {
		av_packet_ref(&new_pkt, &pkt);
		mp_decode_push_packet(d, &new_pkt);
	}

	av_packet_unref(&pkt);
	return ret;
}

static inline bool mp_media_ready_to_start(mp_media_t *m)
{
	if (m->has_audio && !m->a.eof && !m->a.frame_ready)
		return false;
	if (m->has_video && !m->v.eof && !m->v.frame_ready)
		return false;
	return true;
}

static inline bool mp_decode_frame(struct mp_decode *d)
{
	return d->frame_ready || mp_decode_next(d);
}

static inline int get_sws_colorspace(enum AVColorSpace cs)
{
	switch (cs) {
	case AVCOL_SPC_BT709:
		return SWS_CS_ITU709;
	case AVCOL_SPC_FCC:
		return SWS_CS_FCC;
	case AVCOL_SPC_SMPTE170M:
		return SWS_CS_SMPTE170M;
	case AVCOL_SPC_SMPTE240M:
		return SWS_CS_SMPTE240M;
	default:
		break;
	}

	return SWS_CS_ITU601;
}

static inline int get_sws_range(enum AVColorRange r)
{
	return r == AVCOL_RANGE_JPEG ? 1 : 0;
}

#define FIXED_1_0 (1 << 16)

static bool mp_media_init_scaling(mp_media_t *m)
{
	int space = get_sws_colorspace(m->v.decoder->colorspace);
	int range = get_sws_range(m->v.decoder->color_range);
	const int *coeff = sws_getCoefficients(space);

	m->swscale = sws_getCachedContext(NULL, m->v.decoder->width,
					  m->v.decoder->height,
					  m->v.decoder->pix_fmt,
					  m->v.decoder->width,
					  m->v.decoder->height, m->scale_format,
					  SWS_POINT, NULL, NULL, NULL);
	if (!m->swscale) {
		blog(LOG_WARNING, "MP: Failed to initialize scaler");
		return false;
	}

	sws_setColorspaceDetails(m->swscale, coeff, range, coeff, range, 0,
				 FIXED_1_0, FIXED_1_0);

	int ret = av_image_alloc(m->scale_pic, m->scale_linesizes,
				 m->v.decoder->width, m->v.decoder->height,
				 m->scale_format, 32);
	if (ret < 0) {
		blog(LOG_WARNING, "MP: Failed to create scale pic data");
		return false;
	}

	return true;
}

static bool mp_media_prepare_frames(mp_media_t *m)
{
	bool actively_seeking = m->seek_next_ts && m->pause;

	while (!mp_media_ready_to_start(m)) {
		if (!m->eof) {
			int ret = mp_media_next_packet(m);
			if (ret == AVERROR_EOF || ret == AVERROR_EXIT) {
				if (!actively_seeking) {
					m->eof = true;
				} else {
					break;
				}
			} else if (ret < 0) {
				return false;
			}
		}

		if (m->has_video && !mp_decode_frame(&m->v))
			return false;
		if (m->has_audio && !mp_decode_frame(&m->a))
			return false;
	}

	if (m->has_video && m->v.frame_ready && !m->swscale) {
		m->scale_format = closest_format(m->v.frame->format);
		if (m->scale_format != m->v.frame->format) {
			if (!mp_media_init_scaling(m)) {
				return false;
			}
		}
	}

	return true;
}

static inline int64_t mp_media_get_next_min_pts(mp_media_t *m)
{
	int64_t min_next_ns = 0x7FFFFFFFFFFFFFFFLL;

	if (m->has_video && m->v.frame_ready) {
		if (m->v.frame_pts < min_next_ns)
			min_next_ns = m->v.frame_pts;
	}
	if (m->has_audio && m->a.frame_ready) {
		if (m->a.frame_pts < min_next_ns)
			min_next_ns = m->a.frame_pts;
	}

	return min_next_ns;
}

static inline int64_t mp_media_get_base_pts(mp_media_t *m)
{
	int64_t base_ts = 0;

	if (m->has_video && m->v.next_pts > base_ts)
		base_ts = m->v.next_pts;
	if (m->has_audio && m->a.next_pts > base_ts)
		base_ts = m->a.next_pts;

	return base_ts;
}

static inline bool mp_media_can_play_frame(mp_media_t *m, struct mp_decode *d)
{
	return d->frame_ready && d->frame_pts <= m->next_pts_ns;
}

static void mp_media_next_audio(mp_media_t *m)
{
	struct mp_decode *d = &m->a;
	struct obs_source_audio audio = {0};
	AVFrame *f = d->frame;

	if (!mp_media_can_play_frame(m, d))
		return;

	d->frame_ready = false;
	if (!m->a_cb)
		return;

	for (size_t i = 0; i < MAX_AV_PLANES; i++)
		audio.data[i] = f->data[i];

	audio.samples_per_sec = f->sample_rate * m->speed / 100;
	audio.speakers = convert_speaker_layout(f->channels);
	audio.format = convert_sample_format(f->format);
	audio.frames = f->nb_samples;

	audio.timestamp = m->base_ts + d->frame_pts - m->start_ts +
			  m->play_sys_ts - base_sys_ts;

	if (audio.format == AUDIO_FORMAT_UNKNOWN)
		return;

	m->a_cb(m->opaque, &audio);
}

static void mp_media_next_video(mp_media_t *m, bool preload)
{
	struct mp_decode *d = &m->v;
	struct obs_source_frame *frame = &m->obsframe;
	enum video_format new_format;
	enum video_colorspace new_space;
	enum video_range_type new_range;
	AVFrame *f = d->frame;

	if (!preload) {
		if (!mp_media_can_play_frame(m, d))
			return;

		d->frame_ready = false;

		if (!m->v_cb)
			return;
	} else if (!d->frame_ready) {
		return;
	}

	bool flip = false;
	if (m->swscale) {
		int ret = sws_scale(m->swscale, (const uint8_t *const *)f->data,
				    f->linesize, 0, f->height, m->scale_pic,
				    m->scale_linesizes);
		if (ret < 0)
			return;

		flip = m->scale_linesizes[0] < 0 && m->scale_linesizes[1] == 0;
		for (size_t i = 0; i < 4; i++) {
			frame->data[i] = m->scale_pic[i];
			frame->linesize[i] = abs(m->scale_linesizes[i]);
		}

	} else {
		flip = f->linesize[0] < 0 && f->linesize[1] == 0;

		for (size_t i = 0; i < MAX_AV_PLANES; i++) {
			frame->data[i] = f->data[i];
			frame->linesize[i] = abs(f->linesize[i]);
		}
	}

	if (flip)
		frame->data[0] -= frame->linesize[0] * (f->height - 1);

	new_format = convert_pixel_format(m->scale_format);
	new_space = convert_color_space(f->colorspace, f->color_trc);
	new_range = m->force_range == VIDEO_RANGE_DEFAULT
			    ? convert_color_range(f->color_range)
			    : m->force_range;

	if (new_format != frame->format || new_space != m->cur_space ||
	    new_range != m->cur_range) {
		bool success;

		frame->format = new_format;
		frame->full_range = new_range == VIDEO_RANGE_FULL;

		success = video_format_get_parameters(new_space, new_range,
						      frame->color_matrix,
						      frame->color_range_min,
						      frame->color_range_max);

		frame->format = new_format;
		m->cur_space = new_space;
		m->cur_range = new_range;

		if (!success) {
			frame->format = VIDEO_FORMAT_NONE;
			return;
		}
	}

	if (frame->format == VIDEO_FORMAT_NONE)
		return;

	frame->timestamp = m->base_ts + d->frame_pts - m->start_ts +
			   m->play_sys_ts - base_sys_ts;

	frame->width = f->width;
	frame->height = f->height;
	frame->flip = flip;

	if (!m->is_local_file && !d->got_first_keyframe) {
		if (!f->key_frame)
			return;

		d->got_first_keyframe = true;
	}

	if (preload) {
		if (m->seek_next_ts && m->v_seek_cb) {
			m->v_seek_cb(m->opaque, frame);
		} else {
			m->v_preload_cb(m->opaque, frame);
		}
	} else {
		m->v_cb(m->opaque, frame);
	}
}

static void mp_media_calc_next_ns(mp_media_t *m)
{
	int64_t min_next_ns = mp_media_get_next_min_pts(m);
	int64_t delta = min_next_ns - m->next_pts_ns;

	if (m->seek_next_ts) {
		delta = 0;
		m->seek_next_ts = false;
	} else {
#ifdef _DEBUG
		assert(delta >= 0);
#endif
		if (delta < 0)
			delta = 0;
		if (delta > 3000000000)
			delta = 0;
	}

	m->next_ns += delta;
	m->next_pts_ns = min_next_ns;
}

static void seek_to(mp_media_t *m, int64_t pos)
{
	AVStream *stream = m->fmt->streams[0];
	int64_t seek_pos = pos;
	int seek_flags;

	if (m->fmt->duration == AV_NOPTS_VALUE)
		seek_flags = AVSEEK_FLAG_FRAME;
	else
		seek_flags = AVSEEK_FLAG_BACKWARD;

	int64_t seek_target = seek_flags == AVSEEK_FLAG_BACKWARD
				      ? av_rescale_q(seek_pos, AV_TIME_BASE_Q,
						     stream->time_base)
				      : seek_pos;

	if (m->is_local_file) {
		int ret = av_seek_frame(m->fmt, 0, seek_target, seek_flags);
		if (ret < 0) {
			blog(LOG_WARNING, "MP: Failed to seek: %s",
			     av_err2str(ret));
		}
	}

	if (m->has_video && m->is_local_file) {
		mp_decode_flush(&m->v);
		if (m->seek_next_ts && m->pause && m->v_preload_cb &&
		    mp_media_prepare_frames(m))
			mp_media_next_video(m, true);
	}
	if (m->has_audio && m->is_local_file)
		mp_decode_flush(&m->a);
}

static bool mp_media_reset(mp_media_t *m)
{
	bool stopping;
	bool active;

	seek_to(m, m->fmt->start_time);

	int64_t next_ts = mp_media_get_base_pts(m);
	int64_t offset = next_ts - m->next_pts_ns;

	m->eof = false;
	m->base_ts += next_ts;
	m->seek_next_ts = false;

	pthread_mutex_lock(&m->mutex);
	stopping = m->stopping;
	active = m->active;
	m->stopping = false;
	pthread_mutex_unlock(&m->mutex);

	if (!mp_media_prepare_frames(m))
		return false;

	if (active) {
		if (!m->play_sys_ts)
			m->play_sys_ts = (int64_t)os_gettime_ns();
		m->start_ts = m->next_pts_ns = mp_media_get_next_min_pts(m);
		if (m->next_ns)
			m->next_ns += offset;
	} else {
		m->start_ts = m->next_pts_ns = mp_media_get_next_min_pts(m);
		m->play_sys_ts = (int64_t)os_gettime_ns();
		m->next_ns = 0;
	}

	m->pause = false;

	if (!active && m->is_local_file && m->v_preload_cb)
		mp_media_next_video(m, true);
	if (stopping && m->stop_cb)
		m->stop_cb(m->opaque);
	return true;
}

static inline bool mp_media_sleepto(mp_media_t *m)
{
	bool timeout = false;

	if (!m->next_ns) {
		m->next_ns = os_gettime_ns();
	} else {
		uint64_t t = os_gettime_ns();
		const uint64_t timeout_ns = 200000000;

		if (m->next_ns > t && (m->next_ns - t) > timeout_ns) {
			os_sleepto_ns(t + timeout_ns);
			timeout = true;
		} else {
			os_sleepto_ns(m->next_ns);
		}
	}

	return timeout;
}

static inline bool mp_media_eof(mp_media_t *m)
{
	bool v_ended = !m->has_video || !m->v.frame_ready;
	bool a_ended = !m->has_audio || !m->a.frame_ready;
	bool eof = v_ended && a_ended;

	if (eof) {
		bool looping;

		pthread_mutex_lock(&m->mutex);
		looping = m->looping;
		if (!looping) {
			m->active = false;
			m->stopping = true;
		}
		pthread_mutex_unlock(&m->mutex);

		mp_media_reset(m);
	}

	return eof;
}

static int interrupt_callback(void *data)
{
	mp_media_t *m = data;
	bool stop = false;
	uint64_t ts = os_gettime_ns();

	if ((ts - m->interrupt_poll_ts) > 20000000) {
		pthread_mutex_lock(&m->mutex);
		stop = m->kill || m->stopping;
		pthread_mutex_unlock(&m->mutex);

		m->interrupt_poll_ts = ts;
	}

	return stop;
}

static bool init_avformat(mp_media_t *m)
{
	AVInputFormat *format = NULL;

	if (m->format_name && *m->format_name) {
		format = av_find_input_format(m->format_name);
		if (!format)
			blog(LOG_INFO,
			     "MP: Unable to find input format for "
			     "'%s'",
			     m->path);
	}

	AVDictionary *opts = NULL;
	if (m->buffering && !m->is_local_file)
		av_dict_set_int(&opts, "buffer_size", m->buffering, 0);

	m->fmt = avformat_alloc_context();
	if (m->buffering == 0) {
		m->fmt->flags |= AVFMT_FLAG_NOBUFFER;
	}
	if (!m->is_local_file) {
		av_dict_set(&opts, "stimeout", "30000000", 0);
		m->fmt->interrupt_callback.callback = interrupt_callback;
		m->fmt->interrupt_callback.opaque = m;
	}

	int ret = avformat_open_input(&m->fmt, m->path, format,
				      opts ? &opts : NULL);
	av_dict_free(&opts);

	if (ret < 0) {
		if (!m->reconnecting)
			blog(LOG_WARNING, "MP: Failed to open media: '%s'",
			     m->path);
		return false;
	}

	if (avformat_find_stream_info(m->fmt, NULL) < 0) {
		blog(LOG_WARNING, "MP: Failed to find stream info for '%s'",
		     m->path);
		return false;
	}

	m->reconnecting = false;
	m->has_video = mp_decode_init(m, AVMEDIA_TYPE_VIDEO, m->hw);
	m->has_audio = mp_decode_init(m, AVMEDIA_TYPE_AUDIO, m->hw);

	if (!m->has_video && !m->has_audio) {
		blog(LOG_WARNING,
		     "MP: Could not initialize audio or video: "
		     "'%s'",
		     m->path);
		return false;
	}

	return true;
}

static void reset_ts(mp_media_t *m)
{
	m->base_ts += mp_media_get_base_pts(m);
	m->play_sys_ts = (int64_t)os_gettime_ns();
	m->start_ts = m->next_pts_ns = mp_media_get_next_min_pts(m);
	m->next_ns = 0;
}

static inline bool mp_media_thread(mp_media_t *m)
{
	os_set_thread_name("mp_media_thread");

	if (!init_avformat(m)) {
		return false;
	}
	if (!mp_media_reset(m)) {
		return false;
	}

	for (;;) {
		bool reset, kill, is_active, seek, pause, reset_time;
		int64_t seek_pos;
		bool timeout = false;

		pthread_mutex_lock(&m->mutex);
		is_active = m->active;
		pause = m->pause;
		pthread_mutex_unlock(&m->mutex);

		if (!is_active || pause) {
			if (os_sem_wait(m->sem) < 0)
				return false;
			if (pause)
				reset_ts(m);
		} else {
			timeout = mp_media_sleepto(m);
		}

		pthread_mutex_lock(&m->mutex);

		reset = m->reset;
		kill = m->kill;
		m->reset = false;
		m->kill = false;

		pause = m->pause;
		seek_pos = m->seek_pos;
		seek = m->seek;
		reset_time = m->reset_ts;
		m->seek = false;
		m->reset_ts = false;

		pthread_mutex_unlock(&m->mutex);

		if (kill) {
			break;
		}
		if (reset) {
			mp_media_reset(m);
			continue;
		}

		if (seek) {
			m->seek_next_ts = true;
			seek_to(m, seek_pos);
			continue;
		}

		if (reset_time) {
			reset_ts(m);
			continue;
		}

		if (pause)
			continue;

		/* frames are ready */
		if (is_active && !timeout) {
			if (m->has_video)
				mp_media_next_video(m, false);
			if (m->has_audio)
				mp_media_next_audio(m);

			if (!mp_media_prepare_frames(m))
				return false;
			if (mp_media_eof(m))
				continue;

			mp_media_calc_next_ns(m);
		}
	}

	return true;
}

static void *mp_media_thread_start(void *opaque)
{
	mp_media_t *m = opaque;

	if (!mp_media_thread(m)) {
		if (m->stop_cb) {
			m->stop_cb(m->opaque);
		}
	}

	return NULL;
}

static inline bool mp_media_init_internal(mp_media_t *m,
					  const struct mp_media_info *info)
{
	if (pthread_mutex_init(&m->mutex, NULL) != 0) {
		blog(LOG_WARNING, "MP: Failed to init mutex");
		return false;
	}
	if (os_sem_init(&m->sem, 0) != 0) {
		blog(LOG_WARNING, "MP: Failed to init semaphore");
		return false;
	}

	m->path = info->path ? bstrdup(info->path) : NULL;
	m->format_name = info->format ? bstrdup(info->format) : NULL;
	m->hw = info->hardware_decoding;

	if (pthread_create(&m->thread, NULL, mp_media_thread_start, m) != 0) {
		blog(LOG_WARNING, "MP: Could not create media thread");
		return false;
	}

	m->thread_valid = true;
	return true;
}

bool mp_media_init(mp_media_t *media, const struct mp_media_info *info)
{
	memset(media, 0, sizeof(*media));
	pthread_mutex_init_value(&media->mutex);
	media->opaque = info->opaque;
	media->v_cb = info->v_cb;
	media->a_cb = info->a_cb;
	media->stop_cb = info->stop_cb;
	media->v_seek_cb = info->v_seek_cb;
	media->v_preload_cb = info->v_preload_cb;
	media->force_range = info->force_range;
	media->buffering = info->buffering;
	media->speed = info->speed;
	media->is_local_file = info->is_local_file;

	if (!info->is_local_file || media->speed < 1 || media->speed > 200)
		media->speed = 100;

	static bool initialized = false;
	if (!initialized) {
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
		av_register_all();
		avcodec_register_all();
#endif
		avdevice_register_all();
		avformat_network_init();
		initialized = true;
	}

	if (!base_sys_ts)
		base_sys_ts = (int64_t)os_gettime_ns();

	if (!mp_media_init_internal(media, info)) {
		mp_media_free(media);
		return false;
	}

	return true;
}

static void mp_kill_thread(mp_media_t *m)
{
	if (m->thread_valid) {
		pthread_mutex_lock(&m->mutex);
		m->kill = true;
		pthread_mutex_unlock(&m->mutex);
		os_sem_post(m->sem);

		pthread_join(m->thread, NULL);
	}
}

void mp_media_free(mp_media_t *media)
{
	if (!media)
		return;

	mp_media_stop(media);
	mp_kill_thread(media);
	mp_decode_free(&media->v);
	mp_decode_free(&media->a);
	avformat_close_input(&media->fmt);
	pthread_mutex_destroy(&media->mutex);
	os_sem_destroy(media->sem);
	sws_freeContext(media->swscale);
	av_freep(&media->scale_pic[0]);
	bfree(media->path);
	bfree(media->format_name);
	memset(media, 0, sizeof(*media));
	pthread_mutex_init_value(&media->mutex);
}

void mp_media_play(mp_media_t *m, bool loop, bool reconnecting)
{
	pthread_mutex_lock(&m->mutex);

	if (m->active)
		m->reset = true;

	m->looping = loop;
	m->active = true;
	m->reconnecting = reconnecting;

	pthread_mutex_unlock(&m->mutex);

	os_sem_post(m->sem);
}

void mp_media_play_pause(mp_media_t *m, bool pause)
{
	pthread_mutex_lock(&m->mutex);
	if (m->active) {
		m->pause = pause;
		m->reset_ts = !pause;
	}
	pthread_mutex_unlock(&m->mutex);

	os_sem_post(m->sem);
}

void mp_media_stop(mp_media_t *m)
{
	pthread_mutex_lock(&m->mutex);
	if (m->active) {
		m->reset = true;
		m->active = false;
		m->stopping = true;
	}
	pthread_mutex_unlock(&m->mutex);

	os_sem_post(m->sem);
}

int64_t mp_get_current_time(mp_media_t *m)
{
	return mp_media_get_base_pts(m) * (int64_t)m->speed / 100000000LL;
}

void mp_media_seek_to(mp_media_t *m, int64_t pos)
{
	pthread_mutex_lock(&m->mutex);
	if (m->active) {
		m->seek = true;
		m->seek_pos = pos * 1000;
	}
	pthread_mutex_unlock(&m->mutex);

	os_sem_post(m->sem);
}
