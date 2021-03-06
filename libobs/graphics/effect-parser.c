/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <assert.h>
#include <limits.h>
#include "../util/platform.h"
#include "effect-parser.h"
#include "effect.h"

static inline bool ep_parse_param_assign(struct effect_parser *ep,
					 struct ep_param *param);

static enum gs_shader_param_type get_effect_param_type(const char *type)
{
	if (strcmp(type, "float") == 0)
		return GS_SHADER_PARAM_FLOAT;
	else if (strcmp(type, "float2") == 0)
		return GS_SHADER_PARAM_VEC2;
	else if (strcmp(type, "float3") == 0)
		return GS_SHADER_PARAM_VEC3;
	else if (strcmp(type, "float4") == 0)
		return GS_SHADER_PARAM_VEC4;
	else if (strcmp(type, "int2") == 0)
		return GS_SHADER_PARAM_INT2;
	else if (strcmp(type, "int3") == 0)
		return GS_SHADER_PARAM_INT3;
	else if (strcmp(type, "int4") == 0)
		return GS_SHADER_PARAM_INT4;
	else if (astrcmp_n(type, "texture", 7) == 0)
		return GS_SHADER_PARAM_TEXTURE;
	else if (strcmp(type, "float4x4") == 0)
		return GS_SHADER_PARAM_MATRIX4X4;
	else if (strcmp(type, "bool") == 0)
		return GS_SHADER_PARAM_BOOL;
	else if (strcmp(type, "int") == 0)
		return GS_SHADER_PARAM_INT;
	else if (strcmp(type, "string") == 0)
		return GS_SHADER_PARAM_STRING;

	return GS_SHADER_PARAM_UNKNOWN;
}

void ep_free(struct effect_parser *ep)
{
	size_t i;
	for (i = 0; i < ep->params.num; i++)
		ep_param_free(ep->params.array + i);
	for (i = 0; i < ep->structs.num; i++)
		ep_struct_free(ep->structs.array + i);
	for (i = 0; i < ep->funcs.num; i++)
		ep_func_free(ep->funcs.array + i);
	for (i = 0; i < ep->samplers.num; i++)
		ep_sampler_free(ep->samplers.array + i);
	for (i = 0; i < ep->techniques.num; i++)
		ep_technique_free(ep->techniques.array + i);

	ep->cur_pass = NULL;
	cf_parser_free(&ep->cfp);
	da_free(ep->params);
	da_free(ep->structs);
	da_free(ep->funcs);
	da_free(ep->samplers);
	da_free(ep->techniques);
}

static inline struct ep_func *ep_getfunc(struct effect_parser *ep,
					 const char *name)
{
	size_t i;
	for (i = 0; i < ep->funcs.num; i++) {
		if (strcmp(name, ep->funcs.array[i].name) == 0)
			return ep->funcs.array + i;
	}

	return NULL;
}

static inline struct ep_struct *ep_getstruct(struct effect_parser *ep,
					     const char *name)
{
	size_t i;
	for (i = 0; i < ep->structs.num; i++) {
		if (strcmp(name, ep->structs.array[i].name) == 0)
			return ep->structs.array + i;
	}

	return NULL;
}

static inline struct ep_sampler *ep_getsampler(struct effect_parser *ep,
					       const char *name)
{
	size_t i;
	for (i = 0; i < ep->samplers.num; i++) {
		if (strcmp(name, ep->samplers.array[i].name) == 0)
			return ep->samplers.array + i;
	}

	return NULL;
}

static inline struct ep_param *ep_getparam(struct effect_parser *ep,
					   const char *name)
{
	size_t i;
	for (i = 0; i < ep->params.num; i++) {
		if (strcmp(name, ep->params.array[i].name) == 0)
			return ep->params.array + i;
	}

	return NULL;
}

static inline struct ep_param *ep_getannotation(struct ep_param *param,
						const char *name)
{
	size_t i;
	for (i = 0; i < param->annotations.num; i++) {
		if (strcmp(name, param->annotations.array[i].name) == 0)
			return param->annotations.array + i;
	}

	return NULL;
}

static inline struct ep_func *ep_getfunc_strref(struct effect_parser *ep,
						const struct strref *ref)
{
	size_t i;
	for (i = 0; i < ep->funcs.num; i++) {
		if (strref_cmp(ref, ep->funcs.array[i].name) == 0)
			return ep->funcs.array + i;
	}

	return NULL;
}

static inline struct ep_struct *ep_getstruct_strref(struct effect_parser *ep,
						    const struct strref *ref)
{
	size_t i;
	for (i = 0; i < ep->structs.num; i++) {
		if (strref_cmp(ref, ep->structs.array[i].name) == 0)
			return ep->structs.array + i;
	}

	return NULL;
}

static inline struct ep_sampler *ep_getsampler_strref(struct effect_parser *ep,
						      const struct strref *ref)
{
	size_t i;
	for (i = 0; i < ep->samplers.num; i++) {
		if (strref_cmp(ref, ep->samplers.array[i].name) == 0)
			return ep->samplers.array + i;
	}

	return NULL;
}

static inline struct ep_param *ep_getparam_strref(struct effect_parser *ep,
						  const struct strref *ref)
{
	size_t i;
	for (i = 0; i < ep->params.num; i++) {
		if (strref_cmp(ref, ep->params.array[i].name) == 0)
			return ep->params.array + i;
	}

	return NULL;
}

static inline int ep_parse_struct_var(struct effect_parser *ep,
				      struct ep_var *var)
{
	int code;

	/* -------------------------------------- */
	/* variable type */

	if (!cf_next_valid_token(&ep->cfp))
		return PARSE_EOF;

	if (cf_token_is(&ep->cfp, ";"))
		return PARSE_CONTINUE;
	if (cf_token_is(&ep->cfp, "}"))
		return PARSE_BREAK;

	code = cf_token_is_type(&ep->cfp, CFTOKEN_NAME, "type name", ";");
	if (code != PARSE_SUCCESS)
		return code;

	cf_copy_token(&ep->cfp, &var->type);

	/* -------------------------------------- */
	/* variable name */

	if (!cf_next_valid_token(&ep->cfp))
		return PARSE_EOF;

	if (cf_token_is(&ep->cfp, ";"))
		return PARSE_UNEXPECTED_CONTINUE;
	if (cf_token_is(&ep->cfp, "}"))
		return PARSE_UNEXPECTED_BREAK;

	code = cf_token_is_type(&ep->cfp, CFTOKEN_NAME, "variable name", ";");
	if (code != PARSE_SUCCESS)
		return code;

	cf_copy_token(&ep->cfp, &var->name);

	/* -------------------------------------- */
	/* variable mapping if any (POSITION, TEXCOORD, etc) */

	if (!cf_next_valid_token(&ep->cfp))
		return PARSE_EOF;

	if (cf_token_is(&ep->cfp, ":")) {
		if (!cf_next_valid_token(&ep->cfp))
			return PARSE_EOF;

		if (cf_token_is(&ep->cfp, ";"))
			return PARSE_UNEXPECTED_CONTINUE;
		if (cf_token_is(&ep->cfp, "}"))
			return PARSE_UNEXPECTED_BREAK;

		code = cf_token_is_type(&ep->cfp, CFTOKEN_NAME, "mapping name",
					";");
		if (code != PARSE_SUCCESS)
			return code;

		cf_copy_token(&ep->cfp, &var->mapping);

		if (!cf_next_valid_token(&ep->cfp))
			return PARSE_EOF;
	}

	/* -------------------------------------- */

	if (!cf_token_is(&ep->cfp, ";")) {
		if (!cf_go_to_valid_token(&ep->cfp, ";", "}"))
			return PARSE_EOF;
		return PARSE_CONTINUE;
	}

	return PARSE_SUCCESS;
}

