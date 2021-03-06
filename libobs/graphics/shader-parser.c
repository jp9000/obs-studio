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

#include "../util/platform.h"
#include "shader-parser.h"

enum gs_shader_param_type get_shader_param_type(const char *type)
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

enum gs_sample_filter get_sample_filter(const char *filter)
{
	if (astrcmpi(filter, "Anisotropy") == 0)
		return GS_FILTER_ANISOTROPIC;

	else if (astrcmpi(filter, "Point") == 0 ||
		 strcmp(filter, "MIN_MAG_MIP_POINT") == 0)
		return GS_FILTER_POINT;

	else if (astrcmpi(filter, "Linear") == 0 ||
		 strcmp(filter, "MIN_MAG_MIP_LINEAR") == 0)
		return GS_FILTER_LINEAR;

	else if (strcmp(filter, "MIN_MAG_POINT_MIP_LINEAR") == 0)
		return GS_FILTER_MIN_MAG_POINT_MIP_LINEAR;

	else if (strcmp(filter, "MIN_POINT_MAG_LINEAR_MIP_POINT") == 0)
		return GS_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;

	else if (strcmp(filter, "MIN_POINT_MAG_MIP_LINEAR") == 0)
		return GS_FILTER_MIN_POINT_MAG_MIP_LINEAR;

	else if (strcmp(filter, "MIN_LINEAR_MAG_MIP_POINT") == 0)
		return GS_FILTER_MIN_LINEAR_MAG_MIP_POINT;

	else if (strcmp(filter, "MIN_LINEAR_MAG_POINT_MIP_LINEAR") == 0)
		return GS_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;

	else if (strcmp(filter, "MIN_MAG_LINEAR_MIP_POINT") == 0)
		return GS_FILTER_MIN_MAG_LINEAR_MIP_POINT;

	return GS_FILTER_LINEAR;
}

extern enum gs_address_mode get_address_mode(const char *mode)
{
	if (astrcmpi(mode, "Wrap") == 0 || astrcmpi(mode, "Repeat") == 0)
		return GS_ADDRESS_WRAP;
	else if (astrcmpi(mode, "Clamp") == 0 || astrcmpi(mode, "None") == 0)
		return GS_ADDRESS_CLAMP;
	else if (astrcmpi(mode, "Mirror") == 0)
		return GS_ADDRESS_MIRROR;
	else if (astrcmpi(mode, "Border") == 0)
		return GS_ADDRESS_BORDER;
	else if (astrcmpi(mode, "MirrorOnce") == 0)
		return GS_ADDRESS_MIRRORONCE;

	return GS_ADDRESS_CLAMP;
}

void shader_sampler_convert(struct shader_sampler *ss,
			    struct gs_sampler_info *info)
{
	size_t i;
	memset(info, 0, sizeof(struct gs_sampler_info));

	info->max_anisotropy = 1;

	for (i = 0; i < ss->states.num; i++) {
		const char *state = ss->states.array[i];
		const char *value = ss->values.array[i];

		if (astrcmpi(state, "Filter") == 0)
			info->filter = get_sample_filter(value);
		else if (astrcmpi(state, "AddressU") == 0)
			info->address_u = get_address_mode(value);
		else if (astrcmpi(state, "AddressV") == 0)
			info->address_v = get_address_mode(value);
		else if (astrcmpi(state, "AddressW") == 0)
			info->address_w = get_address_mode(value);
		else if (astrcmpi(state, "MaxAnisotropy") == 0)
			info->max_anisotropy = (int)strtol(value, NULL, 10);
		else if (astrcmpi(state, "BorderColor") == 0)
			info->border_color = strtol(value + 1, NULL, 16);
	}
}

/* ------------------------------------------------------------------------- */

static int sp_parse_sampler_state_item(struct shader_parser *sp,
				       struct shader_sampler *ss)
{
	int ret;
	char *state = NULL, *value = NULL;

	ret = cf_next_name(&sp->cfp, &state, "state name", ";");
	if (ret != PARSE_SUCCESS)
		goto fail;

	ret = cf_next_token_should_be(&sp->cfp, "=", ";", NULL);
	if (ret != PARSE_SUCCESS)
		goto fail;

	ret = cf_next_token_copy(&sp->cfp, &value);
	if (ret != PARSE_SUCCESS)
		goto fail;

	ret = cf_next_token_should_be(&sp->cfp, ";", ";", NULL);
	if (ret != PARSE_SUCCESS)
		goto fail;

