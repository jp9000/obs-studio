/*
 * Copyright (c) 2015 Hugh Bailey <obs.jim@gmail.com>
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

#include "../c99defs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct win_version_info {
	int major;
	int minor;
	int build;
	int revis;
};

static inline int win_version_compare(struct win_version_info *dst,
				      struct win_version_info *src)
{
	if (dst->major > src->major)
		return 1;
	if (dst->major == src->major) {
		if (dst->minor > src->minor)
			return 1;
		if (dst->minor == src->minor) {
			if (dst->build > src->build)
				return 1;
			if (dst->build == src->build)
				return 0;
		}
	}
	return -1;
}

EXPORT bool is_64_bit_windows(void);
EXPORT bool get_dll_ver(const wchar_t *lib, struct win_version_info *info);
EXPORT void get_win_ver(struct win_version_info *info);
EXPORT uint32_t get_win_ver_int(void);

#ifdef __cplusplus
}
#endif