static void ep_parse_struct(struct effect_parser *ep)
{
	struct ep_struct eps;
	ep_struct_init(&eps);

	if (cf_next_name(&ep->cfp, &eps.name, "name", ";") != PARSE_SUCCESS)
		goto error;
	if (cf_next_token_should_be(&ep->cfp, "{", ";", NULL) != PARSE_SUCCESS)
		goto error;

	/* get structure variables */
	while (true) {
		bool do_break = false;
		struct ep_var var;

		ep_var_init(&var);

		switch (ep_parse_struct_var(ep, &var)) {

		case PARSE_UNEXPECTED_CONTINUE:
			cf_adderror_syntax_error(&ep->cfp);
			/* Falls through. */
		case PARSE_CONTINUE:
			ep_var_free(&var);
			continue;

		case PARSE_UNEXPECTED_BREAK:
			cf_adderror_syntax_error(&ep->cfp);
			/* Falls through. */
		case PARSE_BREAK:
			ep_var_free(&var);
			do_break = true;
			break;

		case PARSE_EOF:
			ep_var_free(&var);
			goto error;
		}

		if (do_break)
			break;

		da_push_back(eps.vars, &var);
	}

	if (cf_next_token_should_be(&ep->cfp, ";", NULL, NULL) != PARSE_SUCCESS)
		goto error;

	da_push_back(ep->structs, &eps);
	return;

error:
	ep_struct_free(&eps);
}

static inline int ep_parse_param_annotation_var(struct effect_parser *ep,
						struct ep_param *var)
{
	int code;

	/* -------------------------------------- */
	/* variable type */

	if (!cf_next_valid_token(&ep->cfp))
		return PARSE_EOF;

	if (cf_token_is(&ep->cfp, ";"))
		return PARSE_CONTINUE;
	if (cf_token_is(&ep->cfp, ">"))
		return PARSE_BREAK;

	code = cf_token_is_type(&ep->cfp, CFTOKEN_NAME, "type name", ";");
	if (code != PARSE_SUCCESS)
		return code;

	bfree(var->type);
	cf_copy_token(&ep->cfp, &var->type);

	/* -------------------------------------- */
	/* variable name */

	if (!cf_next_valid_token(&ep->cfp))
		return PARSE_EOF;

	if (cf_token_is(&ep->cfp, ";")) {
		cf_adderror_expecting(&ep->cfp, "variable name");
		return PARSE_UNEXPECTED_CONTINUE;
	}
	if (cf_token_is(&ep->cfp, ">")) {
		cf_adderror_expecting(&ep->cfp, "variable name");
		return PARSE_UNEXPECTED_BREAK;
	}

	code = cf_token_is_type(&ep->cfp, CFTOKEN_NAME, "variable name", ";");
	if (code != PARSE_SUCCESS)
		return code;

	bfree(var->name);
	cf_copy_token(&ep->cfp, &var->name);

	/* -------------------------------------- */
	/* variable mapping if any (POSITION, TEXCOORD, etc) */

	if (!cf_next_valid_token(&ep->cfp))
		return PARSE_EOF;

	if (cf_token_is(&ep->cfp, ":")) {
		cf_adderror_expecting(&ep->cfp, "= or ;");
		return PARSE_UNEXPECTED_BREAK;
	} else if (cf_token_is(&ep->cfp, ">")) {
		cf_adderror_expecting(&ep->cfp, "= or ;");
		return PARSE_UNEXPECTED_BREAK;
	} else if (cf_token_is(&ep->cfp, "=")) {
		if (!ep_parse_param_assign(ep, var)) {
			cf_adderror_expecting(&ep->cfp, "assignment value");
			return PARSE_UNEXPECTED_BREAK;
		}
	}

	/* -------------------------------------- */

	if (!cf_token_is(&ep->cfp, ";")) {
		if (!cf_go_to_valid_token(&ep->cfp, ";", ">")) {
			cf_adderror_expecting(&ep->cfp, "; or >");
			return PARSE_EOF;
		}
		return PARSE_CONTINUE;
	}

	return PARSE_SUCCESS;
}

static int ep_parse_annotations(struct effect_parser *ep,
				struct darray *annotations)
{
	if (!cf_token_is(&ep->cfp, "<")) {
		cf_adderror_expecting(&ep->cfp, "<");
		goto error;
	}

	/* get annotation variables */
	while (true) {
		bool do_break = false;
		struct ep_param var;

		ep_param_init(&var, bstrdup(""), bstrdup(""), false, false,
			      false);

		switch (ep_parse_param_annotation_var(ep, &var)) {
		case PARSE_UNEXPECTED_CONTINUE:
			cf_adderror_syntax_error(&ep->cfp);
			/* Falls through. */
		case PARSE_CONTINUE:
			ep_param_free(&var);
			continue;

		case PARSE_UNEXPECTED_BREAK:
			cf_adderror_syntax_error(&ep->cfp);
			/* Falls through. */
		case PARSE_BREAK:
			ep_param_free(&var);
			do_break = true;
			break;

		case PARSE_EOF:
			ep_param_free(&var);
			goto error;
		}

		if (do_break)
			break;

		darray_push_back(sizeof(struct ep_param), annotations, &var);
	}

	if (!cf_token_is(&ep->cfp, ">")) {
		cf_adderror_expecting(&ep->cfp, ">");
		goto error;
	}
	if (!cf_next_valid_token(&ep->cfp))
		goto error;

	return true;

error:
	return false;
}

static int ep_parse_param_annotations(struct effect_parser *ep,
				      struct ep_param *param)
{
	return ep_parse_annotations(ep, &param->annotations.da);
}

static inline int ep_parse_pass_command_call(struct effect_parser *ep,
					     struct darray *call)
{
	struct cf_token end_token;
	cf_token_clear(&end_token);

	while (!cf_token_is(&ep->cfp, ";")) {
		if (cf_token_is(&ep->cfp, "}")) {
			cf_adderror_expecting(&ep->cfp, ";");
			return PARSE_CONTINUE;
		}

		darray_push_back(sizeof(struct cf_token), call,
				 ep->cfp.cur_token);
		if (!cf_next_valid_token(&ep->cfp))
			return PARSE_EOF;
	}

	darray_push_back(sizeof(struct cf_token), call, ep->cfp.cur_token);
	darray_push_back(sizeof(struct cf_token), call, &end_token);
	return PARSE_SUCCESS;
}

static int ep_parse_pass_command(struct effect_parser *ep, struct ep_pass *pass)
{
	struct darray *call; /* struct cf_token */

	if (!cf_next_valid_token(&ep->cfp))
		return PARSE_EOF;

	if (cf_token_is(&ep->cfp, "vertex_shader") ||
	    cf_token_is(&ep->cfp, "vertex_program")) {
		call = &pass->vertex_program.da;

	} else if (cf_token_is(&ep->cfp, "pixel_shader") ||
		   cf_token_is(&ep->cfp, "pixel_program")) {
		call = &pass->fragment_program.da;

	} else {
		cf_adderror_syntax_error(&ep->cfp);
		if (!cf_go_to_valid_token(&ep->cfp, ";", "}"))
			return PARSE_EOF;
		return PARSE_CONTINUE;
	}

	if (cf_next_token_should_be(&ep->cfp, "=", ";", "}") != PARSE_SUCCESS)
		return PARSE_CONTINUE;

	if (!cf_next_valid_token(&ep->cfp))
		return PARSE_EOF;
	if (cf_token_is(&ep->cfp, "compile")) {
		cf_adderror(&ep->cfp, "compile keyword not necessary",
			    LEX_WARNING, NULL, NULL, NULL);
		if (!cf_next_valid_token(&ep->cfp))
			return PARSE_EOF;
	}

	return ep_parse_pass_command_call(ep, call);
}

static int ep_parse_pass(struct effect_parser *ep, struct ep_pass *pass)
{
	struct cf_token peek;

	if (!cf_token_is(&ep->cfp, "pass"))
		return PARSE_UNEXPECTED_CONTINUE;

	if (!cf_next_valid_token(&ep->cfp))
		return PARSE_EOF;

	if (!cf_token_is(&ep->cfp, "{")) {
		pass->name = bstrdup_n(ep->cfp.cur_token->str.array,
				       ep->cfp.cur_token->str.len);
		if (!cf_next_valid_token(&ep->cfp))
			return PARSE_EOF;
	}

	if (!cf_peek_valid_token(&ep->cfp, &peek))
		return PARSE_EOF;

	while (strref_cmp(&peek.str, "}") != 0) {
		int ret = ep_parse_pass_command(ep, pass);
		if (ret < 0 && ret != PARSE_CONTINUE)
			return ret;

		if (!cf_peek_valid_token(&ep->cfp, &peek))
			return PARSE_EOF;
	}

	/* token is '}' */
	cf_next_token(&ep->cfp);

	return PARSE_SUCCESS;
}