	da_push_back(ss->states, &state);
	da_push_back(ss->values, &value);
	return ret;

fail:
	bfree(state);
	bfree(value);
	return ret;
}

static void sp_parse_sampler_state(struct shader_parser *sp)
{
	struct shader_sampler ss;
	struct cf_token peek;
	shader_sampler_init(&ss);

	if (cf_next_name(&sp->cfp, &ss.name, "name", ";") != PARSE_SUCCESS)
		goto error;
	if (cf_next_token_should_be(&sp->cfp, "{", ";", NULL) != PARSE_SUCCESS)
		goto error;

	if (!cf_peek_valid_token(&sp->cfp, &peek))
		goto error;

	while (strref_cmp(&peek.str, "}") != 0) {
		int ret = sp_parse_sampler_state_item(sp, &ss);
		if (ret == PARSE_EOF)
			goto error;

		if (!cf_peek_valid_token(&sp->cfp, &peek))
			goto error;
	}

	if (cf_next_token_should_be(&sp->cfp, "}", ";", NULL) != PARSE_SUCCESS)
		goto error;
	if (cf_next_token_should_be(&sp->cfp, ";", NULL, NULL) != PARSE_SUCCESS)
		goto error;

	da_push_back(sp->samplers, &ss);
	return;

error:
	shader_sampler_free(&ss);
}

static inline int sp_parse_struct_var(struct shader_parser *sp,
				      struct shader_var *var)
{
	int code;

	/* -------------------------------------- */
	/* variable type */

	if (!cf_next_valid_token(&sp->cfp))
		return PARSE_EOF;

	if (cf_token_is(&sp->cfp, ";"))
		return PARSE_CONTINUE;
	if (cf_token_is(&sp->cfp, "}"))
		return PARSE_BREAK;

	code = cf_token_is_type(&sp->cfp, CFTOKEN_NAME, "type name", ";");
	if (code != PARSE_SUCCESS)
		return code;

	cf_copy_token(&sp->cfp, &var->type);

	/* -------------------------------------- */
	/* variable name */

	if (!cf_next_valid_token(&sp->cfp))
		return PARSE_EOF;

	if (cf_token_is(&sp->cfp, ";"))
		return PARSE_UNEXPECTED_CONTINUE;
	if (cf_token_is(&sp->cfp, "}"))
		return PARSE_UNEXPECTED_BREAK;

	code = cf_token_is_type(&sp->cfp, CFTOKEN_NAME, "variable name", ";");
	if (code != PARSE_SUCCESS)
		return code;

	cf_copy_token(&sp->cfp, &var->name);

	/* -------------------------------------- */
	/* variable mapping if any (POSITION, TEXCOORD, etc) */

	if (!cf_next_valid_token(&sp->cfp))
		return PARSE_EOF;

	if (cf_token_is(&sp->cfp, ":")) {
		if (!cf_next_valid_token(&sp->cfp))
			return PARSE_EOF;

		if (cf_token_is(&sp->cfp, ";"))
			return PARSE_UNEXPECTED_CONTINUE;
		if (cf_token_is(&sp->cfp, "}"))
			return PARSE_UNEXPECTED_BREAK;

		code = cf_token_is_type(&sp->cfp, CFTOKEN_NAME, "mapping name",
					";");
		if (code != PARSE_SUCCESS)
			return code;

		cf_copy_token(&sp->cfp, &var->mapping);

		if (!cf_next_valid_token(&sp->cfp))
			return PARSE_EOF;
	}

	/* -------------------------------------- */

	if (!cf_token_is(&sp->cfp, ";")) {
		if (!cf_go_to_valid_token(&sp->cfp, ";", "}"))
			return PARSE_EOF;
		return PARSE_CONTINUE;
	}

	return PARSE_SUCCESS;
}

