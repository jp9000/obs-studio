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

#include <ctype.h>
#include "lexer.h"

static const char *astrblank = "";

int strref_cmp(const struct strref *str1, const char *str2)
{
	size_t i = 0;

	if (strref_is_empty(str1))
		return (!str2 || !*str2) ? 0 : -1;
	if (!str2)
		str2 = astrblank;

	do {
		char ch1, ch2;

		ch1 = (i < str1->len) ? str1->array[i] : 0;
		ch2 = *str2;

		if (ch1 < ch2)
			return -1;
		else if (ch1 > ch2)
			return 1;
	} while (i++ < str1->len && *str2++);

	return 0;
}

int strref_cmpi(const struct strref *str1, const char *str2)
{
	size_t i = 0;

	if (strref_is_empty(str1))
		return (!str2 || !*str2) ? 0 : -1;
	if (!str2)
		str2 = astrblank;

	do {
		char ch1, ch2;

		ch1 = (i < str1->len) ? (char)toupper(str1->array[i]) : 0;
		ch2 = (char)toupper(*str2);

		if (ch1 < ch2)
			return -1;
		else if (ch1 > ch2)
			return 1;
	} while (i++ < str1->len && *str2++);

	return 0;
}

int strref_cmp_strref(const struct strref *str1, const struct strref *str2)
{
	size_t i = 0;

	if (strref_is_empty(str1))
		return strref_is_empty(str2) ? 0 : -1;
	if (strref_is_empty(str2))
		return -1;

	do {
		char ch1, ch2;

		ch1 = (i < str1->len) ? str1->array[i] : 0;
		ch2 = (i < str2->len) ? str2->array[i] : 0;

		if (ch1 < ch2)
			return -1;
		else if (ch1 > ch2)
			return 1;

		i++;
	} while (i <= str1->len && i <= str2->len);

	return 0;
}

int strref_cmpi_strref(const struct strref *str1, const struct strref *str2)
{
	size_t i = 0;

	if (strref_is_empty(str1))
		return strref_is_empty(str2) ? 0 : -1;
	if (strref_is_empty(str2))
		return -1;

	do {
		char ch1, ch2;

		ch1 = (i < str1->len) ? (char)toupper(str1->array[i]) : 0;
		ch2 = (i < str2->len) ? (char)toupper(str2->array[i]) : 0;

		if (ch1 < ch2)
			return -1;
		else if (ch1 > ch2)
			return 1;

		i++;
	} while (i <= str1->len && i <= str2->len);

	return 0;
}
/* ------------------------------------------------------------------------- */

bool valid_int_str(const char *str, size_t n)
{
	bool found_num = false;

	if (!str)
		return false;
	if (!*str)
		return false;

	if (!n)
		n = strlen(str);
	if (*str == '-' || *str == '+')
		++str;

	do {
		if (*str > '9' || *str < '0')
			return false;

		found_num = true;
	} while (*++str && --n);

	return found_num;
}

bool valid_float_str(const char *str, size_t n)
{
	bool found_num = false;
	bool found_exp = false;
	bool found_dec = false;

	if (!str)
		return false;
	if (!*str)
		return false;

	if (!n)
		n = strlen(str);
	if (*str == '-' || *str == '+')
		++str;

	do {
		if (*str == '.') {
			if (found_dec || found_exp || !found_num)
				return false;

			found_dec = true;

		} else if (*str == 'e') {
			if (found_exp || !found_num)
				return false;

			found_exp = true;
			found_num = false;

		} else if (*str == '-' || *str == '+') {
			if (!found_exp || !found_num)
				return false;

		} else if (*str > '9' || *str < '0') {
			return false;
		} else {
			found_num = true;
		}
	} while (*++str && --n);

	return found_num;
}

/* ------------------------------------------------------------------------- */

void error_data_add(struct error_data *data, const char *file, uint32_t row,
		    uint32_t column, const char *msg, int level)
{
	struct error_item item;

	if (!data)
		return;

	item.file = file;
	item.row = row;
	item.column = column;
	item.level = level;
	item.error = bstrdup(msg);

	da_push_back(data->errors, &item);
}

char *error_data_buildstring(struct error_data *ed)
{
	struct dstr str;
	struct error_item *items = ed->errors.array;
	size_t i;

	dstr_init(&str);
	for (i = 0; i < ed->errors.num; i++) {
		struct error_item *item = items + i;
		dstr_catf(&str, "%s (%u, %u): %s\n", item->file, item->row,
			  item->column, item->error);
	}

	return str.array;
}

/* ------------------------------------------------------------------------- */

static inline enum base_token_type get_char_token_type(const char ch)
{
	if (is_whitespace(ch))
		return BASETOKEN_WHITESPACE;
	else if (ch >= '0' && ch <= '9')
		return BASETOKEN_DIGIT;
	else if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))
		return BASETOKEN_ALPHA;

	return BASETOKEN_OTHER;
}

bool lexer_getbasetoken(struct lexer *lex, struct base_token *token,
			enum ignore_whitespace iws)
{
	const char *offset = lex->offset;
	const char *token_start = NULL;
	enum base_token_type type = BASETOKEN_NONE;
	bool ignore_whitespace = (iws == IGNORE_WHITESPACE);

	if (!offset)
		return false;

	while (*offset != 0) {
		char ch = *(offset++);
		enum base_token_type new_type = get_char_token_type(ch);

		if (type == BASETOKEN_NONE) {
			if (new_type == BASETOKEN_WHITESPACE &&
			    ignore_whitespace)
				continue;

			token_start = offset - 1;
			type = new_type;

			if (type != BASETOKEN_DIGIT &&
			    type != BASETOKEN_ALPHA) {
				if (is_newline(ch) &&
				    is_newline_pair(ch, *offset)) {
					offset++;
				}
				break;
			}
		} else if (type != new_type) {
			offset--;
			break;
		}
	}

	lex->offset = offset;

	if (token_start && offset > token_start) {
		strref_set(&token->text, token_start, offset - token_start);
		token->type = type;
		return true;
	}

	return false;
}

void lexer_getstroffset(const struct lexer *lex, const char *str, uint32_t *row,
			uint32_t *col)
{
	uint32_t cur_col = 1, cur_row = 1;
	const char *text = lex->text;

	if (!str)
		return;

	while (text < str) {
		if (is_newline(*text)) {
			text += newline_size(text) - 1;
			cur_col = 1;
			cur_row++;
		} else {
			cur_col++;
		}

		text++;
	}

	*row = cur_row;
	*col = cur_col;
}