static void ep_parse_technique(struct effect_parser *ep)
{
	struct ep_technique ept;
	ep_technique_init(&ept);

	if (cf_next_name(&ep->cfp, &ept.name, "name", ";") != PARSE_SUCCESS)
		goto error;

	if (!cf_next_valid_token(&ep->cfp))
		return;

	if (!cf_token_is(&ep->cfp, "{")) {
		if (!cf_go_to_token(&ep->cfp, ";", NULL)) {
			cf_adderror_expecting(&ep->cfp, ";");
			return;
		}

		cf_adderror_expecting(&ep->cfp, "{");
		goto error;
	}
	if (!cf_next_valid_token(&ep->cfp))
		goto error;

	while (!cf_token_is(&ep->cfp, "}")) {
		struct ep_pass pass;
		ep_pass_init(&pass);

		switch (ep_parse_pass(ep, &pass)) {
		case PARSE_UNEXPECTED_CONTINUE:
			ep_pass_free(&pass);
			if (!cf_go_to_token(&ep->cfp, "}", NULL))
				goto error;
			continue;
		case PARSE_EOF:
			ep_pass_free(&pass);
			goto error;
		}

		da_push_back(ept.passes, &pass);

		if (!cf_next_valid_token(&ep->cfp))
			goto error;
	}

	/* pass the current token (which is '}') if we reached here */
	cf_next_token(&ep->cfp);

	da_push_back(ep->techniques, &ept);
	return;

error:
	cf_next_token(&ep->cfp);
	ep_technique_free(&ept);
}

static int ep_parse_sampler_state_item(struct effect_parser *ep,
				       struct ep_sampler *eps)
{
	int ret;
	char *state = NULL;
	struct dstr value = {0};

	ret = cf_next_name(&ep->cfp, &state, "state name", ";");
	if (ret != PARSE_SUCCESS)
		goto fail;

	ret = cf_next_token_should_be(&ep->cfp, "=", ";", NULL);
	if (ret != PARSE_SUCCESS)
		goto fail;

	for (;;) {
		const char *cur_str;

		if (!cf_next_valid_token(&ep->cfp))
			return PARSE_EOF;

		cur_str = ep->cfp.cur_token->str.array;
		if (*cur_str == ';')
			break;

		dstr_ncat(&value, cur_str, ep->cfp.cur_token->str.len);
	}

	if (value.len) {
		da_push_back(eps->states, &state);
		da_push_back(eps->values, &value.array);
	}

	return ret;

fail:
	bfree(state);
	dstr_free(&value);
	return ret;
}

static void ep_parse_sampler_state(struct effect_parser *ep)
{
	struct ep_sampler eps;
	struct cf_token peek;
	ep_sampler_init(&eps);

	if (cf_next_name(&ep->cfp, &eps.name, "name", ";") != PARSE_SUCCESS)
		goto error;
	if (cf_next_token_should_be(&ep->cfp, "{", ";", NULL) != PARSE_SUCCESS)
		goto error;

	if (!cf_peek_valid_token(&ep->cfp, &peek))
		goto error;

	while (strref_cmp(&peek.str, "}") != 0) {
		int ret = ep_parse_sampler_state_item(ep, &eps);
		if (ret == PARSE_EOF)
			goto error;

		if (!cf_peek_valid_token(&ep->cfp, &peek))
			goto error;
	}

	if (cf_next_token_should_be(&ep->cfp, "}", ";", NULL) != PARSE_SUCCESS)
		goto error;
	if (cf_next_token_should_be(&ep->cfp, ";", NULL, NULL) != PARSE_SUCCESS)
		goto error;

	da_push_back(ep->samplers, &eps);
	return;

error:
	ep_sampler_free(&eps);
}

static inline int ep_check_for_keyword(struct effect_parser *ep,
				       const char *keyword, bool *val)
{
	bool new_val = cf_token_is(&ep->cfp, keyword);
	if (new_val) {
		if (!cf_next_valid_token(&ep->cfp))
			return PARSE_EOF;

		if (new_val && *val)
			cf_adderror(&ep->cfp, "'$1' keyword already specified",
				    LEX_WARNING, keyword, NULL, NULL);
		*val = new_val;

		return PARSE_CONTINUE;
	}

	return PARSE_SUCCESS;
}

static inline int ep_parse_func_param(struct effect_parser *ep,
				      struct ep_func *func, struct ep_var *var)
{
	int code;
	bool var_type_keyword = false;

	if (!cf_next_valid_token(&ep->cfp))
		return PARSE_EOF;

	code = ep_check_for_keyword(ep, "in", &var_type_keyword);
	if (code == PARSE_EOF)
		return PARSE_EOF;
	else if (var_type_keyword)
		var->var_type = EP_VAR_IN;

	if (!var_type_keyword) {
		code = ep_check_for_keyword(ep, "inout", &var_type_keyword);
		if (code == PARSE_EOF)
			return PARSE_EOF;
		else if (var_type_keyword)
			var->var_type = EP_VAR_INOUT;
	}

	if (!var_type_keyword) {
		code = ep_check_for_keyword(ep, "out", &var_type_keyword);
		if (code == PARSE_EOF)
			return PARSE_EOF;
		else if (var_type_keyword)
			var->var_type = EP_VAR_OUT;
	}

	if (!var_type_keyword) {
		code = ep_check_for_keyword(ep, "uniform", &var_type_keyword);
		if (code == PARSE_EOF)
			return PARSE_EOF;
		else if (var_type_keyword)
			var->var_type = EP_VAR_UNIFORM;
	}

	code = cf_get_name(&ep->cfp, &var->type, "type", ")");
	if (code != PARSE_SUCCESS)
		return code;

	code = cf_next_name(&ep->cfp, &var->name, "name", ")");
	if (code != PARSE_SUCCESS)
		return code;

	if (!cf_next_valid_token(&ep->cfp))
		return PARSE_EOF;

	if (cf_token_is(&ep->cfp, ":")) {
		code = cf_next_name(&ep->cfp, &var->mapping,
				    "mapping specifier", ")");
		if (code != PARSE_SUCCESS)
			return code;

		if (!cf_next_valid_token(&ep->cfp))
			return PARSE_EOF;
	}

	if (ep_getstruct(ep, var->type) != NULL)
		da_push_back(func->struct_deps, &var->type);
	else if (ep_getsampler(ep, var->type) != NULL)
		da_push_back(func->sampler_deps, &var->type);

	return PARSE_SUCCESS;
}

static bool ep_parse_func_params(struct effect_parser *ep, struct ep_func *func)
{
	struct cf_token peek;
	int code;

	cf_token_clear(&peek);

	if (!cf_peek_valid_token(&ep->cfp, &peek))
		return false;

	if (*peek.str.array == ')') {
		cf_next_token(&ep->cfp);
		goto exit;
	}

	do {
		struct ep_var var;
		ep_var_init(&var);

		if (!cf_token_is(&ep->cfp, "(") && !cf_token_is(&ep->cfp, ","))
			cf_adderror_syntax_error(&ep->cfp);

		code = ep_parse_func_param(ep, func, &var);
		if (code != PARSE_SUCCESS) {
			ep_var_free(&var);

			if (code == PARSE_CONTINUE)
				goto exit;
			else if (code == PARSE_EOF)
				return false;
		}

		da_push_back(func->param_vars, &var);
	} while (!cf_token_is(&ep->cfp, ")"));

exit:
	return true;
}

static inline bool ep_process_struct_dep(struct effect_parser *ep,
					 struct ep_func *func)
{
	struct ep_struct *val =
		ep_getstruct_strref(ep, &ep->cfp.cur_token->str);
	if (val)
		da_push_back(func->struct_deps, &val->name);
	return val != NULL;
}

