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

#include "../util/base.h"
#include "../util/bmem.h"
#include "../util/platform.h"
#include "graphics-internal.h"
#include "vec2.h"
#include "vec3.h"
#include "quat.h"
#include "axisang.h"
#include "effect-parser.h"
#include "effect.h"

#ifdef _MSC_VER
#undef near
#undef far
#endif

static THREAD_LOCAL graphics_t *thread_graphics = NULL;

static inline bool gs_obj_valid(const void *obj, const char *f,
				const char *name)
{
	if (!obj) {
		blog(LOG_DEBUG, "%s: Null '%s' parameter", f, name);
		return false;
	}

	return true;
}

static inline bool gs_valid(const char *f)
{
	if (!thread_graphics) {
		blog(LOG_DEBUG, "%s: called while not in a graphics context",
		     f);
		return false;
	}

	return true;
}

#define ptr_valid(ptr, func) gs_obj_valid(ptr, func, #ptr)

#define gs_valid_p(func, param1) (gs_valid(func) && ptr_valid(param1, func))

#define gs_valid_p2(func, param1, param2) \
	(gs_valid(func) && ptr_valid(param1, func) && ptr_valid(param2, func))

#define gs_valid_p3(func, param1, param2, param3)     \
	(gs_valid(func) && ptr_valid(param1, func) && \
	 ptr_valid(param2, func) && ptr_valid(param3, func))

#define IMMEDIATE_COUNT 512

void gs_enum_adapters(bool (*callback)(void *param, const char *name,
				       uint32_t id),
		      void *param)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_enum_adapters", callback))
		return;

	if (graphics->exports.device_enum_adapters) {
		if (graphics->exports.device_enum_adapters(callback, param)) {
			return;
		}
	}

	/* If the subsystem does not currently support device enumeration of
	 * adapters or fails to enumerate adapters, just set it to one adapter
	 * named "Default" */
	callback(param, "Default", 0);
}

extern void gs_init_image_deps(void);
extern void gs_free_image_deps(void);

bool load_graphics_imports(struct gs_exports *exports, void *module,
			   const char *module_name);

static bool graphics_init_immediate_vb(struct graphics_subsystem *graphics)
{
	struct gs_vb_data *vbd;

	vbd = gs_vbdata_create();
	vbd->num = IMMEDIATE_COUNT;
	vbd->points = bmalloc(sizeof(struct vec3) * IMMEDIATE_COUNT);
	vbd->normals = bmalloc(sizeof(struct vec3) * IMMEDIATE_COUNT);
	vbd->colors = bmalloc(sizeof(uint32_t) * IMMEDIATE_COUNT);
	vbd->num_tex = 1;
	vbd->tvarray = bmalloc(sizeof(struct gs_tvertarray));
	vbd->tvarray[0].width = 2;
	vbd->tvarray[0].array = bmalloc(sizeof(struct vec2) * IMMEDIATE_COUNT);

	graphics->immediate_vertbuffer =
		graphics->exports.device_vertexbuffer_create(graphics->device,
							     vbd, GS_DYNAMIC);
	if (!graphics->immediate_vertbuffer)
		return false;

	return true;
}

static bool graphics_init_sprite_vb(struct graphics_subsystem *graphics)
{
	struct gs_vb_data *vbd;

	vbd = gs_vbdata_create();
	vbd->num = 4;
	vbd->points = bmalloc(sizeof(struct vec3) * 4);
	vbd->num_tex = 1;
	vbd->tvarray = bmalloc(sizeof(struct gs_tvertarray));
	vbd->tvarray[0].width = 2;
	vbd->tvarray[0].array = bmalloc(sizeof(struct vec2) * 4);

	memset(vbd->points, 0, sizeof(struct vec3) * 4);
	memset(vbd->tvarray[0].array, 0, sizeof(struct vec2) * 4);

	graphics->sprite_buffer = graphics->exports.device_vertexbuffer_create(
		graphics->device, vbd, GS_DYNAMIC);
	if (!graphics->sprite_buffer)
		return false;

	return true;
}

static bool graphics_init(struct graphics_subsystem *graphics)
{
	struct matrix4 top_mat;

	matrix4_identity(&top_mat);
	da_push_back(graphics->matrix_stack, &top_mat);

	graphics->exports.device_enter_context(graphics->device);

	if (!graphics_init_immediate_vb(graphics))
		return false;
	if (!graphics_init_sprite_vb(graphics))
		return false;
	if (pthread_mutex_init(&graphics->mutex, NULL) != 0)
		return false;
	if (pthread_mutex_init(&graphics->effect_mutex, NULL) != 0)
		return false;

	graphics->exports.device_blend_function_separate(
		graphics->device, GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA,
		GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	graphics->cur_blend_state.enabled = true;
	graphics->cur_blend_state.src_c = GS_BLEND_SRCALPHA;
	graphics->cur_blend_state.dest_c = GS_BLEND_INVSRCALPHA;
	graphics->cur_blend_state.src_a = GS_BLEND_ONE;
	graphics->cur_blend_state.dest_a = GS_BLEND_INVSRCALPHA;

	graphics->exports.device_leave_context(graphics->device);

	gs_init_image_deps();
	return true;
}

int gs_create(graphics_t **pgraphics, const char *module, uint32_t adapter)
{
	int errcode = GS_ERROR_FAIL;

	graphics_t *graphics = bzalloc(sizeof(struct graphics_subsystem));
	pthread_mutex_init_value(&graphics->mutex);
	pthread_mutex_init_value(&graphics->effect_mutex);

	graphics->module = os_dlopen(module);
	if (!graphics->module) {
		errcode = GS_ERROR_MODULE_NOT_FOUND;
		goto error;
	}

	if (!load_graphics_imports(&graphics->exports, graphics->module,
				   module))
		goto error;

	errcode = graphics->exports.device_create(&graphics->device, adapter);
	if (errcode != GS_SUCCESS)
		goto error;

	if (!graphics_init(graphics)) {
		errcode = GS_ERROR_FAIL;
		goto error;
	}

	*pgraphics = graphics;
	return errcode;

error:
	gs_destroy(graphics);
	return errcode;
}

extern void gs_effect_actually_destroy(gs_effect_t *effect);

void gs_destroy(graphics_t *graphics)
{
	if (!ptr_valid(graphics, "gs_destroy"))
		return;

	while (thread_graphics)
		gs_leave_context();

	if (graphics->device) {
		struct gs_effect *effect = graphics->first_effect;

		thread_graphics = graphics;
		graphics->exports.device_enter_context(graphics->device);

		while (effect) {
			struct gs_effect *next = effect->next;
			gs_effect_actually_destroy(effect);
			effect = next;
		}

		graphics->exports.gs_vertexbuffer_destroy(
			graphics->sprite_buffer);
		graphics->exports.gs_vertexbuffer_destroy(
			graphics->immediate_vertbuffer);
		graphics->exports.device_destroy(graphics->device);

		thread_graphics = NULL;
	}

	pthread_mutex_destroy(&graphics->mutex);
	pthread_mutex_destroy(&graphics->effect_mutex);
	da_free(graphics->matrix_stack);
	da_free(graphics->viewport_stack);
	da_free(graphics->blend_state_stack);
	if (graphics->module)
		os_dlclose(graphics->module);
	bfree(graphics);

	gs_free_image_deps();
}

void gs_enter_context(graphics_t *graphics)
{
	if (!ptr_valid(graphics, "gs_enter_context"))
		return;

	bool is_current = thread_graphics == graphics;
	if (thread_graphics && !is_current) {
		while (thread_graphics)
			gs_leave_context();
	}

	if (!is_current) {
		pthread_mutex_lock(&graphics->mutex);
		graphics->exports.device_enter_context(graphics->device);
		thread_graphics = graphics;
	}

	os_atomic_inc_long(&graphics->ref);
}

void gs_leave_context(void)
{
	if (gs_valid("gs_leave_context")) {
		if (!os_atomic_dec_long(&thread_graphics->ref)) {
			graphics_t *graphics = thread_graphics;

			graphics->exports.device_leave_context(
				graphics->device);
			pthread_mutex_unlock(&graphics->mutex);
			thread_graphics = NULL;
		}
	}
}

graphics_t *gs_get_context(void)
{
	return thread_graphics;
}

void *gs_get_device_obj(void)
{
	if (!gs_valid("gs_get_device_obj"))
		return NULL;

	return thread_graphics->exports.device_get_device_obj(
		thread_graphics->device);
}

const char *gs_get_device_name(void)
{
	return gs_valid("gs_get_device_name")
		       ? thread_graphics->exports.device_get_name()
		       : NULL;
}

int gs_get_device_type(void)
{
	return gs_valid("gs_get_device_type")
		       ? thread_graphics->exports.device_get_type()
		       : -1;
}

static inline struct matrix4 *top_matrix(graphics_t *graphics)
{
	return graphics->matrix_stack.array + graphics->cur_matrix;
}

void gs_matrix_push(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_matrix_push"))
		return;

	struct matrix4 mat, *top_mat = top_matrix(graphics);

	memcpy(&mat, top_mat, sizeof(struct matrix4));
	da_push_back(graphics->matrix_stack, &mat);
	graphics->cur_matrix++;
}

void gs_matrix_pop(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_matrix_pop"))
		return;

	if (graphics->cur_matrix == 0) {
		blog(LOG_ERROR, "Tried to pop last matrix on stack");
		return;
	}

	da_erase(graphics->matrix_stack, graphics->cur_matrix);
	graphics->cur_matrix--;
}

void gs_matrix_identity(void)
{
	struct matrix4 *top_mat;

	if (!gs_valid("gs_matrix_identity"))
		return;

	top_mat = top_matrix(thread_graphics);
	if (top_mat)
		matrix4_identity(top_mat);
}

void gs_matrix_transpose(void)
{
	struct matrix4 *top_mat;

	if (!gs_valid("gs_matrix_transpose"))
		return;

	top_mat = top_matrix(thread_graphics);
	if (top_mat)
		matrix4_transpose(top_mat, top_mat);
}

void gs_matrix_set(const struct matrix4 *matrix)
{
	struct matrix4 *top_mat;

	if (!gs_valid("gs_matrix_set"))
		return;

	top_mat = top_matrix(thread_graphics);
	if (top_mat)
		matrix4_copy(top_mat, matrix);
}

void gs_matrix_get(struct matrix4 *dst)
{
	struct matrix4 *top_mat;

	if (!gs_valid("gs_matrix_get"))
		return;

	top_mat = top_matrix(thread_graphics);
	if (top_mat)
		matrix4_copy(dst, top_mat);
}

void gs_matrix_mul(const struct matrix4 *matrix)
{
	struct matrix4 *top_mat;

	if (!gs_valid("gs_matrix_mul"))
		return;

	top_mat = top_matrix(thread_graphics);
	if (top_mat)
		matrix4_mul(top_mat, matrix, top_mat);
}

void gs_matrix_rotquat(const struct quat *rot)
{
	struct matrix4 *top_mat;

	if (!gs_valid("gs_matrix_rotquat"))
		return;

	top_mat = top_matrix(thread_graphics);
	if (top_mat)
		matrix4_rotate_i(top_mat, rot, top_mat);
}

void gs_matrix_rotaa(const struct axisang *rot)
{
	struct matrix4 *top_mat;

	if (!gs_valid("gs_matrix_rotaa"))
		return;

	top_mat = top_matrix(thread_graphics);
	if (top_mat)
		matrix4_rotate_aa_i(top_mat, rot, top_mat);
}

void gs_matrix_translate(const struct vec3 *pos)
{
	struct matrix4 *top_mat;

	if (!gs_valid("gs_matrix_translate"))
		return;

	top_mat = top_matrix(thread_graphics);
	if (top_mat)
		matrix4_translate3v_i(top_mat, pos, top_mat);
}

void gs_matrix_scale(const struct vec3 *scale)
{
	struct matrix4 *top_mat;

	if (!gs_valid("gs_matrix_scale"))
		return;

	top_mat = top_matrix(thread_graphics);
	if (top_mat)
		matrix4_scale_i(top_mat, scale, top_mat);
}

void gs_matrix_rotaa4f(float x, float y, float z, float angle)
{
	struct matrix4 *top_mat;
	struct axisang aa;

	if (!gs_valid("gs_matrix_rotaa4f"))
		return;

	top_mat = top_matrix(thread_graphics);
	if (top_mat) {
		axisang_set(&aa, x, y, z, angle);
		matrix4_rotate_aa_i(top_mat, &aa, top_mat);
	}
}

void gs_matrix_translate3f(float x, float y, float z)
{
	struct matrix4 *top_mat;
	struct vec3 p;

	if (!gs_valid("gs_matrix_translate3f"))
		return;

	top_mat = top_matrix(thread_graphics);
	if (top_mat) {
		vec3_set(&p, x, y, z);
		matrix4_translate3v_i(top_mat, &p, top_mat);
	}
}

void gs_matrix_scale3f(float x, float y, float z)
{
	struct matrix4 *top_mat = top_matrix(thread_graphics);
	struct vec3 p;

	if (top_mat) {
		vec3_set(&p, x, y, z);
		matrix4_scale_i(top_mat, &p, top_mat);
	}
}

static inline void reset_immediate_arrays(graphics_t *graphics)
{
	da_init(graphics->verts);
	da_init(graphics->norms);
	da_init(graphics->colors);
	for (size_t i = 0; i < 16; i++)
		da_init(graphics->texverts[i]);
}

void gs_render_start(bool b_new)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_render_start"))
		return;

	graphics->using_immediate = !b_new;
	reset_immediate_arrays(graphics);

	if (b_new) {
		graphics->vbd = gs_vbdata_create();
	} else {
		graphics->vbd = gs_vertexbuffer_get_data(
			graphics->immediate_vertbuffer);
		memset(graphics->vbd->colors, 0xFF,
		       sizeof(uint32_t) * IMMEDIATE_COUNT);

		graphics->verts.array = graphics->vbd->points;
		graphics->norms.array = graphics->vbd->normals;
		graphics->colors.array = graphics->vbd->colors;
		graphics->texverts[0].array = graphics->vbd->tvarray[0].array;

		graphics->verts.capacity = IMMEDIATE_COUNT;
		graphics->norms.capacity = IMMEDIATE_COUNT;
		graphics->colors.capacity = IMMEDIATE_COUNT;
		graphics->texverts[0].capacity = IMMEDIATE_COUNT;
	}
}

