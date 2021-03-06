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
#include <stdio.h>
#include "platform.h"
#include "cf-lexer.h"

static inline void cf_convert_from_escape_literal(char **p_dst,
						  const char **p_src)
{
	char *dst = *p_dst;
	const char *src = *p_src;

	switch (*(src++)) {
	case '\'':
		*(dst++) = '\'';
		break;
	case '\"':
		*(dst++) = '\"';
		break;
	case '\?':
		*(dst++) = '\?';
		break;
	case '\\':
		*(dst++) = '\\';
		break;
	case '0':
		*(dst++) = '\0';
		break;
	case 'a':
		*(dst++) = '\a';
		break;
	case 'b':
		*(dst++) = '\b';
		break;
	case 'f':
		*(dst++) = '\f';
		break;
	case 'n':
		*(dst++) = '\n';
		break;
	case 'r':
		*(dst++) = '\r';
		break;
	case 't':
		*(dst++) = '\t';
		break;
	case 'v':
		*(dst++) = '\v';
		break;

	/* hex */
	case 'X':
	case 'x':
		*(dst++) = (char)strtoul(src, NULL, 16);
		src += 2;
		break;

	/* oct */
	default:
		if (isdigit(*src)) {
			*(dst++) = (char)strtoul(src, NULL, 8);
			src += 3;
		}

		/* case 'u':
		case 'U': */
	}

	*p_dst = dst;
	*p_src = src;
}

char *cf_literal_to_str(const char *literal, size_t count)
{
	const char *temp_src;
	char *str, *temp_dst;

	if (!count)
		count = strlen(literal);

	if (count < 2)
		return NULL;
	if (literal[0] != literal[count - 1])
		return NULL;
	if (literal[0] != '\"' && literal[0] != '\'')
		return NULL;

	/* strip leading and trailing quote characters */
	str = bzalloc(--count);
	temp_src = literal + 1;
	temp_dst = str;

	while (*temp_src && --count > 0) {
		if (*temp_src == '\\') {
			temp_src++;
			cf_convert_from_escape_literal(&temp_dst, &temp_src);
		} else {
			*(temp_dst++) = *(temp_src++);
		}
	}

	*temp_dst = 0;
	return str;
}

static bool cf_is_token_break(struct base_token *start_token,
			      const struct base_token *token)
{
	switch (start_token->type) {
	case BASETOKEN_ALPHA:
		if (token->type == BASETOKEN_OTHER ||
		    token->type == BASETOKEN_WHITESPACE)
			return true;
		break;

	case BASETOKEN_DIGIT:
		if (token->type == BASETOKEN_WHITESPACE ||
		    (token->type == BASETOKEN_OTHER &&
		     *token->text.array != '.'))
			return true;
		break;

	case BASETOKEN_WHITESPACE:
		/* lump all non-newline whitespace together when possible */
		if (is_space_or_tab(*start_token->text.array) &&
		    is_space_or_tab(*token->text.array))
			break;
		return true;

	case BASETOKEN_OTHER:
		if (*start_token->text.array == '.' &&
		    token->type == BASETOKEN_DIGIT) {
			start_token->type = BASETOKEN_DIGIT;
			break;
		}
		/* Falls through. */

	case BASETOKEN_NONE:
		return true;
	}

	return false;
}

static inline bool cf_is_splice(const char *array)
{
	return (*array == '\\' && is_newline(array[1]));
}

static inline void cf_pass_any_splices(const char **parray)
{
	while (cf_is_splice(*parray))
		*parray += 1 + newline_size((*parray) + 1);
}

static inline bool cf_is_comment(const char *array)
{
	const char *offset = array;

	if (*offset++ == '/') {
		cf_pass_any_splices(&offset);
		return (*offset == '*' || *offset == '/');
	}

	return false;
}

static bool cf_lexer_process_comment(struct cf_lexer *lex,
				     struct cf_token *out_token)
{
	const char *offset;

	if (!cf_is_comment(out_token->unmerged_str.array))
		return false;

	offset = lex->base_lexer.offset;
	cf_pass_any_splices(&offset);

	strcpy(lex->write_offset++, " ");
	out_token->str.len = 1;

	if (*offset == '/') {
		while (*++offset && !is_newline(*offset))
			cf_pass_any_splices(&offset);

	} else if (*offset == '*') {
		bool was_star = false;
		lex->unexpected_eof = true;

		while (*++offset) {
			cf_pass_any_splices(&offset);

			if (was_star && *offset == '/') {
				offset++;
				lex->unexpected_eof = false;
				break;
			} else {
				was_star = (*offset == '*');
			}
		}
	}

	out_token->unmerged_str.len +=
		(size_t)(offset - out_token->unmerged_str.array);
	out_token->type = CFTOKEN_SPACETAB;
	lex->base_lexer.offset = offset;