static inline bool ep_process_func_dep(struct effect_parser *ep,
				       struct ep_func *func)
{
	struct ep_func *val = ep_getfunc_strref(ep, &ep->cfp.cur_token->str);
	if (val)
		da_push_back(func->func_deps, &val->name);
	return val != NULL;
}

static inline bool ep_process_sampler_dep(struct effect_parser *ep,
					  struct ep_func *func)
{
	struct ep_sampler *val =
		ep_getsampler_strref(ep, &ep->cfp.cur_token->str);
	if (val)
		da_push_back(func->sampler_deps, &val->name);
	return val != NULL;
}

static inline bool ep_process_param_dep(struct effect_parser *ep,
					struct ep_func *func)
{
	struct ep_param *val = ep_getparam_strref(ep, &ep->cfp.cur_token->str);
	if (val)
		da_push_back(func->param_deps, &val->name);
	return val != NULL;
}

static inline bool ep_parse_func_contents(struct effect_parser *ep,
					  struct ep_func *func)
{
	int braces = 1;

	dstr_cat_strref(&func->contents, &ep->cfp.cur_token->str);

	while (braces > 0) {
		if ((ep->cfp.cur_token++)->type == CFTOKEN_NONE)
			return false;

		if (ep->cfp.cur_token->type == CFTOKEN_SPACETAB ||
		    ep->cfp.cur_token->type == CFTOKEN_NEWLINE) {
		} else if (cf_token_is(&ep->cfp, "{")) {
			braces++;
		} else if (cf_token_is(&ep->cfp, "}")) {
			braces--;
		} else if (ep_process_struct_dep(ep, func) ||
			   ep_process_func_dep(ep, func) ||
			   ep_process_sampler_dep(ep, func) ||
			   ep_process_param_dep(ep, func)) {
		}

		dstr_cat_strref(&func->contents, &ep->cfp.cur_token->str);
	}

	return true;
}

static void ep_parse_function(struct effect_parser *ep, char *type, char *name)
{
	struct ep_func func;
	int code;

	ep_func_init(&func, type, name);
	if (ep_getstruct(ep, type))
		da_push_back(func.struct_deps, &func.ret_type);

	if (!ep_parse_func_params(ep, &func))
		goto error;

	if (!cf_next_valid_token(&ep->cfp))
		goto error;

	/* if function is mapped to something, for example COLOR */
	if (cf_token_is(&ep->cfp, ":")) {
		code = cf_next_name(&ep->cfp, &func.mapping,
				    "mapping specifier", "{");
		if (code == PARSE_EOF)
			goto error;
		else if (code != PARSE_CONTINUE) {
			if (!cf_next_valid_token(&ep->cfp))
				goto error;
		}
	}

	if (!cf_token_is(&ep->cfp, "{")) {
		cf_adderror_expecting(&ep->cfp, "{");
		goto error;
	}

	if (!ep_parse_func_contents(ep, &func))
		goto error;

	/* it is established that the current token is '}' if we reach this */
	cf_next_token(&ep->cfp);

	da_push_back(ep->funcs, &func);
	return;

error:
	ep_func_free(&func);
}

/* parses "array[count]" */
static bool ep_parse_param_array(struct effect_parser *ep,
				 struct ep_param *param)
{
	if (!cf_next_valid_token(&ep->cfp))
		return false;

	if (ep->cfp.cur_token->type != CFTOKEN_NUM ||
	    !valid_int_str(ep->cfp.cur_token->str.array,
			   ep->cfp.cur_token->str.len))
		return false;

	param->array_count =
		(int)strtol(ep->cfp.cur_token->str.array, NULL, 10);

	if (cf_next_token_should_be(&ep->cfp, "]", ";", NULL) == PARSE_EOF)
		return false;

	if (!cf_next_valid_token(&ep->cfp))
		return false;

	return true;
}

static inline int ep_parse_param_assign_texture(struct effect_parser *ep,
						struct ep_param *param)
{
	int code;
	char *str;

	if (!cf_next_valid_token(&ep->cfp))
		return PARSE_EOF;

	code = cf_token_is_type(&ep->cfp, CFTOKEN_STRING, "texture path string",
				";");
	if (code != PARSE_SUCCESS)
		return code;

	str = cf_literal_to_str(ep->cfp.cur_token->str.array,
				ep->cfp.cur_token->str.len);

	if (str) {
		da_copy_array(param->default_val, str, strlen(str) + 1);
		bfree(str);
	}

	return PARSE_SUCCESS;
}

static inline int ep_parse_param_assign_string(struct effect_parser *ep,
					       struct ep_param *param)
{
	int code;
	char *str = NULL;

	if (!cf_next_valid_token(&ep->cfp))
		return PARSE_EOF;

	code = cf_token_is_type(&ep->cfp, CFTOKEN_STRING, "string", ";");
	if (code != PARSE_SUCCESS)
		return code;

	str = cf_literal_to_str(ep->cfp.cur_token->str.array,
				ep->cfp.cur_token->str.len);

	if (str) {
		da_copy_array(param->default_val, str, strlen(str) + 1);
		bfree(str);
	}

	return PARSE_SUCCESS;
}

static inline int ep_parse_param_assign_intfloat(struct effect_parser *ep,
						 struct ep_param *param,
						 bool is_float)
{
	int code;
	bool is_negative = false;

	if (!cf_next_valid_token(&ep->cfp))
		return PARSE_EOF;

	if (cf_token_is(&ep->cfp, "-")) {
		is_negative = true;

		if (!cf_next_token(&ep->cfp))
			return PARSE_EOF;
	}

	code = cf_token_is_type(&ep->cfp, CFTOKEN_NUM, "numeric value", ";");
	if (code != PARSE_SUCCESS)
		return code;

	if (is_float) {
		float f = (float)os_strtod(ep->cfp.cur_token->str.array);
		if (is_negative)
			f = -f;
		da_push_back_array(param->default_val, &f, sizeof(float));
	} else {
		long l = strtol(ep->cfp.cur_token->str.array, NULL, 10);
		if (is_negative)
			l = -l;
		da_push_back_array(param->default_val, &l, sizeof(long));
	}

	return PARSE_SUCCESS;
}

static inline int ep_parse_param_assign_bool(struct effect_parser *ep,
					     struct ep_param *param)
{
	if (!cf_next_valid_token(&ep->cfp))
		return PARSE_EOF;

	if (cf_token_is(&ep->cfp, "true")) {
		long l = 1;
		da_push_back_array(param->default_val, &l, sizeof(long));
		return PARSE_SUCCESS;
	} else if (cf_token_is(&ep->cfp, "false")) {
		long l = 0;
		da_push_back_array(param->default_val, &l, sizeof(long));
		return PARSE_SUCCESS;
	}

	cf_adderror_expecting(&ep->cfp, "true or false");

	return PARSE_EOF;
}

/*
 * parses assignment for float1, float2, float3, float4, int1, int2, int3, int4,
 * and any combination for float3x3, float4x4, int3x3, int4x4, etc
*/
static inline int ep_parse_param_assign_intfloat_array(struct effect_parser *ep,
						       struct ep_param *param,
						       bool is_float)
{
	const char *intfloat_type = param->type + (is_float ? 5 : 3);
	int intfloat_count = 0, code, i;

	/* -------------------------------------------- */

	if (intfloat_type[0] < '1' || intfloat_type[0] > '4')
		cf_adderror(&ep->cfp, "Invalid row count", LEX_ERROR, NULL,
			    NULL, NULL);

	intfloat_count = intfloat_type[0] - '0';

	if (intfloat_type[1] == 'x') {
		if (intfloat_type[2] < '1' || intfloat_type[2] > '4')
			cf_adderror(&ep->cfp, "Invalid column count", LEX_ERROR,
				    NULL, NULL, NULL);

		intfloat_count *= intfloat_type[2] - '0';
	}

	/* -------------------------------------------- */

	code = cf_next_token_should_be(&ep->cfp, "{", ";", NULL);
	if (code != PARSE_SUCCESS)
		return code;

	for (i = 0; i < intfloat_count; i++) {
		char *next = ((i + 1) < intfloat_count) ? "," : "}";

		code = ep_parse_param_assign_intfloat(ep, param, is_float);
		if (code != PARSE_SUCCESS)
			return code;

		code = cf_next_token_should_be(&ep->cfp, next, ";", NULL);
		if (code != PARSE_SUCCESS)
			return code;
	}

	return PARSE_SUCCESS;
}

