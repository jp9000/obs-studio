/*
 * Copyright (c) 2014 Hugh Bailey <obs.jim@gmail.com>
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

#include "darray.h"
#include "array-serializer.h"

static size_t array_output_write(void *param, const void *data, size_t size)
{
	struct array_output_data *output = param;
	da_push_back_array(output->bytes, data, size);
	return size;
}

static int64_t array_output_get_pos(void *param)
{
	struct array_output_data *data = param;
	return (int64_t)data->bytes.num;
}

void array_output_serializer_init(struct serializer *s,
				  struct array_output_data *data)
{
	memset(s, 0, sizeof(struct serializer));
	memset(data, 0, sizeof(struct array_output_data));
	s->data = data;
	s->write = array_output_write;
	s->get_pos = array_output_get_pos;
}

void array_output_serializer_free(struct array_output_data *data)
{
	da_free(data->bytes);
}
