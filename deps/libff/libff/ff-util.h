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

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ff_codec_type { FF_CODEC_AUDIO, FF_CODEC_VIDEO, FF_CODEC_UNKNOWN };

struct ff_format_desc;
struct ff_codec_desc;

void ff_init();

const char *ff_codec_name_from_id(int codec_id);

// Codec Description
const struct ff_codec_desc *
ff_codec_supported(const struct ff_format_desc *format_desc,
                   bool ignore_compatability);
void ff_codec_desc_free(const struct ff_codec_desc *codec_desc);
const char *ff_codec_desc_name(const struct ff_codec_desc *codec_desc);
const char *ff_codec_desc_long_name(const struct ff_codec_desc *codec_desc);
enum ff_codec_type ff_codec_desc_type(const struct ff_codec_desc *codec_desc);
bool ff_codec_desc_is_alias(const struct ff_codec_desc *codec_desc);
const char *ff_codec_desc_base_name(const struct ff_codec_desc *codec_desc);
int ff_codec_desc_id(const struct ff_codec_desc *codec_desc);
const struct ff_codec_desc *
ff_codec_desc_next(const struct ff_codec_desc *codec_desc);

// Format Description
const struct ff_format_desc *ff_format_supported();
void ff_format_desc_free(const struct ff_format_desc *format_desc);
const char *ff_format_desc_name(const struct ff_format_desc *format_desc);
const char *ff_format_desc_long_name(const struct ff_format_desc *format_desc);
const char *ff_format_desc_mime_type(const struct ff_format_desc *format_desc);
const char *ff_format_desc_extensions(const struct ff_format_desc *format_desc);
bool ff_format_desc_has_audio(const struct ff_format_desc *format_desc);
bool ff_format_desc_has_video(const struct ff_format_desc *format_desc);
int ff_format_desc_audio(const struct ff_format_desc *format_desc);
int ff_format_desc_video(const struct ff_format_desc *format_desc);
const char *
ff_format_desc_get_default_name(const struct ff_format_desc *format_desc,
                                enum ff_codec_type codec_type);
const struct ff_format_desc *
ff_format_desc_next(const struct ff_format_desc *format_desc);

#ifdef __cplusplus
}
#endif
