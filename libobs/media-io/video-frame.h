/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>

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

#include "../util/bmem.h"
#include "video-io.h"

struct video_frame {
	uint8_t *data[MAX_AV_PLANES];
	uint32_t linesize[MAX_AV_PLANES];
};

EXPORT void video_frame_init(struct video_frame *frame,
			     enum video_format format, uint32_t width,
			     uint32_t height);

static inline void video_frame_free(struct video_frame *frame)
{
	if (frame) {
		bfree(frame->data[0]);
		memset(frame, 0, sizeof(struct video_frame));
	}
}

static inline struct video_frame *
video_frame_create(enum video_format format, uint32_t width, uint32_t height)
{
	struct video_frame *frame;

	frame = (struct video_frame *)bzalloc(sizeof(struct video_frame));
	video_frame_init(frame, format, width, height);
	return frame;
}

static inline void video_frame_destroy(struct video_frame *frame)
{
	if (frame) {
		bfree(frame->data[0]);
		bfree(frame);
	}
}

EXPORT void video_frame_copy(struct video_frame *dst,
			     const struct video_frame *src,
			     enum video_format format, uint32_t height);
