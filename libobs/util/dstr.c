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

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <wchar.h>
#include <wctype.h>
#include <limits.h>

#include "c99defs.h"
#include "dstr.h"
#include "darray.h"
#include "bmem.h"
#include "utf8.h"
#include "lexer.h"
#include "platform.h"

static const char *astrblank = "";
static const wchar_t *wstrblank = L"";

int astrcmpi(const char *str1, const char *str2)
{
	if (!str1)
		str1 = astrblank;
	if (!str2)
		str2 = astrblank;

	do {
		char ch1 = (char)toupper(*str1);
		char ch2 = (char)toupper(*str2);

		if (ch1 < ch2)
			return -1;
		else if (ch1 > ch2)
			return 1;
	} while (*str1++ && *str2++);

	return 0;
}

int wstrcmpi(const wchar_t *str1, const wchar_t *str2)
{
	if (!str1)
		str1 = wstrblank;
	if (!str2)
		str2 = wstrblank;

	do {
		wchar_t ch1 = (wchar_t)towupper(*str1);
		wchar_t ch2 = (wchar_t)towupper(*str2);

		if (ch1 < ch2)
			return -1;
		else if (ch1 > ch2)
			return 1;
	} while (*str1++ && *str2++);

	return 0;
}

int astrcmp_n(const char *str1, const char *str2, size_t n)
{
	if (!n)
		return 0;
	if (!str1)
		str1 = astrblank;
	if (!str2)
		str2 = astrblank;

	do {
		char ch1 = *str1;
		char ch2 = *str2;

		if (ch1 < ch2)
			return -1;
		else if (ch1 > ch2)
			return 1;
	} while (*str1++ && *str2++ && --n);

	return 0;
}

int wstrcmp_n(const wchar_t *str1, const wchar_t *str2, size_t n)
{
	if (!n)
		return 0;
	if (!str1)
		str1 = wstrblank;
	if (!str2)
		str2 = wstrblank;

	do {
		wchar_t ch1 = *str1;
		wchar_t ch2 = *str2;

		if (ch1 < ch2)
			return -1;
		else if (ch1 > ch2)
			return 1;
	} while (*str1++ && *str2++ && --n);

	return 0;
}

int astrcmpi_n(const char *str1, const char *str2, size_t n)
{
	if (!n)
		return 0;
	if (!str1)
		str1 = astrblank;
	if (!str2)
		str2 = astrblank;

	do {
		char ch1 = (char)toupper(*str1);
		char ch2 = (char)toupper(*str2);

		if (ch1 < ch2)
			return -1;
		else if (ch1 > ch2)
			return 1;
	} while (*str1++ && *str2++ && --n);

	return 0;
}

int wstrcmpi_n(const wchar_t *str1, const wchar_t *str2, size_t n)
{
	if (!n)
		return 0;
	if (!str1)
		str1 = wstrblank;
	if (!str2)
		str2 = wstrblank;

	do {
		wchar_t ch1 = (wchar_t)towupper(*str1);
		wchar_t ch2 = (wchar_t)towupper(*str2);

		if (ch1 < ch2)
			return -1;
		else if (ch1 > ch2)
			return 1;
	} while (*str1++ && *str2++ && --n);

	return 0;
}

char *astrstri(const char *str, const char *find)
{
	size_t len;

	if (!str || !find)
		return NULL;

	len = strlen(find);

	do {
		if (astrcmpi_n(str, find, len) == 0)
			return (char *)str;
	} while (*str++);

	return NULL;
}

wchar_t *wstrstri(const wchar_t *str, const wchar_t *find)
{
	size_t len;

	if (!str || !find)
		return NULL;

	len = wcslen(find);

	do {
		if (wstrcmpi_n(str, find, len) == 0)
			return (wchar_t *)str;
	} while (*str++);

	return NULL;
}