static int ep_parse_param_assignment_val(struct effect_parser *ep,
					 struct ep_param *param)
{
	if (param->is_texture)
		return ep_parse_param_assign_texture(ep, param);
	else if (strcmp(param->type, "int") == 0)
		return ep_parse_param_assign_intfloat(ep, param, false);
	else if (strcmp(param->type, "float") == 0)
		return ep_parse_param_assign_intfloat(ep, param, true);
	else if (astrcmp_n(param->type, "int", 3) == 0)
		return ep_parse_param_assign_intfloat_array(ep, param, false);
	else if (astrcmp_n(param->type, "float", 5) == 0)
		return ep_parse_param_assign_intfloat_array(ep, param, true);
	else if (astrcmp_n(param->type, "string", 6) == 0)
		return ep_parse_param_assign_string(ep, param);
	else if (strcmp(param->type, "bool") == 0)
		return ep_parse_param_assign_bool(ep, param);

	cf_adderror(&ep->cfp, "Invalid type '$1' used for assignment",
		    LEX_ERROR, param->type, NULL, NULL);

	return PARSE_CONTINUE;
}

static inline bool ep_parse_param_assign(struct effect_parser *ep,
					 struct ep_param *param)
{
	if (ep_parse_param_assignment_val(ep, param) != PARSE_SUCCESS)
		return false;

	if (!cf_next_valid_token(&ep->cfp))
		return false;

	return true;
}

/* static bool ep_parse_param_property(struct effect_parser *ep,
		struct ep_param *param)
{
} */

static void ep_parse_param(struct effect_parser *ep, char *type, char *name,
			   bool is_property, bool is_const, bool is_uniform)
{
	struct ep_param param;
	ep_param_init(&param, type, name, is_property, is_const, is_uniform);

	if (cf_token_is(&ep->cfp, ";"))
		goto complete;
	if (cf_token_is(&ep->cfp, "[") && !ep_parse_param_array(ep, &param))
		goto error;
	if (cf_token_is(&ep->cfp, "<") &&
	    !ep_parse_param_annotations(ep, &param))
		goto error;
	if (cf_token_is(&ep->cfp, "=") && !ep_parse_param_assign(ep, &param))
		goto error;
	/*
	if (cf_token_is(&ep->cfp, "<") && !ep_parse_param_property(ep, &param))
		goto error; */
	if (!cf_token_is(&ep->cfp, ";"))
		goto error;

complete:
	da_push_back(ep->params, &param);
	return;

error:
	ep_param_free(&param);
}

static bool ep_get_var_specifiers(struct effect_parser *ep, bool *is_property,
				  bool *is_const, bool *is_uniform)
{
	while (true) {
		int code;
		code = ep_check_for_keyword(ep, "property", is_property);
		if (code == PARSE_EOF)
			return false;
		else if (code == PARSE_CONTINUE)
			continue;

		code = ep_check_for_keyword(ep, "const", is_const);
		if (code == PARSE_EOF)
			return false;
		else if (code == PARSE_CONTINUE)
			continue;

		code = ep_check_for_keyword(ep, "uniform", is_uniform);
		if (code == PARSE_EOF)
			return false;
		else if (code == PARSE_CONTINUE)
			continue;

		break;
	}

	return true;
}

static inline void report_invalid_func_keyword(struct effect_parser *ep,
					       const char *name, bool val)
{
	if (val)
		cf_adderror(&ep->cfp,
			    "'$1' keyword cannot be used with a "
			    "function",
			    LEX_ERROR, name, NULL, NULL);
}

static void ep_parse_other(struct effect_parser *ep)
{
	bool is_property = false, is_const = false, is_uniform = false;
	char *type = NULL, *name = NULL;

	if (!ep_get_var_specifiers(ep, &is_property, &is_const, &is_uniform))
		goto error;

	if (cf_get_name(&ep->cfp, &type, "type", ";") != PARSE_SUCCESS)
		goto error;
	if (cf_next_name(&ep->cfp, &name, "name", ";") != PARSE_SUCCESS)
		goto error;
	if (!cf_next_valid_token(&ep->cfp))
		goto error;

	if (cf_token_is(&ep->cfp, "(")) {
		report_invalid_func_keyword(ep, "property", is_property);
		report_invalid_func_keyword(ep, "const", is_const);
		report_invalid_func_keyword(ep, "uniform", is_uniform);

		ep_parse_function(ep, type, name);
		return;
	} else {
		ep_parse_param(ep, type, name, is_property, is_const,
			       is_uniform);
		return;
	}

error:
	bfree(type);
	bfree(name);
}

static bool ep_compile(struct effect_parser *ep);

extern const char *gs_preprocessor_name(void);

#if defined(_DEBUG) && defined(_DEBUG_SHADERS)
static void debug_get_default_value(struct gs_effect_param *param, char *buffer,
				    unsigned long long buf_size)
{
	if (param->default_val.num == 0) {
		snprintf(buffer, buf_size, "(null)");
		return;
	}

	switch (param->type) {
	case GS_SHADER_PARAM_STRING:
		snprintf(buffer, buf_size, "'%.*s'", param->default_val.num,
			 param->default_val.array);
		break;
	case GS_SHADER_PARAM_INT:
		snprintf(buffer, buf_size, "%ld",
			 *(int *)(param->default_val.array + 0));
		break;
	case GS_SHADER_PARAM_INT2:
		snprintf(buffer, buf_size, "%ld,%ld",
			 *(int *)(param->default_val.array + 0),
			 *(int *)(param->default_val.array + 4));
		break;
	case GS_SHADER_PARAM_INT3:
		snprintf(buffer, buf_size, "%ld,%ld,%ld",
			 *(int *)(param->default_val.array + 0),
			 *(int *)(param->default_val.array + 4),
			 *(int *)(param->default_val.array + 8));
		break;
	case GS_SHADER_PARAM_INT4:
		snprintf(buffer, buf_size, "%ld,%ld,%ld,%ld",
			 *(int *)(param->default_val.array + 0),
			 *(int *)(param->default_val.array + 4),
			 *(int *)(param->default_val.array + 8),
			 *(int *)(param->default_val.array + 12));
		break;
	case GS_SHADER_PARAM_FLOAT:
		snprintf(buffer, buf_size, "%e",
			 *(float *)(param->default_val.array + 0));
		break;
	case GS_SHADER_PARAM_VEC2:
		snprintf(buffer, buf_size, "%e,%e",
			 *(float *)(param->default_val.array + 0),
			 *(float *)(param->default_val.array + 4));
		break;
	case GS_SHADER_PARAM_VEC3:
		snprintf(buffer, buf_size, "%e,%e,%e",
			 *(float *)(param->default_val.array + 0),
			 *(float *)(param->default_val.array + 4),
			 *(float *)(param->default_val.array + 8));
		break;
	case GS_SHADER_PARAM_VEC4:
		snprintf(buffer, buf_size, "%e,%e,%e,%e",
			 *(float *)(param->default_val.array + 0),
			 *(float *)(param->default_val.array + 4),
			 *(float *)(param->default_val.array + 8),
			 *(float *)(param->default_val.array + 12));
		break;
	case GS_SHADER_PARAM_MATRIX4X4:
		snprintf(buffer, buf_size,
			 "[[%e,%e,%e,%e],[%e,%e,%e,%e],"
			 "[%e,%e,%e,%e],[%e,%e,%e,%e]]",
			 *(float *)(param->default_val.array + 0),
			 *(float *)(param->default_val.array + 4),
			 *(float *)(param->default_val.array + 8),
			 *(float *)(param->default_val.array + 12),
			 *(float *)(param->default_val.array + 16),
			 *(float *)(param->default_val.array + 20),
			 *(float *)(param->default_val.array + 24),
			 *(float *)(param->default_val.array + 28),
			 *(float *)(param->default_val.array + 32),
			 *(float *)(param->default_val.array + 36),
			 *(float *)(param->default_val.array + 40),
			 *(float *)(param->default_val.array + 44),
			 *(float *)(param->default_val.array + 48),
			 *(float *)(param->default_val.array + 52),
			 *(float *)(param->default_val.array + 56),
			 *(float *)(param->default_val.array + 60));
		break;
	case GS_SHADER_PARAM_BOOL:
		snprintf(buffer, buf_size, "%s",
			 (*param->default_val.array) != 0 ? "true\0"
							  : "false\0");
		break;
	case GS_SHADER_PARAM_UNKNOWN:
	case GS_SHADER_PARAM_TEXTURE:
		snprintf(buffer, buf_size, "<unknown>");
		break;
	}
}

