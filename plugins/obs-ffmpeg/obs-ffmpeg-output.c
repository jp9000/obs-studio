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

#include <obs-module.h>
#include <util/circlebuf.h>
#include <util/threading.h>
#include <util/dstr.h>
#include <util/darray.h>
#include <util/platform.h>

#include "obs-ffmpeg-output.h"
#include "obs-ffmpeg-formats.h"
#include "obs-ffmpeg-compat.h"

struct ffmpeg_output {
	obs_output_t *output;
	volatile bool active;
	struct ffmpeg_data ff_data;

	bool connecting;
	pthread_t start_thread;

	uint64_t total_bytes;

	uint64_t audio_start_ts;
	uint64_t video_start_ts;
	uint64_t stop_ts;
	volatile bool stopping;

	bool write_thread_active;
	pthread_mutex_t write_mutex;
	pthread_t write_thread;
	os_sem_t *write_sem;
	os_event_t *stop_event;

	DARRAY(AVPacket) packets;
};

/* ------------------------------------------------------------------------- */

static void ffmpeg_output_set_last_error(struct ffmpeg_data *data,
					 const char *error)
{
	if (data->last_error)
		bfree(data->last_error);

	data->last_error = bstrdup(error);
}

void ffmpeg_log_error(int log_level, struct ffmpeg_data *data,
		      const char *format, ...)
{
	va_list args;
	char out[4096];

	va_start(args, format);
	vsnprintf(out, sizeof(out), format, args);
	va_end(args);

	ffmpeg_output_set_last_error(data, out);

	blog(log_level, "%s", out);
}

static bool new_stream(struct ffmpeg_data *data, AVStream **stream,
		       AVCodec **codec, enum AVCodecID id, const char *name)
{
	*codec = (!!name && *name) ? avcodec_find_encoder_by_name(name)
				   : avcodec_find_encoder(id);

	if (!*codec) {
		ffmpeg_log_error(LOG_WARNING, data,
				 "Couldn't find encoder '%s'",
				 avcodec_get_name(id));
		return false;
	}

	*stream = avformat_new_stream(data->output, *codec);
	if (!*stream) {
		ffmpeg_log_error(LOG_WARNING, data,
				 "Couldn't create stream for encoder '%s'",
				 avcodec_get_name(id));
		return false;
	}

	(*stream)->id = data->output->nb_streams - 1;
	return true;
}

static bool parse_params(AVCodecContext *context, char **opts)
{
	bool ret = true;

	if (!context || !context->priv_data)
		return true;

	while (*opts) {
		char *opt = *opts;
		char *assign = strchr(opt, '=');

		if (assign) {
			char *name = opt;
			char *value;

			*assign = 0;
			value = assign + 1;

			if (av_opt_set(context, name, value,
				       AV_OPT_SEARCH_CHILDREN)) {
				blog(LOG_WARNING, "Failed to set %s=%s", name,
				     value);
				ret = false;
			}
		}

		opts++;
	}

	return ret;
}

static bool open_video_codec(struct ffmpeg_data *data)
{
	AVCodecContext *const context = data->video_ctx;
	char **opts = strlist_split(data->config.video_settings, ' ', false);
	int ret;

	if (strcmp(data->vcodec->name, "libx264") == 0)
		av_opt_set(context->priv_data, "preset", "veryfast", 0);

	if (opts) {
		// libav requires x264 parameters in a special format which may be non-obvious
		if (!parse_params(context, opts) &&
		    strcmp(data->vcodec->name, "libx264") == 0)
			blog(LOG_WARNING,
			     "If you're trying to set x264 parameters, use x264-params=name=value:name=value");
		strlist_free(opts);
	}

	ret = avcodec_open2(context, data->vcodec, NULL);
	if (ret < 0) {
		ffmpeg_log_error(LOG_WARNING, data,
				 "Failed to open video codec: %s",
				 av_err2str(ret));
		return false;
	}

	data->vframe = av_frame_alloc();
	if (!data->vframe) {
		ffmpeg_log_error(LOG_WARNING, data,
				 "Failed to allocate video frame");
		return false;
	}

	data->vframe->format = context->pix_fmt;
	data->vframe->width = context->width;
	data->vframe->height = context->height;
	data->vframe->color_range = data->config.color_range;
	data->vframe->color_primaries = data->config.color_primaries;
	data->vframe->color_trc = data->config.color_trc;
	data->vframe->colorspace = data->config.colorspace;

	ret = av_frame_get_buffer(data->vframe, base_get_alignment());
	if (ret < 0) {
		ffmpeg_log_error(LOG_WARNING, data,
				 "Failed to allocate vframe: %s",
				 av_err2str(ret));
		return false;
	}

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	avcodec_parameters_from_context(data->video->codecpar, context);
#endif

	return true;
}

