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

#include "effect.h"
#include "graphics-internal.h"
#include "vec2.h"
#include "vec3.h"
#include "vec4.h"

void gs_effect_actually_destroy(gs_effect_t *effect)
{
	effect_free(effect);
	bfree(effect);
}

void gs_effect_destroy(gs_effect_t *effect)
{
	if (effect) {
		if (!effect->cached)
			gs_effect_actually_destroy(effect);
	}
}

gs_technique_t *gs_effect_get_technique(const gs_effect_t *effect,
					const char *name)
{
	if (!effect)
		return NULL;

	for (size_t i = 0; i < effect->techniques.num; i++) {
		struct gs_effect_technique *tech = effect->techniques.array + i;
		if (strcmp(tech->name, name) == 0)
			return tech;
	}

	return NULL;
}

gs_technique_t *gs_effect_get_current_technique(const gs_effect_t *effect)
{
	if (!effect)
		return NULL;

	return effect->cur_technique;
}

bool gs_effect_loop(gs_effect_t *effect, const char *name)
{
	if (!effect) {
		return false;
	}

	if (!effect->looping) {
		gs_technique_t *tech;

		if (!!gs_get_effect()) {
			blog(LOG_WARNING, "gs_effect_loop: An effect is "
					  "already active");
			return false;
		}

		tech = gs_effect_get_technique(effect, name);
		if (!tech) {
			blog(LOG_WARNING,
			     "gs_effect_loop: Technique '%s' "
			     "not found.",
			     name);
			return false;
		}

		gs_technique_begin(tech);

		effect->looping = true;
	} else {
		gs_technique_end_pass(effect->cur_technique);
	}

	if (!gs_technique_begin_pass(effect->cur_technique,
				     effect->loop_pass++)) {
		gs_technique_end(effect->cur_technique);
		effect->looping = false;
		effect->loop_pass = 0;
		return false;
	}

	return true;
}

size_t gs_technique_begin(gs_technique_t *tech)
{
	if (!tech)
		return 0;

	tech->effect->cur_technique = tech;
	tech->effect->graphics->cur_effect = tech->effect;

	return tech->passes.num;
}

void gs_technique_end(gs_technique_t *tech)
{
	if (!tech)
		return;

	struct gs_effect *effect = tech->effect;
	struct gs_effect_param *params = effect->params.array;
	size_t i;

	gs_load_vertexshader(NULL);
	gs_load_pixelshader(NULL);

	tech->effect->cur_technique = NULL;
	tech->effect->graphics->cur_effect = NULL;

	for (i = 0; i < effect->params.num; i++) {
		struct gs_effect_param *param = params + i;

		da_free(param->cur_val);
		param->changed = false;
		if (param->next_sampler)
			param->next_sampler = NULL;
	}
}

static inline void reset_params(struct darray *shaderparams)
{
	struct pass_shaderparam *params = shaderparams->array;
	size_t i;

	for (i = 0; i < shaderparams->num; i++)
		params[i].eparam->changed = false;
}

static void upload_shader_params(struct darray *pass_params, bool changed_only)
{
	struct pass_shaderparam *params = pass_params->array;
	size_t i;

	for (i = 0; i < pass_params->num; i++) {
		struct pass_shaderparam *param = params + i;
		struct gs_effect_param *eparam = param->eparam;
		gs_sparam_t *sparam = param->sparam;

		if (eparam->next_sampler)
			gs_shader_set_next_sampler(sparam,
						   eparam->next_sampler);

		if (changed_only && !eparam->changed)
			continue;

		if (!eparam->cur_val.num) {
			if (eparam->default_val.num)
				da_copy(eparam->cur_val, eparam->default_val);
			else
				continue;
		}

		gs_shader_set_val(sparam, eparam->cur_val.array,
				  eparam->cur_val.num);
	}
}

static inline void upload_parameters(struct gs_effect *effect,
				     bool changed_only)
{
	struct darray *vshader_params, *pshader_params;

	if (!effect->cur_pass)
		return;

	vshader_params = &effect->cur_pass->vertshader_params.da;
	pshader_params = &effect->cur_pass->pixelshader_params.da;

	upload_shader_params(vshader_params, changed_only);
	upload_shader_params(pshader_params, changed_only);
	reset_params(vshader_params);
	reset_params(pshader_params);
}

void gs_effect_update_params(gs_effect_t *effect)
{
	if (effect)
		upload_parameters(effect, true);
}

bool gs_technique_begin_pass(gs_technique_t *tech, size_t idx)
{
	struct gs_effect_pass *passes;
	struct gs_effect_pass *cur_pass;

	if (!tech || idx >= tech->passes.num)
		return false;

	passes = tech->passes.array;
	cur_pass = passes + idx;

	tech->effect->cur_pass = cur_pass;
	gs_load_vertexshader(cur_pass->vertshader);
	gs_load_pixelshader(cur_pass->pixelshader);
	upload_parameters(tech->effect, false);

	return true;
}