static void sp_parse_struct(struct shader_parser *sp)
{
	struct shader_struct ss;
	shader_struct_init(&ss);

	if (cf_next_name(&sp->cfp, &ss.name, "name", ";") != PARSE_SUCCESS)
		goto error;
	if (cf_next_token_should_be(&sp->cfp, "{", ";", NULL) != PARSE_SUCCESS)
		goto error;

	/* get structure variables */
	while (true) {
		bool do_break = false;
		struct shader_var var;

		shader_var_init(&var);

		switch (sp_parse_struct_var(sp, &var)) {

		case PARSE_UNEXPECTED_CONTINUE:
			cf_adderror_syntax_error(&sp->cfp);
			/* Falls through. */
		case PARSE_CONTINUE:
			shader_var_free(&var);
			continue;

		case PARSE_UNEXPECTED_BREAK:
			cf_adderror_syntax_error(&sp->cfp);
			/* Falls through. */
		case PARSE_BREAK:
			shader_var_free(&var);
			do_break = true;
			break;

		case PARSE_EOF:
			shader_var_free(&var);
			goto error;
		}

		if (do_break)
			break;

		da_push_back(ss.vars, &var);
	}

	if (cf_next_token_should_be(&sp->cfp, ";", NULL, NULL) != PARSE_SUCCESS)
		goto error;

	da_push_back(sp->structs, &ss);
	return;

error:
	shader_struct_free(&ss);
}

static inline int sp_check_for_keyword(struct shader_parser *sp,
				       const char *keyword, bool *val)
{
	bool new_val = cf_token_is(&sp->cfp, keyword);
	if (new_val) {
		if (!cf_next_valid_token(&sp->cfp))
			return PARSE_EOF;

		if (new_val && *val)
			cf_adderror(&sp->cfp, "'$1' keyword already specified",
				    LEX_WARNING, keyword, NULL, NULL);
		*val = new_val;

		return PARSE_CONTINUE;
	}

	return PARSE_SUCCESS;
}

static inline int sp_parse_func_param(struct shader_parser *sp,
				      struct shader_var *var)
{
	int code;
	bool var_type_keyword = false;

	if (!cf_next_valid_token(&sp->cfp))
		return PARSE_EOF;

	code = sp_check_for_keyword(sp, "in", &var_type_keyword);
	if (code == PARSE_EOF)
		return PARSE_EOF;
	else if (var_type_keyword)
		var->var_type = SHADER_VAR_IN;

	if (!var_type_keyword) {
		code = sp_check_for_keyword(sp, "inout", &var_type_keyword);
		if (code == PARSE_EOF)
			return PARSE_EOF;
		else if (var_type_keyword)
			var->var_type = SHADER_VAR_INOUT;
	}

	if (!var_type_keyword) {
		code = sp_check_for_keyword(sp, "out", &var_type_keyword);
		if (code == PARSE_EOF)
			return PARSE_EOF;
		else if (var_type_keyword)
			var->var_type = SHADER_VAR_OUT;
	}

	if (!var_type_keyword) {
		code = sp_check_for_keyword(sp, "uniform", &var_type_keyword);
		if (code == PARSE_EOF)
			return PARSE_EOF;
		else if (var_type_keyword)
			var->var_type = SHADER_VAR_UNIFORM;
	}

	code = cf_get_name(&sp->cfp, &var->type, "type", ")");
	if (code != PARSE_SUCCESS)
		return code;

	code = cf_next_name(&sp->cfp, &var->name, "name", ")");
	if (code != PARSE_SUCCESS)
		return code;

	if (!cf_next_valid_token(&sp->cfp))
		return PARSE_EOF;

	if (cf_token_is(&sp->cfp, ":")) {
		code = cf_next_name(&sp->cfp, &var->mapping,
				    "mapping specifier", ")");
		if (code != PARSE_SUCCESS)
			return code;

		if (!cf_next_valid_token(&sp->cfp))
			return PARSE_EOF;
	}

	return PARSE_SUCCESS;
}

static bool sp_parse_func_params(struct shader_parser *sp,
				 struct shader_func *func)
{
	struct cf_token peek;
	int code;

	cf_token_clear(&peek);

	if (!cf_peek_valid_token(&sp->cfp, &peek))
		return false;

	if (*peek.str.array == ')') {
		cf_next_token(&sp->cfp);
		goto exit;
	}

	do {
		struct shader_var var;
		shader_var_init(&var);

		if (!cf_token_is(&sp->cfp, "(") && !cf_token_is(&sp->cfp, ","))
			cf_adderror_syntax_error(&sp->cfp);

		code = sp_parse_func_param(sp, &var);
		if (code != PARSE_SUCCESS) {
			shader_var_free(&var);

			if (code == PARSE_CONTINUE)
				goto exit;
			else if (code == PARSE_EOF)
				return false;
		}

		da_push_back(func->params, &var);
	} while (!cf_token_is(&sp->cfp, ")"));

exit:
	return true;
}

