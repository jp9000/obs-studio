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

#pragma once

#include "ff-frame.h"

#include <libavcodec/avcodec.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*ff_callback_frame)(struct ff_frame *frame, void *opaque);
typedef bool (*ff_callback_format)(AVCodecContext *codec_context, void *opaque);
typedef bool (*ff_callback_initialize)(void *opaque);

struct ff_callbacks {
	ff_callback_frame frame;
	ff_callback_format format;
	ff_callback_initialize initialize;
	ff_callback_frame frame_initialize;
	ff_callback_frame frame_free;
	void *opaque;
};

bool ff_callbacks_frame(struct ff_callbacks *callbacks, struct ff_frame *frame);
bool ff_callbacks_format(struct ff_callbacks *callbacks,
                         AVCodecContext *codec_context);
bool ff_callbacks_initialize(struct ff_callbacks *callbacks);
bool ff_callbacks_frame_initialize(struct ff_frame *frame,
                                   struct ff_callbacks *callbacks);
bool ff_callbacks_frame_free(struct ff_frame *frame,
                             struct ff_callbacks *callbacks);

#ifdef __cplusplus
}
#endif
