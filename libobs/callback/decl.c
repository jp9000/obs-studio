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

#include "../util/cf-parser.h"
#include "decl.h"

static inline void err_specifier_exists(struct cf_parser *cfp,
					const char *storage)
{
	cf_adderror(cfp, "'$1' specifier already exists", LEX_ERROR, storage,
		    NULL, NULL);
}

static inline void err_reserved_name(struct cf_parser *cfp, const char *name)
{
	cf_adderror(cfp, "'$1' is a reserved name", LEX_ERROR, name, NULL,
		    NULL);
}

static inline void err_existing_name(struct cf_parser *cfp, const char *name)
{
	cf_adderror(cfp, "'$1' already exists", LEX_ERROR, name, NULL, NULL);
}

static bool is_in_out_specifier(struct cf_parser *cfp, struct strref *name,
				uint32_t *type)
{
	if (strref_cmp(name, "in") == 0) {
		if (*type & CALL_PARAM_IN)
			err_specifier_exists(cfp, "in");

		*type |= CALL_PARAM_IN;

	} else if (strref_cmp(name, "out") == 0) {
		if (*type & CALL_PARAM_OUT)
			err_specifier_exists(cfp, "out");

		*type |= CALL_PARAM_OUT;

	} else {
		return false;
	}

	return true;
}

#define TYPE_OR_STORAGE "type or storage specifier"

static bool get_type(struct strref *ref, enum call_param_type *type,
		     bool is_return)
{
	if (strref_cmp(ref, "int") == 0)
		*type = CALL_PARAM_TYPE_INT;
	else if (strref_cmp(ref, "float") == 0)
		*type = CALL_PARAM_TYPE_FLOAT;
	else if (strref_cmp(ref, "bool") == 0)
		*type = CALL_PARAM_TYPE_BOOL;
	else if (strref_cmp(ref, "ptr") == 0)
		*type = CALL_PARAM_TYPE_PTR;
	else if (strref_cmp(ref, "string") == 0)
		*type = CALL_PARAM_TYPE_STRING;
	else if (is_return && strref_cmp(ref, "void") == 0)
		*type = CALL_PARAM_TYPE_VOID;
	else
		return false;

	return true;
}

static bool is_reserved_name(const char *str)
{
	return (strcmp(str, "int") == 0) || (strcmp(str, "float") == 0) ||
	       (strcmp(str, "bool") == 0) || (strcmp(str, "ptr") == 0) ||
	       (strcmp(str, "string") == 0) || (strcmp(str, "void") == 0) ||
	       (strcmp(str, "return") == 0);
}

static bool name_exists(struct decl_info *decl, const char *name)
{
	for (size_t i = 0; i < decl->params.num; i++) {
		const char *param_name = decl->params.array[i].name;

		if (strcmp(name, param_name) == 0)
			return true;
	}

	return false;
}

static int parse_param(struct cf_parser *cfp, struct decl_info *decl)
{
	struct strref ref;
	int code;
	struct decl_param param = {0};

	/* get storage specifiers */
	code = cf_next_name_ref(cfp, &ref, TYPE_OR_STORAGE, ",");
	if (code != PARSE_SUCCESS)
		return code;

	while (is_in_out_specifier(cfp, &ref, &param.flags)) {
		code = cf_next_name_ref(cfp, &ref, TYPE_OR_STORAGE, ",");
		if (code != PARSE_SUCCESS)
			return code;
	}

	/* parameters not marked with specifiers are input parameters */
	if (param.flags == 0)
		param.flags = CALL_PARAM_IN;

	if (!get_type(&ref, &param.type, false)) {
		cf_adderror_expecting(cfp, "type");
		cf_go_to_token(cfp, ",", ")");
		return PARSE_CONTINUE;
	}

	/* name */
	code = cf_next_name(cfp, &param.name, "parameter name", ",");
	if (code != PARSE_SUCCESS)
		return code;

	if (name_exists(decl, param.name))
		err_existing_name(cfp, param.name);

	if (is_reserved_name(param.name))
		err_reserved_name(cfp, param.name);

	da_push_back(decl->params, &param);
	return PARSE_SUCCESS;
}

static void parse_params(struct cf_parser *cfp, struct decl_info *decl)
{
	struct cf_token peek;
	int code;

	if (!cf_peek_valid_token(cfp, &peek))
		return;

	while (peek.type == CFTOKEN_NAME) {
		code = parse_param(cfp, decl);
		if (code == PARSE_EOF)
			return;

		if (code != PARSE_CONTINUE && !cf_next_valid_token(cfp))
			return;

		if (cf_token_is(cfp, ")"))
			break;
		else if (cf_token_should_be(cfp, ",", ",", NULL) == PARSE_EOF)
			return;

		if (!cf_peek_valid_token(cfp, &peek))
			return;
	}

	if (!cf_token_is(cfp, ")"))
		cf_next_token_should_be(cfp, ")", NULL, NULL);
}

static void print_errors(struct cf_parser *cfp, const char *decl_string)
{
	char *errors = error_data_buildstring(&cfp->error_list);

	if (errors) {
		blog(LOG_WARNING, "Errors/warnings for '%s':\n\n%s",
		     decl_string, errors);

		bfree(errors);
	}
}

bool parse_decl_string(struct decl_info *decl, const char *decl_string)
{
	struct cf_parser cfp;
	struct strref ret_type;
	struct decl_param ret_param = {0};
	int code;
	bool success;

	decl->decl_string = decl_string;
	ret_param.flags = CALL_PARAM_OUT;

	cf_parser_init(&cfp);
	if (!cf_parser_parse(&cfp, decl_string, "declaration"))
		goto fail;

	code = cf_get_name_ref(&cfp, &ret_type, "return type", NULL);
	if (code == PARSE_EOF)
		goto fail;

	if (!get_type(&ret_type, &ret_param.type, true))
		cf_adderror_expecting(&cfp, "return type");

	code = cf_next_name(&cfp, &decl->name, "function name", "(");
	if (code == PARSE_EOF)
		goto fail;

	if (is_reserved_name(decl->name))
		err_reserved_name(&cfp, decl->name);

	code = cf_next_token_should_be(&cfp, "(", "(", NULL);
	if (code == PARSE_EOF)
		goto fail;

	parse_params(&cfp, decl);

fail:
	success = !error_data_has_errors(&cfp.error_list);

	if (success && ret_param.type != CALL_PARAM_TYPE_VOID) {
		ret_param.name = bstrdup("return");
		da_push_back(decl->params, &ret_param);
	}

	if (!success)
		decl_info_free(decl);

	print_errors(&cfp, decl_string);

	cf_parser_free(&cfp);
	return success;
}