	return true;
}

static inline void cf_lexer_write_strref(struct cf_lexer *lex,
					 const struct strref *ref)
{
	strncpy(lex->write_offset, ref->array, ref->len);
	lex->write_offset[ref->len] = 0;
	lex->write_offset += ref->len;
}

static bool cf_lexer_is_include(struct cf_lexer *lex)
{
	bool found_include_import = false;
	bool found_preprocessor = false;
	size_t i;

	for (i = lex->tokens.num; i > 0; i--) {
		struct cf_token *token = lex->tokens.array + (i - 1);

		if (is_space_or_tab(*token->str.array))
			continue;

		if (!found_include_import) {
			if (strref_cmp(&token->str, "include") != 0 &&
			    strref_cmp(&token->str, "import") != 0)
				break;

			found_include_import = true;

		} else if (!found_preprocessor) {
			if (*token->str.array != '#')
				break;

			found_preprocessor = true;

		} else {
			return is_newline(*token->str.array);
		}
	}

	/* if starting line */
	return found_preprocessor && found_include_import;
}

static void cf_lexer_getstrtoken(struct cf_lexer *lex,
				 struct cf_token *out_token, char delimiter,
				 bool allow_escaped_delimiters)
{
	const char *offset = lex->base_lexer.offset;
	bool escaped = false;

	out_token->unmerged_str.len++;
	out_token->str.len++;
	cf_lexer_write_strref(lex, &out_token->unmerged_str);

	while (*offset) {
		cf_pass_any_splices(&offset);
		if (*offset == delimiter) {
			if (!escaped) {
				*lex->write_offset++ = *offset;
				out_token->str.len++;
				offset++;
				break;
			}
		} else if (is_newline(*offset)) {
			break;
		}

		*lex->write_offset++ = *offset;
		out_token->str.len++;

		escaped = (allow_escaped_delimiters && *offset == '\\');
		offset++;
	}

	*lex->write_offset = 0;
	out_token->unmerged_str.len +=
		(size_t)(offset - out_token->unmerged_str.array);
	out_token->type = CFTOKEN_STRING;
	lex->base_lexer.offset = offset;
}

static bool cf_lexer_process_string(struct cf_lexer *lex,
				    struct cf_token *out_token)
{
	char ch = *out_token->unmerged_str.array;

	if (ch == '<' && cf_lexer_is_include(lex)) {
		cf_lexer_getstrtoken(lex, out_token, '>', false);
		return true;

	} else if (ch == '"' || ch == '\'') {
		cf_lexer_getstrtoken(lex, out_token, ch,
				     !cf_lexer_is_include(lex));
		return true;
	}

	return false;
}

static inline enum cf_token_type
cf_get_token_type(const struct cf_token *token,
		  const struct base_token *start_token)
{
	switch (start_token->type) {
	case BASETOKEN_ALPHA:
		return CFTOKEN_NAME;

	case BASETOKEN_DIGIT:
		return CFTOKEN_NUM;

	case BASETOKEN_WHITESPACE:
		if (is_newline(*token->str.array))
			return CFTOKEN_NEWLINE;
		else
			return CFTOKEN_SPACETAB;

	case BASETOKEN_NONE:
	case BASETOKEN_OTHER:
		break;
	}

	return CFTOKEN_OTHER;
}

static bool cf_lexer_nexttoken(struct cf_lexer *lex, struct cf_token *out_token)
{
	struct base_token token, start_token;
	bool wrote_data = false;

	base_token_clear(&token);
	base_token_clear(&start_token);
	cf_token_clear(out_token);

	while (lexer_getbasetoken(&lex->base_lexer, &token, PARSE_WHITESPACE)) {
		/* reclassify underscore as alpha for alnum tokens */
		if (*token.text.array == '_')
			token.type = BASETOKEN_ALPHA;

		/* ignore escaped newlines to merge spliced lines */
		if (cf_is_splice(token.text.array)) {
			lex->base_lexer.offset +=
				newline_size(token.text.array + 1);
			continue;
		}

		if (!wrote_data) {
			out_token->unmerged_str.array = token.text.array;
			out_token->str.array = lex->write_offset;

			/* if comment then output a space */
			if (cf_lexer_process_comment(lex, out_token))
				return true;

			/* process string tokens if any */
			if (cf_lexer_process_string(lex, out_token))
				return true;

			base_token_copy(&start_token, &token);
			wrote_data = true;

		} else if (cf_is_token_break(&start_token, &token)) {
			lex->base_lexer.offset -= token.text.len;
			break;
		}

		/* write token to CF lexer to account for splicing/comments */
		cf_lexer_write_strref(lex, &token.text);
		out_token->str.len += token.text.len;
	}

	if (wrote_data) {
		out_token->unmerged_str.len = (size_t)(
			lex->base_lexer.offset - out_token->unmerged_str.array);
		out_token->type = cf_get_token_type(out_token, &start_token);
	}

	return wrote_data;
}

