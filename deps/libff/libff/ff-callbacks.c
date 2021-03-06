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

#include "ff-callbacks.h"

bool ff_callbacks_frame(struct ff_callbacks *callbacks, struct ff_frame *frame)
{
	if (callbacks->frame == NULL)
		return true;

	return callbacks->frame(frame, callbacks->opaque);
}

bool ff_callbacks_format(struct ff_callbacks *callbacks,
                         AVCodecContext *codec_context)
{
	if (callbacks->format == NULL)
		return true;

	return callbacks->format(codec_context, callbacks->opaque);
}

bool ff_callbacks_initialize(struct ff_callbacks *callbacks)
{
	if (callbacks->initialize == NULL)
		return true;

	return callbacks->initialize(callbacks->opaque);
}

bool ff_callbacks_frame_initialize(struct ff_frame *frame,
                                   struct ff_callbacks *callbacks)
{
	if (callbacks->frame_initialize == NULL)
		return true;

	return callbacks->frame_initialize(frame, callbacks->opaque);
}

bool ff_callbacks_frame_free(struct ff_frame *frame,
                             struct ff_callbacks *callbacks)
{
	if (callbacks->frame_free == NULL)
		return true;

	return callbacks->frame_free(frame, callbacks->opaque);
}