static bool init_swscale(struct ffmpeg_data *data, AVCodecContext *context)
{
	data->swscale = sws_getContext(
		data->config.width, data->config.height, data->config.format,
		data->config.scale_width, data->config.scale_height,
		context->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);

	if (!data->swscale) {
		ffmpeg_log_error(LOG_WARNING, data,
				 "Could not initialize swscale");
		return false;
	}

	return true;
}

static bool create_video_stream(struct ffmpeg_data *data)
{
	enum AVPixelFormat closest_format;
	AVCodecContext *context;
	struct obs_video_info ovi;

	if (!obs_get_video_info(&ovi)) {
		ffmpeg_log_error(LOG_WARNING, data, "No active video");
		return false;
	}

	if (!new_stream(data, &data->video, &data->vcodec,
			data->output->oformat->video_codec,
			data->config.video_encoder))
		return false;

	closest_format = data->config.format;
	if (data->vcodec->pix_fmts) {
		closest_format = avcodec_find_best_pix_fmt_of_list(
			data->vcodec->pix_fmts, data->config.format, 0, NULL);
	}

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	context = avcodec_alloc_context3(data->vcodec);
#else
	context = data->video->codec;
#endif
	context->bit_rate = (int64_t)data->config.video_bitrate * 1000;
	context->width = data->config.scale_width;
	context->height = data->config.scale_height;
	context->time_base = (AVRational){ovi.fps_den, ovi.fps_num};
	context->gop_size = data->config.gop_size;
	context->pix_fmt = closest_format;
	context->color_range = data->config.color_range;
	context->color_primaries = data->config.color_primaries;
	context->color_trc = data->config.color_trc;
	context->colorspace = data->config.colorspace;
	context->thread_count = 0;

	data->video->time_base = context->time_base;

	if (data->output->oformat->flags & AVFMT_GLOBALHEADER)
		context->flags |= CODEC_FLAG_GLOBAL_H;

	data->video_ctx = context;

	if (!open_video_codec(data))
		return false;

	if (context->pix_fmt != data->config.format ||
	    data->config.width != data->config.scale_width ||
	    data->config.height != data->config.scale_height) {

		if (!init_swscale(data, context))
			return false;
	}

	return true;
}

static bool open_audio_codec(struct ffmpeg_data *data, int idx)
{
	AVCodecContext *const context = data->audio_infos[idx].ctx;
	char **opts = strlist_split(data->config.audio_settings, ' ', false);
	int ret;

	if (opts) {
		parse_params(context, opts);
		strlist_free(opts);
	}

	data->aframe[idx] = av_frame_alloc();
	if (!data->aframe[idx]) {
		ffmpeg_log_error(LOG_WARNING, data,
				 "Failed to allocate audio frame");
		return false;
	}

	data->aframe[idx]->format = context->sample_fmt;
	data->aframe[idx]->channels = context->channels;
	data->aframe[idx]->channel_layout = context->channel_layout;
	data->aframe[idx]->sample_rate = context->sample_rate;

	context->strict_std_compliance = -2;

	ret = avcodec_open2(context, data->acodec, NULL);
	if (ret < 0) {
		ffmpeg_log_error(LOG_WARNING, data,
				 "Failed to open audio codec: %s",
				 av_err2str(ret));
		return false;
	}

	data->frame_size = context->frame_size ? context->frame_size : 1024;

	ret = av_samples_alloc(data->samples[idx], NULL, context->channels,
			       data->frame_size, context->sample_fmt, 0);
	if (ret < 0) {
		ffmpeg_log_error(LOG_WARNING, data,
				 "Failed to create audio buffer: %s",
				 av_err2str(ret));
		return false;
	}

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	avcodec_parameters_from_context(data->audio_infos[idx].stream->codecpar,
					context);
#endif

	return true;
}

