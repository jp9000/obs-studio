/*
 * Copyright (c) 2015 John R. Bradley <jrb@turrettech.com>
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

#include "ff-demuxer.h"

#include <libavutil/avstring.h>
#include <libavutil/time.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>

#include <assert.h>

#include "ff-compat.h"

#define DEFAULT_AV_SYNC_TYPE AV_SYNC_VIDEO_MASTER

#define AUDIO_FRAME_QUEUE_SIZE 1
#define VIDEO_FRAME_QUEUE_SIZE 1

#define AUDIO_PACKET_QUEUE_SIZE (5 * 16 * 1024)
#define VIDEO_PACKET_QUEUE_SIZE (5 * 256 * 1024)

static void *demux_thread(void *opaque_demuxer);

struct ff_demuxer *ff_demuxer_init()
{
	struct ff_demuxer *demuxer;

	av_register_all();
	avdevice_register_all();
	avfilter_register_all();
	avformat_network_init();

	demuxer = av_mallocz(sizeof(struct ff_demuxer));
	if (demuxer == NULL)
		return NULL;

	demuxer->clock.sync_type = DEFAULT_AV_SYNC_TYPE;
	demuxer->options.frame_drop = AVDISCARD_DEFAULT;
	demuxer->options.audio_frame_queue_size = AUDIO_FRAME_QUEUE_SIZE;
	demuxer->options.video_frame_queue_size = VIDEO_FRAME_QUEUE_SIZE;
	demuxer->options.audio_packet_queue_size = AUDIO_PACKET_QUEUE_SIZE;
	demuxer->options.video_packet_queue_size = VIDEO_PACKET_QUEUE_SIZE;
	demuxer->options.is_hw_decoding = false;

	return demuxer;
}

bool ff_demuxer_open(struct ff_demuxer *demuxer, char *input,
                     char *input_format)
{
	int ret;

	demuxer->input = av_strdup(input);
	if (input_format != NULL)
		demuxer->input_format = av_strdup(input_format);

	ret = pthread_create(&demuxer->demuxer_thread, NULL, demux_thread,
	                     demuxer);
	return ret == 0;
}

void ff_demuxer_free(struct ff_demuxer *demuxer)
{
	void *demuxer_thread_result;

	demuxer->abort = true;

	pthread_join(demuxer->demuxer_thread, &demuxer_thread_result);

	if (demuxer->input != NULL)
		av_free(demuxer->input);

	if (demuxer->input_format != NULL)
		av_free(demuxer->input_format);

	if (demuxer->audio_decoder != NULL)
		ff_decoder_free(demuxer->audio_decoder);

	if (demuxer->video_decoder != NULL)
		ff_decoder_free(demuxer->video_decoder);

	if (demuxer->format_context)
		avformat_close_input(&demuxer->format_context);

	av_free(demuxer);
}

void ff_demuxer_set_callbacks(struct ff_callbacks *callbacks,
                              ff_callback_frame frame,
                              ff_callback_format format,
                              ff_callback_initialize initialize,
                              ff_callback_frame frame_initialize,
                              ff_callback_frame frame_free, void *opaque)
{
	callbacks->opaque = opaque;
	callbacks->frame = frame;
	callbacks->format = format;
	callbacks->initialize = initialize;
	callbacks->frame_initialize = frame_initialize;
	callbacks->frame_free = frame_free;
}

static int demuxer_interrupted_callback(void *opaque)
{
	return opaque != NULL && ((struct ff_demuxer *)opaque)->abort;
}

static double ff_external_clock(void *opaque)
{
	(void)opaque;

	return av_gettime() / 1000000.0;
}

static bool set_clock_sync_type(struct ff_demuxer *demuxer)
{
	if (demuxer->video_decoder == NULL) {
		if (demuxer->clock.sync_type == AV_SYNC_VIDEO_MASTER)
			demuxer->clock.sync_type = AV_SYNC_AUDIO_MASTER;
	}

	if (demuxer->audio_decoder == NULL) {
		if (demuxer->clock.sync_type == AV_SYNC_AUDIO_MASTER)
			demuxer->clock.sync_type = AV_SYNC_VIDEO_MASTER;
	}

	switch (demuxer->clock.sync_type) {
	case AV_SYNC_AUDIO_MASTER:
		demuxer->clock.sync_clock = ff_decoder_clock;
		demuxer->clock.opaque = demuxer->audio_decoder;
		break;
	case AV_SYNC_VIDEO_MASTER:
		demuxer->clock.sync_clock = ff_decoder_clock;
		demuxer->clock.opaque = demuxer->video_decoder;
		break;
	case AV_SYNC_EXTERNAL_MASTER:
		demuxer->clock.sync_clock = ff_external_clock;
		demuxer->clock.opaque = NULL;
		break;
	default:
		return false;
	}

	return true;
}

AVHWAccel *find_hwaccel_codec(AVCodecContext *codec_context)
{
	AVHWAccel *hwaccel = NULL;

	while ((hwaccel = av_hwaccel_next(hwaccel)) != NULL) {
		if (hwaccel->id == codec_context->codec_id &&
		    (hwaccel->pix_fmt == AV_PIX_FMT_VDA_VLD ||
		     hwaccel->pix_fmt == AV_PIX_FMT_DXVA2_VLD ||
		     hwaccel->pix_fmt == AV_PIX_FMT_VAAPI_VLD)) {
			return hwaccel;
		}
	}

	return NULL;
}

enum AVPixelFormat get_hwaccel_format(struct AVCodecContext *s,
                                      const enum AVPixelFormat *fmt)
{
	(void)s;
	(void)fmt;

	// for now force output to common denominator
	return AV_PIX_FMT_YUV420P;
}

static bool initialize_decoder(struct ff_demuxer *demuxer,
                               AVCodecContext *codec_context, AVStream *stream,
                               bool hwaccel_decoder)
{
	switch (codec_context->codec_type) {
	case AVMEDIA_TYPE_AUDIO:
		demuxer->audio_decoder = ff_decoder_init(
		        codec_context, stream,
		        demuxer->options.audio_packet_queue_size,
		        demuxer->options.audio_frame_queue_size);

		demuxer->audio_decoder->hwaccel_decoder = hwaccel_decoder;
		demuxer->audio_decoder->frame_drop =
		        demuxer->options.frame_drop;
		demuxer->audio_decoder->natural_sync_clock =
		        AV_SYNC_AUDIO_MASTER;
		demuxer->audio_decoder->callbacks = &demuxer->audio_callbacks;

		if (!ff_callbacks_format(&demuxer->audio_callbacks,
		                         codec_context)) {
			ff_decoder_free(demuxer->audio_decoder);
			demuxer->audio_decoder = NULL;
			return false;
		}

		demuxer->audio_decoder = demuxer->audio_decoder;
		return true;

	case AVMEDIA_TYPE_VIDEO:
		demuxer->video_decoder = ff_decoder_init(
		        codec_context, stream,
		        demuxer->options.video_packet_queue_size,
		        demuxer->options.video_frame_queue_size);

		demuxer->video_decoder->hwaccel_decoder = hwaccel_decoder;
		demuxer->video_decoder->frame_drop =
		        demuxer->options.frame_drop;
		demuxer->video_decoder->natural_sync_clock =
		        AV_SYNC_VIDEO_MASTER;
		demuxer->video_decoder->callbacks = &demuxer->video_callbacks;

		if (!ff_callbacks_format(&demuxer->video_callbacks,
		                         codec_context)) {
			ff_decoder_free(demuxer->video_decoder);
			demuxer->video_decoder = NULL;
			return false;
		}

		return true;
	default:
		return false;
	}
}

typedef enum AVPixelFormat (*AVGetFormatCb)(struct AVCodecContext *s,
                                            const enum AVPixelFormat *fmt);

static bool find_decoder(struct ff_demuxer *demuxer, AVStream *stream)
{
	AVCodecContext *codec_context = NULL;
	AVCodec *codec = NULL;
	AVDictionary *options_dict = NULL;
	int ret;

	bool hwaccel_decoder = false;
	codec_context = stream->codec;

	// enable reference counted frames since we may have a buffer size
	// > 1
	codec_context->refcounted_frames = 1;

	// png/tiff decoders have serious issues with multiple threads
	if (codec_context->codec_id == AV_CODEC_ID_PNG ||
	    codec_context->codec_id == AV_CODEC_ID_TIFF ||
	    codec_context->codec_id == AV_CODEC_ID_JPEG2000 ||
	    codec_context->codec_id == AV_CODEC_ID_WEBP)
		codec_context->thread_count = 1;

	if (demuxer->options.is_hw_decoding) {
		AVHWAccel *hwaccel = find_hwaccel_codec(codec_context);

		if (hwaccel) {
			AVCodec *codec_vda =
			        avcodec_find_decoder_by_name(hwaccel->name);

			if (codec_vda != NULL) {
				AVGetFormatCb original_get_format =
				        codec_context->get_format;

				codec_context->get_format = get_hwaccel_format;
				codec_context->opaque = hwaccel;

				ret = avcodec_open2(codec_context, codec_vda,
				                    &options_dict);
				if (ret < 0) {
					av_log(NULL, AV_LOG_WARNING,
					       "no hardware decoder found for"
					       " codec with id %d",
					       codec_context->codec_id);
					codec_context->get_format =
					        original_get_format;
					codec_context->opaque = NULL;
				} else {
					codec = codec_vda;
					hwaccel_decoder = true;
				}
			}
		}
	}

	if (codec == NULL) {
		if (codec_context->codec_id == AV_CODEC_ID_VP8)
			codec = avcodec_find_decoder_by_name("libvpx");
		else if (codec_context->codec_id == AV_CODEC_ID_VP9)
			codec = avcodec_find_decoder_by_name("libvpx-vp9");

		if (!codec)
			codec = avcodec_find_decoder(codec_context->codec_id);
		if (codec == NULL) {
			av_log(NULL, AV_LOG_WARNING,
			       "no decoder found for"
			       " codec with id %d",
			       codec_context->codec_id);
			return false;
		}
		if (avcodec_open2(codec_context, codec, &options_dict) < 0) {
			av_log(NULL, AV_LOG_WARNING,
			       "unable to open decoder"
			       " with codec id %d",
			       codec_context->codec_id);
			return false;
		}
	}

	return initialize_decoder(demuxer, codec_context, stream,
	                          hwaccel_decoder);
}

void ff_demuxer_flush(struct ff_demuxer *demuxer)
{
	if (demuxer->video_decoder != NULL &&
	    demuxer->video_decoder->stream != NULL) {
		packet_queue_flush(&demuxer->video_decoder->packet_queue);
		packet_queue_put_flush_packet(
		        &demuxer->video_decoder->packet_queue);
	}

	if (demuxer->audio_decoder != NULL &&
	    demuxer->audio_decoder->stream != NULL) {
		packet_queue_flush(&demuxer->audio_decoder->packet_queue);
		packet_queue_put_flush_packet(
		        &demuxer->audio_decoder->packet_queue);
	}
}

void ff_demuxer_reset(struct ff_demuxer *demuxer)
{
	struct ff_packet packet = {0};
	struct ff_clock *clock = ff_clock_init();
	clock->sync_type = demuxer->clock.sync_type;
	clock->sync_clock = demuxer->clock.sync_clock;
	clock->opaque = demuxer->clock.opaque;

	packet.clock = clock;

	if (demuxer->audio_decoder != NULL) {
		ff_clock_retain(clock);
		packet_queue_put(&demuxer->audio_decoder->packet_queue,
		                 &packet);
	}

	if (demuxer->video_decoder != NULL) {
		ff_clock_retain(clock);
		packet_queue_put(&demuxer->video_decoder->packet_queue,
		                 &packet);
	}
}

static bool open_input(struct ff_demuxer *demuxer,
                       AVFormatContext **format_context)
{
	AVInputFormat *input_format = NULL;

	AVIOInterruptCB interrupted_callback;
	interrupted_callback.callback = demuxer_interrupted_callback;
	interrupted_callback.opaque = demuxer;

	*format_context = avformat_alloc_context();
	(*format_context)->interrupt_callback = interrupted_callback;

	if (demuxer->input_format != NULL) {
		input_format = av_find_input_format(demuxer->input_format);
		if (input_format == NULL)
			av_log(NULL, AV_LOG_WARNING,
			       "unable to find input "
			       "format %s",
			       demuxer->input_format);
	}

	if (avformat_open_input(format_context, demuxer->input, input_format,
	                        &demuxer->options.custom_options) != 0)
		return false;

	return avformat_find_stream_info(*format_context, NULL) >= 0;
}

static inline void set_decoder_start_time(struct ff_decoder *decoder,
                                          int64_t start_time)
{
	if (decoder)
		decoder->start_pts = av_rescale_q(start_time, AV_TIME_BASE_Q,
		                                  decoder->stream->time_base);
}

static bool find_and_initialize_stream_decoders(struct ff_demuxer *demuxer)
{
	AVFormatContext *format_context = demuxer->format_context;
	unsigned int i;
	AVStream *audio_stream = NULL;
	AVStream *video_stream = NULL;
	int64_t start_time = INT64_MAX;

	for (i = 0; i < format_context->nb_streams; i++) {
		AVCodecContext *codec = format_context->streams[i]->codec;

		if (codec->codec_type == AVMEDIA_TYPE_VIDEO && !video_stream)
			video_stream = format_context->streams[i];

		if (codec->codec_type == AVMEDIA_TYPE_AUDIO && !audio_stream)
			audio_stream = format_context->streams[i];
	}

	int default_stream_index =
	        av_find_default_stream_index(demuxer->format_context);

	if (default_stream_index >= 0) {
		AVStream *stream =
		        format_context->streams[default_stream_index];

		if (stream->codec->codec_type == AVMEDIA_TYPE_AUDIO)
			demuxer->clock.sync_type = AV_SYNC_AUDIO_MASTER;
		else if (stream->codec->codec_type == AVMEDIA_TYPE_VIDEO)
			demuxer->clock.sync_type = AV_SYNC_VIDEO_MASTER;
	}

	if (video_stream != NULL)
		find_decoder(demuxer, video_stream);

	if (audio_stream != NULL)
		find_decoder(demuxer, audio_stream);

	if (demuxer->video_decoder == NULL && demuxer->audio_decoder == NULL) {
		return false;
	}

	if (!set_clock_sync_type(demuxer)) {
		return false;
	}

	for (i = 0; i < format_context->nb_streams; i++) {
		AVStream *st = format_context->streams[i];
		int64_t st_start_time;

		if (st->discard == AVDISCARD_ALL ||
		    st->start_time == AV_NOPTS_VALUE) {
			continue;
		}

		st_start_time = av_rescale_q(st->start_time, st->time_base,
		                             AV_TIME_BASE_Q);
		start_time = FFMIN(start_time, st_start_time);
	}

	if (format_context->start_time != AV_NOPTS_VALUE) {
		if (start_time > format_context->start_time ||
		    start_time == INT64_MAX) {
			start_time = format_context->start_time;
		}
	}

	if (start_time != INT64_MAX) {
		set_decoder_start_time(demuxer->video_decoder, start_time);
		set_decoder_start_time(demuxer->audio_decoder, start_time);
	}

	if (demuxer->audio_decoder != NULL) {
		if (ff_callbacks_initialize(&demuxer->audio_callbacks)) {
			ff_decoder_start(demuxer->audio_decoder);
		} else {
			ff_decoder_free(demuxer->audio_decoder);
			demuxer->audio_decoder = NULL;
			if (!set_clock_sync_type(demuxer))
				return false;
		}
	}

	if (demuxer->video_decoder != NULL) {
		if (ff_callbacks_initialize(&demuxer->video_callbacks)) {
			ff_decoder_start(demuxer->video_decoder);
		} else {
			ff_decoder_free(demuxer->video_decoder);
			demuxer->video_decoder = NULL;
			if (!set_clock_sync_type(demuxer))
				return false;
		}
	}

	return set_clock_sync_type(demuxer);
}

static bool handle_seek(struct ff_demuxer *demuxer)
{
	int ret;

	if (demuxer->seek_request) {
		AVStream *seek_stream = NULL;
		int64_t seek_target = demuxer->seek_pos;

		if (demuxer->video_decoder != NULL) {
			seek_stream = demuxer->video_decoder->stream;

		} else if (demuxer->audio_decoder != NULL) {
			seek_stream = demuxer->audio_decoder->stream;
		}

		if (seek_stream != NULL &&
		    demuxer->format_context->duration != AV_NOPTS_VALUE) {
			seek_target = av_rescale_q(seek_target, AV_TIME_BASE_Q,
			                           seek_stream->time_base);
		}

		ret = av_seek_frame(demuxer->format_context, 0, seek_target,
		                    demuxer->seek_flags);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "unable to seek stream: %s",
			       av_err2str(ret));
			demuxer->seek_pos = 0;
			demuxer->seek_request = false;
			return false;

		} else {
			if (demuxer->seek_flush)
				ff_demuxer_flush(demuxer);
			ff_demuxer_reset(demuxer);
		}

		demuxer->seek_request = false;
	}
	return true;
}

static void seek_beginning(struct ff_demuxer *demuxer)
{
	if (demuxer->format_context->duration == AV_NOPTS_VALUE) {
		demuxer->seek_flags = AVSEEK_FLAG_FRAME;
		demuxer->seek_pos = 0;
	} else {
		demuxer->seek_flags = AVSEEK_FLAG_BACKWARD;
		demuxer->seek_pos = demuxer->format_context->start_time;
	}
	demuxer->seek_request = true;
	demuxer->seek_flush = false;
	av_log(NULL, AV_LOG_VERBOSE, "looping media %s", demuxer->input);
}

static void *demux_thread(void *opaque)
{
	struct ff_demuxer *demuxer = (struct ff_demuxer *)opaque;
	int result;

	struct ff_packet packet = {0};

	if (!open_input(demuxer, &demuxer->format_context))
		goto fail;

	av_dump_format(demuxer->format_context, 0, demuxer->input, 0);

	if (!find_and_initialize_stream_decoders(demuxer))
		goto fail;

	ff_demuxer_reset(demuxer);

	while (!demuxer->abort) {
		// failed to seek (looping?)
		if (!handle_seek(demuxer))
			break;

		if (ff_decoder_full(demuxer->audio_decoder) ||
		    ff_decoder_full(demuxer->video_decoder)) {
			av_usleep(10 * 1000); // 10ms
			continue;
		}

		result = av_read_frame(demuxer->format_context, &packet.base);
		if (result < 0) {
			bool eof = false;
			if (result == AVERROR_EOF) {
				eof = true;
			} else if (demuxer->format_context->pb != NULL) {
				AVIOContext *io_context =
				        demuxer->format_context->pb;
				if (io_context->error == 0) {
					av_usleep(100 * 1000); // 100ms
					continue;
				} else {
					if (io_context->eof_reached != 0)
						eof = true;
				}
			}

			if (eof) {
				if (demuxer->options.is_looping) {
					seek_beginning(demuxer);
				} else {
					break;
				}
				continue;
			} else {
				av_log(NULL, AV_LOG_ERROR,
				       "av_read_frame() failed: %s",
				       av_err2str(result));
				break;
			}
		}

		if (ff_decoder_accept(demuxer->video_decoder, &packet))
			continue;
		else if (ff_decoder_accept(demuxer->audio_decoder, &packet))
			continue;
		else
			av_free_packet(&packet.base);
	}
	if (demuxer->audio_decoder != NULL)
		demuxer->audio_decoder->eof = true;
	if (demuxer->video_decoder != NULL)
		demuxer->video_decoder->eof = true;
fail:
	demuxer->abort = true;

	return NULL;
}