static inline bool is_padding(int ch)
{
	return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

char *strdepad(char *str)
{
	char *temp;
	size_t len;

	if (!str)
		return str;
	if (!*str)
		return str;

	temp = str;

	/* remove preceding spaces/tabs */
	while (is_padding(*temp))
		++temp;

	len = strlen(temp);
	if (temp != str)
		memmove(str, temp, len + 1);

	if (len) {
		temp = str + (len - 1);
		while (is_padding(*temp))
			*(temp--) = 0;
	}

	return str;
}

wchar_t *wcsdepad(wchar_t *str)
{
	wchar_t *temp;
	size_t len;

	if (!str)
		return str;
	if (!*str)
		return str;

	temp = str;

	/* remove preceding spaces/tabs */
	while (is_padding(*temp))
		++temp;

	len = wcslen(temp);
	if (temp != str)
		memmove(str, temp, (len + 1) * sizeof(wchar_t));

	if (len) {
		temp = str + (len - 1);
		while (is_padding(*temp))
			*(temp--) = 0;
	}

	return str;
}

char **strlist_split(const char *str, char split_ch, bool include_empty)
{
	const char *cur_str = str;
	const char *next_str;
	char *out = NULL;
	size_t count = 0;
	size_t total_size = 0;

	if (str) {
		char **table;
		char *offset;
		size_t cur_idx = 0;
		size_t cur_pos = 0;

		next_str = strchr(str, split_ch);

		while (next_str) {
			size_t size = next_str - cur_str;

			if (size || include_empty) {
				++count;
				total_size += size + 1;
			}

			cur_str = next_str + 1;
			next_str = strchr(cur_str, split_ch);
		}

		if (*cur_str || include_empty) {
			++count;
			total_size += strlen(cur_str) + 1;
		}

		/* ------------------ */

		cur_pos = (count + 1) * sizeof(char *);
		total_size += cur_pos;
		out = bmalloc(total_size);
		offset = out + cur_pos;
		table = (char **)out;

		/* ------------------ */

		next_str = strchr(str, split_ch);
		cur_str = str;

		while (next_str) {
			size_t size = next_str - cur_str;

			if (size || include_empty) {
				table[cur_idx++] = offset;
				strncpy(offset, cur_str, size);
				offset[size] = 0;
				offset += size + 1;
			}

			cur_str = next_str + 1;
			next_str = strchr(cur_str, split_ch);
		}

		if (*cur_str || include_empty) {
			table[cur_idx++] = offset;
			strcpy(offset, cur_str);
		}

		table[cur_idx] = NULL;
	}

	return (char **)out;
}

void strlist_free(char **strlist)
{
	bfree(strlist);
}

void dstr_init_copy_strref(struct dstr *dst, const struct strref *src)
{
	dstr_init(dst);
	dstr_copy_strref(dst, src);
}

void dstr_copy(struct dstr *dst, const char *array)
{
	size_t len;

	if (!array || !*array) {
		dstr_free(dst);
		return;
	}

	len = strlen(array);
	dstr_ensure_capacity(dst, len + 1);
	memcpy(dst->array, array, len + 1);
	dst->len = len;
}

void dstr_copy_strref(struct dstr *dst, const struct strref *src)
{
	if (dst->array)
		dstr_free(dst);

	dstr_ncopy(dst, src->array, src->len);
}

static inline size_t size_min(size_t a, size_t b)
{
	return (a < b) ? a : b;
}

void dstr_ncopy(struct dstr *dst, const char *array, const size_t len)
{
	if (dst->array)
		dstr_free(dst);

	if (!len)
		return;

	dst->array = bmemdup(array, len + 1);
	dst->len = len;
	dst->capacity = len + 1;

	dst->array[len] = 0;
}

void dstr_ncopy_dstr(struct dstr *dst, const struct dstr *str, const size_t len)
{
	size_t newlen;

	if (dst->array)
		dstr_free(dst);

	if (!len)
		return;

	newlen = size_min(len, str->len);
	dst->array = bmemdup(str->array, newlen + 1);
	dst->len = newlen;
	dst->capacity = newlen + 1;

	dst->array[newlen] = 0;
}

void dstr_cat_dstr(struct dstr *dst, const struct dstr *str)
{
	size_t new_len;
	if (!str->len)
		return;

	new_len = dst->len + str->len;

	dstr_ensure_capacity(dst, new_len + 1);
	memcpy(dst->array + dst->len, str->array, str->len + 1);
	dst->len = new_len;
}

void dstr_cat_strref(struct dstr *dst, const struct strref *str)
{
	dstr_ncat(dst, str->array, str->len);
}

void dstr_ncat(struct dstr *dst, const char *array, const size_t len)
{
	size_t new_len;
	if (!array || !*array || !len)
		return;

	new_len = dst->len + len;

	dstr_ensure_capacity(dst, new_len + 1);
	memcpy(dst->array + dst->len, array, len);

	dst->len = new_len;
	dst->array[new_len] = 0;
}

void dstr_ncat_dstr(struct dstr *dst, const struct dstr *str, const size_t len)
{
	size_t new_len, in_len;
	if (!str->array || !*str->array || !len)
		return;

	in_len = size_min(len, str->len);
	new_len = dst->len + in_len;

	dstr_ensure_capacity(dst, new_len + 1);
	memcpy(dst->array + dst->len, str->array, in_len);

	dst->len = new_len;
	dst->array[new_len] = 0;
}

void dstr_insert(struct dstr *dst, const size_t idx, const char *array)
{
	size_t new_len, len;
	if (!array || !*array)
		return;
	if (idx == dst->len) {
		dstr_cat(dst, array);
		return;
	}

	len = strlen(array);
	new_len = dst->len + len;

	dstr_ensure_capacity(dst, new_len + 1);

	memmove(dst->array + idx + len, dst->array + idx, dst->len - idx + 1);
	memcpy(dst->array + idx, array, len);

	dst->len = new_len;
}

void dstr_insert_dstr(struct dstr *dst, const size_t idx,
		      const struct dstr *str)
{
	size_t new_len;
	if (!str->len)
		return;
	if (idx == dst->len) {
		dstr_cat_dstr(dst, str);
		return;
	}

	new_len = dst->len + str->len;

	dstr_ensure_capacity(dst, (new_len + 1));

	memmove(dst->array + idx + str->len, dst->array + idx,
		dst->len - idx + 1);
	memcpy(dst->array + idx, str->array, str->len);

	dst->len = new_len;
}

void dstr_insert_ch(struct dstr *dst, const size_t idx, const char ch)
{
	if (idx == dst->len) {
		dstr_cat_ch(dst, ch);
		return;
	}

	dstr_ensure_capacity(dst, (++dst->len + 1));
	memmove(dst->array + idx + 1, dst->array + idx, dst->len - idx + 1);
	dst->array[idx] = ch;
}

void dstr_remove(struct dstr *dst, const size_t idx, const size_t count)
{
	size_t end;
	if (!count)
		return;
	if (count == dst->len) {
		dstr_free(dst);
		return;
	}

	end = idx + count;
	if (end == dst->len)
		dst->array[idx] = 0;
	else
		memmove(dst->array + idx, dst->array + end, dst->len - end + 1);

	dst->len -= count;
}

void dstr_printf(struct dstr *dst, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	dstr_vprintf(dst, format, args);
	va_end(args);
}

void dstr_catf(struct dstr *dst, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	dstr_vcatf(dst, format, args);
	va_end(args);
}

void dstr_vprintf(struct dstr *dst, const char *format, va_list args)
{
	va_list args_cp;
	va_copy(args_cp, args);

	int len = vsnprintf(NULL, 0, format, args_cp);
	va_end(args_cp);

	if (len < 0)
		len = 4095;

	dstr_ensure_capacity(dst, ((size_t)len) + 1);
	len = vsnprintf(dst->array, ((size_t)len) + 1, format, args);

	if (!*dst->array) {
		dstr_free(dst);
		return;
	}

	dst->len = len < 0 ? strlen(dst->array) : (size_t)len;
}

void dstr_vcatf(struct dstr *dst, const char *format, va_list args)
{
	va_list args_cp;
	va_copy(args_cp, args);

	int len = vsnprintf(NULL, 0, format, args_cp);
	va_end(args_cp);

	if (len < 0)
		len = 4095;

	dstr_ensure_capacity(dst, dst->len + ((size_t)len) + 1);
	len = vsnprintf(dst->array + dst->len, ((size_t)len) + 1, format, args);

	if (!*dst->array) {
		dstr_free(dst);
		return;
	}

	dst->len += len < 0 ? strlen(dst->array + dst->len) : (size_t)len;
}

void dstr_safe_printf(struct dstr *dst, const char *format, const char *val1,
		      const char *val2, const char *val3, const char *val4)
{
	dstr_copy(dst, format);
	if (val1)
		dstr_replace(dst, "$1", val1);
	if (val2)
		dstr_replace(dst, "$2", val2);
	if (val3)
		dstr_replace(dst, "$3", val3);
	if (val4)
		dstr_replace(dst, "$4", val4);
}

void dstr_replace(struct dstr *str, const char *find, const char *replace)
{
	size_t find_len, replace_len;
	char *temp;

	if (dstr_is_empty(str))
		return;

	if (!replace)
		replace = "";

	find_len = strlen(find);
	replace_len = strlen(replace);
	temp = str->array;

	if (replace_len < find_len) {
		unsigned long count = 0;

		while ((temp = strstr(temp, find)) != NULL) {
			char *end = temp + find_len;
			size_t end_len = strlen(end);

			if (end_len) {
				memmove(temp + replace_len, end, end_len + 1);
				if (replace_len)
					memcpy(temp, replace, replace_len);
			} else {
				strcpy(temp, replace);
			}

			temp += replace_len;
			++count;
		}

		if (count)
			str->len += (replace_len - find_len) * count;

	} else if (replace_len > find_len) {
		unsigned long count = 0;

		while ((temp = strstr(temp, find)) != NULL) {
			temp += find_len;
			++count;
		}

		if (!count)
			return;

		str->len += (replace_len - find_len) * count;
		dstr_ensure_capacity(str, str->len + 1);
		temp = str->array;

		while ((temp = strstr(temp, find)) != NULL) {
			char *end = temp + find_len;
			size_t end_len = strlen(end);

			if (end_len) {
				memmove(temp + replace_len, end, end_len + 1);
				memcpy(temp, replace, replace_len);
			} else {
				strcpy(temp, replace);
			}

			temp += replace_len;
		}

	} else {
		while ((temp = strstr(temp, find)) != NULL) {
			memcpy(temp, replace, replace_len);
			temp += replace_len;
		}
	}
}

void dstr_depad(struct dstr *str)
{
	if (str->array) {
		str->array = strdepad(str->array);
		if (*str->array)
			str->len = strlen(str->array);
		else
			dstr_free(str);
	}
}

void dstr_left(struct dstr *dst, const struct dstr *str, const size_t pos)
{
	dstr_resize(dst, pos);
	if (dst != str)
		memcpy(dst->array, str->array, pos);
}

void dstr_mid(struct dstr *dst, const struct dstr *str, const size_t start,
	      const size_t count)
{
	struct dstr temp;
	dstr_init(&temp);
	dstr_copy_dstr(&temp, str);
	dstr_ncopy(dst, temp.array + start, count);
	dstr_free(&temp);
}

void dstr_right(struct dstr *dst, const struct dstr *str, const size_t pos)
{
	struct dstr temp;
	dstr_init(&temp);
	dstr_ncopy(&temp, str->array + pos, str->len - pos);
	dstr_copy_dstr(dst, &temp);
	dstr_free(&temp);
}

void dstr_from_mbs(struct dstr *dst, const char *mbstr)
{
	dstr_free(dst);
	dst->len = os_mbs_to_utf8_ptr(mbstr, 0, &dst->array);
}

char *dstr_to_mbs(const struct dstr *str)
{
	char *dst;
	os_mbs_to_utf8_ptr(str->array, str->len, &dst);
	return dst;
}

wchar_t *dstr_to_wcs(const struct dstr *str)
{
	wchar_t *dst;
	os_utf8_to_wcs_ptr(str->array, str->len, &dst);
	return dst;
}

void dstr_from_wcs(struct dstr *dst, const wchar_t *wstr)
{
	size_t len = wchar_to_utf8(wstr, 0, NULL, 0, 0);

	if (len) {
		dstr_resize(dst, len);
		wchar_to_utf8(wstr, 0, dst->array, len + 1, 0);
	} else {
		dstr_free(dst);
	}
}

void dstr_to_upper(struct dstr *str)
{
	wchar_t *wstr;
	wchar_t *temp;

	if (dstr_is_empty(str))
		return;

	wstr = dstr_to_wcs(str);
	temp = wstr;

	if (!wstr)
		return;

	while (*temp) {
		*temp = (wchar_t)towupper(*temp);
		temp++;
	}

	dstr_from_wcs(str, wstr);
	bfree(wstr);
}

void dstr_to_lower(struct dstr *str)
{
	wchar_t *wstr;
	wchar_t *temp;

	if (dstr_is_empty(str))
		return;

	wstr = dstr_to_wcs(str);
	temp = wstr;

	if (!wstr)
		return;

	while (*temp) {
		*temp = (wchar_t)towlower(*temp);
		temp++;
	}

	dstr_from_wcs(str, wstr);
	bfree(wstr);
}
