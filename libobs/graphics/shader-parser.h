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

#pragma once

#include "../util/cf-parser.h"
#include "graphics.h"

#ifdef __cplusplus
extern "C" {
#endif

EXPORT enum gs_shader_param_type get_shader_param_type(const char *type);
EXPORT enum gs_sample_filter get_sample_filter(const char *filter);
EXPORT enum gs_address_mode get_address_mode(const char *address_mode);

/*
 * Shader Parser
 *
 *   Parses a shader and extracts data such as shader constants, samplers,
 * and vertex input information.  Also allows the reformatting of shaders for
 * different libraries.  This is usually used only by graphics libraries,
 */

enum shader_var_type {
	SHADER_VAR_NONE,
	SHADER_VAR_IN = SHADER_VAR_NONE,
	SHADER_VAR_INOUT,
	SHADER_VAR_OUT,
	SHADER_VAR_UNIFORM,
	SHADER_VAR_CONST
};

struct shader_var {
	char *type;
	char *name;
	char *mapping;
	enum shader_var_type var_type;
	int array_count;
	size_t gl_sampler_id; /* optional: used/parsed by GL */

	DARRAY(uint8_t) default_val;
};

static inline void shader_var_init(struct shader_var *sv)
{
	memset(sv, 0, sizeof(struct shader_var));
}

static inline void shader_var_init_param(struct shader_var *sv, char *type,
					 char *name, bool is_uniform,
					 bool is_const)
{
	if (is_uniform)
		sv->var_type = SHADER_VAR_UNIFORM;
	else if (is_const)
		sv->var_type = SHADER_VAR_CONST;
	else
		sv->var_type = SHADER_VAR_NONE;

	sv->type = type;
	sv->name = name;
	sv->mapping = NULL;
	sv->array_count = 0;
	sv->gl_sampler_id = (size_t)-1;
	da_init(sv->default_val);
}

static inline void shader_var_free(struct shader_var *sv)
{
	bfree(sv->type);
	bfree(sv->name);
	bfree(sv->mapping);
	da_free(sv->default_val);
}

/* ------------------------------------------------------------------------- */

struct shader_sampler {
	char *name;
	DARRAY(char *) states;
	DARRAY(char *) values;
};

static inline void shader_sampler_init(struct shader_sampler *ss)
{
	memset(ss, 0, sizeof(struct shader_sampler));
}

static inline void shader_sampler_free(struct shader_sampler *ss)
{
	size_t i;
	for (i = 0; i < ss->states.num; i++)
		bfree(ss->states.array[i]);
	for (i = 0; i < ss->values.num; i++)
		bfree(ss->values.array[i]);

	bfree(ss->name);
	da_free(ss->states);
	da_free(ss->values);
}

EXPORT void shader_sampler_convert(struct shader_sampler *ss,
				   struct gs_sampler_info *info);

/* ------------------------------------------------------------------------- */

struct shader_struct {
	char *name;
	DARRAY(struct shader_var) vars;
};

static inline void shader_struct_init(struct shader_struct *ss)
{
	memset(ss, 0, sizeof(struct shader_struct));
}

static inline void shader_struct_free(struct shader_struct *ss)
{
	size_t i;

	for (i = 0; i < ss->vars.num; i++)
		shader_var_free(ss->vars.array + i);

	bfree(ss->name);
	da_free(ss->vars);
}

/* ------------------------------------------------------------------------- */

struct shader_func {
	char *name;
	char *return_type;
	char *mapping;
	DARRAY(struct shader_var) params;

	struct cf_token *start, *end;
};

static inline void shader_func_init(struct shader_func *sf, char *return_type,
				    char *name)
{
	da_init(sf->params);

	sf->return_type = return_type;
	sf->mapping = NULL;
	sf->name = name;
	sf->start = NULL;
	sf->end = NULL;
}

static inline void shader_func_free(struct shader_func *sf)
{
	size_t i;

	for (i = 0; i < sf->params.num; i++)
		shader_var_free(sf->params.array + i);

	bfree(sf->name);
	bfree(sf->return_type);
	bfree(sf->mapping);
	da_free(sf->params);
}

/* ------------------------------------------------------------------------- */

struct shader_parser {
	struct cf_parser cfp;

	DARRAY(struct shader_var) params;
	DARRAY(struct shader_struct) structs;
	DARRAY(struct shader_sampler) samplers;
	DARRAY(struct shader_func) funcs;
};

static inline void shader_parser_init(struct shader_parser *sp)
{
	cf_parser_init(&sp->cfp);

	da_init(sp->params);
	da_init(sp->structs);
	da_init(sp->samplers);
	da_init(sp->funcs);
}

static inline void shader_parser_free(struct shader_parser *sp)
{
	size_t i;

	for (i = 0; i < sp->params.num; i++)
		shader_var_free(sp->params.array + i);
	for (i = 0; i < sp->structs.num; i++)
		shader_struct_free(sp->structs.array + i);
	for (i = 0; i < sp->samplers.num; i++)
		shader_sampler_free(sp->samplers.array + i);
	for (i = 0; i < sp->funcs.num; i++)
		shader_func_free(sp->funcs.array + i);

	cf_parser_free(&sp->cfp);
	da_free(sp->params);
	da_free(sp->structs);
	da_free(sp->samplers);
	da_free(sp->funcs);
}

EXPORT bool shader_parse(struct shader_parser *sp, const char *shader,
			 const char *file);

static inline char *shader_parser_geterrors(struct shader_parser *sp)
{
	return error_data_buildstring(&sp->cfp.error_list);
}

static inline struct shader_var *
shader_parser_getparam(struct shader_parser *sp, const char *param_name)
{
	size_t i;
	for (i = 0; i < sp->params.num; i++) {
		struct shader_var *param = sp->params.array + i;
		if (strcmp(param->name, param_name) == 0)
			return param;
	}

	return NULL;
}

static inline struct shader_struct *
shader_parser_getstruct(struct shader_parser *sp, const char *struct_name)
{
	size_t i;
	for (i = 0; i < sp->structs.num; i++) {
		struct shader_struct *st = sp->structs.array + i;
		if (strcmp(st->name, struct_name) == 0)
			return st;
	}

	return NULL;
}

static inline struct shader_sampler *
shader_parser_getsampler(struct shader_parser *sp, const char *sampler_name)
{
	size_t i;
	for (i = 0; i < sp->samplers.num; i++) {
		struct shader_sampler *sampler = sp->samplers.array + i;
		if (strcmp(sampler->name, sampler_name) == 0)
			return sampler;
	}

	return NULL;
}

static inline struct shader_func *
shader_parser_getfunc(struct shader_parser *sp, const char *func_name)
{
	size_t i;
	for (i = 0; i < sp->funcs.num; i++) {
		struct shader_func *func = sp->funcs.array + i;
		if (strcmp(func->name, func_name) == 0)
			return func;
	}

	return NULL;
}

#ifdef __cplusplus
}
#endif