bool gs_technique_begin_pass_by_name(gs_technique_t *tech, const char *name)
{
	if (!tech)
		return false;

	for (size_t i = 0; i < tech->passes.num; i++) {
		struct gs_effect_pass *pass = tech->passes.array + i;
		if (strcmp(pass->name, name) == 0) {
			gs_technique_begin_pass(tech, i);
			return true;
		}
	}

	return false;
}

static inline void clear_tex_params(struct darray *in_params)
{
	struct pass_shaderparam *params = in_params->array;

	for (size_t i = 0; i < in_params->num; i++) {
		struct pass_shaderparam *param = params + i;
		struct gs_shader_param_info info;

		gs_shader_get_param_info(param->sparam, &info);
		if (info.type == GS_SHADER_PARAM_TEXTURE)
			gs_shader_set_texture(param->sparam, NULL);
	}
}

void gs_technique_end_pass(gs_technique_t *tech)
{
	if (!tech)
		return;

	struct gs_effect_pass *pass = tech->effect->cur_pass;
	if (!pass)
		return;

	clear_tex_params(&pass->vertshader_params.da);
	clear_tex_params(&pass->pixelshader_params.da);
	tech->effect->cur_pass = NULL;
}

size_t gs_effect_get_num_params(const gs_effect_t *effect)
{
	return effect ? effect->params.num : 0;
}

gs_eparam_t *gs_effect_get_param_by_idx(const gs_effect_t *effect, size_t param)
{
	if (!effect)
		return NULL;

	struct gs_effect_param *params = effect->params.array;
	if (param >= effect->params.num)
		return NULL;

	return params + param;
}

gs_eparam_t *gs_effect_get_param_by_name(const gs_effect_t *effect,
					 const char *name)
{
	if (!effect)
		return NULL;

	struct gs_effect_param *params = effect->params.array;

	for (size_t i = 0; i < effect->params.num; i++) {
		struct gs_effect_param *param = params + i;

		if (strcmp(param->name, name) == 0)
			return param;
	}

	return NULL;
}

size_t gs_param_get_num_annotations(const gs_eparam_t *param)
{
	return param ? param->annotations.num : 0;
}

gs_eparam_t *gs_param_get_annotation_by_idx(const gs_eparam_t *param,
					    size_t annotation)
{
	if (!param)
		return NULL;

	struct gs_effect_param *params = param->annotations.array;
	if (annotation > param->annotations.num)
		return NULL;

	return params + annotation;
}

gs_eparam_t *gs_param_get_annotation_by_name(const gs_eparam_t *param,
					     const char *name)
{
	if (!param)
		return NULL;
	struct gs_effect_param *params = param->annotations.array;

	for (size_t i = 0; i < param->annotations.num; i++) {
		struct gs_effect_param *g_param = params + i;
		if (strcmp(g_param->name, name) == 0)
			return g_param;
	}
	return NULL;
}

gs_epass_t *gs_technique_get_pass_by_idx(const gs_technique_t *technique,
					 size_t pass)
{
	if (!technique)
		return NULL;
	struct gs_effect_pass *passes = technique->passes.array;

	if (pass > technique->passes.num)
		return NULL;

	return passes + pass;
}

gs_epass_t *gs_technique_get_pass_by_name(const gs_technique_t *technique,
					  const char *name)
{
	if (!technique)
		return NULL;
	struct gs_effect_pass *passes = technique->passes.array;

	for (size_t i = 0; i < technique->passes.num; i++) {
		struct gs_effect_pass *g_pass = passes + i;
		if (strcmp(g_pass->name, name) == 0)
			return g_pass;
	}
	return NULL;
}

gs_eparam_t *gs_effect_get_viewproj_matrix(const gs_effect_t *effect)
{
	return effect ? effect->view_proj : NULL;
}

gs_eparam_t *gs_effect_get_world_matrix(const gs_effect_t *effect)
{
	return effect ? effect->world : NULL;
}

void gs_effect_get_param_info(const gs_eparam_t *param,
			      struct gs_effect_param_info *info)
{
	if (!param)
		return;

	info->name = param->name;
	info->type = param->type;
}

