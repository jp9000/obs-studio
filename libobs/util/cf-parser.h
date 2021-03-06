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

#include "cf-lexer.h"

/*
 * C-family parser
 *
 *   Handles preprocessing/lexing/errors when parsing a file, and provides a
 * set of parsing functions to be able to go through all the resulting tokens
 * more easily.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define PARSE_SUCCESS 0
#define PARSE_CONTINUE -1
#define PARSE_BREAK -2
#define PARSE_UNEXPECTED_CONTINUE -3
#define PARSE_UNEXPECTED_BREAK -4
#define PARSE_EOF -5

struct cf_parser {
	struct cf_lexer lex;
	struct cf_preprocessor pp;
	struct error_data error_list;

	struct cf_token *cur_token;
};

static inline void cf_parser_init(struct cf_parser *parser)
{
	cf_lexer_init(&parser->lex);
	cf_preprocessor_init(&parser->pp);
	error_data_init(&parser->error_list);

	parser->cur_token = NULL;
}

static inline void cf_parser_free(struct cf_parser *parser)
{
	cf_lexer_free(&parser->lex);
	cf_preprocessor_free(&parser->pp);
	error_data_free(&parser->error_list);

	parser->cur_token = NULL;
}

static inline bool cf_parser_parse(struct cf_parser *parser, const char *str,
				   const char *file)
{
	if (!cf_lexer_lex(&parser->lex, str, file))
		return false;

	if (!cf_preprocess(&parser->pp, &parser->lex, &parser->error_list))
		return false;

	parser->cur_token = cf_preprocessor_get_tokens(&parser->pp);
	return true;
}

EXPORT void cf_adderror(struct cf_parser *parser, const char *error, int level,
			const char *val1, const char *val2, const char *val3);

static inline void cf_adderror_expecting(struct cf_parser *p,
					 const char *expected)
{
	cf_adderror(p, "Expected '$1'", LEX_ERROR, expected, NULL, NULL);
}

static inline void cf_adderror_unexpected_eof(struct cf_parser *p)
{
	cf_adderror(p, "Unexpected EOF", LEX_ERROR, NULL, NULL, NULL);
}

static inline void cf_adderror_syntax_error(struct cf_parser *p)
{
	cf_adderror(p, "Syntax error", LEX_ERROR, NULL, NULL, NULL);
}

static inline bool cf_next_token(struct cf_parser *p)
{
	if (p->cur_token->type != CFTOKEN_SPACETAB &&
	    p->cur_token->type != CFTOKEN_NEWLINE &&
	    p->cur_token->type != CFTOKEN_NONE)
		p->cur_token++;

	while (p->cur_token->type == CFTOKEN_SPACETAB ||
	       p->cur_token->type == CFTOKEN_NEWLINE)
		p->cur_token++;

	return p->cur_token->type != CFTOKEN_NONE;
}

static inline bool cf_next_valid_token(struct cf_parser *p)
{
	if (!cf_next_token(p)) {
		cf_adderror_unexpected_eof(p);
		return false;
	}

	return true;
}

EXPORT bool cf_pass_pair(struct cf_parser *p, char in, char out);

static inline bool cf_go_to_token(struct cf_parser *p, const char *str1,
				  const char *str2)
{
	while (cf_next_token(p)) {
		if (strref_cmp(&p->cur_token->str, str1) == 0) {
			return true;
		} else if (str2 && strref_cmp(&p->cur_token->str, str2) == 0) {
			return true;
		} else if (*p->cur_token->str.array == '{') {
			if (!cf_pass_pair(p, '{', '}'))
				break;
		}
	}

	return false;
}

static inline bool cf_go_to_valid_token(struct cf_parser *p, const char *str1,
					const char *str2)
{
	if (!cf_go_to_token(p, str1, str2)) {
		cf_adderror_unexpected_eof(p);
		return false;
	}

	return true;
}

static inline bool cf_go_to_token_type(struct cf_parser *p,
				       enum cf_token_type type)
{
	while (p->cur_token->type != CFTOKEN_NONE && p->cur_token->type != type)
		p->cur_token++;

	return p->cur_token->type != CFTOKEN_NONE;
}

static inline int cf_token_should_be(struct cf_parser *p, const char *str,
				     const char *goto1, const char *goto2)
{
	if (strref_cmp(&p->cur_token->str, str) == 0)
		return PARSE_SUCCESS;

	if (goto1) {
		if (!cf_go_to_token(p, goto1, goto2))
			return PARSE_EOF;
	}

	cf_adderror_expecting(p, str);
	return PARSE_CONTINUE;
}

static inline int cf_next_token_should_be(struct cf_parser *p, const char *str,
					  const char *goto1, const char *goto2)
{
	if (!cf_next_token(p)) {
		cf_adderror_unexpected_eof(p);
		return PARSE_EOF;
	} else if (strref_cmp(&p->cur_token->str, str) == 0) {
		return PARSE_SUCCESS;
	}

	if (goto1) {
		if (!cf_go_to_token(p, goto1, goto2))
			return PARSE_EOF;
	}

	cf_adderror_expecting(p, str);
	return PARSE_CONTINUE;
}

static inline bool cf_peek_token(struct cf_parser *p, struct cf_token *peek)
{
	struct cf_token *cur_token = p->cur_token;
	bool success = cf_next_token(p);

	*peek = *p->cur_token;
	p->cur_token = cur_token;

	return success;
}

static inline bool cf_peek_valid_token(struct cf_parser *p,
				       struct cf_token *peek)
{
	bool success = cf_peek_token(p, peek);
	if (!success)
		cf_adderror_unexpected_eof(p);
	return success;
}

static inline bool cf_token_is(struct cf_parser *p, const char *val)
{
	return strref_cmp(&p->cur_token->str, val) == 0;
}

static inline int cf_token_is_type(struct cf_parser *p, enum cf_token_type type,
				   const char *type_expected,
				   const char *goto_token)
{
	if (p->cur_token->type != type) {
		cf_adderror_expecting(p, type_expected);
		if (goto_token) {
			if (!cf_go_to_valid_token(p, goto_token, NULL))
				return PARSE_EOF;
		}
		return PARSE_CONTINUE;
	}

	return PARSE_SUCCESS;
}

static inline void cf_copy_token(struct cf_parser *p, char **dst)
{
	*dst = bstrdup_n(p->cur_token->str.array, p->cur_token->str.len);
}

static inline int cf_get_name(struct cf_parser *p, char **dst, const char *name,
			      const char *goto_token)
{
	int errcode;

	errcode = cf_token_is_type(p, CFTOKEN_NAME, name, goto_token);
	if (errcode != PARSE_SUCCESS)
		return errcode;

	*dst = bstrdup_n(p->cur_token->str.array, p->cur_token->str.len);
	return PARSE_SUCCESS;
}

static inline int cf_next_name(struct cf_parser *p, char **dst,
			       const char *name, const char *goto_token)
{
	if (!cf_next_valid_token(p))
		return PARSE_EOF;

	return cf_get_name(p, dst, name, goto_token);
}

static inline int cf_next_token_copy(struct cf_parser *p, char **dst)
{
	if (!cf_next_valid_token(p))
		return PARSE_EOF;

	cf_copy_token(p, dst);
	return PARSE_SUCCESS;
}

static inline int cf_get_name_ref(struct cf_parser *p, struct strref *dst,
				  const char *name, const char *goto_token)
{
	int errcode;

	errcode = cf_token_is_type(p, CFTOKEN_NAME, name, goto_token);
	if (errcode != PARSE_SUCCESS)
		return errcode;

	strref_copy(dst, &p->cur_token->str);
	return PARSE_SUCCESS;
}

static inline int cf_next_name_ref(struct cf_parser *p, struct strref *dst,
				   const char *name, const char *goto_token)
{
	if (!cf_next_valid_token(p))
		return PARSE_EOF;

	return cf_get_name_ref(p, dst, name, goto_token);
}

#ifdef __cplusplus
}
#endif
