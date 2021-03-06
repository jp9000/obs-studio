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
#include "dstr.h"
#include "darray.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* string reference (string segment within an already existing array) */

struct strref {
	const char *array;
	size_t len;
};

static inline void strref_clear(struct strref *dst)
{
	dst->array = NULL;
	dst->len = 0;
}

static inline void strref_set(struct strref *dst, const char *array, size_t len)
{
	dst->array = array;
	dst->len = len;
}

static inline void strref_copy(struct strref *dst, const struct strref *src)
{
	dst->array = src->array;
	dst->len = src->len;
}

static inline void strref_add(struct strref *dst, const struct strref *t)
{
	if (!dst->array)
		strref_copy(dst, t);
	else
		dst->len += t->len;
}

static inline bool strref_is_empty(const struct strref *str)
{
	return !str || !str->array || !str->len || !*str->array;
}

EXPORT int strref_cmp(const struct strref *str1, const char *str2);
EXPORT int strref_cmpi(const struct strref *str1, const char *str2);
EXPORT int strref_cmp_strref(const struct strref *str1,
			     const struct strref *str2);
EXPORT int strref_cmpi_strref(const struct strref *str1,
			      const struct strref *str2);

/* ------------------------------------------------------------------------- */

EXPORT bool valid_int_str(const char *str, size_t n);
EXPORT bool valid_float_str(const char *str, size_t n);

static inline bool valid_int_strref(const struct strref *str)
{
	return valid_int_str(str->array, str->len);
}

static inline bool valid_float_strref(const struct strref *str)
{
	return valid_float_str(str->array, str->len);
}

static inline bool is_whitespace(char ch)
{
	return ch == ' ' || ch == '\r' || ch == '\t' || ch == '\n';
}

static inline bool is_newline(char ch)
{
	return ch == '\r' || ch == '\n';
}

static inline bool is_space_or_tab(const char ch)
{
	return ch == ' ' || ch == '\t';
}

static inline bool is_newline_pair(char ch1, char ch2)
{
	return (ch1 == '\r' && ch2 == '\n') || (ch1 == '\n' && ch2 == '\r');
}

static inline int newline_size(const char *array)
{
	if (strncmp(array, "\r\n", 2) == 0 || strncmp(array, "\n\r", 2) == 0)
		return 2;
	else if (*array == '\r' || *array == '\n')
		return 1;

	return 0;
}

/* ------------------------------------------------------------------------- */

/* 
 * A "base" token is one of four things:
 *   1.) A sequence of alpha characters
 *   2.) A sequence of numeric characters
 *   3.) A single whitespace character if whitespace is not ignored
 *   4.) A single character that does not fall into the above 3 categories
 */

enum base_token_type {
	BASETOKEN_NONE,
	BASETOKEN_ALPHA,
	BASETOKEN_DIGIT,
	BASETOKEN_WHITESPACE,
	BASETOKEN_OTHER,
};

struct base_token {
	struct strref text;
	enum base_token_type type;
	bool passed_whitespace;
};

static inline void base_token_clear(struct base_token *t)
{
	memset(t, 0, sizeof(struct base_token));
}

static inline void base_token_copy(struct base_token *dst,
				   struct base_token *src)
{
	memcpy(dst, src, sizeof(struct base_token));
}

/* ------------------------------------------------------------------------- */

#define LEX_ERROR 0
#define LEX_WARNING 1

struct error_item {
	char *error;
	const char *file;
	uint32_t row, column;
	int level;
};

static inline void error_item_init(struct error_item *ei)
{
	memset(ei, 0, sizeof(struct error_item));
}

static inline void error_item_free(struct error_item *ei)
{
	bfree(ei->error);
	error_item_init(ei);
}

static inline void error_item_array_free(struct error_item *array, size_t num)
{
	size_t i;
	for (i = 0; i < num; i++)
		error_item_free(array + i);
}

/* ------------------------------------------------------------------------- */

struct error_data {
	DARRAY(struct error_item) errors;
};

static inline void error_data_init(struct error_data *data)
{
	da_init(data->errors);
}

static inline void error_data_free(struct error_data *data)
{
	error_item_array_free(data->errors.array, data->errors.num);
	da_free(data->errors);
}

static inline const struct error_item *error_data_item(struct error_data *ed,
						       size_t idx)
{
	return ed->errors.array + idx;
}

EXPORT char *error_data_buildstring(struct error_data *ed);

EXPORT void error_data_add(struct error_data *ed, const char *file,
			   uint32_t row, uint32_t column, const char *msg,
			   int level);

static inline size_t error_data_type_count(struct error_data *ed, int type)
{
	size_t count = 0, i;
	for (i = 0; i < ed->errors.num; i++) {
		if (ed->errors.array[i].level == type)
			count++;
	}

	return count;
}

static inline bool error_data_has_errors(struct error_data *ed)
{
	size_t i;
	for (i = 0; i < ed->errors.num; i++)
		if (ed->errors.array[i].level == LEX_ERROR)
			return true;

	return false;
}

/* ------------------------------------------------------------------------- */

struct lexer {
	char *text;
	const char *offset;
};

static inline void lexer_init(struct lexer *lex)
{
	memset(lex, 0, sizeof(struct lexer));
}

static inline void lexer_free(struct lexer *lex)
{
	bfree(lex->text);
	lexer_init(lex);
}

static inline void lexer_start(struct lexer *lex, const char *text)
{
	lexer_free(lex);
	lex->text = bstrdup(text);
	lex->offset = lex->text;
}

static inline void lexer_start_move(struct lexer *lex, char *text)
{
	lexer_free(lex);
	lex->text = text;
	lex->offset = lex->text;
}

static inline void lexer_reset(struct lexer *lex)
{
	lex->offset = lex->text;
}

enum ignore_whitespace { PARSE_WHITESPACE, IGNORE_WHITESPACE };

EXPORT bool lexer_getbasetoken(struct lexer *lex, struct base_token *t,
			       enum ignore_whitespace iws);

EXPORT void lexer_getstroffset(const struct lexer *lex, const char *str,
			       uint32_t *row, uint32_t *col);

#ifdef __cplusplus
}
#endif