static void sp_parse_function(struct shader_parser *sp, char *type, char *name)
{
	struct shader_func func;

	shader_func_init(&func, type, name);
	if (!sp_parse_func_params(sp, &func))
		goto error;

	if (!cf_next_valid_token(&sp->cfp))
		goto error;

	/* if function is mapped to something, for example COLOR */
	if (cf_token_is(&sp->cfp, ":")) {
		char *mapping = NULL;
		int errorcode =
			cf_next_name(&sp->cfp, &mapping, "mapping", "{");
		if (errorcode != PARSE_SUCCESS)
			goto error;

		func.mapping = mapping;

		if (!cf_next_valid_token(&sp->cfp))
			goto error;
	}

	if (!cf_token_is(&sp->cfp, "{")) {
		cf_adderror_expecting(&sp->cfp, "{");
		goto error;
	}

	func.start = sp->cfp.cur_token;
	if (!cf_pass_pair(&sp->cfp, '{', '}'))
		goto error;

	/* it is established that the current token is '}' if we reach this */
	cf_next_token(&sp->cfp);

	func.end = sp->cfp.cur_token;
	da_push_back(sp->funcs, &func);
	return;

error:
	shader_func_free(&func);
}

/* parses "array[count]" */
static bool sp_parse_param_array(struct shader_parser *sp,
				 struct shader_var *param)
{
	if (!cf_next_valid_token(&sp->cfp))
		return false;

	if (sp->cfp.cur_token->type != CFTOKEN_NUM ||
	    !valid_int_str(sp->cfp.cur_token->str.array,
			   sp->cfp.cur_token->str.len))
		return false;

	param->array_count =
		(int)strtol(sp->cfp.cur_token->str.array, NULL, 10);

	if (cf_next_token_should_be(&sp->cfp, "]", ";", NULL) == PARSE_EOF)
		return false;

	if (!cf_next_valid_token(&sp->cfp))
		return false;

	return true;
}

static inline int sp_parse_param_assign_intfloat(struct shader_parser *sp,
						 struct shader_var *param,
						 bool is_float)
{
	int code;
	bool is_negative = false;

	if (!cf_next_valid_token(&sp->cfp))
		return PARSE_EOF;

	if (cf_token_is(&sp->cfp, "-")) {
		is_negative = true;

		if (!cf_next_token(&sp->cfp))
			return PARSE_EOF;
	}

	code = cf_token_is_type(&sp->cfp, CFTOKEN_NUM, "numeric value", ";");
	if (code != PARSE_SUCCESS)
		return code;

	if (is_float) {
		float f = (float)os_strtod(sp->cfp.cur_token->str.array);
		if (is_negative)
			f = -f;
		da_push_back_array(param->default_val, &f, sizeof(float));
	} else {
		long l = strtol(sp->cfp.cur_token->str.array, NULL, 10);
		if (is_negative)
			l = -l;
		da_push_back_array(param->default_val, &l, sizeof(long));
	}

	return PARSE_SUCCESS;
}

/*
 * parses assignment for float1, float2, float3, float4, and any combination
 * for float3x3, float4x4, etc
 */
static inline int sp_parse_param_assign_float_array(struct shader_parser *sp,
						    struct shader_var *param)
{
	const char *float_type = param->type + 5;
	int float_count = 0, code, i;

	/* -------------------------------------------- */

	if (float_type[0] < '1' || float_type[0] > '4')
		cf_adderror(&sp->cfp, "Invalid row count", LEX_ERROR, NULL,
			    NULL, NULL);

	float_count = float_type[0] - '0';

	if (float_type[1] == 'x') {
		if (float_type[2] < '1' || float_type[2] > '4')
			cf_adderror(&sp->cfp, "Invalid column count", LEX_ERROR,
				    NULL, NULL, NULL);

		float_count *= float_type[2] - '0';
	}

	/* -------------------------------------------- */

	code = cf_next_token_should_be(&sp->cfp, "{", ";", NULL);
	if (code != PARSE_SUCCESS)
		return code;

	for (i = 0; i < float_count; i++) {
		char *next = ((i + 1) < float_count) ? "," : "}";

		code = sp_parse_param_assign_intfloat(sp, param, true);
		if (code != PARSE_SUCCESS)
			return code;

		code = cf_next_token_should_be(&sp->cfp, next, ";", NULL);
		if (code != PARSE_SUCCESS)
			return code;
	}

	return PARSE_SUCCESS;
}