void cf_lexer_init(struct cf_lexer *lex)
{
	lexer_init(&lex->base_lexer);
	da_init(lex->tokens);

	lex->file = NULL;
	lex->reformatted = NULL;
	lex->write_offset = NULL;
	lex->unexpected_eof = false;
}

void cf_lexer_free(struct cf_lexer *lex)
{
	bfree(lex->file);
	bfree(lex->reformatted);
	lexer_free(&lex->base_lexer);
	da_free(lex->tokens);

	lex->file = NULL;
	lex->reformatted = NULL;
	lex->write_offset = NULL;
	lex->unexpected_eof = false;
}

bool cf_lexer_lex(struct cf_lexer *lex, const char *str, const char *file)
{
	struct cf_token token;
	struct cf_token *last_token = NULL;

	cf_lexer_free(lex);
	if (!str || !*str)
		return false;

	if (file)
		lex->file = bstrdup(file);

	lexer_start(&lex->base_lexer, str);
	cf_token_clear(&token);

	lex->reformatted = bmalloc(strlen(str) + 1);
	lex->reformatted[0] = 0;
	lex->write_offset = lex->reformatted;

	while (cf_lexer_nexttoken(lex, &token)) {
		if (last_token && is_space_or_tab(*last_token->str.array) &&
		    is_space_or_tab(*token.str.array)) {
			cf_token_add(last_token, &token);
			continue;
		}

		token.lex = lex;
		last_token = da_push_back_new(lex->tokens);
		memcpy(last_token, &token, sizeof(struct cf_token));
	}

	cf_token_clear(&token);

	token.str.array = lex->write_offset;
	token.unmerged_str.array = lex->base_lexer.offset;
	token.lex = lex;
	da_push_back(lex->tokens, &token);

	return !lex->unexpected_eof;
}

/* ------------------------------------------------------------------------- */

struct macro_param {
	struct cf_token name;
	DARRAY(struct cf_token) tokens;
};

static inline void macro_param_init(struct macro_param *param)
{
	cf_token_clear(&param->name);
	da_init(param->tokens);
}

static inline void macro_param_free(struct macro_param *param)
{
	cf_token_clear(&param->name);
	da_free(param->tokens);
}

/* ------------------------------------------------------------------------- */

struct macro_params {
	DARRAY(struct macro_param) params;
};

static inline void macro_params_init(struct macro_params *params)
{
	da_init(params->params);
}

static inline void macro_params_free(struct macro_params *params)
{
	size_t i;
	for (i = 0; i < params->params.num; i++)
		macro_param_free(params->params.array + i);
	da_free(params->params);
}

static inline struct macro_param *
get_macro_param(const struct macro_params *params, const struct strref *name)
{
	size_t i;
	if (!params)
		return NULL;

	for (i = 0; i < params->params.num; i++) {
		struct macro_param *param = params->params.array + i;
		if (strref_cmp_strref(&param->name.str, name) == 0)
			return param;
	}

	return NULL;
}

/* ------------------------------------------------------------------------- */

static bool cf_preprocessor(struct cf_preprocessor *pp, bool if_block,
			    struct cf_token **p_cur_token);
static void cf_preprocess_tokens(struct cf_preprocessor *pp, bool if_block,
				 struct cf_token **p_cur_token);

static inline bool go_to_newline(struct cf_token **p_cur_token)
{
	struct cf_token *cur_token = *p_cur_token;
	while (cur_token->type != CFTOKEN_NEWLINE &&
	       cur_token->type != CFTOKEN_NONE)
		cur_token++;

	*p_cur_token = cur_token;

	return cur_token->type != CFTOKEN_NONE;
}

static inline bool next_token(struct cf_token **p_cur_token, bool preprocessor)
{
	struct cf_token *cur_token = *p_cur_token;

	if (cur_token->type != CFTOKEN_NONE)
		cur_token++;

	/* if preprocessor, stop at newline */
	while (cur_token->type == CFTOKEN_SPACETAB &&
	       (preprocessor || cur_token->type == CFTOKEN_NEWLINE))
		cur_token++;

	*p_cur_token = cur_token;
	return cur_token->type != CFTOKEN_NONE;
}

static inline void cf_gettokenoffset(struct cf_preprocessor *pp,
				     const struct cf_token *token,
				     uint32_t *row, uint32_t *col)
{
	lexer_getstroffset(&pp->lex->base_lexer, token->unmerged_str.array, row,
			   col);
}