static bool create_audio_stream(struct ffmpeg_data *data, int idx)
{
	AVCodecContext *context;
	AVStream *stream;
	struct obs_audio_info aoi;

	if (!obs_get_audio_info(&aoi)) {
		ffmpeg_log_error(LOG_WARNING, data, "No active audio");
		return false;
	}

	if (!new_stream(data, &stream, &data->acodec,
			data->output->oformat->audio_codec,
			data->config.audio_encoder))
		return false;

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	context = avcodec_alloc_context3(data->acodec);
#else
	context = stream->codec;
#endif
	context->bit_rate = (int64_t)data->config.audio_bitrate * 1000;
	context->time_base = (AVRational){1, aoi.samples_per_sec};
	context->channels = get_audio_channels(aoi.speakers);
	context->sample_rate = aoi.samples_per_sec;
	context->channel_layout =
		av_get_default_channel_layout(context->channels);

	//AVlib default channel layout for 5 channels is 5.0 ; fix for 4.1
	if (aoi.speakers == SPEAKERS_4POINT1)
		context->channel_layout = av_get_channel_layout("4.1");

	context->sample_fmt = data->acodec->sample_fmts
				      ? data->acodec->sample_fmts[0]
				      : AV_SAMPLE_FMT_FLTP;

	stream->time_base = context->time_base;

	data->audio_samplerate = aoi.samples_per_sec;
	data->audio_format = convert_ffmpeg_sample_format(context->sample_fmt);
	data->audio_planes = get_audio_planes(data->audio_format, aoi.speakers);
	data->audio_size = get_audio_size(data->audio_format, aoi.speakers, 1);

	if (data->output->oformat->flags & AVFMT_GLOBALHEADER)
		context->flags |= CODEC_FLAG_GLOBAL_H;

	data->audio_infos[idx].stream = stream;
	data->audio_infos[idx].ctx = context;

	return open_audio_codec(data, idx);
}

static inline bool init_streams(struct ffmpeg_data *data)
{
	AVOutputFormat *format = data->output->oformat;

	if (format->video_codec != AV_CODEC_ID_NONE)
		if (!create_video_stream(data))
			return false;

	if (format->audio_codec != AV_CODEC_ID_NONE &&
	    data->num_audio_streams) {
		data->audio_infos = calloc(data->num_audio_streams,
					   sizeof(*data->audio_infos));
		for (int i = 0; i < data->num_audio_streams; i++) {
			if (!create_audio_stream(data, i))
				return false;
		}
	}

	return true;
}

static inline bool open_output_file(struct ffmpeg_data *data)
{
	AVOutputFormat *format = data->output->oformat;
	int ret;

	AVDictionary *dict = NULL;
	if ((ret = av_dict_parse_string(&dict, data->config.muxer_settings, "=",
					" ", 0))) {
		ffmpeg_log_error(LOG_WARNING, data,
				 "Failed to parse muxer settings: %s\n%s",
				 av_err2str(ret), data->config.muxer_settings);

		av_dict_free(&dict);
		return false;
	}

	if (av_dict_count(dict) > 0) {
		struct dstr str = {0};

		AVDictionaryEntry *entry = NULL;
		while ((entry = av_dict_get(dict, "", entry,
					    AV_DICT_IGNORE_SUFFIX)))
			dstr_catf(&str, "\n\t%s=%s", entry->key, entry->value);

		blog(LOG_INFO, "Using muxer settings: %s", str.array);
		dstr_free(&str);
	}

	if ((format->flags & AVFMT_NOFILE) == 0) {
		ret = avio_open2(&data->output->pb, data->config.url,
				 AVIO_FLAG_WRITE, NULL, &dict);
		if (ret < 0) {
			ffmpeg_log_error(LOG_WARNING, data,
					 "Couldn't open '%s', %s",
					 data->config.url, av_err2str(ret));
			av_dict_free(&dict);
			return false;
		}
	}

	ret = avformat_write_header(data->output, &dict);
	if (ret < 0) {
		ffmpeg_log_error(LOG_WARNING, data, "Error opening '%s': %s",
				 data->config.url, av_err2str(ret));
		return false;
	}

	if (av_dict_count(dict) > 0) {
		struct dstr str = {0};

		AVDictionaryEntry *entry = NULL;
		while ((entry = av_dict_get(dict, "", entry,
					    AV_DICT_IGNORE_SUFFIX)))
			dstr_catf(&str, "\n\t%s=%s", entry->key, entry->value);

		blog(LOG_INFO, "Invalid muxer settings: %s", str.array);
		dstr_free(&str);
	}

	av_dict_free(&dict);

	return true;
}

static void close_video(struct ffmpeg_data *data)
{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	avcodec_free_context(&data->video_ctx);
#else
	avcodec_close(data->video->codec);
#endif
	av_frame_unref(data->vframe);

	// This format for some reason derefs video frame
	// too many times
	if (data->vcodec->id == AV_CODEC_ID_A64_MULTI ||
	    data->vcodec->id == AV_CODEC_ID_A64_MULTI5)
		return;

	av_frame_free(&data->vframe);
}

static void close_audio(struct ffmpeg_data *data)
{
	for (int idx = 0; idx < data->num_audio_streams; idx++) {
		for (size_t i = 0; i < MAX_AV_PLANES; i++)
			circlebuf_free(&data->excess_frames[idx][i]);

		if (data->samples[idx][0])
			av_freep(&data->samples[idx][0]);
		if (data->audio_infos[idx].ctx) {
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
			avcodec_free_context(&data->audio_infos[idx].ctx);
#else
			avcodec_close(data->audio_infos[idx].stream->codec);
#endif
		}
		if (data->aframe[idx])
			av_frame_free(&data->aframe[idx]);
	}
}