static int sp_parse_param_assignment_val(struct shader_parser *sp,
					 struct shader_var *param)
{
	if (strcmp(param->type, "int") == 0)
		return sp_parse_param_assign_intfloat(sp, param, false);
	else if (strcmp(param->type, "float") == 0)
		return sp_parse_param_assign_intfloat(sp, param, true);
	else if (astrcmp_n(param->type, "float", 5) == 0)
		return sp_parse_param_assign_float_array(sp, param);

	cf_adderror(&sp->cfp, "Invalid type '$1' used for assignment",
		    LEX_ERROR, param->type, NULL, NULL);

	return PARSE_CONTINUE;
}

static inline bool sp_parse_param_assign(struct shader_parser *sp,
					 struct shader_var *param)
{
	if (sp_parse_param_assignment_val(sp, param) != PARSE_SUCCESS)
		return false;

	if (!cf_next_valid_token(&sp->cfp))
		return false;

	return true;
}

static void sp_parse_param(struct shader_parser *sp, char *type, char *name,
			   bool is_const, bool is_uniform)
{
	struct shader_var param;
	shader_var_init_param(&param, type, name, is_uniform, is_const);

	if (cf_token_is(&sp->cfp, ";"))
		goto complete;
	if (cf_token_is(&sp->cfp, "[") && !sp_parse_param_array(sp, &param))
		goto error;
	if (cf_token_is(&sp->cfp, "=") && !sp_parse_param_assign(sp, &param))
		goto error;
	if (!cf_token_is(&sp->cfp, ";"))
		goto error;

complete:
	da_push_back(sp->params, &param);
	return;

error:
	shader_var_free(&param);
}

static bool sp_get_var_specifiers(struct shader_parser *sp, bool *is_const,
				  bool *is_uniform)
{
	while (true) {
		int code = sp_check_for_keyword(sp, "const", is_const);
		if (code == PARSE_EOF)
			return false;
		else if (code == PARSE_CONTINUE)
			continue;

		code = sp_check_for_keyword(sp, "uniform", is_uniform);
		if (code == PARSE_EOF)
			return false;
		else if (code == PARSE_CONTINUE)
			continue;

		break;
	}

	return true;
}

static inline void report_invalid_func_keyword(struct shader_parser *sp,
					       const char *name, bool val)
{
	if (val)
		cf_adderror(&sp->cfp,
			    "'$1' keyword cannot be used with a "
			    "function",
			    LEX_ERROR, name, NULL, NULL);
}

static void sp_parse_other(struct shader_parser *sp)
{
	bool is_const = false, is_uniform = false;
	char *type = NULL, *name = NULL;

	if (!sp_get_var_specifiers(sp, &is_const, &is_uniform))
		goto error;

	if (cf_get_name(&sp->cfp, &type, "type", ";") != PARSE_SUCCESS)
		goto error;
	if (cf_next_name(&sp->cfp, &name, "name", ";") != PARSE_SUCCESS)
		goto error;

	if (!cf_next_valid_token(&sp->cfp))
		goto error;

	if (cf_token_is(&sp->cfp, "(")) {
		report_invalid_func_keyword(sp, "const", is_const);
		report_invalid_func_keyword(sp, "uniform", is_uniform);

		sp_parse_function(sp, type, name);
		return;
	} else {
		sp_parse_param(sp, type, name, is_const, is_uniform);
		return;
	}

error:
	bfree(type);
	bfree(name);
}

bool shader_parse(struct shader_parser *sp, const char *shader,
		  const char *file)
{
	if (!cf_parser_parse(&sp->cfp, shader, file))
		return false;

	while (sp->cfp.cur_token && sp->cfp.cur_token->type != CFTOKEN_NONE) {
		if (cf_token_is(&sp->cfp, ";") ||
		    is_whitespace(*sp->cfp.cur_token->str.array)) {
			sp->cfp.cur_token++;

		} else if (cf_token_is(&sp->cfp, "struct")) {
			sp_parse_struct(sp);

		} else if (cf_token_is(&sp->cfp, "sampler_state")) {
			sp_parse_sampler_state(sp);

		} else if (cf_token_is(&sp->cfp, "{")) {
			cf_adderror(&sp->cfp, "Unexpected code segment",
				    LEX_ERROR, NULL, NULL, NULL);
			cf_pass_pair(&sp->cfp, '{', '}');

		} else {
			/* parameters and functions */
			sp_parse_other(sp);
		}
	}

	return !error_data_has_errors(&sp->cfp.error_list);
}