static inline size_t min_size(const size_t a, const size_t b)
{
	return (a < b) ? a : b;
}

void gs_render_stop(enum gs_draw_mode mode)
{
	graphics_t *graphics = thread_graphics;
	size_t i, num;

	if (!gs_valid("gs_render_stop"))
		return;

	num = graphics->verts.num;
	if (!num) {
		if (!graphics->using_immediate) {
			da_free(graphics->verts);
			da_free(graphics->norms);
			da_free(graphics->colors);
			for (i = 0; i < 16; i++)
				da_free(graphics->texverts[i]);
			gs_vbdata_destroy(graphics->vbd);
		}

		return;
	}

	if (graphics->norms.num &&
	    (graphics->norms.num != graphics->verts.num)) {
		blog(LOG_ERROR, "gs_render_stop: normal count does "
				"not match vertex count");
		num = min_size(num, graphics->norms.num);
	}

	if (graphics->colors.num &&
	    (graphics->colors.num != graphics->verts.num)) {
		blog(LOG_ERROR, "gs_render_stop: color count does "
				"not match vertex count");
		num = min_size(num, graphics->colors.num);
	}

	if (graphics->texverts[0].num &&
	    (graphics->texverts[0].num != graphics->verts.num)) {
		blog(LOG_ERROR, "gs_render_stop: texture vertex count does "
				"not match vertex count");
		num = min_size(num, graphics->texverts[0].num);
	}

	if (graphics->using_immediate) {
		gs_vertexbuffer_flush(graphics->immediate_vertbuffer);

		gs_load_vertexbuffer(graphics->immediate_vertbuffer);
		gs_load_indexbuffer(NULL);
		gs_draw(mode, 0, (uint32_t)num);

		reset_immediate_arrays(graphics);
	} else {
		gs_vertbuffer_t *vb = gs_render_save();

		gs_load_vertexbuffer(vb);
		gs_load_indexbuffer(NULL);
		gs_draw(mode, 0, 0);

		gs_vertexbuffer_destroy(vb);
	}

	graphics->vbd = NULL;
}

gs_vertbuffer_t *gs_render_save(void)
{
	graphics_t *graphics = thread_graphics;
	size_t num_tex, i;

	if (!gs_valid("gs_render_save"))
		return NULL;
	if (graphics->using_immediate)
		return NULL;

	if (!graphics->verts.num) {
		gs_vbdata_destroy(graphics->vbd);
		return NULL;
	}

	for (num_tex = 0; num_tex < 16; num_tex++)
		if (!graphics->texverts[num_tex].num)
			break;

	graphics->vbd->points = graphics->verts.array;
	graphics->vbd->normals = graphics->norms.array;
	graphics->vbd->colors = graphics->colors.array;
	graphics->vbd->num = graphics->verts.num;
	graphics->vbd->num_tex = num_tex;

	if (graphics->vbd->num_tex) {
		graphics->vbd->tvarray =
			bmalloc(sizeof(struct gs_tvertarray) * num_tex);

		for (i = 0; i < num_tex; i++) {
			graphics->vbd->tvarray[i].width = 2;
			graphics->vbd->tvarray[i].array =
				graphics->texverts[i].array;
		}
	}

	reset_immediate_arrays(graphics);

	return gs_vertexbuffer_create(graphics->vbd, 0);
}

void gs_vertex2f(float x, float y)
{
	struct vec3 v3;

	if (!gs_valid("gs_verte"))
		return;

	vec3_set(&v3, x, y, 0.0f);
	gs_vertex3v(&v3);
}

void gs_vertex3f(float x, float y, float z)
{
	struct vec3 v3;

	if (!gs_valid("gs_vertex3f"))
		return;

	vec3_set(&v3, x, y, z);
	gs_vertex3v(&v3);
}

void gs_normal3f(float x, float y, float z)
{
	struct vec3 v3;

	if (!gs_valid("gs_normal3f"))
		return;

	vec3_set(&v3, x, y, z);
	gs_normal3v(&v3);
}

static inline bool validvertsize(graphics_t *graphics, size_t num,
				 const char *name)
{
	if (graphics->using_immediate && num == IMMEDIATE_COUNT) {
		blog(LOG_ERROR,
		     "%s: tried to use over %u "
		     "for immediate rendering",
		     name, IMMEDIATE_COUNT);
		return false;
	}

	return true;
}

void gs_color(uint32_t color)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_color"))
		return;
	if (!validvertsize(graphics, graphics->colors.num, "gs_color"))
		return;

	da_push_back(graphics->colors, &color);
}

