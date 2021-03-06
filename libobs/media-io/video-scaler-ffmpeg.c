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

#include "../util/bmem.h"
#include "video-scaler.h"

#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

struct video_scaler {
	struct SwsContext *swscale;
	int src_height;
	int dst_heights[4];
	uint8_t *dst_pointers[4];
	int dst_linesizes[4];
};

static inline enum AVPixelFormat
get_ffmpeg_video_format(enum video_format format)
{
	switch (format) {
	case VIDEO_FORMAT_I420:
		return AV_PIX_FMT_YUV420P;
	case VIDEO_FORMAT_NV12:
		return AV_PIX_FMT_NV12;
	case VIDEO_FORMAT_YUY2:
		return AV_PIX_FMT_YUYV422;
	case VIDEO_FORMAT_UYVY:
		return AV_PIX_FMT_UYVY422;
	case VIDEO_FORMAT_RGBA:
		return AV_PIX_FMT_RGBA;
	case VIDEO_FORMAT_BGRA:
		return AV_PIX_FMT_BGRA;
	case VIDEO_FORMAT_BGRX:
		return AV_PIX_FMT_BGRA;
	case VIDEO_FORMAT_Y800:
		return AV_PIX_FMT_GRAY8;
	case VIDEO_FORMAT_I444:
		return AV_PIX_FMT_YUV444P;
	case VIDEO_FORMAT_BGR3:
		return AV_PIX_FMT_BGR24;
	case VIDEO_FORMAT_I422:
		return AV_PIX_FMT_YUV422P;
	case VIDEO_FORMAT_I40A:
		return AV_PIX_FMT_YUVA420P;
	case VIDEO_FORMAT_I42A:
		return AV_PIX_FMT_YUVA422P;
	case VIDEO_FORMAT_YUVA:
		return AV_PIX_FMT_YUVA444P;
	case VIDEO_FORMAT_NONE:
	case VIDEO_FORMAT_YVYU:
	case VIDEO_FORMAT_AYUV:
		/* not supported by FFmpeg */
		return AV_PIX_FMT_NONE;
	}

	return AV_PIX_FMT_NONE;
}

static inline int get_ffmpeg_scale_type(enum video_scale_type type)
{
	switch (type) {
	case VIDEO_SCALE_DEFAULT:
		return SWS_FAST_BILINEAR;
	case VIDEO_SCALE_POINT:
		return SWS_POINT;
	case VIDEO_SCALE_FAST_BILINEAR:
		return SWS_FAST_BILINEAR;
	case VIDEO_SCALE_BILINEAR:
		return SWS_BILINEAR | SWS_AREA;
	case VIDEO_SCALE_BICUBIC:
		return SWS_BICUBIC;
	}

	return SWS_POINT;
}

static inline const int *get_ffmpeg_coeffs(enum video_colorspace cs)
{
	const int colorspace = (cs == VIDEO_CS_601) ? SWS_CS_ITU601
						    : SWS_CS_ITU709;
	return sws_getCoefficients(colorspace);
}

static inline int get_ffmpeg_range_type(enum video_range_type type)
{
	switch (type) {
	case VIDEO_RANGE_DEFAULT:
		return 0;
	case VIDEO_RANGE_PARTIAL:
		return 0;
	case VIDEO_RANGE_FULL:
		return 1;
	}

	return 0;
}

#define FIXED_1_0 (1 << 16)

int video_scaler_create(video_scaler_t **scaler_out,
			const struct video_scale_info *dst,
			const struct video_scale_info *src,
			enum video_scale_type type)
{
	enum AVPixelFormat format_src = get_ffmpeg_video_format(src->format);
	enum AVPixelFormat format_dst = get_ffmpeg_video_format(dst->format);
	int scale_type = get_ffmpeg_scale_type(type);
	const int *coeff_src = get_ffmpeg_coeffs(src->colorspace);
	const int *coeff_dst = get_ffmpeg_coeffs(dst->colorspace);
	int range_src = get_ffmpeg_range_type(src->range);
	int range_dst = get_ffmpeg_range_type(dst->range);
	struct video_scaler *scaler;
	int ret;

