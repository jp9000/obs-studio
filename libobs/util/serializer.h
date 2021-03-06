/*
 * Copyright (c) 2013 Hugh Bailey <obs.jim@gmail.com>
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

#include "c99defs.h"

/*
 *   General programmable serialization functions.  (A shared interface to
 * various reading/writing to/from different inputs/outputs)
 */

#ifdef __cplusplus
extern "C" {
#endif

enum serialize_seek_type {
	SERIALIZE_SEEK_START,
	SERIALIZE_SEEK_CURRENT,
	SERIALIZE_SEEK_END
};

struct serializer {
	void *data;

	size_t (*read)(void *, void *, size_t);
	size_t (*write)(void *, const void *, size_t);
	int64_t (*seek)(void *, int64_t, enum serialize_seek_type);
	int64_t (*get_pos)(void *);
};

static inline size_t s_read(struct serializer *s, void *data, size_t size)
{
	if (s && s->read && data && size)
		return s->read(s->data, (void *)data, size);
	return 0;
}

static inline size_t s_write(struct serializer *s, const void *data,
			     size_t size)
{
	if (s && s->write && data && size)
		return s->write(s->data, (void *)data, size);
	return 0;
}

static inline size_t serialize(struct serializer *s, void *data, size_t len)
{
	if (s) {
		if (s->write)
			return s->write(s->data, data, len);
		else if (s->read)
			return s->read(s->data, data, len);
	}

	return 0;
}

static inline int64_t serializer_seek(struct serializer *s, int64_t offset,
				      enum serialize_seek_type seek_type)
{
	if (s && s->seek)
		return s->seek(s->data, offset, seek_type);
	return -1;
}

static inline int64_t serializer_get_pos(struct serializer *s)
{
	if (s && s->get_pos)
		return s->get_pos(s->data);
	return -1;
}

/* formatted this to be similar to the AVIO layout that ffmpeg uses */

static inline void s_w8(struct serializer *s, uint8_t u8)
{
	s_write(s, &u8, sizeof(uint8_t));
}

static inline void s_wl16(struct serializer *s, uint16_t u16)
{
	s_w8(s, (uint8_t)u16);
	s_w8(s, u16 >> 8);
}

static inline void s_wl24(struct serializer *s, uint32_t u24)
{
	s_w8(s, (uint8_t)u24);
	s_wl16(s, (uint16_t)(u24 >> 8));
}

static inline void s_wl32(struct serializer *s, uint32_t u32)
{
	s_wl16(s, (uint16_t)u32);
	s_wl16(s, (uint16_t)(u32 >> 16));
}

static inline void s_wl64(struct serializer *s, uint64_t u64)
{
	s_wl32(s, (uint32_t)u64);
	s_wl32(s, (uint32_t)(u64 >> 32));
}

static inline void s_wlf(struct serializer *s, float f)
{
	s_wl32(s, *(uint32_t *)&f);
}

static inline void s_wld(struct serializer *s, double d)
{
	s_wl64(s, *(uint64_t *)&d);
}

static inline void s_wb16(struct serializer *s, uint16_t u16)
{
	s_w8(s, u16 >> 8);
	s_w8(s, (uint8_t)u16);
}

static inline void s_wb24(struct serializer *s, uint32_t u24)
{
	s_wb16(s, (uint16_t)(u24 >> 8));
	s_w8(s, (uint8_t)u24);
}

static inline void s_wb32(struct serializer *s, uint32_t u32)
{
	s_wb16(s, (uint16_t)(u32 >> 16));
	s_wb16(s, (uint16_t)u32);
}

static inline void s_wb64(struct serializer *s, uint64_t u64)
{
	s_wb32(s, (uint32_t)(u64 >> 32));
	s_wb32(s, (uint32_t)u64);
}

static inline void s_wbf(struct serializer *s, float f)
{
	s_wb32(s, *(uint32_t *)&f);
}

static inline void s_wbd(struct serializer *s, double d)
{
	s_wb64(s, *(uint64_t *)&d);
}

#ifdef __cplusplus
}
#endif
