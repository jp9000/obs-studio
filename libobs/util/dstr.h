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

#include <string.h>
#include <stdarg.h>
#include "c99defs.h"
#include "bmem.h"

/*
 * Dynamic string
 *
 *   Helper struct/functions for dynamically sizing string buffers.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct strref;

struct dstr {
	char *array;
	size_t len; /* number of characters, excluding null terminator */
	size_t capacity;
};

#ifndef _MSC_VER
#define PRINTFATTR(f, a) __attribute__((__format__(__printf__, f, a)))
#else
#define PRINTFATTR(f, a)
#endif

EXPORT int astrcmpi(const char *str1, const char *str2);
EXPORT int wstrcmpi(const wchar_t *str1, const wchar_t *str2);
EXPORT int astrcmp_n(const char *str1, const char *str2, size_t n);
EXPORT int wstrcmp_n(const wchar_t *str1, const wchar_t *str2, size_t n);
EXPORT int astrcmpi_n(const char *str1, const char *str2, size_t n);
EXPORT int wstrcmpi_n(const wchar_t *str1, const wchar_t *str2, size_t n);

EXPORT char *astrstri(const char *str, const char *find);
EXPORT wchar_t *wstrstri(const wchar_t *str, const wchar_t *find);

EXPORT char *strdepad(char *str);
EXPORT wchar_t *wcsdepad(wchar_t *str);

EXPORT char **strlist_split(const char *str, char split_ch, bool include_empty);
EXPORT void strlist_free(char **strlist);

static inline void dstr_init(struct dstr *dst);
static inline void dstr_init_move(struct dstr *dst, struct dstr *src);
static inline void dstr_init_move_array(struct dstr *dst, char *str);
static inline void dstr_init_copy(struct dstr *dst, const char *src);
static inline void dstr_init_copy_dstr(struct dstr *dst,
				       const struct dstr *src);
EXPORT void dstr_init_copy_strref(struct dstr *dst, const struct strref *src);

static inline void dstr_free(struct dstr *dst);
static inline void dstr_array_free(struct dstr *array, const size_t count);

static inline void dstr_move(struct dstr *dst, struct dstr *src);
static inline void dstr_move_array(struct dstr *dst, char *str);

EXPORT void dstr_copy(struct dstr *dst, const char *array);
static inline void dstr_copy_dstr(struct dstr *dst, const struct dstr *src);
EXPORT void dstr_copy_strref(struct dstr *dst, const struct strref *src);

EXPORT void dstr_ncopy(struct dstr *dst, const char *array, const size_t len);
EXPORT void dstr_ncopy_dstr(struct dstr *dst, const struct dstr *src,
			    const size_t len);

static inline void dstr_resize(struct dstr *dst, const size_t num);
static inline void dstr_reserve(struct dstr *dst, const size_t num);

static inline bool dstr_is_empty(const struct dstr *str);

static inline void dstr_cat(struct dstr *dst, const char *array);
EXPORT void dstr_cat_dstr(struct dstr *dst, const struct dstr *str);
EXPORT void dstr_cat_strref(struct dstr *dst, const struct strref *str);

static inline void dstr_cat_ch(struct dstr *dst, char ch);

EXPORT void dstr_ncat(struct dstr *dst, const char *array, const size_t len);
EXPORT void dstr_ncat_dstr(struct dstr *dst, const struct dstr *str,
			   const size_t len);

EXPORT void dstr_insert(struct dstr *dst, const size_t idx, const char *array);
EXPORT void dstr_insert_dstr(struct dstr *dst, const size_t idx,
			     const struct dstr *str);
EXPORT void dstr_insert_ch(struct dstr *dst, const size_t idx, const char ch);

EXPORT void dstr_remove(struct dstr *dst, const size_t idx, const size_t count);

PRINTFATTR(2, 3)
EXPORT void dstr_printf(struct dstr *dst, const char *format, ...);
PRINTFATTR(2, 3)
EXPORT void dstr_catf(struct dstr *dst, const char *format, ...);

EXPORT void dstr_vprintf(struct dstr *dst, const char *format, va_list args);
EXPORT void dstr_vcatf(struct dstr *dst, const char *format, va_list args);

EXPORT void dstr_safe_printf(struct dstr *dst, const char *format,
			     const char *val1, const char *val2,
			     const char *val3, const char *val4);

static inline const char *dstr_find_i(const struct dstr *str, const char *find);
static inline const char *dstr_find(const struct dstr *str, const char *find);

EXPORT void dstr_replace(struct dstr *str, const char *find,
			 const char *replace);

static inline int dstr_cmp(const struct dstr *str1, const char *str2);
static inline int dstr_cmpi(const struct dstr *str1, const char *str2);
static inline int dstr_ncmp(const struct dstr *str1, const char *str2,
			    const size_t n);
static inline int dstr_ncmpi(const struct dstr *str1, const char *str2,
			     const size_t n);

EXPORT void dstr_depad(struct dstr *dst);

EXPORT void dstr_left(struct dstr *dst, const struct dstr *str,
		      const size_t pos);
