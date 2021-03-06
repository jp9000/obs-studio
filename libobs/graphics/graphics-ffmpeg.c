#include "graphics.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include "../obs-ffmpeg-compat.h"

struct ffmpeg_image {
	const char *file;
	AVFormatContext *fmt_ctx;
	AVCodecContext *decoder_ctx;

	int cx, cy;
	enum AVPixelFormat format;
};

static bool ffmpeg_image_open_decoder_context(struct ffmpeg_image *info)
{
	AVFormatContext *const fmt_ctx = info->fmt_ctx;
	int ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, 1, NULL,
				      0);
	if (ret < 0) {
		blog(LOG_WARNING, "Couldn't find video stream in file '%s': %s",
		     info->file, av_err2str(ret));
		return false;
	}

	AVStream *const stream = fmt_ctx->streams[ret];
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	AVCodecParameters *const codecpar = stream->codecpar;
	AVCodec *const decoder = avcodec_find_decoder(codecpar->codec_id);
#else
	AVCodecContext *const decoder_ctx = stream->codec;
	AVCodec *const decoder = avcodec_find_decoder(decoder_ctx->codec_id);
#endif
	if (!decoder) {
		blog(LOG_WARNING, "Failed to find decoder for file '%s'",
		     info->file);
		return false;
	}

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	AVCodecContext *const decoder_ctx = avcodec_alloc_context3(decoder);
	avcodec_parameters_to_context(decoder_ctx, codecpar);
#endif

	info->decoder_ctx = decoder_ctx;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	info->cx = codecpar->width;
	info->cy = codecpar->height;
	info->format = codecpar->format;
#else
	info->cx = decoder_ctx->width;
	info->cy = decoder_ctx->height;
	info->format = decoder_ctx->pix_fmt;
#endif

	ret = avcodec_open2(decoder_ctx, decoder, NULL);
	if (ret < 0) {
		blog(LOG_WARNING,
		     "Failed to open video codec for file '%s': "
		     "%s",
		     info->file, av_err2str(ret));
		return false;
	}

	return true;
}

static void ffmpeg_image_free(struct ffmpeg_image *info)
{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	avcodec_free_context(&info->decoder_ctx);
#else
	avcodec_close(info->decoder_ctx);
#endif
	avformat_close_input(&info->fmt_ctx);
}

static bool ffmpeg_image_init(struct ffmpeg_image *info, const char *file)
{
	int ret;

	if (!file || !*file)
		return false;

	memset(info, 0, sizeof(struct ffmpeg_image));
	info->file = file;

	ret = avformat_open_input(&info->fmt_ctx, file, NULL, NULL);
	if (ret < 0) {
		blog(LOG_WARNING, "Failed to open file '%s': %s", info->file,
		     av_err2str(ret));
		return false;
	}

	ret = avformat_find_stream_info(info->fmt_ctx, NULL);
	if (ret < 0) {
		blog(LOG_WARNING,
		     "Could not find stream info for file '%s':"
		     " %s",
		     info->file, av_err2str(ret));
		goto fail;
	}

	if (!ffmpeg_image_open_decoder_context(info))
		goto fail;

	return true;

fail:
	ffmpeg_image_free(info);
	return false;
}