static void cf_addew(struct cf_preprocessor *pp, const struct cf_token *token,
		     const char *message, int error_level, const char *val1,
		     const char *val2, const char *val3)
{
	uint32_t row, col;
	cf_gettokenoffset(pp, token, &row, &col);

	if (!val1 && !val2 && !val3) {
		error_data_add(pp->ed, token->lex->file, row, col, message,
			       error_level);
	} else {
		struct dstr formatted;
		dstr_init(&formatted);
		dstr_safe_printf(&formatted, message, val1, val2, val3, NULL);

		error_data_add(pp->ed, token->lex->file, row, col,
			       formatted.array, error_level);
		dstr_free(&formatted);
	}
}

static inline void cf_adderror(struct cf_preprocessor *pp,
			       const struct cf_token *token, const char *error,
			       const char *val1, const char *val2,
			       const char *val3)
{
	cf_addew(pp, token, error, LEX_ERROR, val1, val2, val3);
}

static inline void cf_addwarning(struct cf_preprocessor *pp,
				 const struct cf_token *token,
				 const char *warning, const char *val1,
				 const char *val2, const char *val3)
{
	cf_addew(pp, token, warning, LEX_WARNING, val1, val2, val3);
}

static inline void cf_adderror_expecting(struct cf_preprocessor *pp,
					 const struct cf_token *token,
					 const char *expecting)
{
	cf_adderror(pp, token, "Expected $1", expecting, NULL, NULL);
}

static inline void cf_adderror_expected_newline(struct cf_preprocessor *pp,
						const struct cf_token *token)
{
	cf_adderror(pp, token,
		    "Unexpected token after preprocessor, expected "
		    "newline",
		    NULL, NULL, NULL);
}

static inline void
cf_adderror_unexpected_endif_eof(struct cf_preprocessor *pp,
				 const struct cf_token *token)
{
	cf_adderror(pp, token, "Unexpected end of file before #endif", NULL,
		    NULL, NULL);
}

static inline void cf_adderror_unexpected_eof(struct cf_preprocessor *pp,
					      const struct cf_token *token)
{
	cf_adderror(pp, token, "Unexpected end of file", NULL, NULL, NULL);
}

static inline void insert_path(struct cf_preprocessor *pp,
			       struct dstr *str_file)
{
	const char *file;
	const char *slash;

	if (pp && pp->lex && pp->lex->file) {
		file = pp->lex->file;
		slash = strrchr(file, '/');
		if (slash) {
			struct dstr path = {0};
			dstr_ncopy(&path, file, slash - file + 1);
			dstr_insert_dstr(str_file, 0, &path);
			dstr_free(&path);
		}
	}
}

static void cf_include_file(struct cf_preprocessor *pp,
			    const struct cf_token *file_token)
{
	struct cf_lexer new_lex;
	struct dstr str_file;
	FILE *file;
	char *file_data;
	struct cf_token *tokens;
	size_t i;

	dstr_init(&str_file);
	dstr_copy_strref(&str_file, &file_token->str);
	dstr_mid(&str_file, &str_file, 1, str_file.len - 2);
	insert_path(pp, &str_file);

	/* if dependency already exists, run preprocessor on it */
	for (i = 0; i < pp->dependencies.num; i++) {
		struct cf_lexer *dep = pp->dependencies.array + i;

		if (strcmp(dep->file, str_file.array) == 0) {
			tokens = cf_lexer_get_tokens(dep);
			cf_preprocess_tokens(pp, false, &tokens);
			goto exit;
		}
	}

	file = os_fopen(str_file.array, "rb");
	if (!file) {
		cf_adderror(pp, file_token, "Could not open file '$1'",
			    file_token->str.array, NULL, NULL);
		goto exit;
	}

	os_fread_utf8(file, &file_data);
	fclose(file);

	cf_lexer_init(&new_lex);
	cf_lexer_lex(&new_lex, file_data, str_file.array);
	tokens = cf_lexer_get_tokens(&new_lex);
	cf_preprocess_tokens(pp, false, &tokens);
	bfree(file_data);

	da_push_back(pp->dependencies, &new_lex);

exit:
	dstr_free(&str_file);
}

static inline bool is_sys_include(struct strref *ref)
{
	return ref->len >= 2 && ref->array[0] == '<' &&
	       ref->array[ref->len - 1] == '>';
}

static inline bool is_loc_include(struct strref *ref)
{
	return ref->len >= 2 && ref->array[0] == '"' &&
	       ref->array[ref->len - 1] == '"';
}

static void cf_preprocess_include(struct cf_preprocessor *pp,
				  struct cf_token **p_cur_token)
{
	struct cf_token *cur_token = *p_cur_token;

	if (pp->ignore_state) {
		go_to_newline(p_cur_token);
		return;
	}

	next_token(&cur_token, true);