void ffmpeg_data_free(struct ffmpeg_data *data)
{
	if (data->initialized)
		av_write_trailer(data->output);

	if (data->video)
		close_video(data);
	if (data->audio_infos) {
		close_audio(data);
		free(data->audio_infos);
		data->audio_infos = NULL;
	}

	if (data->output) {
		if ((data->output->oformat->flags & AVFMT_NOFILE) == 0)
			avio_close(data->output->pb);

		avformat_free_context(data->output);
	}

	if (data->last_error)
		bfree(data->last_error);

	memset(data, 0, sizeof(struct ffmpeg_data));
}

static inline const char *safe_str(const char *s)
{
	if (s == NULL)
		return "(NULL)";
	else
		return s;
}

static enum AVCodecID get_codec_id(const char *name, int id)
{
	AVCodec *codec;

	if (id != 0)
		return (enum AVCodecID)id;

	if (!name || !*name)
		return AV_CODEC_ID_NONE;

	codec = avcodec_find_encoder_by_name(name);
	if (!codec)
		return AV_CODEC_ID_NONE;

	return codec->id;
}

static void set_encoder_ids(struct ffmpeg_data *data)
{
	data->output->oformat->video_codec = get_codec_id(
		data->config.video_encoder, data->config.video_encoder_id);

	data->output->oformat->audio_codec = get_codec_id(
		data->config.audio_encoder, data->config.audio_encoder_id);
}

bool ffmpeg_data_init(struct ffmpeg_data *data, struct ffmpeg_cfg *config)
{
	bool is_rtmp = false;

	memset(data, 0, sizeof(struct ffmpeg_data));
	data->config = *config;
	data->num_audio_streams = config->audio_mix_count;
	data->audio_tracks = config->audio_tracks;
	if (!config->url || !*config->url)
		return false;

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
	av_register_all();
#endif
	avformat_network_init();

	is_rtmp = (astrcmpi_n(config->url, "rtmp://", 7) == 0);

	AVOutputFormat *output_format = av_guess_format(
		is_rtmp ? "flv" : data->config.format_name, data->config.url,
		is_rtmp ? NULL : data->config.format_mime_type);

	if (output_format == NULL) {
		ffmpeg_log_error(
			LOG_WARNING, data,
			"Couldn't find matching output format with "
			"parameters: name=%s, url=%s, mime=%s",
			safe_str(is_rtmp ? "flv" : data->config.format_name),
			safe_str(data->config.url),
			safe_str(is_rtmp ? NULL
					 : data->config.format_mime_type));

		goto fail;
	}

	avformat_alloc_output_context2(&data->output, output_format, NULL,
				       data->config.url);

	if (!data->output) {
		ffmpeg_log_error(LOG_WARNING, data,
				 "Couldn't create avformat context");
		goto fail;
	}

	if (is_rtmp) {
		data->output->oformat->video_codec = AV_CODEC_ID_H264;
		data->output->oformat->audio_codec = AV_CODEC_ID_AAC;
	} else {
		if (data->config.format_name)
			set_encoder_ids(data);
	}

	if (!init_streams(data))
		goto fail;
	if (!open_output_file(data))
		goto fail;

	av_dump_format(data->output, 0, NULL, 1);

	data->initialized = true;
	return true;

fail:
	blog(LOG_WARNING, "ffmpeg_data_init failed");
	return false;
}

/* ------------------------------------------------------------------------- */

static inline bool stopping(struct ffmpeg_output *output)
{
	return os_atomic_load_bool(&output->stopping);
}

static const char *ffmpeg_output_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("FFmpegOutput");
}

static void ffmpeg_log_callback(void *param, int level, const char *format,
				va_list args)
{
	if (level <= AV_LOG_INFO)
		blogva(LOG_DEBUG, format, args);

	UNUSED_PARAMETER(param);
}

static void *ffmpeg_output_create(obs_data_t *settings, obs_output_t *output)
{
	struct ffmpeg_output *data = bzalloc(sizeof(struct ffmpeg_output));
	pthread_mutex_init_value(&data->write_mutex);
	data->output = output;

	if (pthread_mutex_init(&data->write_mutex, NULL) != 0)
		goto fail;
	if (os_event_init(&data->stop_event, OS_EVENT_TYPE_AUTO) != 0)
		goto fail;
	if (os_sem_init(&data->write_sem, 0) != 0)
		goto fail;

	av_log_set_callback(ffmpeg_log_callback);

	UNUSED_PARAMETER(settings);
	return data;

fail:
	pthread_mutex_destroy(&data->write_mutex);
	os_event_destroy(data->stop_event);
	bfree(data);
	return NULL;
}