static bool ffmpeg_image_reformat_frame(struct ffmpeg_image *info,
					AVFrame *frame, uint8_t *out,
					int linesize)
{
	struct SwsContext *sws_ctx = NULL;
	int ret = 0;

	if (info->format == AV_PIX_FMT_RGBA ||
	    info->format == AV_PIX_FMT_BGRA ||
	    info->format == AV_PIX_FMT_BGR0) {

		if (linesize != frame->linesize[0]) {
			int min_line = linesize < frame->linesize[0]
					       ? linesize
					       : frame->linesize[0];

			for (int y = 0; y < info->cy; y++)
				memcpy(out + y * linesize,
				       frame->data[0] + y * frame->linesize[0],
				       min_line);
		} else {
			memcpy(out, frame->data[0], linesize * info->cy);
		}

	} else {
		static const enum AVPixelFormat format = AV_PIX_FMT_BGRA;

		sws_ctx = sws_getContext(info->cx, info->cy, info->format,
					 info->cx, info->cy, format, SWS_POINT,
					 NULL, NULL, NULL);
		if (!sws_ctx) {
			blog(LOG_WARNING,
			     "Failed to create scale context "
			     "for '%s'",
			     info->file);
			return false;
		}

		uint8_t *pointers[4];
		int linesizes[4];
		ret = av_image_alloc(pointers, linesizes, info->cx, info->cy,
				     format, 32);
		if (ret < 0) {
			blog(LOG_WARNING, "av_image_alloc failed for '%s': %s",
			     info->file, av_err2str(ret));
			sws_freeContext(sws_ctx);
			return false;
		}

		ret = sws_scale(sws_ctx, (const uint8_t *const *)frame->data,
				frame->linesize, 0, info->cy, pointers,
				linesizes);
		sws_freeContext(sws_ctx);

		if (ret < 0) {
			blog(LOG_WARNING, "sws_scale failed for '%s': %s",
			     info->file, av_err2str(ret));
			av_freep(pointers);
			return false;
		}

		for (size_t y = 0; y < (size_t)info->cy; y++)
			memcpy(out + y * linesize,
			       pointers[0] + y * linesizes[0], linesize);

		av_freep(pointers);

		info->format = format;
	}

	return true;
}

static bool ffmpeg_image_decode(struct ffmpeg_image *info, uint8_t *out,
				int linesize)
{
	AVPacket packet = {0};
	bool success = false;
	AVFrame *frame = av_frame_alloc();
	int got_frame = 0;
	int ret;

	if (!frame) {
		blog(LOG_WARNING, "Failed to create frame data for '%s'",
		     info->file);
		return false;
	}

	ret = av_read_frame(info->fmt_ctx, &packet);
	if (ret < 0) {
		blog(LOG_WARNING, "Failed to read image frame from '%s': %s",
		     info->file, av_err2str(ret));
		goto fail;
	}

	while (!got_frame) {
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 40, 101)
		ret = avcodec_send_packet(info->decoder_ctx, &packet);
		if (ret == 0)
			ret = avcodec_receive_frame(info->decoder_ctx, frame);

		got_frame = (ret == 0);

		if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
			ret = 0;
#else
		ret = avcodec_decode_video2(info->decoder_ctx, frame,
					    &got_frame, &packet);
#endif
		if (ret < 0) {
			blog(LOG_WARNING, "Failed to decode frame for '%s': %s",
			     info->file, av_err2str(ret));
			goto fail;
		}
	}

	success = ffmpeg_image_reformat_frame(info, frame, out, linesize);

fail:
	av_packet_unref(&packet);
	av_frame_free(&frame);
	return success;
}

void gs_init_image_deps(void)
{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
	av_register_all();
#endif
}

void gs_free_image_deps(void) {}

static inline enum gs_color_format convert_format(enum AVPixelFormat format)
{
	switch ((int)format) {
	case AV_PIX_FMT_RGBA:
		return GS_RGBA;
	case AV_PIX_FMT_BGRA:
		return GS_BGRA;
	case AV_PIX_FMT_BGR0:
		return GS_BGRX;
	}

	return GS_BGRX;
}

uint8_t *gs_create_texture_file_data(const char *file,
				     enum gs_color_format *format,
				     uint32_t *cx_out, uint32_t *cy_out)
{
	struct ffmpeg_image image;
	uint8_t *data = NULL;

	if (ffmpeg_image_init(&image, file)) {
		data = bmalloc(image.cx * image.cy * 4);

		if (ffmpeg_image_decode(&image, data, image.cx * 4)) {
			*format = convert_format(image.format);
			*cx_out = (uint32_t)image.cx;
			*cy_out = (uint32_t)image.cy;
		} else {
			bfree(data);
			data = NULL;
		}

		ffmpeg_image_free(&image);
	}

	return data;
}