	if (cur_token->type != CFTOKEN_STRING) {
		cf_adderror_expecting(pp, cur_token, "string");
		go_to_newline(&cur_token);
		goto exit;
	}

	if (is_sys_include(&cur_token->str)) {
		/* TODO */
	} else if (is_loc_include(&cur_token->str)) {
		if (!pp->ignore_state)
			cf_include_file(pp, cur_token);
	} else {
		cf_adderror(pp, cur_token, "Invalid or incomplete string", NULL,
			    NULL, NULL);
		go_to_newline(&cur_token);
		goto exit;
	}

	cur_token++;

exit:
	*p_cur_token = cur_token;
}

static bool cf_preprocess_macro_params(struct cf_preprocessor *pp,
				       struct cf_def *def,
				       struct cf_token **p_cur_token)
{
	struct cf_token *cur_token = *p_cur_token;
	bool success = false;
	def->macro = true;

	do {
		next_token(&cur_token, true);
		if (cur_token->type != CFTOKEN_NAME) {
			cf_adderror_expecting(pp, cur_token, "identifier");
			go_to_newline(&cur_token);
			goto exit;
		}

		cf_def_addparam(def, cur_token);

		next_token(&cur_token, true);
		if (cur_token->type != CFTOKEN_OTHER ||
		    (*cur_token->str.array != ',' &&
		     *cur_token->str.array != ')')) {

			cf_adderror_expecting(pp, cur_token, "',' or ')'");
			go_to_newline(&cur_token);
			goto exit;
		}
	} while (*cur_token->str.array != ')');

	/* ended properly, now go to first define token (or newline) */
	next_token(&cur_token, true);
	success = true;

exit:
	*p_cur_token = cur_token;
	return success;
}

#define INVALID_INDEX ((size_t)-1)

static inline size_t cf_preprocess_get_def_idx(struct cf_preprocessor *pp,
					       const struct strref *def_name)
{
	struct cf_def *array = pp->defines.array;
	size_t i;

	for (i = 0; i < pp->defines.num; i++) {
		struct cf_def *cur_def = array + i;

		if (strref_cmp_strref(&cur_def->name.str, def_name) == 0)
			return i;
	}

	return INVALID_INDEX;
}

static inline struct cf_def *
cf_preprocess_get_def(struct cf_preprocessor *pp, const struct strref *def_name)
{
	size_t idx = cf_preprocess_get_def_idx(pp, def_name);
	if (idx == INVALID_INDEX)
		return NULL;

	return pp->defines.array + idx;
}

static char space_filler[2] = " ";

static inline void append_space(struct cf_preprocessor *pp,
				struct darray *tokens,
				const struct cf_token *base)
{
	struct cf_token token;

	strref_set(&token.str, space_filler, 1);
	token.type = CFTOKEN_SPACETAB;
	if (base) {
		token.lex = base->lex;
		strref_copy(&token.unmerged_str, &base->unmerged_str);
	} else {
		token.lex = pp->lex;
		strref_copy(&token.unmerged_str, &token.str);
	}

	darray_push_back(sizeof(struct cf_token), tokens, &token);
}

static inline void append_end_token(struct darray *tokens)
{
	struct cf_token end;
	cf_token_clear(&end);
	darray_push_back(sizeof(struct cf_token), tokens, &end);
}

static void cf_preprocess_define(struct cf_preprocessor *pp,
				 struct cf_token **p_cur_token)
{
	struct cf_token *cur_token = *p_cur_token;
	struct cf_def def;

	if (pp->ignore_state) {
		go_to_newline(p_cur_token);
		return;
	}

	cf_def_init(&def);

	next_token(&cur_token, true);
	if (cur_token->type != CFTOKEN_NAME) {
		cf_adderror_expecting(pp, cur_token, "identifier");
		go_to_newline(&cur_token);
		goto exit;
	}

	append_space(pp, &def.tokens.da, NULL);
	cf_token_copy(&def.name, cur_token);

	if (!next_token(&cur_token, true))
		goto complete;

	/* process macro */
	if (*cur_token->str.array == '(') {
		if (!cf_preprocess_macro_params(pp, &def, &cur_token))
			goto error;
	}

	while (cur_token->type != CFTOKEN_NEWLINE &&
	       cur_token->type != CFTOKEN_NONE)
		cf_def_addtoken(&def, cur_token++);

complete:
	append_end_token(&def.tokens.da);
	append_space(pp, &def.tokens.da, NULL);
	da_push_back(pp->defines, &def);
	goto exit;

error:
	cf_def_free(&def);

exit:
	*p_cur_token = cur_token;
}

static inline void cf_preprocess_remove_def_strref(struct cf_preprocessor *pp,
						   const struct strref *ref)
{
	size_t def_idx = cf_preprocess_get_def_idx(pp, ref);
	if (def_idx != INVALID_INDEX) {
		struct cf_def *array = pp->defines.array;
		cf_def_free(array + def_idx);
		da_erase(pp->defines, def_idx);
	}
}