EXPORT void dstr_mid(struct dstr *dst, const struct dstr *str,
		     const size_t start, const size_t count);
EXPORT void dstr_right(struct dstr *dst, const struct dstr *str,
		       const size_t pos);

static inline char dstr_end(const struct dstr *str);

EXPORT void dstr_from_mbs(struct dstr *dst, const char *mbstr);
EXPORT char *dstr_to_mbs(const struct dstr *str);
EXPORT void dstr_from_wcs(struct dstr *dst, const wchar_t *wstr);
EXPORT wchar_t *dstr_to_wcs(const struct dstr *str);

EXPORT void dstr_to_upper(struct dstr *str);
EXPORT void dstr_to_lower(struct dstr *str);

#undef PRINTFATTR

/* ------------------------------------------------------------------------- */

static inline void dstr_init(struct dstr *dst)
{
	dst->array = NULL;
	dst->len = 0;
	dst->capacity = 0;
}

static inline void dstr_init_move_array(struct dstr *dst, char *str)
{
	dst->array = str;
	dst->len = (!str) ? 0 : strlen(str);
	dst->capacity = dst->len + 1;
}

static inline void dstr_init_move(struct dstr *dst, struct dstr *src)
{
	*dst = *src;
	dstr_init(src);
}

static inline void dstr_init_copy(struct dstr *dst, const char *str)
{
	dstr_init(dst);
	dstr_copy(dst, str);
}

static inline void dstr_init_copy_dstr(struct dstr *dst, const struct dstr *src)
{
	dstr_init(dst);
	dstr_copy_dstr(dst, src);
}

static inline void dstr_free(struct dstr *dst)
{
	bfree(dst->array);
	dst->array = NULL;
	dst->len = 0;
	dst->capacity = 0;
}

static inline void dstr_array_free(struct dstr *array, const size_t count)
{
	size_t i;
	for (i = 0; i < count; i++)
		dstr_free(array + i);
}

static inline void dstr_move_array(struct dstr *dst, char *str)
{
	dstr_free(dst);
	dst->array = str;
	dst->len = (!str) ? 0 : strlen(str);
	dst->capacity = dst->len + 1;
}

static inline void dstr_move(struct dstr *dst, struct dstr *src)
{
	dstr_free(dst);
	dstr_init_move(dst, src);
}

static inline void dstr_ensure_capacity(struct dstr *dst, const size_t new_size)
{
	size_t new_cap;
	if (new_size <= dst->capacity)
		return;

	new_cap = (!dst->capacity) ? new_size : dst->capacity * 2;
	if (new_size > new_cap)
		new_cap = new_size;
	dst->array = (char *)brealloc(dst->array, new_cap);
	dst->capacity = new_cap;
}

static inline void dstr_copy_dstr(struct dstr *dst, const struct dstr *src)
{
	if (dst->array)
		dstr_free(dst);

	if (src->len) {
		dstr_ensure_capacity(dst, src->len + 1);
		memcpy(dst->array, src->array, src->len + 1);
		dst->len = src->len;
	}
}

static inline void dstr_reserve(struct dstr *dst, const size_t capacity)
{
	if (capacity == 0 || capacity <= dst->len)
		return;

	dst->array = (char *)brealloc(dst->array, capacity);
	dst->capacity = capacity;
}

static inline void dstr_resize(struct dstr *dst, const size_t num)
{
	if (!num) {
		dstr_free(dst);
		return;
	}

	dstr_ensure_capacity(dst, num + 1);
	dst->array[num] = 0;
	dst->len = num;
}

static inline bool dstr_is_empty(const struct dstr *str)
{
	if (!str->array || !str->len)
		return true;
	if (!*str->array)
		return true;

	return false;
}

static inline void dstr_cat(struct dstr *dst, const char *array)
{
	size_t len;
	if (!array || !*array)
		return;

	len = strlen(array);
	dstr_ncat(dst, array, len);
}

static inline void dstr_cat_ch(struct dstr *dst, char ch)
{
	dstr_ensure_capacity(dst, ++dst->len + 1);
	dst->array[dst->len - 1] = ch;
	dst->array[dst->len] = 0;
}

static inline const char *dstr_find_i(const struct dstr *str, const char *find)
{
	return astrstri(str->array, find);
}

static inline const char *dstr_find(const struct dstr *str, const char *find)
{
	return strstr(str->array, find);
}

static inline int dstr_cmp(const struct dstr *str1, const char *str2)
{
	return strcmp(str1->array, str2);
}

static inline int dstr_cmpi(const struct dstr *str1, const char *str2)
{
	return astrcmpi(str1->array, str2);
}

static inline int dstr_ncmp(const struct dstr *str1, const char *str2,
			    const size_t n)
{
	return astrcmp_n(str1->array, str2, n);
}

static inline int dstr_ncmpi(const struct dstr *str1, const char *str2,
			     const size_t n)
{
	return astrcmpi_n(str1->array, str2, n);
}

static inline char dstr_end(const struct dstr *str)
{
	if (dstr_is_empty(str))
		return 0;

	return str->array[str->len - 1];
}

#ifdef __cplusplus
}
#endif