void gs_texcoord(float x, float y, int unit)
{
	struct vec2 v2;

	if (!gs_valid("gs_texcoord"))
		return;

	vec2_set(&v2, x, y);
	gs_texcoord2v(&v2, unit);
}

void gs_vertex2v(const struct vec2 *v)
{
	struct vec3 v3;

	if (!gs_valid("gs_vertex2v"))
		return;

	vec3_set(&v3, v->x, v->y, 0.0f);
	gs_vertex3v(&v3);
}

void gs_vertex3v(const struct vec3 *v)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_vertex3v"))
		return;
	if (!validvertsize(graphics, graphics->verts.num, "gs_vertex"))
		return;

	da_push_back(graphics->verts, v);
}

void gs_normal3v(const struct vec3 *v)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_normal3v"))
		return;
	if (!validvertsize(graphics, graphics->norms.num, "gs_normal"))
		return;

	da_push_back(graphics->norms, v);
}

void gs_color4v(const struct vec4 *v)
{
	/* TODO */
	UNUSED_PARAMETER(v);
}

void gs_texcoord2v(const struct vec2 *v, int unit)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_texcoord2v"))
		return;
	if (!validvertsize(graphics, graphics->texverts[unit].num,
			   "gs_texcoord"))
		return;

	da_push_back(graphics->texverts[unit], v);
}

input_t *gs_get_input(void)
{
	/* TODO */
	return NULL;
}

gs_effect_t *gs_get_effect(void)
{
	if (!gs_valid("gs_get_effect"))
		return NULL;

	return thread_graphics ? thread_graphics->cur_effect : NULL;
}

static inline struct gs_effect *find_cached_effect(const char *filename)
{
	struct gs_effect *effect = thread_graphics->first_effect;

	while (effect) {
		if (strcmp(effect->effect_path, filename) == 0)
			break;
		effect = effect->next;
	}

	return effect;
}

gs_effect_t *gs_effect_create_from_file(const char *file, char **error_string)
{
	char *file_string;
	gs_effect_t *effect = NULL;

	if (!gs_valid_p("gs_effect_create_from_file", file))
		return NULL;

	effect = find_cached_effect(file);
	if (effect)
		return effect;

	file_string = os_quick_read_utf8_file(file);
	if (!file_string) {
		blog(LOG_ERROR, "Could not load effect file '%s'", file);
		return NULL;
	}

	effect = gs_effect_create(file_string, file, error_string);
	bfree(file_string);

	return effect;
}

gs_effect_t *gs_effect_create(const char *effect_string, const char *filename,
			      char **error_string)
{
	if (!gs_valid_p("gs_effect_create", effect_string))
		return NULL;

	struct gs_effect *effect = bzalloc(sizeof(struct gs_effect));
	struct effect_parser parser;
	bool success;

	effect->graphics = thread_graphics;
	effect->effect_path = bstrdup(filename);

	ep_init(&parser);
	success = ep_parse(&parser, effect, effect_string, filename);
	if (!success) {
		if (error_string)
			*error_string =
				error_data_buildstring(&parser.cfp.error_list);
		gs_effect_destroy(effect);
		effect = NULL;
	}

	if (effect) {
		pthread_mutex_lock(&thread_graphics->effect_mutex);

		if (effect->effect_path) {
			effect->cached = true;
			effect->next = thread_graphics->first_effect;
			thread_graphics->first_effect = effect;
		}

		pthread_mutex_unlock(&thread_graphics->effect_mutex);
	}

	ep_free(&parser);
	return effect;
}

gs_shader_t *gs_vertexshader_create_from_file(const char *file,
					      char **error_string)
{
	if (!gs_valid_p("gs_vertexshader_create_from_file", file))
		return NULL;

	char *file_string;
	gs_shader_t *shader = NULL;

	file_string = os_quick_read_utf8_file(file);
	if (!file_string) {
		blog(LOG_ERROR, "Could not load vertex shader file '%s'", file);
		return NULL;
	}

	shader = gs_vertexshader_create(file_string, file, error_string);
	bfree(file_string);

	return shader;
}

gs_shader_t *gs_pixelshader_create_from_file(const char *file,
					     char **error_string)
{
	char *file_string;
	gs_shader_t *shader = NULL;

	if (!gs_valid_p("gs_pixelshader_create_from_file", file))
		return NULL;

	file_string = os_quick_read_utf8_file(file);
	if (!file_string) {
		blog(LOG_ERROR, "Could not load pixel shader file '%s'", file);
		return NULL;
	}

	shader = gs_pixelshader_create(file_string, file, error_string);
	bfree(file_string);

	return shader;
}

gs_texture_t *gs_texture_create_from_file(const char *file)
{
	enum gs_color_format format;
	uint32_t cx;
	uint32_t cy;
	uint8_t *data = gs_create_texture_file_data(file, &format, &cx, &cy);
	gs_texture_t *tex = NULL;

	if (data) {
		tex = gs_texture_create(cx, cy, format, 1,
					(const uint8_t **)&data, 0);
		bfree(data);
	}

	return tex;
}

static inline void assign_sprite_rect(float *start, float *end, float size,
				      bool flip)
{
	if (!flip) {
		*start = 0.0f;
		*end = size;
	} else {
		*start = size;
		*end = 0.0f;
	}
}

static inline void assign_sprite_uv(float *start, float *end, bool flip)
{
	if (!flip) {
		*start = 0.0f;
		*end = 1.0f;
	} else {
		*start = 1.0f;
		*end = 0.0f;
	}
}

static void build_sprite(struct gs_vb_data *data, float fcx, float fcy,
			 float start_u, float end_u, float start_v, float end_v)
{
	struct vec2 *tvarray = data->tvarray[0].array;

	vec3_zero(data->points);
	vec3_set(data->points + 1, fcx, 0.0f, 0.0f);
	vec3_set(data->points + 2, 0.0f, fcy, 0.0f);
	vec3_set(data->points + 3, fcx, fcy, 0.0f);
	vec2_set(tvarray, start_u, start_v);
	vec2_set(tvarray + 1, end_u, start_v);
	vec2_set(tvarray + 2, start_u, end_v);
	vec2_set(tvarray + 3, end_u, end_v);
}

static inline void build_sprite_norm(struct gs_vb_data *data, float fcx,
				     float fcy, uint32_t flip)
{
	float start_u, end_u;
	float start_v, end_v;

	assign_sprite_uv(&start_u, &end_u, (flip & GS_FLIP_U) != 0);
	assign_sprite_uv(&start_v, &end_v, (flip & GS_FLIP_V) != 0);
	build_sprite(data, fcx, fcy, start_u, end_u, start_v, end_v);
}

static inline void build_subsprite_norm(struct gs_vb_data *data, float fsub_x,
					float fsub_y, float fsub_cx,
					float fsub_cy, float fcx, float fcy,
					uint32_t flip)
{
	float start_u, end_u;
	float start_v, end_v;

	if ((flip & GS_FLIP_U) == 0) {
		start_u = fsub_x / fcx;
		end_u = (fsub_x + fsub_cx) / fcx;
	} else {
		start_u = (fsub_x + fsub_cx) / fcx;
		end_u = fsub_x / fcx;
	}

	if ((flip & GS_FLIP_V) == 0) {
		start_v = fsub_y / fcy;
		end_v = (fsub_y + fsub_cy) / fcy;
	} else {
		start_v = (fsub_y + fsub_cy) / fcy;
		end_v = fsub_y / fcy;
	}

	build_sprite(data, fsub_cx, fsub_cy, start_u, end_u, start_v, end_v);
}

static inline void build_sprite_rect(struct gs_vb_data *data, gs_texture_t *tex,
				     float fcx, float fcy, uint32_t flip)
{
	float start_u, end_u;
	float start_v, end_v;
	float width = (float)gs_texture_get_width(tex);
	float height = (float)gs_texture_get_height(tex);

	assign_sprite_rect(&start_u, &end_u, width, (flip & GS_FLIP_U) != 0);
	assign_sprite_rect(&start_v, &end_v, height, (flip & GS_FLIP_V) != 0);
	build_sprite(data, fcx, fcy, start_u, end_u, start_v, end_v);
}

void gs_draw_sprite(gs_texture_t *tex, uint32_t flip, uint32_t width,
		    uint32_t height)
{
	graphics_t *graphics = thread_graphics;
	float fcx, fcy;
	struct gs_vb_data *data;

	if (tex) {
		if (gs_get_texture_type(tex) != GS_TEXTURE_2D) {
			blog(LOG_ERROR, "A sprite must be a 2D texture");
			return;
		}
	} else {
		if (!width || !height) {
			blog(LOG_ERROR, "A sprite cannot be drawn without "
					"a width/height");
			return;
		}
	}

	fcx = width ? (float)width : (float)gs_texture_get_width(tex);
	fcy = height ? (float)height : (float)gs_texture_get_height(tex);

	data = gs_vertexbuffer_get_data(graphics->sprite_buffer);
	if (tex && gs_texture_is_rect(tex))
		build_sprite_rect(data, tex, fcx, fcy, flip);
	else
		build_sprite_norm(data, fcx, fcy, flip);

	gs_vertexbuffer_flush(graphics->sprite_buffer);
	gs_load_vertexbuffer(graphics->sprite_buffer);
	gs_load_indexbuffer(NULL);

	gs_draw(GS_TRISTRIP, 0, 0);
}