static void cf_preprocess_undef(struct cf_preprocessor *pp,
				struct cf_token **p_cur_token)
{
	struct cf_token *cur_token = *p_cur_token;

	if (pp->ignore_state) {
		go_to_newline(p_cur_token);
		return;
	}

	next_token(&cur_token, true);
	if (cur_token->type != CFTOKEN_NAME) {
		cf_adderror_expecting(pp, cur_token, "identifier");
		go_to_newline(&cur_token);
		goto exit;
	}

	cf_preprocess_remove_def_strref(pp, &cur_token->str);
	cur_token++;

exit:
	*p_cur_token = cur_token;
}

/* Processes an #ifdef/#ifndef/#if/#else/#elif sub block recursively */
static inline bool cf_preprocess_subblock(struct cf_preprocessor *pp,
					  bool ignore,
					  struct cf_token **p_cur_token)
{
	bool eof;

	if (!next_token(p_cur_token, true))
		return false;

	if (!pp->ignore_state) {
		pp->ignore_state = ignore;
		cf_preprocess_tokens(pp, true, p_cur_token);
		pp->ignore_state = false;
	} else {
		cf_preprocess_tokens(pp, true, p_cur_token);
	}

	eof = ((*p_cur_token)->type == CFTOKEN_NONE);
	if (eof)
		cf_adderror_unexpected_endif_eof(pp, *p_cur_token);
	return !eof;
}

static void cf_preprocess_ifdef(struct cf_preprocessor *pp, bool ifnot,
				struct cf_token **p_cur_token)
{
	struct cf_token *cur_token = *p_cur_token;
	struct cf_def *def;
	bool is_true;

	next_token(&cur_token, true);
	if (cur_token->type != CFTOKEN_NAME) {
		cf_adderror_expecting(pp, cur_token, "identifier");
		go_to_newline(&cur_token);
		goto exit;
	}

	def = cf_preprocess_get_def(pp, &cur_token->str);
	is_true = (def == NULL) == ifnot;

	if (!cf_preprocess_subblock(pp, !is_true, &cur_token))
		goto exit;

	if (strref_cmp(&cur_token->str, "else") == 0) {
		if (!cf_preprocess_subblock(pp, is_true, &cur_token))
			goto exit;
		/*} else if (strref_cmp(&cur_token->str, "elif") == 0) {*/
	}

	cur_token++;

exit:
	*p_cur_token = cur_token;
}

static bool cf_preprocessor(struct cf_preprocessor *pp, bool if_block,
			    struct cf_token **p_cur_token)
{
	struct cf_token *cur_token = *p_cur_token;

	if (strref_cmp(&cur_token->str, "include") == 0) {
		cf_preprocess_include(pp, p_cur_token);

	} else if (strref_cmp(&cur_token->str, "define") == 0) {
		cf_preprocess_define(pp, p_cur_token);

	} else if (strref_cmp(&cur_token->str, "undef") == 0) {
		cf_preprocess_undef(pp, p_cur_token);

	} else if (strref_cmp(&cur_token->str, "ifdef") == 0) {
		cf_preprocess_ifdef(pp, false, p_cur_token);

	} else if (strref_cmp(&cur_token->str, "ifndef") == 0) {
		cf_preprocess_ifdef(pp, true, p_cur_token);

		/*} else if (strref_cmp(&cur_token->str, "if") == 0) {
		TODO;*/
	} else if (strref_cmp(&cur_token->str, "else") == 0 ||
		   /*strref_cmp(&cur_token->str, "elif") == 0 ||*/
		   strref_cmp(&cur_token->str, "endif") == 0) {
		if (!if_block) {
			struct dstr name;
			dstr_init_copy_strref(&name, &cur_token->str);
			cf_adderror(pp, cur_token,
				    "#$1 outside of "
				    "#if/#ifdef/#ifndef block",
				    name.array, NULL, NULL);
			dstr_free(&name);
			(*p_cur_token)++;

			return true;
		}

		return false;

	} else if (cur_token->type != CFTOKEN_NEWLINE &&
		   cur_token->type != CFTOKEN_NONE) {
		/*
		 * TODO: language-specific preprocessor stuff should be sent to
		 * handler of some sort
		 */
		(*p_cur_token)++;
	}

	return true;
}

static void cf_preprocess_addtoken(struct cf_preprocessor *pp,
				   struct darray *dst, /* struct cf_token */
				   struct cf_token **p_cur_token,
				   const struct cf_token *base,
				   const struct macro_params *params);