static void debug_param(struct gs_effect_param *param,
			struct ep_param *param_in, unsigned long long idx,
			const char *offset)
{
	char _debug_type[4096];
	switch (param->type) {
	case GS_SHADER_PARAM_STRING:
		snprintf(_debug_type, sizeof(_debug_type), "string");
		break;
	case GS_SHADER_PARAM_INT:
		snprintf(_debug_type, sizeof(_debug_type), "int");
		break;
	case GS_SHADER_PARAM_INT2:
		snprintf(_debug_type, sizeof(_debug_type), "int2");
		break;
	case GS_SHADER_PARAM_INT3:
		snprintf(_debug_type, sizeof(_debug_type), "int3");
		break;
	case GS_SHADER_PARAM_INT4:
		snprintf(_debug_type, sizeof(_debug_type), "int4");
		break;
	case GS_SHADER_PARAM_FLOAT:
		snprintf(_debug_type, sizeof(_debug_type), "float");
		break;
	case GS_SHADER_PARAM_VEC2:
		snprintf(_debug_type, sizeof(_debug_type), "float2");
		break;
	case GS_SHADER_PARAM_VEC3:
		snprintf(_debug_type, sizeof(_debug_type), "float3");
		break;
	case GS_SHADER_PARAM_VEC4:
		snprintf(_debug_type, sizeof(_debug_type), "float4");
		break;
	case GS_SHADER_PARAM_MATRIX4X4:
		snprintf(_debug_type, sizeof(_debug_type), "float4x4");
		break;
	case GS_SHADER_PARAM_BOOL:
		snprintf(_debug_type, sizeof(_debug_type), "bool");
		break;
	case GS_SHADER_PARAM_UNKNOWN:
		snprintf(_debug_type, sizeof(_debug_type), "unknown");
		break;
	case GS_SHADER_PARAM_TEXTURE:
		snprintf(_debug_type, sizeof(_debug_type), "texture");
		break;
	}

	char _debug_buf[4096];
	debug_get_default_value(param, _debug_buf, sizeof(_debug_buf));
	if (param->annotations.num > 0) {
		blog(LOG_DEBUG,
		     "%s[%4lld] %.*s '%s' with value %.*s and %lld annotations:",
		     offset, idx, sizeof(_debug_type), _debug_type, param->name,
		     sizeof(_debug_buf), _debug_buf, param->annotations.num);
	} else {
		blog(LOG_DEBUG, "%s[%4lld] %.*s '%s' with value %.*s.", offset,
		     idx, sizeof(_debug_type), _debug_type, param->name,
		     sizeof(_debug_buf), _debug_buf);
	}
}

static void debug_param_annotation(struct gs_effect_param *param,
				   struct ep_param *param_in,
				   unsigned long long idx, const char *offset)
{
	char _debug_buf[4096];
	debug_get_default_value(param, _debug_buf, sizeof(_debug_buf));
	blog(LOG_DEBUG, "%s[%4lld] %s '%s' with value %.*s", offset, idx,
	     param_in->type, param->name, sizeof(_debug_buf), _debug_buf);
}

static void debug_print_string(const char *offset, const char *str)
{
	// Bypass 4096 limit in def_log_handler.
	char const *begin = str;
	unsigned long long line = 1;
	for (char const *here = begin; here[0] != '\0'; here++) {
		char const *str = begin;
		unsigned long long len = here - begin;
		bool is_line = false;

		if (here[0] == '\r') {
			is_line = true;
			if (here[1] == '\n') {
				here += 1;
			}
			begin = here + 1;
		} else if (here[0] == '\n') {
			is_line = true;
			begin = here + 1;
		}

		if (is_line) {
			blog(LOG_DEBUG, "\t\t\t\t[%4lld] %.*s", line, len, str);
			line++;
		}
	}
	if (begin[0] != '\0') {
		// Final line was not written.
		blog(LOG_DEBUG, "\t\t\t\t[%4lld] %*s", line, strlen(begin),
		     begin);
	}
}
#endif

bool ep_parse(struct effect_parser *ep, gs_effect_t *effect,
	      const char *effect_string, const char *file)
{
	bool success;

	const char *graphics_preprocessor = gs_preprocessor_name();

	if (graphics_preprocessor) {
		struct cf_def def;

		cf_def_init(&def);
		def.name.str.array = graphics_preprocessor;
		def.name.str.len = strlen(graphics_preprocessor);

		strref_copy(&def.name.unmerged_str, &def.name.str);
		cf_preprocessor_add_def(&ep->cfp.pp, &def);
	}

	ep->effect = effect;
	if (!cf_parser_parse(&ep->cfp, effect_string, file))
		return false;

	while (ep->cfp.cur_token && ep->cfp.cur_token->type != CFTOKEN_NONE) {
		if (cf_token_is(&ep->cfp, ";") ||
		    is_whitespace(*ep->cfp.cur_token->str.array)) {
			/* do nothing */
			ep->cfp.cur_token++;

		} else if (cf_token_is(&ep->cfp, "struct")) {
			ep_parse_struct(ep);

		} else if (cf_token_is(&ep->cfp, "technique")) {
			ep_parse_technique(ep);

		} else if (cf_token_is(&ep->cfp, "sampler_state")) {
			ep_parse_sampler_state(ep);

		} else if (cf_token_is(&ep->cfp, "{")) {
			/* add error and pass braces */
			cf_adderror(&ep->cfp, "Unexpected code segment",
				    LEX_ERROR, NULL, NULL, NULL);
			cf_pass_pair(&ep->cfp, '{', '}');

		} else {
			/* parameters and functions */
			ep_parse_other(ep);
		}
	}

#if defined(_DEBUG) && defined(_DEBUG_SHADERS)
	blog(LOG_DEBUG,
	     "================================================================================");
	blog(LOG_DEBUG, "Effect Parser reformatted shader '%s' to:", file);
	debug_print_string("\t", ep->cfp.lex.reformatted);
#endif

	success = !error_data_has_errors(&ep->cfp.error_list);
	if (success)
		success = ep_compile(ep);

#if defined(_DEBUG) && defined(_DEBUG_SHADERS)
	blog(LOG_DEBUG,
	     "================================================================================");
#endif

	return success;
}

/* ------------------------------------------------------------------------- */

static inline void ep_write_param(struct dstr *shader, struct ep_param *param,
				  struct darray *used_params)
{
	if (param->written)
		return;

	if (param->is_const) {
		dstr_cat(shader, "const ");
	} else if (param->is_uniform) {
		struct dstr new;
		dstr_init_copy(&new, param->name);
		darray_push_back(sizeof(struct dstr), used_params, &new);

		dstr_cat(shader, "uniform ");
	}

	dstr_cat(shader, param->type);
	dstr_cat(shader, " ");
	dstr_cat(shader, param->name);

	if (param->array_count)
		dstr_catf(shader, "[%u]", param->array_count);

	dstr_cat(shader, ";\n");

	param->written = true;
}

static inline void ep_write_func_param_deps(struct effect_parser *ep,
					    struct dstr *shader,
					    struct ep_func *func,
					    struct darray *used_params)
{
	size_t i;
	for (i = 0; i < func->param_deps.num; i++) {
		const char *name = func->param_deps.array[i];
		struct ep_param *param = ep_getparam(ep, name);
		ep_write_param(shader, param, used_params);
	}

	if (func->param_deps.num)
		dstr_cat(shader, "\n\n");
}