void gs_draw_sprite_subregion(gs_texture_t *tex, uint32_t flip, uint32_t sub_x,
			      uint32_t sub_y, uint32_t sub_cx, uint32_t sub_cy)
{
	graphics_t *graphics = thread_graphics;
	float fcx, fcy;
	struct gs_vb_data *data;

	if (tex) {
		if (gs_get_texture_type(tex) != GS_TEXTURE_2D) {
			blog(LOG_ERROR, "A sprite must be a 2D texture");
			return;
		}
	}

	fcx = (float)gs_texture_get_width(tex);
	fcy = (float)gs_texture_get_height(tex);

	data = gs_vertexbuffer_get_data(graphics->sprite_buffer);
	build_subsprite_norm(data, (float)sub_x, (float)sub_y, (float)sub_cx,
			     (float)sub_cy, fcx, fcy, flip);

	gs_vertexbuffer_flush(graphics->sprite_buffer);
	gs_load_vertexbuffer(graphics->sprite_buffer);
	gs_load_indexbuffer(NULL);

	gs_draw(GS_TRISTRIP, 0, 0);
}

void gs_draw_cube_backdrop(gs_texture_t *cubetex, const struct quat *rot,
			   float left, float right, float top, float bottom,
			   float znear)
{
	/* TODO */
	UNUSED_PARAMETER(cubetex);
	UNUSED_PARAMETER(rot);
	UNUSED_PARAMETER(left);
	UNUSED_PARAMETER(right);
	UNUSED_PARAMETER(top);
	UNUSED_PARAMETER(bottom);
	UNUSED_PARAMETER(znear);
}

void gs_reset_viewport(void)
{
	uint32_t cx, cy;

	if (!gs_valid("gs_reset_viewport"))
		return;

	gs_get_size(&cx, &cy);
	gs_set_viewport(0, 0, (int)cx, (int)cy);
}

void gs_set_2d_mode(void)
{
	uint32_t cx, cy;

	if (!gs_valid("gs_set_2d_mode"))
		return;

	gs_get_size(&cx, &cy);
	gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -1.0, -1024.0f);
}

void gs_set_3d_mode(double fovy, double znear, double zvar)
{
	/* TODO */
	UNUSED_PARAMETER(fovy);
	UNUSED_PARAMETER(znear);
	UNUSED_PARAMETER(zvar);
}

void gs_viewport_push(void)
{
	if (!gs_valid("gs_viewport_push"))
		return;

	struct gs_rect *rect =
		da_push_back_new(thread_graphics->viewport_stack);
	gs_get_viewport(rect);
}

void gs_viewport_pop(void)
{
	struct gs_rect *rect;

	if (!gs_valid("gs_viewport_pop"))
		return;
	if (!thread_graphics->viewport_stack.num)
		return;

	rect = da_end(thread_graphics->viewport_stack);
	gs_set_viewport(rect->x, rect->y, rect->cx, rect->cy);
	da_pop_back(thread_graphics->viewport_stack);
}

void gs_texture_set_image(gs_texture_t *tex, const uint8_t *data,
			  uint32_t linesize, bool flip)
{
	uint8_t *ptr;
	uint32_t linesize_out;
	size_t row_copy;
	size_t height;

	if (!gs_valid_p2("gs_texture_set_image", tex, data))
		return;

	if (!gs_texture_map(tex, &ptr, &linesize_out))
		return;

	row_copy = (linesize < linesize_out) ? linesize : linesize_out;

	height = gs_texture_get_height(tex);

	if (flip) {
		uint8_t *const end = ptr + height * linesize_out;
		data += (height - 1) * linesize;
		while (ptr < end) {
			memcpy(ptr, data, row_copy);
			ptr += linesize_out;
			data -= linesize;
		}

	} else if (linesize == linesize_out) {
		memcpy(ptr, data, row_copy * height);

	} else {
		uint8_t *const end = ptr + height * linesize_out;
		while (ptr < end) {
			memcpy(ptr, data, row_copy);
			ptr += linesize_out;
			data += linesize;
		}
	}

	gs_texture_unmap(tex);
}

void gs_cubetexture_set_image(gs_texture_t *cubetex, uint32_t side,
			      const void *data, uint32_t linesize, bool invert)
{
	/* TODO */
	UNUSED_PARAMETER(cubetex);
	UNUSED_PARAMETER(side);
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(linesize);
	UNUSED_PARAMETER(invert);
}

void gs_perspective(float angle, float aspect, float near, float far)
{
	graphics_t *graphics = thread_graphics;
	float xmin, xmax, ymin, ymax;

	if (!gs_valid("gs_perspective"))
		return;

	ymax = near * tanf(RAD(angle) * 0.5f);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	graphics->exports.device_frustum(graphics->device, xmin, xmax, ymin,
					 ymax, near, far);
}

void gs_blend_state_push(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_blend_state_push"))
		return;

	da_push_back(graphics->blend_state_stack, &graphics->cur_blend_state);
}

void gs_blend_state_pop(void)
{
	graphics_t *graphics = thread_graphics;
	struct blend_state *state;

	if (!gs_valid("gs_blend_state_pop"))
		return;

	state = da_end(graphics->blend_state_stack);
	if (!state)
		return;

	gs_enable_blending(state->enabled);
	gs_blend_function_separate(state->src_c, state->dest_c, state->src_a,
				   state->dest_a);

	da_pop_back(graphics->blend_state_stack);
}

void gs_reset_blend_state(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_preprocessor_name"))
		return;

	if (!graphics->cur_blend_state.enabled)
		gs_enable_blending(true);

	if (graphics->cur_blend_state.src_c != GS_BLEND_SRCALPHA ||
	    graphics->cur_blend_state.dest_c != GS_BLEND_INVSRCALPHA ||
	    graphics->cur_blend_state.src_a != GS_BLEND_ONE ||
	    graphics->cur_blend_state.dest_a != GS_BLEND_INVSRCALPHA)
		gs_blend_function_separate(GS_BLEND_SRCALPHA,
					   GS_BLEND_INVSRCALPHA, GS_BLEND_ONE,
					   GS_BLEND_INVSRCALPHA);
}

/* ------------------------------------------------------------------------- */

const char *gs_preprocessor_name(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_preprocessor_name"))
		return NULL;

	return graphics->exports.device_preprocessor_name();
}

gs_swapchain_t *gs_swapchain_create(const struct gs_init_data *data)
{
	struct gs_init_data new_data = *data;
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_swapchain_create", data))
		return NULL;

	if (new_data.num_backbuffers == 0)
		new_data.num_backbuffers = 1;

	return graphics->exports.device_swapchain_create(graphics->device,
							 &new_data);
}

void gs_resize(uint32_t x, uint32_t y)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_resize"))
		return;

	graphics->exports.device_resize(graphics->device, x, y);
}

void gs_get_size(uint32_t *x, uint32_t *y)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_get_size"))
		return;

	graphics->exports.device_get_size(graphics->device, x, y);
}

uint32_t gs_get_width(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_get_width"))
		return 0;

	return graphics->exports.device_get_width(graphics->device);
}

uint32_t gs_get_height(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_get_height"))
		return 0;

	return graphics->exports.device_get_height(graphics->device);
}

static inline bool is_pow2(uint32_t size)
{
	return size >= 2 && (size & (size - 1)) == 0;
}

gs_texture_t *gs_texture_create(uint32_t width, uint32_t height,
				enum gs_color_format color_format,
				uint32_t levels, const uint8_t **data,
				uint32_t flags)
{
	graphics_t *graphics = thread_graphics;
	bool pow2tex = is_pow2(width) && is_pow2(height);
	bool uses_mipmaps = (flags & GS_BUILD_MIPMAPS || levels != 1);

	if (!gs_valid("gs_texture_create"))
		return NULL;

	if (uses_mipmaps && !pow2tex) {
		blog(LOG_WARNING, "Cannot use mipmaps with a "
				  "non-power-of-two texture.  Disabling "
				  "mipmaps for this texture.");

		uses_mipmaps = false;
		flags &= ~GS_BUILD_MIPMAPS;
		levels = 1;
	}

	if (uses_mipmaps && flags & GS_RENDER_TARGET) {
		blog(LOG_WARNING, "Cannot use mipmaps with render targets.  "
				  "Disabling mipmaps for this texture.");
		flags &= ~GS_BUILD_MIPMAPS;
		levels = 1;
	}

	return graphics->exports.device_texture_create(graphics->device, width,
						       height, color_format,
						       levels, data, flags);
}

#if __linux__

gs_texture_t *gs_texture_create_from_dmabuf(
	unsigned int width, unsigned int height, uint32_t drm_format,
	enum gs_color_format color_format, uint32_t n_planes, const int *fds,
	const uint32_t *strides, const uint32_t *offsets,
	const uint64_t *modifiers)
{
	graphics_t *graphics = thread_graphics;

	return graphics->exports.device_texture_create_from_dmabuf(
		graphics->device, width, height, drm_format, color_format,
		n_planes, fds, strides, offsets, modifiers);
}

#endif

gs_texture_t *gs_cubetexture_create(uint32_t size,
				    enum gs_color_format color_format,
				    uint32_t levels, const uint8_t **data,
				    uint32_t flags)
{
	graphics_t *graphics = thread_graphics;
	bool pow2tex = is_pow2(size);
	bool uses_mipmaps = (flags & GS_BUILD_MIPMAPS || levels != 1);

	if (!gs_valid("gs_cubetexture_create"))
		return NULL;

	if (uses_mipmaps && !pow2tex) {
		blog(LOG_WARNING, "Cannot use mipmaps with a "
				  "non-power-of-two texture.  Disabling "
				  "mipmaps for this texture.");

		uses_mipmaps = false;
		flags &= ~GS_BUILD_MIPMAPS;
		levels = 1;
	}

	if (uses_mipmaps && flags & GS_RENDER_TARGET) {
		blog(LOG_WARNING, "Cannot use mipmaps with render targets.  "
				  "Disabling mipmaps for this texture.");
		flags &= ~GS_BUILD_MIPMAPS;
		levels = 1;
		data = NULL;
	}

	return graphics->exports.device_cubetexture_create(
		graphics->device, size, color_format, levels, data, flags);
}