/*
 * collects tokens for a macro parameter
 *
 * note that it is important to make sure that any usage of function calls
 * within a macro parameter is preserved, example MACRO(func(1, 2), 3), do not
 * let it stop on the comma at "1,"
 */
static void cf_preprocess_save_macro_param(
	struct cf_preprocessor *pp, struct cf_token **p_cur_token,
	struct macro_param *param, const struct cf_token *base,
	const struct macro_params *cur_params)
{
	struct cf_token *cur_token = *p_cur_token;
	int brace_count = 0;

	append_space(pp, &param->tokens.da, base);

	while (cur_token->type != CFTOKEN_NONE) {
		if (*cur_token->str.array == '(') {
			brace_count++;
		} else if (*cur_token->str.array == ')') {
			if (brace_count)
				brace_count--;
			else
				break;
		} else if (*cur_token->str.array == ',') {
			if (!brace_count)
				break;
		}

		cf_preprocess_addtoken(pp, &param->tokens.da, &cur_token, base,
				       cur_params);
	}

	if (cur_token->type == CFTOKEN_NONE)
		cf_adderror_unexpected_eof(pp, cur_token);

	append_space(pp, &param->tokens.da, base);
	append_end_token(&param->tokens.da);

	*p_cur_token = cur_token;
}

static inline bool param_is_whitespace(const struct macro_param *param)
{
	struct cf_token *array = param->tokens.array;
	size_t i;

	for (i = 0; i < param->tokens.num; i++)
		if (array[i].type != CFTOKEN_NONE &&
		    array[i].type != CFTOKEN_SPACETAB &&
		    array[i].type != CFTOKEN_NEWLINE)
			return false;

	return true;
}

/* collects parameter tokens of a used macro and stores them for the unwrap */
static void cf_preprocess_save_macro_params(
	struct cf_preprocessor *pp, struct cf_token **p_cur_token,
	const struct cf_def *def, const struct cf_token *base,
	const struct macro_params *cur_params, struct macro_params *dst)
{
	struct cf_token *cur_token = *p_cur_token;
	size_t count = 0;

	next_token(&cur_token, false);
	if (cur_token->type != CFTOKEN_OTHER || *cur_token->str.array != '(') {
		cf_adderror_expecting(pp, cur_token, "'('");
		goto exit;
	}

	do {
		struct macro_param param;
		macro_param_init(&param);
		cur_token++;
		count++;

		cf_preprocess_save_macro_param(pp, &cur_token, &param, base,
					       cur_params);
		if (cur_token->type != CFTOKEN_OTHER ||
		    (*cur_token->str.array != ',' &&
		     *cur_token->str.array != ')')) {

			macro_param_free(&param);
			cf_adderror_expecting(pp, cur_token, "',' or ')'");
			goto exit;
		}

		if (param_is_whitespace(&param)) {
			/* if 0-param macro, ignore first entry */
			if (count == 1 && !def->params.num &&
			    *cur_token->str.array == ')') {
				macro_param_free(&param);
				break;
			}
		}

		if (count <= def->params.num) {
			cf_token_copy(&param.name,
				      cf_def_getparam(def, count - 1));
			da_push_back(dst->params, &param);
		} else {
			macro_param_free(&param);
		}
	} while (*cur_token->str.array != ')');

	if (count != def->params.num)
		cf_adderror(pp, cur_token,
			    "Mismatching number of macro parameters", NULL,
			    NULL, NULL);

exit:
	*p_cur_token = cur_token;
}

static inline void cf_preprocess_unwrap_param(
	struct cf_preprocessor *pp, struct darray *dst, /* struct cf_token */
	struct cf_token **p_cur_token, const struct cf_token *base,
	const struct macro_param *param)
{
	struct cf_token *cur_token = *p_cur_token;
	struct cf_token *cur_param_token = param->tokens.array;

	while (cur_param_token->type != CFTOKEN_NONE)
		cf_preprocess_addtoken(pp, dst, &cur_param_token, base, NULL);

	cur_token++;
	*p_cur_token = cur_token;
}

static inline void cf_preprocess_unwrap_define(
	struct cf_preprocessor *pp, struct darray *dst, /* struct cf_token */
	struct cf_token **p_cur_token, const struct cf_token *base,
	const struct cf_def *def, const struct macro_params *cur_params)
{
	struct cf_token *cur_token = *p_cur_token;
	struct macro_params new_params;
	struct cf_token *cur_def_token = def->tokens.array;

	macro_params_init(&new_params);

	if (def->macro)
		cf_preprocess_save_macro_params(pp, &cur_token, def, base,
						cur_params, &new_params);

	while (cur_def_token->type != CFTOKEN_NONE)
		cf_preprocess_addtoken(pp, dst, &cur_def_token, base,
				       &new_params);

	macro_params_free(&new_params);

	cur_token++;
	*p_cur_token = cur_token;
}