static inline void effect_setval_inline(gs_eparam_t *param, const void *data,
					size_t size)
{
	bool size_changed;

	if (!param) {
		blog(LOG_ERROR, "effect_setval_inline: invalid param");
		return;
	}

	if (!data) {
		blog(LOG_ERROR, "effect_setval_inline: invalid data");
		return;
	}

	size_changed = param->cur_val.num != size;

	if (size_changed)
		da_resize(param->cur_val, size);

	if (size_changed || memcmp(param->cur_val.array, data, size) != 0) {
		memcpy(param->cur_val.array, data, size);
		param->changed = true;
	}
}

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
static inline void effect_getval_inline(gs_eparam_t *param, void *data,
					size_t size)
{
	if (!param) {
		blog(LOG_ERROR, "effect_getval_inline: invalid param");
		return;
	}

	if (!data) {
		blog(LOG_ERROR, "effect_getval_inline: invalid data");
		return;
	}

	size_t bytes = min(size, param->cur_val.num);

	memcpy(data, param->cur_val.array, bytes);
}

static inline void effect_getdefaultval_inline(gs_eparam_t *param, void *data,
					       size_t size)
{
	if (!param) {
		blog(LOG_ERROR, "effect_getdefaultval_inline: invalid param");
		return;
	}

	if (!data) {
		blog(LOG_ERROR, "effect_getdefaultval_inline: invalid data");
		return;
	}

	size_t bytes = min(size, param->default_val.num);

	memcpy(data, param->default_val.array, bytes);
}

void gs_effect_set_bool(gs_eparam_t *param, bool val)
{
	int b_val = (int)val;
	effect_setval_inline(param, &b_val, sizeof(int));
}

void gs_effect_set_float(gs_eparam_t *param, float val)
{
	effect_setval_inline(param, &val, sizeof(float));
}

void gs_effect_set_int(gs_eparam_t *param, int val)
{
	effect_setval_inline(param, &val, sizeof(int));
}

void gs_effect_set_matrix4(gs_eparam_t *param, const struct matrix4 *val)
{
	effect_setval_inline(param, val, sizeof(struct matrix4));
}

void gs_effect_set_vec2(gs_eparam_t *param, const struct vec2 *val)
{
	effect_setval_inline(param, val, sizeof(struct vec2));
}

void gs_effect_set_vec3(gs_eparam_t *param, const struct vec3 *val)
{
	effect_setval_inline(param, val, sizeof(float) * 3);
}

void gs_effect_set_vec4(gs_eparam_t *param, const struct vec4 *val)
{
	effect_setval_inline(param, val, sizeof(struct vec4));
}

void gs_effect_set_color(gs_eparam_t *param, uint32_t argb)
{
	struct vec4 v_color;
	vec4_from_bgra(&v_color, argb);
	effect_setval_inline(param, &v_color, sizeof(struct vec4));
}

void gs_effect_set_texture(gs_eparam_t *param, gs_texture_t *val)
{
	struct gs_shader_texture shader_tex;
	shader_tex.tex = val;
	shader_tex.srgb = false;
	effect_setval_inline(param, &shader_tex, sizeof(shader_tex));
}

void gs_effect_set_texture_srgb(gs_eparam_t *param, gs_texture_t *val)
{
	struct gs_shader_texture shader_tex;
	shader_tex.tex = val;
	shader_tex.srgb = true;
	effect_setval_inline(param, &shader_tex, sizeof(shader_tex));
}

void gs_effect_set_val(gs_eparam_t *param, const void *val, size_t size)
{
	effect_setval_inline(param, val, size);
}

void *gs_effect_get_val(gs_eparam_t *param)
{
	if (!param) {
		blog(LOG_ERROR, "gs_effect_get_val: invalid param");
		return NULL;
	}
	size_t size = param->cur_val.num;
	void *data;

	if (size)
		data = (void *)bzalloc(size);
	else
		return NULL;

	effect_getval_inline(param, data, size);

	return data;
}

size_t gs_effect_get_val_size(gs_eparam_t *param)
{
	return param ? param->cur_val.num : 0;
}

void *gs_effect_get_default_val(gs_eparam_t *param)
{
	if (!param) {
		blog(LOG_ERROR, "gs_effect_get_default_val: invalid param");
		return NULL;
	}
	size_t size = param->default_val.num;
	void *data;

	if (size)
		data = (void *)bzalloc(size);
	else
		return NULL;

	effect_getdefaultval_inline(param, data, size);

	return data;
}

size_t gs_effect_get_default_val_size(gs_eparam_t *param)
{
	return param ? param->default_val.num : 0;
}

void gs_effect_set_default(gs_eparam_t *param)
{
	effect_setval_inline(param, param->default_val.array,
			     param->default_val.num);
}

void gs_effect_set_next_sampler(gs_eparam_t *param, gs_samplerstate_t *sampler)
{
	if (!param) {
		blog(LOG_ERROR, "gs_effect_set_next_sampler: invalid param");
		return;
	}

	if (param->type == GS_SHADER_PARAM_TEXTURE)
		param->next_sampler = sampler;
}