gs_texture_t *gs_voltexture_create(uint32_t width, uint32_t height,
				   uint32_t depth,
				   enum gs_color_format color_format,
				   uint32_t levels, const uint8_t **data,
				   uint32_t flags)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_voltexture_create"))
		return NULL;

	return graphics->exports.device_voltexture_create(graphics->device,
							  width, height, depth,
							  color_format, levels,
							  data, flags);
}

gs_zstencil_t *gs_zstencil_create(uint32_t width, uint32_t height,
				  enum gs_zstencil_format format)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_zstencil_create"))
		return NULL;

	return graphics->exports.device_zstencil_create(graphics->device, width,
							height, format);
}

gs_stagesurf_t *gs_stagesurface_create(uint32_t width, uint32_t height,
				       enum gs_color_format color_format)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_stagesurface_create"))
		return NULL;

	return graphics->exports.device_stagesurface_create(
		graphics->device, width, height, color_format);
}

gs_samplerstate_t *gs_samplerstate_create(const struct gs_sampler_info *info)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_samplerstate_create", info))
		return NULL;

	return graphics->exports.device_samplerstate_create(graphics->device,
							    info);
}

gs_shader_t *gs_vertexshader_create(const char *shader, const char *file,
				    char **error_string)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_vertexshader_create", shader))
		return NULL;

	return graphics->exports.device_vertexshader_create(
		graphics->device, shader, file, error_string);
}

gs_shader_t *gs_pixelshader_create(const char *shader, const char *file,
				   char **error_string)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_pixelshader_create", shader))
		return NULL;

	return graphics->exports.device_pixelshader_create(
		graphics->device, shader, file, error_string);
}

gs_vertbuffer_t *gs_vertexbuffer_create(struct gs_vb_data *data, uint32_t flags)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_vertexbuffer_create"))
		return NULL;

	if (data && data->num && (flags & GS_DUP_BUFFER) != 0) {
		struct gs_vb_data *new_data = gs_vbdata_create();

		new_data->num = data->num;

#define DUP_VAL(val)                                                        \
	do {                                                                \
		if (data->val)                                              \
			new_data->val = bmemdup(                            \
				data->val, sizeof(*data->val) * data->num); \
	} while (false)

		DUP_VAL(points);
		DUP_VAL(normals);
		DUP_VAL(tangents);
		DUP_VAL(colors);
#undef DUP_VAL

		if (data->tvarray && data->num_tex) {
			new_data->num_tex = data->num_tex;
			new_data->tvarray = bzalloc(
				sizeof(struct gs_tvertarray) * data->num_tex);

			for (size_t i = 0; i < data->num_tex; i++) {
				struct gs_tvertarray *tv = &data->tvarray[i];
				struct gs_tvertarray *new_tv =
					&new_data->tvarray[i];
				size_t size = tv->width * sizeof(float);

				new_tv->width = tv->width;
				new_tv->array =
					bmemdup(tv->array, size * data->num);
			}
		}

		data = new_data;
	}

	return graphics->exports.device_vertexbuffer_create(graphics->device,
							    data, flags);
}

gs_indexbuffer_t *gs_indexbuffer_create(enum gs_index_type type, void *indices,
					size_t num, uint32_t flags)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_indexbuffer_create"))
		return NULL;

	if (indices && num && (flags & GS_DUP_BUFFER) != 0) {
		size_t size = type == GS_UNSIGNED_SHORT ? 2 : 4;
		indices = bmemdup(indices, size * num);
	}

	return graphics->exports.device_indexbuffer_create(
		graphics->device, type, indices, num, flags);
}

gs_timer_t *gs_timer_create()
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_timer_create"))
		return NULL;

	return graphics->exports.device_timer_create(graphics->device);
}

gs_timer_range_t *gs_timer_range_create()
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_timer_range_create"))
		return NULL;

	return graphics->exports.device_timer_range_create(graphics->device);
}

enum gs_texture_type gs_get_texture_type(const gs_texture_t *texture)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_get_texture_type", texture))
		return GS_TEXTURE_2D;

	return graphics->exports.device_get_texture_type(texture);
}

void gs_load_vertexbuffer(gs_vertbuffer_t *vertbuffer)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_load_vertexbuffer"))
		return;

	graphics->exports.device_load_vertexbuffer(graphics->device,
						   vertbuffer);
}

void gs_load_indexbuffer(gs_indexbuffer_t *indexbuffer)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_load_indexbuffer"))
		return;

	graphics->exports.device_load_indexbuffer(graphics->device,
						  indexbuffer);
}

void gs_load_texture(gs_texture_t *tex, int unit)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_load_texture"))
		return;

	graphics->exports.device_load_texture(graphics->device, tex, unit);
}

void gs_load_samplerstate(gs_samplerstate_t *samplerstate, int unit)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_load_samplerstate"))
		return;

	graphics->exports.device_load_samplerstate(graphics->device,
						   samplerstate, unit);
}

void gs_load_vertexshader(gs_shader_t *vertshader)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_load_vertexshader"))
		return;

	graphics->exports.device_load_vertexshader(graphics->device,
						   vertshader);
}

void gs_load_pixelshader(gs_shader_t *pixelshader)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_load_pixelshader"))
		return;

	graphics->exports.device_load_pixelshader(graphics->device,
						  pixelshader);
}

void gs_load_default_samplerstate(bool b_3d, int unit)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_load_default_samplerstate"))
		return;

	graphics->exports.device_load_default_samplerstate(graphics->device,
							   b_3d, unit);
}

gs_shader_t *gs_get_vertex_shader(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_get_vertex_shader"))
		return NULL;

	return graphics->exports.device_get_vertex_shader(graphics->device);
}

gs_shader_t *gs_get_pixel_shader(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_get_pixel_shader"))
		return NULL;

	return graphics->exports.device_get_pixel_shader(graphics->device);
}

gs_texture_t *gs_get_render_target(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_get_render_target"))
		return NULL;

	return graphics->exports.device_get_render_target(graphics->device);
}

gs_zstencil_t *gs_get_zstencil_target(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_get_zstencil_target"))
		return NULL;

	return graphics->exports.device_get_zstencil_target(graphics->device);
}

void gs_set_render_target(gs_texture_t *tex, gs_zstencil_t *zstencil)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_set_render_target"))
		return;

	graphics->exports.device_set_render_target(graphics->device, tex,
						   zstencil);
}

void gs_set_cube_render_target(gs_texture_t *cubetex, int side,
			       gs_zstencil_t *zstencil)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_set_cube_render_target"))
		return;

	graphics->exports.device_set_cube_render_target(
		graphics->device, cubetex, side, zstencil);
}

void gs_enable_framebuffer_srgb(bool enable)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_enable_framebuffer_srgb"))
		return;

	graphics->exports.device_enable_framebuffer_srgb(graphics->device,
							 enable);
}

bool gs_framebuffer_srgb_enabled(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_framebuffer_srgb_enabled"))
		return false;

	return graphics->exports.device_framebuffer_srgb_enabled(
		graphics->device);
}

bool gs_get_linear_srgb(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_get_linear_srgb"))
		return false;

	return graphics->linear_srgb;
}

bool gs_set_linear_srgb(bool linear_srgb)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_set_linear_srgb"))
		return false;

	const bool previous = graphics->linear_srgb;
	graphics->linear_srgb = linear_srgb;
	return previous;
}

void gs_copy_texture(gs_texture_t *dst, gs_texture_t *src)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p2("gs_copy_texture", dst, src))
		return;

	graphics->exports.device_copy_texture(graphics->device, dst, src);
}

void gs_copy_texture_region(gs_texture_t *dst, uint32_t dst_x, uint32_t dst_y,
			    gs_texture_t *src, uint32_t src_x, uint32_t src_y,
			    uint32_t src_w, uint32_t src_h)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_copy_texture_region", dst))
		return;

	graphics->exports.device_copy_texture_region(graphics->device, dst,
						     dst_x, dst_y, src, src_x,
						     src_y, src_w, src_h);
}

void gs_stage_texture(gs_stagesurf_t *dst, gs_texture_t *src)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_stage_texture"))
		return;

	graphics->exports.device_stage_texture(graphics->device, dst, src);
}

void gs_begin_frame(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_begin_frame"))
		return;

	graphics->exports.device_begin_frame(graphics->device);
}

void gs_begin_scene(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_begin_scene"))
		return;

	graphics->exports.device_begin_scene(graphics->device);
}

void gs_draw(enum gs_draw_mode draw_mode, uint32_t start_vert,
	     uint32_t num_verts)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_draw"))
		return;

	graphics->exports.device_draw(graphics->device, draw_mode, start_vert,
				      num_verts);
}

void gs_end_scene(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_end_scene"))
		return;

	graphics->exports.device_end_scene(graphics->device);
}

void gs_load_swapchain(gs_swapchain_t *swapchain)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_load_swapchain"))
		return;

	graphics->exports.device_load_swapchain(graphics->device, swapchain);
}

void gs_clear(uint32_t clear_flags, const struct vec4 *color, float depth,
	      uint8_t stencil)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_clear"))
		return;

	graphics->exports.device_clear(graphics->device, clear_flags, color,
				       depth, stencil);
}