static void ffmpeg_output_full_stop(void *data);
static void ffmpeg_deactivate(struct ffmpeg_output *output);

static void ffmpeg_output_destroy(void *data)
{
	struct ffmpeg_output *output = data;

	if (output) {
		if (output->connecting)
			pthread_join(output->start_thread, NULL);

		ffmpeg_output_full_stop(output);

		pthread_mutex_destroy(&output->write_mutex);
		os_sem_destroy(output->write_sem);
		os_event_destroy(output->stop_event);
		bfree(data);
	}
}

static inline void copy_data(AVFrame *pic, const struct video_data *frame,
			     int height, enum AVPixelFormat format)
{
	int h_chroma_shift, v_chroma_shift;
	av_pix_fmt_get_chroma_sub_sample(format, &h_chroma_shift,
					 &v_chroma_shift);
	for (int plane = 0; plane < MAX_AV_PLANES; plane++) {
		if (!frame->data[plane])
			continue;

		int frame_rowsize = (int)frame->linesize[plane];
		int pic_rowsize = pic->linesize[plane];
		int bytes = frame_rowsize < pic_rowsize ? frame_rowsize
							: pic_rowsize;
		int plane_height = height >> (plane ? v_chroma_shift : 0);

		for (int y = 0; y < plane_height; y++) {
			int pos_frame = y * frame_rowsize;
			int pos_pic = y * pic_rowsize;

			memcpy(pic->data[plane] + pos_pic,
			       frame->data[plane] + pos_frame, bytes);
		}
	}
}

static void receive_video(void *param, struct video_data *frame)
{
	struct ffmpeg_output *output = param;
	struct ffmpeg_data *data = &output->ff_data;

	// codec doesn't support video or none configured
	if (!data->video)
		return;

	AVCodecContext *context = data->video_ctx;
	AVPacket packet = {0};
	int ret = 0, got_packet;

	av_init_packet(&packet);

	if (!output->video_start_ts)
		output->video_start_ts = frame->timestamp;
	if (!data->start_timestamp)
		data->start_timestamp = frame->timestamp;

	ret = av_frame_make_writable(data->vframe);
	if (ret < 0) {
		blog(LOG_WARNING,
		     "receive_video: Error obtaining writable "
		     "AVFrame: %s",
		     av_err2str(ret));
		//FIXME: stop the encode with an error
		return;
	}
	if (!!data->swscale)
		sws_scale(data->swscale, (const uint8_t *const *)frame->data,
			  (const int *)frame->linesize, 0, data->config.height,
			  data->vframe->data, data->vframe->linesize);
	else
		copy_data(data->vframe, frame, context->height,
			  context->pix_fmt);
#if LIBAVFORMAT_VERSION_MAJOR < 58
	if (data->output->flags & AVFMT_RAWPICTURE) {
		packet.flags |= AV_PKT_FLAG_KEY;
		packet.stream_index = data->video->index;
		packet.data = data->vframe->data[0];
		packet.size = sizeof(AVPicture);

		pthread_mutex_lock(&output->write_mutex);
		da_push_back(output->packets, &packet);
		pthread_mutex_unlock(&output->write_mutex);
		os_sem_post(output->write_sem);

	} else {
#endif
		data->vframe->pts = data->total_frames;
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 40, 101)
		ret = avcodec_send_frame(context, data->vframe);
		if (ret == 0)
			ret = avcodec_receive_packet(context, &packet);

		got_packet = (ret == 0);

		if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
			ret = 0;
#else
	ret = avcodec_encode_video2(context, &packet, data->vframe,
				    &got_packet);
#endif
		if (ret < 0) {
			blog(LOG_WARNING,
			     "receive_video: Error encoding "
			     "video: %s",
			     av_err2str(ret));
			//FIXME: stop the encode with an error
			return;
		}

		if (!ret && got_packet && packet.size) {
			packet.pts = rescale_ts(packet.pts, context,
						data->video->time_base);
			packet.dts = rescale_ts(packet.dts, context,
						data->video->time_base);
			packet.duration = (int)av_rescale_q(
				packet.duration, context->time_base,
				data->video->time_base);

			pthread_mutex_lock(&output->write_mutex);
			da_push_back(output->packets, &packet);
			pthread_mutex_unlock(&output->write_mutex);
			os_sem_post(output->write_sem);
		} else {
			ret = 0;
		}
#if LIBAVFORMAT_VERSION_MAJOR < 58
	}