static void cf_preprocess_addtoken(struct cf_preprocessor *pp,
				   struct darray *dst, /* struct cf_token */
				   struct cf_token **p_cur_token,
				   const struct cf_token *base,
				   const struct macro_params *params)
{
	struct cf_token *cur_token = *p_cur_token;

	if (pp->ignore_state)
		goto ignore;

	if (!base)
		base = cur_token;

	if (cur_token->type == CFTOKEN_NAME) {
		struct cf_def *def;
		struct macro_param *param;

		param = get_macro_param(params, &cur_token->str);
		if (param) {
			cf_preprocess_unwrap_param(pp, dst, &cur_token, base,
						   param);
			goto exit;
		}

		def = cf_preprocess_get_def(pp, &cur_token->str);
		if (def) {
			cf_preprocess_unwrap_define(pp, dst, &cur_token, base,
						    def, params);
			goto exit;
		}
	}

	darray_push_back(sizeof(struct cf_token), dst, cur_token);

ignore:
	cur_token++;

exit:
	*p_cur_token = cur_token;
}

static void cf_preprocess_tokens(struct cf_preprocessor *pp, bool if_block,
				 struct cf_token **p_cur_token)
{
	bool newline = true;
	bool preprocessor_line = if_block;
	struct cf_token *cur_token = *p_cur_token;

	while (cur_token->type != CFTOKEN_NONE) {
		if (cur_token->type != CFTOKEN_SPACETAB &&
		    cur_token->type != CFTOKEN_NEWLINE) {
			if (preprocessor_line) {
				cf_adderror_expected_newline(pp, cur_token);
				if (!go_to_newline(&cur_token))
					break;
			}

			if (newline && *cur_token->str.array == '#') {
				next_token(&cur_token, true);
				preprocessor_line = true;
				if (!cf_preprocessor(pp, if_block, &cur_token))
					break;

				continue;
			}

			newline = false;
		}

		if (cur_token->type == CFTOKEN_NEWLINE) {
			newline = true;
			preprocessor_line = false;
		} else if (cur_token->type == CFTOKEN_NONE) {
			break;
		}

		cf_preprocess_addtoken(pp, &pp->tokens.da, &cur_token, NULL,
				       NULL);
	}

	*p_cur_token = cur_token;
}

void cf_preprocessor_init(struct cf_preprocessor *pp)
{
	da_init(pp->defines);
	da_init(pp->sys_include_dirs);
	da_init(pp->dependencies);
	da_init(pp->tokens);
	pp->lex = NULL;
	pp->ed = NULL;
	pp->ignore_state = false;
}

void cf_preprocessor_free(struct cf_preprocessor *pp)
{
	struct cf_lexer *dependencies = pp->dependencies.array;
	char **sys_include_dirs = pp->sys_include_dirs.array;
	struct cf_def *defs = pp->defines.array;
	size_t i;

	for (i = 0; i < pp->defines.num; i++)
		cf_def_free(defs + i);
	for (i = 0; i < pp->sys_include_dirs.num; i++)
		bfree(sys_include_dirs[i]);
	for (i = 0; i < pp->dependencies.num; i++)
		cf_lexer_free(dependencies + i);

	da_free(pp->defines);
	da_free(pp->sys_include_dirs);
	da_free(pp->dependencies);
	da_free(pp->tokens);

	pp->lex = NULL;
	pp->ed = NULL;
	pp->ignore_state = false;
}

bool cf_preprocess(struct cf_preprocessor *pp, struct cf_lexer *lex,
		   struct error_data *ed)
{
	struct cf_token *token = cf_lexer_get_tokens(lex);
	if (!token)
		return false;

	pp->ed = ed;
	pp->lex = lex;
	cf_preprocess_tokens(pp, false, &token);
	da_push_back(pp->tokens, token);

	return !lex->unexpected_eof;
}

void cf_preprocessor_add_def(struct cf_preprocessor *pp, struct cf_def *def)
{
	struct cf_def *existing = cf_preprocess_get_def(pp, &def->name.str);

	if (existing) {
		struct dstr name;
		dstr_init_copy_strref(&name, &def->name.str);
		cf_addwarning(pp, &def->name, "Token $1 already defined",
			      name.array, NULL, NULL);
		cf_addwarning(pp, &existing->name,
			      "Previous definition of $1 is here", name.array,
			      NULL, NULL);

		cf_def_free(existing);
		memcpy(existing, def, sizeof(struct cf_def));
	} else {
		da_push_back(pp->defines, def);
	}
}

void cf_preprocessor_remove_def(struct cf_preprocessor *pp,
				const char *def_name)
{
	struct strref ref;
	ref.array = def_name;
	ref.len = strlen(def_name);
	cf_preprocess_remove_def_strref(pp, &ref);
}
