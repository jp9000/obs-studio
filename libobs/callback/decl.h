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

#pragma once

#include "calldata.h"
#include "../util/darray.h"

#ifdef __cplusplus
extern "C" {
#endif

struct decl_param {
	char *name;
	enum call_param_type type;
	uint32_t flags;
};

static inline void decl_param_free(struct decl_param *param)
{
	if (param->name)
		bfree(param->name);
	memset(param, 0, sizeof(struct decl_param));
}

struct decl_info {
	char *name;
	const char *decl_string;
	DARRAY(struct decl_param) params;
};

static inline void decl_info_free(struct decl_info *decl)
{
	if (decl) {
		for (size_t i = 0; i < decl->params.num; i++)
			decl_param_free(decl->params.array + i);
		da_free(decl->params);

		bfree(decl->name);
		memset(decl, 0, sizeof(struct decl_info));
	}
}

EXPORT bool parse_decl_string(struct decl_info *decl, const char *decl_string);

#ifdef __cplusplus
}
#endif