#endif
	if (ret != 0) {
		blog(LOG_WARNING, "receive_video: Error writing video: %s",
		     av_err2str(ret));
		//FIXME: stop the encode with an error
	}

	data->total_frames++;
}

static void encode_audio(struct ffmpeg_output *output, int idx,
			 struct AVCodecContext *context, size_t block_size)
{
	struct ffmpeg_data *data = &output->ff_data;

	AVPacket packet = {0};
	int ret, got_packet;
	size_t total_size = data->frame_size * block_size * context->channels;

	data->aframe[idx]->nb_samples = data->frame_size;
	data->aframe[idx]->pts = av_rescale_q(
		data->total_samples[idx], (AVRational){1, context->sample_rate},
		context->time_base);

	ret = avcodec_fill_audio_frame(data->aframe[idx], context->channels,
				       context->sample_fmt,
				       data->samples[idx][0], (int)total_size,
				       1);
	if (ret < 0) {
		blog(LOG_WARNING,
		     "encode_audio: avcodec_fill_audio_frame "
		     "failed: %s",
		     av_err2str(ret));
		//FIXME: stop the encode with an error
		return;
	}

	data->total_samples[idx] += data->frame_size;

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 40, 101)
	ret = avcodec_send_frame(context, data->aframe[idx]);
	if (ret == 0)
		ret = avcodec_receive_packet(context, &packet);

	got_packet = (ret == 0);

	if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
		ret = 0;
#else
	ret = avcodec_encode_audio2(context, &packet, data->aframe[idx],
				    &got_packet);
#endif
	if (ret < 0) {
		blog(LOG_WARNING, "encode_audio: Error encoding audio: %s",
		     av_err2str(ret));
		//FIXME: stop the encode with an error
		return;
	}

	if (!got_packet)
		return;

	packet.pts = rescale_ts(packet.pts, context,
				data->audio_infos[idx].stream->time_base);
	packet.dts = rescale_ts(packet.dts, context,
				data->audio_infos[idx].stream->time_base);
	packet.duration =
		(int)av_rescale_q(packet.duration, context->time_base,
				  data->audio_infos[idx].stream->time_base);
	packet.stream_index = data->audio_infos[idx].stream->index;

	pthread_mutex_lock(&output->write_mutex);
	da_push_back(output->packets, &packet);
	pthread_mutex_unlock(&output->write_mutex);
	os_sem_post(output->write_sem);
}

/* Given a bitmask for the selected tracks and the mix index,
 * this returns the stream index which will be passed to the muxer. */
static int get_track_order(int track_config, size_t mix_index)
{
	int position = 0;
	for (size_t i = 0; i < mix_index; i++) {
		if (track_config & 1 << i)
			position++;
	}
	return position;
}

static void receive_audio(void *param, size_t mix_idx, struct audio_data *frame)
{
	struct ffmpeg_output *output = param;
	struct ffmpeg_data *data = &output->ff_data;
	size_t frame_size_bytes;
	struct audio_data in = *frame;
	int track_order;

	// codec doesn't support audio or none configured
	if (!data->audio_infos)
		return;

	/* check that the track was selected */
	if ((data->audio_tracks & (1 << mix_idx)) == 0)
		return;

	/* get track order (first selected, etc ...) */
	track_order = get_track_order(data->audio_tracks, mix_idx);

	AVCodecContext *context = data->audio_infos[track_order].ctx;

	if (!data->start_timestamp)
		return;

	if (!output->audio_start_ts)
		output->audio_start_ts = in.timestamp;

	frame_size_bytes = (size_t)data->frame_size * data->audio_size;

	for (size_t i = 0; i < data->audio_planes; i++)
		circlebuf_push_back(&data->excess_frames[track_order][i],
				    in.data[i], in.frames * data->audio_size);

	while (data->excess_frames[track_order][0].size >= frame_size_bytes) {
		for (size_t i = 0; i < data->audio_planes; i++)
			circlebuf_pop_front(
				&data->excess_frames[track_order][i],
				data->samples[track_order][i],
				frame_size_bytes);

		encode_audio(output, track_order, context, data->audio_size);
	}
}

static uint64_t get_packet_sys_dts(struct ffmpeg_output *output,
				   AVPacket *packet)
{
	struct ffmpeg_data *data = &output->ff_data;
	uint64_t pause_offset = obs_output_get_pause_offset(output->output);
	uint64_t start_ts;

	AVRational time_base;

	if (data->video && data->video->index == packet->stream_index) {
		time_base = data->video->time_base;
		start_ts = output->video_start_ts;
	} else {
		time_base = data->audio_infos[0].stream->time_base;
		start_ts = output->audio_start_ts;
	}

	return start_ts + pause_offset +
	       (uint64_t)av_rescale_q(packet->dts, time_base,
				      (AVRational){1, 1000000000});
}

