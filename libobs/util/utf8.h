/*
 * Copyright (c) 2007 Alexey Vatchenko <av@bsdua.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

/*
 * utf8: implementation of UTF-8 charset encoding (RFC3629).
 */

#ifdef __cplusplus
extern "C" {
#endif

#define UTF8_IGNORE_ERROR 0x01
#define UTF8_SKIP_BOM 0x02

size_t utf8_to_wchar(const char *in, size_t insize, wchar_t *out,
		     size_t outsize, int flags);
size_t wchar_to_utf8(const wchar_t *in, size_t insize, char *out,
		     size_t outsize, int flags);

#ifdef __cplusplus
}
#endif