static void ep_write_sampler(struct dstr *shader, struct ep_sampler *sampler)
{
	size_t i;

	if (sampler->written)
		return;

	dstr_cat(shader, "sampler_state ");
	dstr_cat(shader, sampler->name);
	dstr_cat(shader, " {");

	for (i = 0; i < sampler->values.num; i++) {
		dstr_cat(shader, "\n\t");
		dstr_cat(shader, sampler->states.array[i]);
		dstr_cat(shader, " = ");
		dstr_cat(shader, sampler->values.array[i]);
		dstr_cat(shader, ";\n");
	}

	dstr_cat(shader, "\n};\n");
	sampler->written = true;
}

static inline void ep_write_func_sampler_deps(struct effect_parser *ep,
					      struct dstr *shader,
					      struct ep_func *func)
{
	size_t i;
	for (i = 0; i < func->sampler_deps.num; i++) {
		const char *name = func->sampler_deps.array[i];

		struct ep_sampler *sampler = ep_getsampler(ep, name);
		ep_write_sampler(shader, sampler);
		dstr_cat(shader, "\n");
	}
}

static inline void ep_write_var(struct dstr *shader, struct ep_var *var)
{
	if (var->var_type == EP_VAR_INOUT)
		dstr_cat(shader, "inout ");
	else if (var->var_type == EP_VAR_OUT)
		dstr_cat(shader, "out ");
	else if (var->var_type == EP_VAR_UNIFORM)
		dstr_cat(shader, "uniform ");
	// The "in" input modifier is implied by default, so leave it blank
	// in that case.

	dstr_cat(shader, var->type);
	dstr_cat(shader, " ");
	dstr_cat(shader, var->name);

	if (var->mapping) {
		dstr_cat(shader, " : ");
		dstr_cat(shader, var->mapping);
	}
}

static void ep_write_struct(struct dstr *shader, struct ep_struct *st)
{
	size_t i;

	if (st->written)
		return;

	dstr_cat(shader, "struct ");
	dstr_cat(shader, st->name);
	dstr_cat(shader, " {");

	for (i = 0; i < st->vars.num; i++) {
		dstr_cat(shader, "\n\t");
		ep_write_var(shader, st->vars.array + i);
		dstr_cat(shader, ";");
	}

	dstr_cat(shader, "\n};\n");
	st->written = true;
}

static inline void ep_write_func_struct_deps(struct effect_parser *ep,
					     struct dstr *shader,
					     struct ep_func *func)
{
	size_t i;
	for (i = 0; i < func->struct_deps.num; i++) {
		const char *name = func->struct_deps.array[i];
		struct ep_struct *st = ep_getstruct(ep, name);

		if (!st->written) {
			ep_write_struct(shader, st);
			dstr_cat(shader, "\n");
			st->written = true;
		}
	}
}

static void ep_write_func(struct effect_parser *ep, struct dstr *shader,
			  struct ep_func *func, struct darray *used_params);

static inline void ep_write_func_func_deps(struct effect_parser *ep,
					   struct dstr *shader,
					   struct ep_func *func,
					   struct darray *used_params)
{
	size_t i;
	for (i = 0; i < func->func_deps.num; i++) {
		const char *name = func->func_deps.array[i];
		struct ep_func *func_dep = ep_getfunc(ep, name);

		if (!func_dep->written) {
			ep_write_func(ep, shader, func_dep, used_params);
			dstr_cat(shader, "\n\n");
		}
	}
}

static void ep_write_func(struct effect_parser *ep, struct dstr *shader,
			  struct ep_func *func, struct darray *used_params)
{
	size_t i;

	func->written = true;

	ep_write_func_param_deps(ep, shader, func, used_params);
	ep_write_func_sampler_deps(ep, shader, func);
	ep_write_func_struct_deps(ep, shader, func);
	ep_write_func_func_deps(ep, shader, func, used_params);

	/* ------------------------------------ */

	dstr_cat(shader, func->ret_type);
	dstr_cat(shader, " ");
	dstr_cat(shader, func->name);
	dstr_cat(shader, "(");

	for (i = 0; i < func->param_vars.num; i++) {
		struct ep_var *var = func->param_vars.array + i;

		if (i)
			dstr_cat(shader, ", ");
		ep_write_var(shader, var);
	}

	dstr_cat(shader, ")\n");
	dstr_cat_dstr(shader, &func->contents);
	dstr_cat(shader, "\n");
}

/* writes mapped vars used by the call as parameters for main */
static void ep_write_main_params(struct effect_parser *ep, struct dstr *shader,
				 struct dstr *param_str, struct ep_func *func)
{
	size_t i;
	bool empty_params = dstr_is_empty(param_str);

	for (i = 0; i < func->param_vars.num; i++) {
		struct ep_var *var = func->param_vars.array + i;
		struct ep_struct *st = NULL;
		bool mapped = (var->mapping != NULL);

		if (!mapped) {
			st = ep_getstruct(ep, var->type);
			if (st)
				mapped = ep_struct_mapped(st);
		}

		if (mapped) {
			dstr_cat(shader, var->type);
			dstr_cat(shader, " ");
			dstr_cat(shader, var->name);

			if (!st) {
				dstr_cat(shader, " : ");
				dstr_cat(shader, var->mapping);
			}

			if (!dstr_is_empty(param_str))
				dstr_cat(param_str, ", ");
			dstr_cat(param_str, var->name);
		}
	}

	if (!empty_params)
		dstr_cat(param_str, ", ");
}

static void ep_write_main(struct effect_parser *ep, struct dstr *shader,
			  struct ep_func *func, struct dstr *call_str)
{
	struct dstr param_str;
	struct dstr adjusted_call;

	dstr_init(&param_str);
	dstr_init_copy_dstr(&adjusted_call, call_str);

	dstr_cat(shader, "\n");
	dstr_cat(shader, func->ret_type);
	dstr_cat(shader, " main(");

	ep_write_main_params(ep, shader, &param_str, func);

	dstr_cat(shader, ")");
	if (func->mapping) {
		dstr_cat(shader, " : ");
		dstr_cat(shader, func->mapping);
	}

	dstr_cat(shader, "\n{\n\treturn ");
	dstr_cat_dstr(shader, &adjusted_call);
	dstr_cat(shader, "\n}\n");

	dstr_free(&adjusted_call);
	dstr_free(&param_str);
}

static inline void ep_reset_written(struct effect_parser *ep)
{
	size_t i;
	for (i = 0; i < ep->params.num; i++)
		ep->params.array[i].written = false;
	for (i = 0; i < ep->structs.num; i++)
		ep->structs.array[i].written = false;
	for (i = 0; i < ep->funcs.num; i++)
		ep->funcs.array[i].written = false;
	for (i = 0; i < ep->samplers.num; i++)
		ep->samplers.array[i].written = false;
}

static void ep_makeshaderstring(struct effect_parser *ep, struct dstr *shader,
				struct darray *shader_call,
				struct darray *used_params)
{
	struct cf_token *token = shader_call->array;
	struct cf_token *func_name;
	struct ep_func *func;
	struct dstr call_str;

	dstr_init(&call_str);

	if (!token)
		return;

	while (token->type != CFTOKEN_NONE && is_whitespace(*token->str.array))
		token++;

	if (token->type == CFTOKEN_NONE || strref_cmp(&token->str, "NULL") == 0)
		return;

	func_name = token;

	while (token->type != CFTOKEN_NONE) {
		struct ep_param *param = ep_getparam_strref(ep, &token->str);
		if (param)
			ep_write_param(shader, param, used_params);

		dstr_cat_strref(&call_str, &token->str);
		token++;
	}

	func = ep_getfunc_strref(ep, &func_name->str);
	if (!func)
		return;

	ep_write_func(ep, shader, func, used_params);
	ep_write_main(ep, shader, func, &call_str);

	dstr_free(&call_str);

	ep_reset_written(ep);
}

static void ep_compile_annotations(struct darray *ep_annotations,
				   struct darray *gsp_annotations,
				   struct effect_parser *ep)
{
	darray_resize(sizeof(struct gs_effect_param), gsp_annotations,
		      ep_annotations->num);