static int process_packet(struct ffmpeg_output *output)
{
	AVPacket packet;
	bool new_packet = false;
	int ret;

	pthread_mutex_lock(&output->write_mutex);
	if (output->packets.num) {
		packet = output->packets.array[0];
		da_erase(output->packets, 0);
		new_packet = true;
	}
	pthread_mutex_unlock(&output->write_mutex);

	if (!new_packet)
		return 0;

	/*blog(LOG_DEBUG, "size = %d, flags = %lX, stream = %d, "
			"packets queued: %lu",
			packet.size, packet.flags,
			packet.stream_index, output->packets.num);*/

	if (stopping(output)) {
		uint64_t sys_ts = get_packet_sys_dts(output, &packet);
		if (sys_ts >= output->stop_ts)
			return 0;
	}

	output->total_bytes += packet.size;

	ret = av_interleaved_write_frame(output->ff_data.output, &packet);
	if (ret < 0) {
		av_free_packet(&packet);
		ffmpeg_log_error(LOG_WARNING, &output->ff_data,
				 "process_packet: Error writing packet: %s",
				 av_err2str(ret));
		return ret;
	}

	return 0;
}

static void *write_thread(void *data)
{
	struct ffmpeg_output *output = data;

	while (os_sem_wait(output->write_sem) == 0) {
		/* check to see if shutting down */
		if (os_event_try(output->stop_event) == 0)
			break;

		int ret = process_packet(output);
		if (ret != 0) {
			int code = OBS_OUTPUT_ERROR;

			pthread_detach(output->write_thread);
			output->write_thread_active = false;

			if (ret == -ENOSPC)
				code = OBS_OUTPUT_NO_SPACE;

			obs_output_signal_stop(output->output, code);
			ffmpeg_deactivate(output);
			break;
		}
	}

	output->active = false;
	return NULL;
}

static inline const char *get_string_or_null(obs_data_t *settings,
					     const char *name)
{
	const char *value = obs_data_get_string(settings, name);
	if (!value || !strlen(value))
		return NULL;
	return value;
}

static int get_audio_mix_count(int audio_mix_mask)
{
	int mix_count = 0;
	for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
		if ((audio_mix_mask & (1 << i)) != 0) {
			mix_count++;
		}
	}

	return mix_count;
}

static bool try_connect(struct ffmpeg_output *output)
{
	video_t *video = obs_output_video(output->output);
	const struct video_output_info *voi = video_output_get_info(video);
	struct ffmpeg_cfg config;
	obs_data_t *settings;
	bool success;
	int ret;

	settings = obs_output_get_settings(output->output);

	obs_data_set_default_int(settings, "gop_size", 120);

	config.url = obs_data_get_string(settings, "url");
	config.format_name = get_string_or_null(settings, "format_name");
	config.format_mime_type =
		get_string_or_null(settings, "format_mime_type");
	config.muxer_settings = obs_data_get_string(settings, "muxer_settings");
	config.video_bitrate = (int)obs_data_get_int(settings, "video_bitrate");
	config.audio_bitrate = (int)obs_data_get_int(settings, "audio_bitrate");
	config.gop_size = (int)obs_data_get_int(settings, "gop_size");
	config.video_encoder = get_string_or_null(settings, "video_encoder");
	config.video_encoder_id =
		(int)obs_data_get_int(settings, "video_encoder_id");
	config.audio_encoder = get_string_or_null(settings, "audio_encoder");
	config.audio_encoder_id =
		(int)obs_data_get_int(settings, "audio_encoder_id");
	config.video_settings = obs_data_get_string(settings, "video_settings");
	config.audio_settings = obs_data_get_string(settings, "audio_settings");
	config.scale_width = (int)obs_data_get_int(settings, "scale_width");
	config.scale_height = (int)obs_data_get_int(settings, "scale_height");
	config.width = (int)obs_output_get_width(output->output);
	config.height = (int)obs_output_get_height(output->output);
	config.format =
		obs_to_ffmpeg_video_format(video_output_get_format(video));
	config.audio_tracks = (int)obs_output_get_mixers(output->output);
	config.audio_mix_count = get_audio_mix_count(config.audio_tracks);

	config.color_range = voi->range == VIDEO_RANGE_FULL ? AVCOL_RANGE_JPEG
							    : AVCOL_RANGE_MPEG;
	switch (voi->colorspace) {
	case VIDEO_CS_601:
		config.color_primaries = AVCOL_PRI_SMPTE170M;
		config.color_trc = AVCOL_TRC_SMPTE170M;
		break;
	case VIDEO_CS_DEFAULT:
	case VIDEO_CS_709:
		config.color_primaries = AVCOL_PRI_BT709;
		config.color_trc = AVCOL_TRC_BT709;
		break;
	case VIDEO_CS_SRGB:
		config.color_primaries = AVCOL_PRI_BT709;
		config.color_trc = AVCOL_TRC_IEC61966_2_1;
		break;
	}

	if (format_is_yuv(voi->format)) {
		config.colorspace = (voi->colorspace == VIDEO_CS_601)
					    ? AVCOL_SPC_SMPTE170M
					    : AVCOL_SPC_BT709;
	} else {
		config.colorspace = AVCOL_SPC_RGB;
	}

	if (config.format == AV_PIX_FMT_NONE) {
		blog(LOG_DEBUG, "invalid pixel format used for FFmpeg output");
		return false;
	}

	if (!config.scale_width)
		config.scale_width = config.width;
	if (!config.scale_height)
		config.scale_height = config.height;

	success = ffmpeg_data_init(&output->ff_data, &config);
	obs_data_release(settings);

	if (!success) {
		if (output->ff_data.last_error) {
			obs_output_set_last_error(output->output,
						  output->ff_data.last_error);
		}
		ffmpeg_data_free(&output->ff_data);
		return false;
	}

	struct audio_convert_info aci = {.format =
						 output->ff_data.audio_format};

	output->active = true;

	if (!obs_output_can_begin_data_capture(output->output, 0))
		return false;

	ret = pthread_create(&output->write_thread, NULL, write_thread, output);
	if (ret != 0) {
		ffmpeg_log_error(LOG_WARNING, &output->ff_data,
				 "ffmpeg_output_start: failed to create write "
				 "thread.");
		ffmpeg_output_full_stop(output);
		return false;
	}

	obs_output_set_video_conversion(output->output, NULL);
	obs_output_set_audio_conversion(output->output, &aci);
	obs_output_begin_data_capture(output->output, 0);
	output->write_thread_active = true;
	return true;
}