void gs_present(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_present"))
		return;

	graphics->exports.device_present(graphics->device);
}

void gs_flush(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_flush"))
		return;

	graphics->exports.device_flush(graphics->device);
}

void gs_set_cull_mode(enum gs_cull_mode mode)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_set_cull_mode"))
		return;

	graphics->exports.device_set_cull_mode(graphics->device, mode);
}

enum gs_cull_mode gs_get_cull_mode(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_get_cull_mode"))
		return GS_NEITHER;

	return graphics->exports.device_get_cull_mode(graphics->device);
}

void gs_enable_blending(bool enable)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_enable_blending"))
		return;

	graphics->cur_blend_state.enabled = enable;
	graphics->exports.device_enable_blending(graphics->device, enable);
}

void gs_enable_depth_test(bool enable)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_enable_depth_test"))
		return;

	graphics->exports.device_enable_depth_test(graphics->device, enable);
}

void gs_enable_stencil_test(bool enable)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_enable_stencil_test"))
		return;

	graphics->exports.device_enable_stencil_test(graphics->device, enable);
}

void gs_enable_stencil_write(bool enable)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_enable_stencil_write"))
		return;

	graphics->exports.device_enable_stencil_write(graphics->device, enable);
}

void gs_enable_color(bool red, bool green, bool blue, bool alpha)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_enable_color"))
		return;

	graphics->exports.device_enable_color(graphics->device, red, green,
					      blue, alpha);
}

void gs_blend_function(enum gs_blend_type src, enum gs_blend_type dest)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_blend_function"))
		return;

	graphics->cur_blend_state.src_c = src;
	graphics->cur_blend_state.dest_c = dest;
	graphics->cur_blend_state.src_a = src;
	graphics->cur_blend_state.dest_a = dest;
	graphics->exports.device_blend_function(graphics->device, src, dest);
}

void gs_blend_function_separate(enum gs_blend_type src_c,
				enum gs_blend_type dest_c,
				enum gs_blend_type src_a,
				enum gs_blend_type dest_a)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_blend_function_separate"))
		return;

	graphics->cur_blend_state.src_c = src_c;
	graphics->cur_blend_state.dest_c = dest_c;
	graphics->cur_blend_state.src_a = src_a;
	graphics->cur_blend_state.dest_a = dest_a;
	graphics->exports.device_blend_function_separate(
		graphics->device, src_c, dest_c, src_a, dest_a);
}

void gs_depth_function(enum gs_depth_test test)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_depth_function"))
		return;

	graphics->exports.device_depth_function(graphics->device, test);
}

void gs_stencil_function(enum gs_stencil_side side, enum gs_depth_test test)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_stencil_function"))
		return;

	graphics->exports.device_stencil_function(graphics->device, side, test);
}

void gs_stencil_op(enum gs_stencil_side side, enum gs_stencil_op_type fail,
		   enum gs_stencil_op_type zfail, enum gs_stencil_op_type zpass)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_stencil_op"))
		return;

	graphics->exports.device_stencil_op(graphics->device, side, fail, zfail,
					    zpass);
}

void gs_set_viewport(int x, int y, int width, int height)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_set_viewport"))
		return;

	graphics->exports.device_set_viewport(graphics->device, x, y, width,
					      height);
}

void gs_get_viewport(struct gs_rect *rect)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_get_viewport", rect))
		return;

	graphics->exports.device_get_viewport(graphics->device, rect);
}

void gs_set_scissor_rect(const struct gs_rect *rect)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_set_scissor_rect"))
		return;

	graphics->exports.device_set_scissor_rect(graphics->device, rect);
}

void gs_ortho(float left, float right, float top, float bottom, float znear,
	      float zfar)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_ortho"))
		return;

	graphics->exports.device_ortho(graphics->device, left, right, top,
				       bottom, znear, zfar);
}

void gs_frustum(float left, float right, float top, float bottom, float znear,
		float zfar)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_frustum"))
		return;

	graphics->exports.device_frustum(graphics->device, left, right, top,
					 bottom, znear, zfar);
}

void gs_projection_push(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_projection_push"))
		return;

	graphics->exports.device_projection_push(graphics->device);
}

void gs_projection_pop(void)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_projection_pop"))
		return;

	graphics->exports.device_projection_pop(graphics->device);
}

void gs_swapchain_destroy(gs_swapchain_t *swapchain)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_swapchain_destroy"))
		return;
	if (!swapchain)
		return;

	graphics->exports.gs_swapchain_destroy(swapchain);
}

void gs_shader_destroy(gs_shader_t *shader)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_shader_destroy"))
		return;
	if (!shader)
		return;

	graphics->exports.gs_shader_destroy(shader);
}

int gs_shader_get_num_params(const gs_shader_t *shader)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_shader_get_num_params", shader))
		return 0;

	return graphics->exports.gs_shader_get_num_params(shader);
}

gs_sparam_t *gs_shader_get_param_by_idx(gs_shader_t *shader, uint32_t param)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_shader_get_param_by_idx", shader))
		return NULL;

	return graphics->exports.gs_shader_get_param_by_idx(shader, param);
}

gs_sparam_t *gs_shader_get_param_by_name(gs_shader_t *shader, const char *name)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p2("gs_shader_get_param_by_name", shader, name))
		return NULL;

	return graphics->exports.gs_shader_get_param_by_name(shader, name);
}

gs_sparam_t *gs_shader_get_viewproj_matrix(const gs_shader_t *shader)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_shader_get_viewproj_matrix", shader))
		return NULL;

	return graphics->exports.gs_shader_get_viewproj_matrix(shader);
}

gs_sparam_t *gs_shader_get_world_matrix(const gs_shader_t *shader)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_shader_get_world_matrix", shader))
		return NULL;

	return graphics->exports.gs_shader_get_world_matrix(shader);
}

void gs_shader_get_param_info(const gs_sparam_t *param,
			      struct gs_shader_param_info *info)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p2("gs_shader_get_param_info", param, info))
		return;

	graphics->exports.gs_shader_get_param_info(param, info);
}

void gs_shader_set_bool(gs_sparam_t *param, bool val)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_shader_set_bool", param))
		return;

	graphics->exports.gs_shader_set_bool(param, val);
}

void gs_shader_set_float(gs_sparam_t *param, float val)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_shader_set_float", param))
		return;

	graphics->exports.gs_shader_set_float(param, val);
}

void gs_shader_set_int(gs_sparam_t *param, int val)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_shader_set_int", param))
		return;

	graphics->exports.gs_shader_set_int(param, val);
}

void gs_shader_set_matrix3(gs_sparam_t *param, const struct matrix3 *val)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p2("gs_shader_set_matrix3", param, val))
		return;

	graphics->exports.gs_shader_set_matrix3(param, val);
}

void gs_shader_set_matrix4(gs_sparam_t *param, const struct matrix4 *val)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p2("gs_shader_set_matrix4", param, val))
		return;

	graphics->exports.gs_shader_set_matrix4(param, val);
}

void gs_shader_set_vec2(gs_sparam_t *param, const struct vec2 *val)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p2("gs_shader_set_vec2", param, val))
		return;

	graphics->exports.gs_shader_set_vec2(param, val);
}

void gs_shader_set_vec3(gs_sparam_t *param, const struct vec3 *val)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p2("gs_shader_set_vec3", param, val))
		return;

	graphics->exports.gs_shader_set_vec3(param, val);
}

void gs_shader_set_vec4(gs_sparam_t *param, const struct vec4 *val)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p2("gs_shader_set_vec4", param, val))
		return;

	graphics->exports.gs_shader_set_vec4(param, val);
}

void gs_shader_set_texture(gs_sparam_t *param, gs_texture_t *val)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_shader_set_texture", param))
		return;

	graphics->exports.gs_shader_set_texture(param, val);
}

void gs_shader_set_val(gs_sparam_t *param, const void *val, size_t size)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p2("gs_shader_set_val", param, val))
		return;

	graphics->exports.gs_shader_set_val(param, val, size);
}

void gs_shader_set_default(gs_sparam_t *param)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_shader_set_default", param))
		return;

	graphics->exports.gs_shader_set_default(param);
}

void gs_shader_set_next_sampler(gs_sparam_t *param, gs_samplerstate_t *sampler)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_shader_set_next_sampler", param))
		return;

	graphics->exports.gs_shader_set_next_sampler(param, sampler);
}

void gs_texture_destroy(gs_texture_t *tex)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_texture_destroy"))
		return;
	if (!tex)
		return;

	graphics->exports.gs_texture_destroy(tex);
}

uint32_t gs_texture_get_width(const gs_texture_t *tex)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_texture_get_width", tex))
		return 0;

	return graphics->exports.gs_texture_get_width(tex);
}

uint32_t gs_texture_get_height(const gs_texture_t *tex)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_texture_get_height", tex))
		return 0;

	return graphics->exports.gs_texture_get_height(tex);
}

enum gs_color_format gs_texture_get_color_format(const gs_texture_t *tex)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_texture_get_color_format", tex))
		return GS_UNKNOWN;

	return graphics->exports.gs_texture_get_color_format(tex);
}

bool gs_texture_map(gs_texture_t *tex, uint8_t **ptr, uint32_t *linesize)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p3("gs_texture_map", tex, ptr, linesize))
		return false;

	return graphics->exports.gs_texture_map(tex, ptr, linesize);
}

void gs_texture_unmap(gs_texture_t *tex)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_texture_unmap", tex))
		return;

	graphics->exports.gs_texture_unmap(tex);
}