	size_t i;
	for (i = 0; i < ep_annotations->num; i++) {
		struct gs_effect_param *param =
			((struct gs_effect_param *)gsp_annotations->array) + i;
		struct ep_param *param_in =
			((struct ep_param *)ep_annotations->array) + i;

		param->name = bstrdup(param_in->name);
		param->section = EFFECT_ANNOTATION;
		param->effect = ep->effect;
		da_move(param->default_val, param_in->default_val);

		param->type = get_effect_param_type(param_in->type);

#if defined(_DEBUG) && defined(_DEBUG_SHADERS)
		debug_param(param, param_in, i, "\t\t");
#endif
	}
}

static void
ep_compile_param_annotations(struct ep_param *ep_param_input,
			     struct gs_effect_param *gs_effect_input,
			     struct effect_parser *ep)
{
	ep_compile_annotations(&(ep_param_input->annotations.da),
			       &(gs_effect_input->annotations.da), ep);
}

static void ep_compile_param(struct effect_parser *ep, size_t idx)
{
	struct gs_effect_param *param;
	struct ep_param *param_in;

	param = ep->effect->params.array + idx;
	param_in = ep->params.array + idx;
	param_in->param = param;

	param->name = bstrdup(param_in->name);
	param->section = EFFECT_PARAM;
	param->effect = ep->effect;
	da_move(param->default_val, param_in->default_val);

	param->type = get_effect_param_type(param_in->type);

	if (strcmp(param_in->name, "ViewProj") == 0)
		ep->effect->view_proj = param;
	else if (strcmp(param_in->name, "World") == 0)
		ep->effect->world = param;

#if defined(_DEBUG) && defined(_DEBUG_SHADERS)
	debug_param(param, param_in, idx, "\t");
#endif

	ep_compile_param_annotations(param_in, param, ep);
}

static bool ep_compile_pass_shaderparams(struct effect_parser *ep,
					 struct darray *pass_params,
					 struct darray *used_params,
					 gs_shader_t *shader)
{
	size_t i;
	darray_resize(sizeof(struct pass_shaderparam), pass_params,
		      used_params->num);

	for (i = 0; i < pass_params->num; i++) {
		struct dstr *param_name;
		struct pass_shaderparam *param;

		param_name = darray_item(sizeof(struct dstr), used_params, i);
		param = darray_item(sizeof(struct pass_shaderparam),
				    pass_params, i);

		param->eparam = gs_effect_get_param_by_name(ep->effect,
							    param_name->array);
		param->sparam =
			gs_shader_get_param_by_name(shader, param_name->array);

#if defined(_DEBUG) && defined(_DEBUG_SHADERS)
		debug_param(param->eparam, 0, i, "\t\t\t\t");
#endif

		if (!param->sparam) {
			blog(LOG_ERROR, "Effect shader parameter not found");
			return false;
		}
	}

	return true;
}

static inline bool ep_compile_pass_shader(struct effect_parser *ep,
					  struct gs_effect_technique *tech,
					  struct gs_effect_pass *pass,
					  struct ep_pass *pass_in,
					  size_t pass_idx,
					  enum gs_shader_type type)
{
	struct dstr shader_str;
	struct dstr location;
	struct darray used_params;         /* struct dstr */
	struct darray *pass_params = NULL; /* struct pass_shaderparam */
	gs_shader_t *shader = NULL;
	bool success = true;

	dstr_init(&shader_str);
	darray_init(&used_params);
	dstr_init(&location);

	dstr_copy(&location, ep->cfp.lex.file);
	if (type == GS_SHADER_VERTEX)
		dstr_cat(&location, " (Vertex ");
	else if (type == GS_SHADER_PIXEL)
		dstr_cat(&location, " (Pixel ");
	/*else if (type == SHADER_GEOMETRY)
		dstr_cat(&location, " (Geometry ");*/

	assert(pass_idx <= UINT_MAX);
	dstr_catf(&location, "shader, technique %s, pass %u)", tech->name,
		  (unsigned)pass_idx);

	if (type == GS_SHADER_VERTEX) {
		ep_makeshaderstring(ep, &shader_str,
				    &pass_in->vertex_program.da, &used_params);

		pass->vertshader = gs_vertexshader_create(shader_str.array,
							  location.array, NULL);

		shader = pass->vertshader;
		pass_params = &pass->vertshader_params.da;
	} else if (type == GS_SHADER_PIXEL) {
		ep_makeshaderstring(ep, &shader_str,
				    &pass_in->fragment_program.da,
				    &used_params);

		pass->pixelshader = gs_pixelshader_create(shader_str.array,
							  location.array, NULL);

		shader = pass->pixelshader;
		pass_params = &pass->pixelshader_params.da;
	}

#if defined(_DEBUG) && defined(_DEBUG_SHADERS)
	blog(LOG_DEBUG, "\t\t\t%s Shader:",
	     type == GS_SHADER_VERTEX ? "Vertex" : "Fragment");
	blog(LOG_DEBUG, "\t\t\tCode:");
	debug_print_string("\t\t\t\t\t", shader_str.array);
	blog(LOG_DEBUG, "\t\t\tParameters:");
#endif

	if (shader)
		success = ep_compile_pass_shaderparams(ep, pass_params,
						       &used_params, shader);
	else
		success = false;

	dstr_free(&location);
	dstr_array_free(used_params.array, used_params.num);
	darray_free(&used_params);
	dstr_free(&shader_str);

	return success;
}

static bool ep_compile_pass(struct effect_parser *ep,
			    struct gs_effect_technique *tech,
			    struct ep_technique *tech_in, size_t idx)
{
	struct gs_effect_pass *pass;
	struct ep_pass *pass_in;
	bool success = true;

	pass = tech->passes.array + idx;
	pass_in = tech_in->passes.array + idx;

	pass->name = bstrdup(pass_in->name);
	pass->section = EFFECT_PASS;

#if defined(_DEBUG) && defined(_DEBUG_SHADERS)
	blog(LOG_DEBUG, "\t\t[%4lld] Pass '%s':", idx, pass->name);
#endif

	if (!ep_compile_pass_shader(ep, tech, pass, pass_in, idx,
				    GS_SHADER_VERTEX)) {
		success = false;
		blog(LOG_ERROR, "Pass (%zu) <%s> missing vertex shader!", idx,
		     pass->name ? pass->name : "");
	}
	if (!ep_compile_pass_shader(ep, tech, pass, pass_in, idx,
				    GS_SHADER_PIXEL)) {
		success = false;
		blog(LOG_ERROR, "Pass (%zu) <%s> missing pixel shader!", idx,
		     pass->name ? pass->name : "");
	}
	return success;
}

static inline bool ep_compile_technique(struct effect_parser *ep, size_t idx)
{
	struct gs_effect_technique *tech;
	struct ep_technique *tech_in;
	bool success = true;
	size_t i;

	tech = ep->effect->techniques.array + idx;
	tech_in = ep->techniques.array + idx;

	tech->name = bstrdup(tech_in->name);
	tech->section = EFFECT_TECHNIQUE;
	tech->effect = ep->effect;

	da_resize(tech->passes, tech_in->passes.num);

#if defined(_DEBUG) && defined(_DEBUG_SHADERS)
	blog(LOG_DEBUG, "\t[%4lld] Technique '%s' has %lld passes:", idx,
	     tech->name, tech->passes.num);
#endif

	for (i = 0; i < tech->passes.num; i++) {
		if (!ep_compile_pass(ep, tech, tech_in, i))
			success = false;
	}

	return success;
}

static bool ep_compile(struct effect_parser *ep)
{
	bool success = true;
	size_t i;

	assert(ep->effect);

	da_resize(ep->effect->params, ep->params.num);
	da_resize(ep->effect->techniques, ep->techniques.num);

#if defined(_DEBUG) && defined(_DEBUG_SHADERS)
	blog(LOG_DEBUG, "Shader has %lld parameters:", ep->params.num);
#endif

	for (i = 0; i < ep->params.num; i++)
		ep_compile_param(ep, i);

#if defined(_DEBUG) && defined(_DEBUG_SHADERS)
	blog(LOG_DEBUG, "Shader has %lld techniques:", ep->techniques.num);
#endif

	for (i = 0; i < ep->techniques.num; i++) {
		if (!ep_compile_technique(ep, i))
			success = false;
	}

	return success;
}