static void *start_thread(void *data)
{
	struct ffmpeg_output *output = data;

	if (!try_connect(output))
		obs_output_signal_stop(output->output,
				       OBS_OUTPUT_CONNECT_FAILED);

	output->connecting = false;
	return NULL;
}

static bool ffmpeg_output_start(void *data)
{
	struct ffmpeg_output *output = data;
	int ret;

	if (output->connecting)
		return false;

	os_atomic_set_bool(&output->stopping, false);
	output->audio_start_ts = 0;
	output->video_start_ts = 0;
	output->total_bytes = 0;

	ret = pthread_create(&output->start_thread, NULL, start_thread, output);
	return (output->connecting = (ret == 0));
}

static void ffmpeg_output_full_stop(void *data)
{
	struct ffmpeg_output *output = data;

	if (output->active) {
		obs_output_end_data_capture(output->output);
		ffmpeg_deactivate(output);
	}
}

static void ffmpeg_output_stop(void *data, uint64_t ts)
{
	struct ffmpeg_output *output = data;

	if (output->active) {
		if (ts > 0) {
			output->stop_ts = ts;
			os_atomic_set_bool(&output->stopping, true);
		}

		ffmpeg_output_full_stop(output);
	}
}

static void ffmpeg_deactivate(struct ffmpeg_output *output)
{
	if (output->write_thread_active) {
		os_event_signal(output->stop_event);
		os_sem_post(output->write_sem);
		pthread_join(output->write_thread, NULL);
		output->write_thread_active = false;
	}

	pthread_mutex_lock(&output->write_mutex);

	for (size_t i = 0; i < output->packets.num; i++)
		av_free_packet(output->packets.array + i);
	da_free(output->packets);

	pthread_mutex_unlock(&output->write_mutex);

	ffmpeg_data_free(&output->ff_data);
}

static uint64_t ffmpeg_output_total_bytes(void *data)
{
	struct ffmpeg_output *output = data;
	return output->total_bytes;
}

struct obs_output_info ffmpeg_output = {
	.id = "ffmpeg_output",
	.flags = OBS_OUTPUT_AUDIO | OBS_OUTPUT_VIDEO | OBS_OUTPUT_MULTI_TRACK |
		 OBS_OUTPUT_CAN_PAUSE,
	.get_name = ffmpeg_output_getname,
	.create = ffmpeg_output_create,
	.destroy = ffmpeg_output_destroy,
	.start = ffmpeg_output_start,
	.stop = ffmpeg_output_stop,
	.raw_video = receive_video,
	.raw_audio2 = receive_audio,
	.get_total_bytes = ffmpeg_output_total_bytes,
};
