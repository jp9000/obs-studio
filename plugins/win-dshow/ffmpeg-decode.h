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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <obs.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4204)
#endif

#include <libavcodec/avcodec.h>
#include <libavutil/log.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

struct ffmpeg_decode {
	AVCodecContext *decoder;
	AVCodec *codec;

	AVFrame *hw_frame;
	AVFrame *frame;
	bool hw;

	uint8_t *packet_buffer;
	size_t packet_size;
};

extern int ffmpeg_decode_init(struct ffmpeg_decode *decode, enum AVCodecID id,
			      bool use_hw);
extern void ffmpeg_decode_free(struct ffmpeg_decode *decode);

extern bool ffmpeg_decode_audio(struct ffmpeg_decode *decode, uint8_t *data,
				size_t size, struct obs_source_audio *audio,
				bool *got_output);

extern bool ffmpeg_decode_video(struct ffmpeg_decode *decode, uint8_t *data,
				size_t size, long long *ts,
				enum video_range_type range,
				struct obs_source_frame2 *frame,
				bool *got_output);

static inline bool ffmpeg_decode_valid(struct ffmpeg_decode *decode)
{
	return decode->decoder != NULL;
}

#ifdef __cplusplus
}
#endif