bool gs_texture_is_rect(const gs_texture_t *tex)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_texture_is_rect", tex))
		return false;

	if (graphics->exports.gs_texture_is_rect)
		return graphics->exports.gs_texture_is_rect(tex);
	else
		return false;
}

void *gs_texture_get_obj(gs_texture_t *tex)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_texture_get_obj", tex))
		return NULL;

	return graphics->exports.gs_texture_get_obj(tex);
}

void gs_cubetexture_destroy(gs_texture_t *cubetex)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_cubetexture_destroy"))
		return;
	if (!cubetex)
		return;

	graphics->exports.gs_cubetexture_destroy(cubetex);
}

uint32_t gs_cubetexture_get_size(const gs_texture_t *cubetex)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_cubetexture_get_size", cubetex))
		return 0;

	return graphics->exports.gs_cubetexture_get_size(cubetex);
}

enum gs_color_format
gs_cubetexture_get_color_format(const gs_texture_t *cubetex)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_cubetexture_get_color_format", cubetex))
		return GS_UNKNOWN;

	return graphics->exports.gs_cubetexture_get_color_format(cubetex);
}

void gs_voltexture_destroy(gs_texture_t *voltex)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_voltexture_destroy"))
		return;
	if (!voltex)
		return;

	graphics->exports.gs_voltexture_destroy(voltex);
}

uint32_t gs_voltexture_get_width(const gs_texture_t *voltex)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_voltexture_get_width", voltex))
		return 0;

	return graphics->exports.gs_voltexture_get_width(voltex);
}

uint32_t gs_voltexture_get_height(const gs_texture_t *voltex)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_voltexture_get_height", voltex))
		return 0;

	return graphics->exports.gs_voltexture_get_height(voltex);
}

uint32_t gs_voltexture_get_depth(const gs_texture_t *voltex)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_voltexture_get_depth", voltex))
		return 0;

	return graphics->exports.gs_voltexture_get_depth(voltex);
}

enum gs_color_format gs_voltexture_get_color_format(const gs_texture_t *voltex)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_voltexture_get_color_format", voltex))
		return GS_UNKNOWN;

	return graphics->exports.gs_voltexture_get_color_format(voltex);
}

void gs_stagesurface_destroy(gs_stagesurf_t *stagesurf)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_stagesurface_destroy"))
		return;
	if (!stagesurf)
		return;

	graphics->exports.gs_stagesurface_destroy(stagesurf);
}

uint32_t gs_stagesurface_get_width(const gs_stagesurf_t *stagesurf)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_stagesurface_get_width", stagesurf))
		return 0;

	return graphics->exports.gs_stagesurface_get_width(stagesurf);
}

uint32_t gs_stagesurface_get_height(const gs_stagesurf_t *stagesurf)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_stagesurface_get_height", stagesurf))
		return 0;

	return graphics->exports.gs_stagesurface_get_height(stagesurf);
}

enum gs_color_format
gs_stagesurface_get_color_format(const gs_stagesurf_t *stagesurf)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_stagesurface_get_color_format", stagesurf))
		return GS_UNKNOWN;

	return graphics->exports.gs_stagesurface_get_color_format(stagesurf);
}

bool gs_stagesurface_map(gs_stagesurf_t *stagesurf, uint8_t **data,
			 uint32_t *linesize)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p3("gs_stagesurface_map", stagesurf, data, linesize))
		return 0;

	return graphics->exports.gs_stagesurface_map(stagesurf, data, linesize);
}

void gs_stagesurface_unmap(gs_stagesurf_t *stagesurf)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_stagesurface_unmap", stagesurf))
		return;

	graphics->exports.gs_stagesurface_unmap(stagesurf);
}

void gs_zstencil_destroy(gs_zstencil_t *zstencil)
{
	if (!gs_valid("gs_zstencil_destroy"))
		return;
	if (!zstencil)
		return;

	thread_graphics->exports.gs_zstencil_destroy(zstencil);
}

void gs_samplerstate_destroy(gs_samplerstate_t *samplerstate)
{
	if (!gs_valid("gs_samplerstate_destroy"))
		return;
	if (!samplerstate)
		return;

	thread_graphics->exports.gs_samplerstate_destroy(samplerstate);
}

void gs_vertexbuffer_destroy(gs_vertbuffer_t *vertbuffer)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_vertexbuffer_destroy"))
		return;
	if (!vertbuffer)
		return;

	graphics->exports.gs_vertexbuffer_destroy(vertbuffer);
}

void gs_vertexbuffer_flush(gs_vertbuffer_t *vertbuffer)
{
	if (!gs_valid_p("gs_vertexbuffer_flush", vertbuffer))
		return;

	thread_graphics->exports.gs_vertexbuffer_flush(vertbuffer);
}

void gs_vertexbuffer_flush_direct(gs_vertbuffer_t *vertbuffer,
				  const struct gs_vb_data *data)
{
	if (!gs_valid_p2("gs_vertexbuffer_flush_direct", vertbuffer, data))
		return;

	thread_graphics->exports.gs_vertexbuffer_flush_direct(vertbuffer, data);
}

struct gs_vb_data *gs_vertexbuffer_get_data(const gs_vertbuffer_t *vertbuffer)
{
	if (!gs_valid_p("gs_vertexbuffer_get_data", vertbuffer))
		return NULL;

	return thread_graphics->exports.gs_vertexbuffer_get_data(vertbuffer);
}

void gs_indexbuffer_destroy(gs_indexbuffer_t *indexbuffer)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_indexbuffer_destroy"))
		return;
	if (!indexbuffer)
		return;

	graphics->exports.gs_indexbuffer_destroy(indexbuffer);
}

void gs_indexbuffer_flush(gs_indexbuffer_t *indexbuffer)
{
	if (!gs_valid_p("gs_indexbuffer_flush", indexbuffer))
		return;

	thread_graphics->exports.gs_indexbuffer_flush(indexbuffer);
}

void gs_indexbuffer_flush_direct(gs_indexbuffer_t *indexbuffer,
				 const void *data)
{
	if (!gs_valid_p2("gs_indexbuffer_flush_direct", indexbuffer, data))
		return;

	thread_graphics->exports.gs_indexbuffer_flush_direct(indexbuffer, data);
}

void *gs_indexbuffer_get_data(const gs_indexbuffer_t *indexbuffer)
{
	if (!gs_valid_p("gs_indexbuffer_get_data", indexbuffer))
		return NULL;

	return thread_graphics->exports.gs_indexbuffer_get_data(indexbuffer);
}

size_t gs_indexbuffer_get_num_indices(const gs_indexbuffer_t *indexbuffer)
{
	if (!gs_valid_p("gs_indexbuffer_get_num_indices", indexbuffer))
		return 0;

	return thread_graphics->exports.gs_indexbuffer_get_num_indices(
		indexbuffer);
}

enum gs_index_type gs_indexbuffer_get_type(const gs_indexbuffer_t *indexbuffer)
{
	if (!gs_valid_p("gs_indexbuffer_get_type", indexbuffer))
		return (enum gs_index_type)0;

	return thread_graphics->exports.gs_indexbuffer_get_type(indexbuffer);
}

void gs_timer_destroy(gs_timer_t *timer)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_timer_destroy"))
		return;
	if (!timer)
		return;

	graphics->exports.gs_timer_destroy(timer);
}

void gs_timer_begin(gs_timer_t *timer)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_timer_begin"))
		return;
	if (!timer)
		return;

	graphics->exports.gs_timer_begin(timer);
}

void gs_timer_end(gs_timer_t *timer)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_timer_end"))
		return;
	if (!timer)
		return;

	graphics->exports.gs_timer_end(timer);
}

bool gs_timer_get_data(gs_timer_t *timer, uint64_t *ticks)
{
	if (!gs_valid_p2("gs_timer_get_data", timer, ticks))
		return false;

	return thread_graphics->exports.gs_timer_get_data(timer, ticks);
}

void gs_timer_range_destroy(gs_timer_range_t *range)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_timer_range_destroy"))
		return;
	if (!range)
		return;

	graphics->exports.gs_timer_range_destroy(range);
}

void gs_timer_range_begin(gs_timer_range_t *range)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_timer_range_begin"))
		return;
	if (!range)
		return;

	graphics->exports.gs_timer_range_begin(range);
}

void gs_timer_range_end(gs_timer_range_t *range)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_timer_range_end"))
		return;
	if (!range)
		return;

	graphics->exports.gs_timer_range_end(range);
}

bool gs_timer_range_get_data(gs_timer_range_t *range, bool *disjoint,
			     uint64_t *frequency)
{
	if (!gs_valid_p2("gs_timer_range_get_data", disjoint, frequency))
		return false;

	return thread_graphics->exports.gs_timer_range_get_data(range, disjoint,
								frequency);
}

bool gs_nv12_available(void)
{
	if (!gs_valid("gs_nv12_available"))
		return false;

	if (!thread_graphics->exports.device_nv12_available)
		return false;

	return thread_graphics->exports.device_nv12_available(
		thread_graphics->device);
}

void gs_debug_marker_begin(const float color[4], const char *markername)
{
	if (!gs_valid("gs_debug_marker_begin"))
		return;

	if (!markername)
		markername = "(null)";

	thread_graphics->exports.device_debug_marker_begin(
		thread_graphics->device, markername, color);
}