	if (!scaler_out)
		return VIDEO_SCALER_FAILED;

	if (format_src == AV_PIX_FMT_NONE || format_dst == AV_PIX_FMT_NONE)
		return VIDEO_SCALER_BAD_CONVERSION;

	scaler = bzalloc(sizeof(struct video_scaler));
	scaler->src_height = src->height;

	const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(format_dst);
	bool has_plane[4] = {0};
	for (size_t i = 0; i < 4; i++)
		has_plane[desc->comp[i].plane] = 1;

	scaler->dst_heights[0] = dst->height;
	for (size_t i = 1; i < 4; ++i) {
		if (has_plane[i]) {
			const int s = (i == 1 || i == 2) ? desc->log2_chroma_h
							 : 0;
			scaler->dst_heights[i] = dst->height >> s;
		}
	}

	ret = av_image_alloc(scaler->dst_pointers, scaler->dst_linesizes,
			     dst->width, dst->height, format_dst, 32);
	if (ret < 0) {
		blog(LOG_WARNING,
		     "video_scaler_create: av_image_alloc failed: %d", ret);
		goto fail;
	}

	scaler->swscale = sws_getCachedContext(NULL, src->width, src->height,
					       format_src, dst->width,
					       dst->height, format_dst,
					       scale_type, NULL, NULL, NULL);
	if (!scaler->swscale) {
		blog(LOG_ERROR, "video_scaler_create: Could not create "
				"swscale");
		goto fail;
	}

	ret = sws_setColorspaceDetails(scaler->swscale, coeff_src, range_src,
				       coeff_dst, range_dst, 0, FIXED_1_0,
				       FIXED_1_0);
	if (ret < 0) {
		blog(LOG_DEBUG, "video_scaler_create: "
				"sws_setColorspaceDetails failed, ignoring");
	}

	*scaler_out = scaler;
	return VIDEO_SCALER_SUCCESS;

fail:
	video_scaler_destroy(scaler);
	return VIDEO_SCALER_FAILED;
}

void video_scaler_destroy(video_scaler_t *scaler)
{
	if (scaler) {
		sws_freeContext(scaler->swscale);

		if (scaler->dst_pointers[0])
			av_freep(scaler->dst_pointers);

		bfree(scaler);
	}
}

bool video_scaler_scale(video_scaler_t *scaler, uint8_t *output[],
			const uint32_t out_linesize[],
			const uint8_t *const input[],
			const uint32_t in_linesize[])
{
	if (!scaler)
		return false;

	int ret = sws_scale(scaler->swscale, input, (const int *)in_linesize, 0,
			    scaler->src_height, scaler->dst_pointers,
			    scaler->dst_linesizes);
	if (ret <= 0) {
		blog(LOG_ERROR, "video_scaler_scale: sws_scale failed: %d",
		     ret);
		return false;
	}

	for (size_t plane = 0; plane < 4; ++plane) {
		if (!scaler->dst_pointers[plane])
			continue;

		const size_t scaled_linesize = scaler->dst_linesizes[plane];
		const size_t plane_linesize = out_linesize[plane];
		uint8_t *dst = output[plane];
		const uint8_t *src = scaler->dst_pointers[plane];
		const size_t height = scaler->dst_heights[plane];
		if (scaled_linesize == plane_linesize) {
			memcpy(dst, src, scaled_linesize * height);
		} else {
			size_t linesize = scaled_linesize;
			if (linesize > plane_linesize)
				linesize = plane_linesize;

			for (size_t y = 0; y < height; y++) {
				memcpy(dst, src, linesize);
				dst += plane_linesize;
				src += scaled_linesize;
			}
		}
	}

	return true;
}