void gs_debug_marker_begin_format(const float color[4], const char *format, ...)
{
	if (!gs_valid("gs_debug_marker_begin"))
		return;

	if (format) {
		char markername[64];
		va_list args;
		va_start(args, format);
		vsnprintf(markername, sizeof(markername), format, args);
		va_end(args);
		thread_graphics->exports.device_debug_marker_begin(
			thread_graphics->device, markername, color);
	} else {
		gs_debug_marker_begin(color, NULL);
	}
}

void gs_debug_marker_end(void)
{
	if (!gs_valid("gs_debug_marker_end"))
		return;

	thread_graphics->exports.device_debug_marker_end(
		thread_graphics->device);
}

#ifdef __APPLE__

/** Platform specific functions */
gs_texture_t *gs_texture_create_from_iosurface(void *iosurf)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_texture_create_from_iosurface", iosurf))
		return NULL;
	if (!graphics->exports.device_texture_create_from_iosurface)
		return NULL;

	return graphics->exports.device_texture_create_from_iosurface(
		graphics->device, iosurf);
}

bool gs_texture_rebind_iosurface(gs_texture_t *texture, void *iosurf)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_texture_rebind_iosurface", texture))
		return false;
	if (!graphics->exports.gs_texture_rebind_iosurface)
		return false;

	return graphics->exports.gs_texture_rebind_iosurface(texture, iosurf);
}

bool gs_shared_texture_available(void)
{
	if (!gs_valid("gs_shared_texture_available"))
		return false;

	return thread_graphics->exports.device_shared_texture_available();
}

gs_texture_t *gs_texture_open_shared(uint32_t handle)
{
	graphics_t *graphics = thread_graphics;
	if (!gs_valid("gs_texture_open_shared"))
		return NULL;

	if (graphics->exports.device_texture_open_shared)
		return graphics->exports.device_texture_open_shared(
			graphics->device, handle);
	return NULL;
}

#elif _WIN32

bool gs_gdi_texture_available(void)
{
	if (!gs_valid("gs_gdi_texture_available"))
		return false;

	return thread_graphics->exports.device_gdi_texture_available();
}

bool gs_shared_texture_available(void)
{
	if (!gs_valid("gs_shared_texture_available"))
		return false;

	return thread_graphics->exports.device_shared_texture_available();
}

bool gs_get_duplicator_monitor_info(int monitor_idx,
				    struct gs_monitor_info *monitor_info)
{
	if (!gs_valid_p("gs_get_duplicator_monitor_info", monitor_info))
		return false;
	if (!thread_graphics->exports.device_get_duplicator_monitor_info)
		return false;

	return thread_graphics->exports.device_get_duplicator_monitor_info(
		thread_graphics->device, monitor_idx, monitor_info);
}

int gs_duplicator_get_monitor_index(void *monitor)
{
	if (!gs_valid("gs_duplicator_get_monitor_index"))
		return false;
	if (!thread_graphics->exports.device_duplicator_get_monitor_index)
		return false;

	return thread_graphics->exports.device_duplicator_get_monitor_index(
		thread_graphics->device, monitor);
}

gs_duplicator_t *gs_duplicator_create(int monitor_idx)
{
	if (!gs_valid("gs_duplicator_create"))
		return NULL;
	if (!thread_graphics->exports.device_duplicator_create)
		return NULL;

	return thread_graphics->exports.device_duplicator_create(
		thread_graphics->device, monitor_idx);
}

void gs_duplicator_destroy(gs_duplicator_t *duplicator)
{
	if (!gs_valid("gs_duplicator_destroy"))
		return;
	if (!duplicator)
		return;
	if (!thread_graphics->exports.gs_duplicator_destroy)
		return;

	thread_graphics->exports.gs_duplicator_destroy(duplicator);
}

bool gs_duplicator_update_frame(gs_duplicator_t *duplicator)
{
	if (!gs_valid_p("gs_duplicator_update_frame", duplicator))
		return false;
	if (!thread_graphics->exports.gs_duplicator_update_frame)
		return false;

	return thread_graphics->exports.gs_duplicator_update_frame(duplicator);
}

uint32_t gs_get_adapter_count(void)
{
	if (!gs_valid("gs_get_adapter_count"))
		return 0;
	if (!thread_graphics->exports.gs_get_adapter_count)
		return 0;

	return thread_graphics->exports.gs_get_adapter_count();
}

gs_texture_t *gs_duplicator_get_texture(gs_duplicator_t *duplicator)
{
	if (!gs_valid_p("gs_duplicator_get_texture", duplicator))
		return NULL;
	if (!thread_graphics->exports.gs_duplicator_get_texture)
		return NULL;

	return thread_graphics->exports.gs_duplicator_get_texture(duplicator);
}

/** creates a windows GDI-lockable texture */
gs_texture_t *gs_texture_create_gdi(uint32_t width, uint32_t height)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_texture_create_gdi"))
		return NULL;

	if (graphics->exports.device_texture_create_gdi)
		return graphics->exports.device_texture_create_gdi(
			graphics->device, width, height);
	return NULL;
}

void *gs_texture_get_dc(gs_texture_t *gdi_tex)
{
	if (!gs_valid_p("gs_texture_release_dc", gdi_tex))
		return NULL;

	if (thread_graphics->exports.gs_texture_get_dc)
		return thread_graphics->exports.gs_texture_get_dc(gdi_tex);
	return NULL;
}

void gs_texture_release_dc(gs_texture_t *gdi_tex)
{
	if (!gs_valid_p("gs_texture_release_dc", gdi_tex))
		return;

	if (thread_graphics->exports.gs_texture_release_dc)
		thread_graphics->exports.gs_texture_release_dc(gdi_tex);
}

gs_texture_t *gs_texture_open_shared(uint32_t handle)
{
	graphics_t *graphics = thread_graphics;
	if (!gs_valid("gs_texture_open_shared"))
		return NULL;

	if (graphics->exports.device_texture_open_shared)
		return graphics->exports.device_texture_open_shared(
			graphics->device, handle);
	return NULL;
}

uint32_t gs_texture_get_shared_handle(gs_texture_t *tex)
{
	graphics_t *graphics = thread_graphics;
	if (!gs_valid("gs_texture_get_shared_handle"))
		return GS_INVALID_HANDLE;

	if (graphics->exports.device_texture_get_shared_handle)
		return graphics->exports.device_texture_get_shared_handle(tex);
	return GS_INVALID_HANDLE;
}

gs_texture_t *gs_texture_wrap_obj(void *obj)
{
	graphics_t *graphics = thread_graphics;
	if (!gs_valid("gs_texture_wrap_obj"))
		return NULL;

	if (graphics->exports.device_texture_wrap_obj)
		return graphics->exports.device_texture_wrap_obj(
			graphics->device, obj);
	return NULL;
}

int gs_texture_acquire_sync(gs_texture_t *tex, uint64_t key, uint32_t ms)
{
	graphics_t *graphics = thread_graphics;
	if (!gs_valid("gs_texture_acquire_sync"))
		return -1;

	if (graphics->exports.device_texture_acquire_sync)
		return graphics->exports.device_texture_acquire_sync(tex, key,
								     ms);
	return -1;
}

int gs_texture_release_sync(gs_texture_t *tex, uint64_t key)
{
	graphics_t *graphics = thread_graphics;
	if (!gs_valid("gs_texture_release_sync"))
		return -1;

	if (graphics->exports.device_texture_release_sync)
		return graphics->exports.device_texture_release_sync(tex, key);
	return -1;
}

bool gs_texture_create_nv12(gs_texture_t **tex_y, gs_texture_t **tex_uv,
			    uint32_t width, uint32_t height, uint32_t flags)
{
	graphics_t *graphics = thread_graphics;
	bool success = false;

	if (!gs_valid("gs_texture_create_nv12"))
		return false;

	if ((width & 1) == 1 || (height & 1) == 1) {
		blog(LOG_ERROR, "NV12 textures must have dimensions "
				"divisible by 2.");
		return false;
	}

	if (graphics->exports.device_texture_create_nv12) {
		success = graphics->exports.device_texture_create_nv12(
			graphics->device, tex_y, tex_uv, width, height, flags);
		if (success)
			return true;
	}

	*tex_y = gs_texture_create(width, height, GS_R8, 1, NULL, flags);
	*tex_uv = gs_texture_create(width / 2, height / 2, GS_R8G8, 1, NULL,
				    flags);

	if (!*tex_y || !*tex_uv) {
		if (*tex_y)
			gs_texture_destroy(*tex_y);
		if (*tex_uv)
			gs_texture_destroy(*tex_uv);
		*tex_y = NULL;
		*tex_uv = NULL;
		return false;
	}

	return true;
}

gs_stagesurf_t *gs_stagesurface_create_nv12(uint32_t width, uint32_t height)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_stagesurface_create_nv12"))
		return NULL;

	if ((width & 1) == 1 || (height & 1) == 1) {
		blog(LOG_ERROR, "NV12 textures must have dimensions "
				"divisible by 2.");
		return NULL;
	}

	if (graphics->exports.device_stagesurface_create_nv12)
		return graphics->exports.device_stagesurface_create_nv12(
			graphics->device, width, height);

	return NULL;
}

void gs_register_loss_callbacks(const struct gs_device_loss *callbacks)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_register_loss_callbacks"))
		return;

	if (graphics->exports.device_register_loss_callbacks)
		graphics->exports.device_register_loss_callbacks(
			graphics->device, callbacks);
}

void gs_unregister_loss_callbacks(void *data)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_unregister_loss_callbacks"))
		return;

	if (graphics->exports.device_unregister_loss_callbacks)
		graphics->exports.device_unregister_loss_callbacks(
			graphics->device, data);
}

#endif
