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

#include "../util/bmem.h"
#include "input.h"
#ifdef __APPLE__
#include <objc/objc-runtime.h>
#endif

/*
 * This is an API-independent graphics subsystem wrapper.
 *
 *   This allows the use of OpenGL and different Direct3D versions through
 * one shared interface.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define GS_MAX_TEXTURES 8

struct vec2;
struct vec3;
struct vec4;
struct quat;
struct axisang;
struct plane;
struct matrix3;
struct matrix4;

enum gs_draw_mode {
	GS_POINTS,
	GS_LINES,
	GS_LINESTRIP,
	GS_TRIS,
	GS_TRISTRIP,
};

enum gs_color_format {
	GS_UNKNOWN,
	GS_A8,
	GS_R8,
	GS_RGBA,
	GS_BGRX,
	GS_BGRA,
	GS_R10G10B10A2,
	GS_RGBA16,
	GS_R16,
	GS_RGBA16F,
	GS_RGBA32F,
	GS_RG16F,
	GS_RG32F,
	GS_R16F,
	GS_R32F,
	GS_DXT1,
	GS_DXT3,
	GS_DXT5,
	GS_R8G8,
	GS_RGBA_UNORM,
	GS_BGRX_UNORM,
	GS_BGRA_UNORM,
};

enum gs_zstencil_format {
	GS_ZS_NONE,
	GS_Z16,
	GS_Z24_S8,
	GS_Z32F,
	GS_Z32F_S8X24,
};

enum gs_index_type {
	GS_UNSIGNED_SHORT,
	GS_UNSIGNED_LONG,
};

enum gs_cull_mode {
	GS_BACK,
	GS_FRONT,
	GS_NEITHER,
};

enum gs_blend_type {
	GS_BLEND_ZERO,
	GS_BLEND_ONE,
	GS_BLEND_SRCCOLOR,
	GS_BLEND_INVSRCCOLOR,
	GS_BLEND_SRCALPHA,
	GS_BLEND_INVSRCALPHA,
	GS_BLEND_DSTCOLOR,
	GS_BLEND_INVDSTCOLOR,
	GS_BLEND_DSTALPHA,
	GS_BLEND_INVDSTALPHA,
	GS_BLEND_SRCALPHASAT,
};

enum gs_depth_test {
	GS_NEVER,
	GS_LESS,
	GS_LEQUAL,
	GS_EQUAL,
	GS_GEQUAL,
	GS_GREATER,
	GS_NOTEQUAL,
	GS_ALWAYS,
};

enum gs_stencil_side {
	GS_STENCIL_FRONT = 1,
	GS_STENCIL_BACK,
	GS_STENCIL_BOTH,
};

enum gs_stencil_op_type {
	GS_KEEP,
	GS_ZERO,
	GS_REPLACE,
	GS_INCR,
	GS_DECR,
	GS_INVERT,
};

enum gs_cube_sides {
	GS_POSITIVE_X,
	GS_NEGATIVE_X,
	GS_POSITIVE_Y,
	GS_NEGATIVE_Y,
	GS_POSITIVE_Z,
	GS_NEGATIVE_Z,
};

enum gs_sample_filter {
	GS_FILTER_POINT,
	GS_FILTER_LINEAR,
	GS_FILTER_ANISOTROPIC,
	GS_FILTER_MIN_MAG_POINT_MIP_LINEAR,
	GS_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT,
	GS_FILTER_MIN_POINT_MAG_MIP_LINEAR,
	GS_FILTER_MIN_LINEAR_MAG_MIP_POINT,
	GS_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
	GS_FILTER_MIN_MAG_LINEAR_MIP_POINT,
};

enum gs_address_mode {
	GS_ADDRESS_CLAMP,
	GS_ADDRESS_WRAP,
	GS_ADDRESS_MIRROR,
	GS_ADDRESS_BORDER,
	GS_ADDRESS_MIRRORONCE,
};

enum gs_texture_type {
	GS_TEXTURE_2D,
	GS_TEXTURE_3D,
	GS_TEXTURE_CUBE,
};

struct gs_device_loss {
	void (*device_loss_release)(void *data);
	void (*device_loss_rebuild)(void *device, void *data);
	void *data;
};

struct gs_monitor_info {
	int rotation_degrees;
	long x;
	long y;
	long cx;
	long cy;
};

struct gs_tvertarray {
	size_t width;
	void *array;
};

struct gs_vb_data {
	size_t num;
	struct vec3 *points;
	struct vec3 *normals;
	struct vec3 *tangents;
	uint32_t *colors;

	size_t num_tex;
	struct gs_tvertarray *tvarray;
};

static inline struct gs_vb_data *gs_vbdata_create(void)
{
	return (struct gs_vb_data *)bzalloc(sizeof(struct gs_vb_data));
}

static inline void gs_vbdata_destroy(struct gs_vb_data *data)
{
	uint32_t i;
	if (!data)
		return;

	bfree(data->points);
	bfree(data->normals);
	bfree(data->tangents);
	bfree(data->colors);
	for (i = 0; i < data->num_tex; i++)
		bfree(data->tvarray[i].array);
	bfree(data->tvarray);
	bfree(data);
}

struct gs_sampler_info {
	enum gs_sample_filter filter;
	enum gs_address_mode address_u;
	enum gs_address_mode address_v;
	enum gs_address_mode address_w;
	int max_anisotropy;
	uint32_t border_color;
};

struct gs_display_mode {
	uint32_t width;
	uint32_t height;
	uint32_t bits;
	uint32_t freq;
};

struct gs_rect {
	int x;
	int y;
	int cx;
	int cy;
};

/* wrapped opaque data types */

struct gs_texture;
struct gs_stage_surface;
struct gs_zstencil_buffer;
struct gs_vertex_buffer;
struct gs_index_buffer;
struct gs_sampler_state;
struct gs_shader;
struct gs_swap_chain;
struct gs_timer;
struct gs_texrender;
struct gs_shader_param;
struct gs_effect;
struct gs_effect_technique;
struct gs_effect_pass;
struct gs_effect_param;
struct gs_device;
struct graphics_subsystem;

typedef struct gs_texture gs_texture_t;
typedef struct gs_stage_surface gs_stagesurf_t;
typedef struct gs_zstencil_buffer gs_zstencil_t;
typedef struct gs_vertex_buffer gs_vertbuffer_t;
typedef struct gs_index_buffer gs_indexbuffer_t;
typedef struct gs_sampler_state gs_samplerstate_t;
typedef struct gs_swap_chain gs_swapchain_t;
typedef struct gs_timer gs_timer_t;
typedef struct gs_timer_range gs_timer_range_t;
typedef struct gs_texture_render gs_texrender_t;
typedef struct gs_shader gs_shader_t;
typedef struct gs_shader_param gs_sparam_t;
typedef struct gs_effect gs_effect_t;
typedef struct gs_effect_technique gs_technique_t;
typedef struct gs_effect_pass gs_epass_t;
typedef struct gs_effect_param gs_eparam_t;
typedef struct gs_device gs_device_t;
typedef struct graphics_subsystem graphics_t;

/* ---------------------------------------------------
 * shader functions
 * --------------------------------------------------- */

enum gs_shader_param_type {
	GS_SHADER_PARAM_UNKNOWN,
	GS_SHADER_PARAM_BOOL,
	GS_SHADER_PARAM_FLOAT,
	GS_SHADER_PARAM_INT,
	GS_SHADER_PARAM_STRING,
	GS_SHADER_PARAM_VEC2,
	GS_SHADER_PARAM_VEC3,
	GS_SHADER_PARAM_VEC4,
	GS_SHADER_PARAM_INT2,
	GS_SHADER_PARAM_INT3,
	GS_SHADER_PARAM_INT4,
	GS_SHADER_PARAM_MATRIX4X4,
	GS_SHADER_PARAM_TEXTURE,
};

struct gs_shader_texture {
	gs_texture_t *tex;
	bool srgb;
};

#ifndef SWIG
struct gs_shader_param_info {
	enum gs_shader_param_type type;
	const char *name;
};

enum gs_shader_type {
	GS_SHADER_VERTEX,
	GS_SHADER_PIXEL,
};

EXPORT void gs_shader_destroy(gs_shader_t *shader);

EXPORT int gs_shader_get_num_params(const gs_shader_t *shader);
EXPORT gs_sparam_t *gs_shader_get_param_by_idx(gs_shader_t *shader,
					       uint32_t param);
EXPORT gs_sparam_t *gs_shader_get_param_by_name(gs_shader_t *shader,
						const char *name);

EXPORT gs_sparam_t *gs_shader_get_viewproj_matrix(const gs_shader_t *shader);
EXPORT gs_sparam_t *gs_shader_get_world_matrix(const gs_shader_t *shader);

EXPORT void gs_shader_get_param_info(const gs_sparam_t *param,
				     struct gs_shader_param_info *info);
EXPORT void gs_shader_set_bool(gs_sparam_t *param, bool val);
EXPORT void gs_shader_set_float(gs_sparam_t *param, float val);
EXPORT void gs_shader_set_int(gs_sparam_t *param, int val);
EXPORT void gs_shader_set_matrix3(gs_sparam_t *param,
				  const struct matrix3 *val);
EXPORT void gs_shader_set_matrix4(gs_sparam_t *param,
				  const struct matrix4 *val);
EXPORT void gs_shader_set_vec2(gs_sparam_t *param, const struct vec2 *val);
EXPORT void gs_shader_set_vec3(gs_sparam_t *param, const struct vec3 *val);
EXPORT void gs_shader_set_vec4(gs_sparam_t *param, const struct vec4 *val);
EXPORT void gs_shader_set_texture(gs_sparam_t *param, gs_texture_t *val);
EXPORT void gs_shader_set_val(gs_sparam_t *param, const void *val, size_t size);
EXPORT void gs_shader_set_default(gs_sparam_t *param);
EXPORT void gs_shader_set_next_sampler(gs_sparam_t *param,
				       gs_samplerstate_t *sampler);
#endif

/* ---------------------------------------------------
 * effect functions
 * --------------------------------------------------- */

/*enum gs_effect_property_type {
	GS_EFFECT_NONE,
	GS_EFFECT_BOOL,
	GS_EFFECT_FLOAT,
	GS_EFFECT_COLOR,
	GS_EFFECT_TEXTURE
};*/

#ifndef SWIG
struct gs_effect_param_info {
	const char *name;
	enum gs_shader_param_type type;

	/* const char *full_name;
	enum gs_effect_property_type prop_type;

	float min, max, inc, mul; */
};
#endif

EXPORT void gs_effect_destroy(gs_effect_t *effect);

EXPORT gs_technique_t *gs_effect_get_technique(const gs_effect_t *effect,
					       const char *name);

EXPORT gs_technique_t *
gs_effect_get_current_technique(const gs_effect_t *effect);

EXPORT size_t gs_technique_begin(gs_technique_t *technique);
EXPORT void gs_technique_end(gs_technique_t *technique);
EXPORT bool gs_technique_begin_pass(gs_technique_t *technique, size_t pass);
EXPORT bool gs_technique_begin_pass_by_name(gs_technique_t *technique,
					    const char *name);
EXPORT void gs_technique_end_pass(gs_technique_t *technique);
EXPORT gs_epass_t *gs_technique_get_pass_by_idx(const gs_technique_t *technique,
						size_t pass);
EXPORT gs_epass_t *
gs_technique_get_pass_by_name(const gs_technique_t *technique,
			      const char *name);

EXPORT size_t gs_effect_get_num_params(const gs_effect_t *effect);
EXPORT gs_eparam_t *gs_effect_get_param_by_idx(const gs_effect_t *effect,
					       size_t param);
EXPORT gs_eparam_t *gs_effect_get_param_by_name(const gs_effect_t *effect,
						const char *name);
EXPORT size_t gs_param_get_num_annotations(const gs_eparam_t *param);
EXPORT gs_eparam_t *gs_param_get_annotation_by_idx(const gs_eparam_t *param,
						   size_t annotation);
EXPORT gs_eparam_t *gs_param_get_annotation_by_name(const gs_eparam_t *param,
						    const char *name);

/** Helper function to simplify effect usage.  Use with a while loop that
 * contains drawing functions.  Automatically handles techniques, passes, and
 * unloading. */
EXPORT bool gs_effect_loop(gs_effect_t *effect, const char *name);

/** used internally */
EXPORT void gs_effect_update_params(gs_effect_t *effect);

EXPORT gs_eparam_t *gs_effect_get_viewproj_matrix(const gs_effect_t *effect);
EXPORT gs_eparam_t *gs_effect_get_world_matrix(const gs_effect_t *effect);

#ifndef SWIG
EXPORT void gs_effect_get_param_info(const gs_eparam_t *param,
				     struct gs_effect_param_info *info);
#endif

EXPORT void gs_effect_set_bool(gs_eparam_t *param, bool val);
EXPORT void gs_effect_set_float(gs_eparam_t *param, float val);
EXPORT void gs_effect_set_int(gs_eparam_t *param, int val);
EXPORT void gs_effect_set_matrix4(gs_eparam_t *param,
				  const struct matrix4 *val);
EXPORT void gs_effect_set_vec2(gs_eparam_t *param, const struct vec2 *val);
EXPORT void gs_effect_set_vec3(gs_eparam_t *param, const struct vec3 *val);
EXPORT void gs_effect_set_vec4(gs_eparam_t *param, const struct vec4 *val);
EXPORT void gs_effect_set_texture(gs_eparam_t *param, gs_texture_t *val);
EXPORT void gs_effect_set_texture_srgb(gs_eparam_t *param, gs_texture_t *val);
EXPORT void gs_effect_set_val(gs_eparam_t *param, const void *val, size_t size);
EXPORT void gs_effect_set_default(gs_eparam_t *param);
EXPORT size_t gs_effect_get_val_size(gs_eparam_t *param);
EXPORT void *gs_effect_get_val(gs_eparam_t *param);
EXPORT size_t gs_effect_get_default_val_size(gs_eparam_t *param);
EXPORT void *gs_effect_get_default_val(gs_eparam_t *param);
EXPORT void gs_effect_set_next_sampler(gs_eparam_t *param,
				       gs_samplerstate_t *sampler);

EXPORT void gs_effect_set_color(gs_eparam_t *param, uint32_t argb);

/* ---------------------------------------------------
 * texture render helper functions
 * --------------------------------------------------- */

EXPORT gs_texrender_t *gs_texrender_create(enum gs_color_format format,
					   enum gs_zstencil_format zsformat);
EXPORT void gs_texrender_destroy(gs_texrender_t *texrender);
EXPORT bool gs_texrender_begin(gs_texrender_t *texrender, uint32_t cx,
			       uint32_t cy);
EXPORT void gs_texrender_end(gs_texrender_t *texrender);
EXPORT void gs_texrender_reset(gs_texrender_t *texrender);
EXPORT gs_texture_t *gs_texrender_get_texture(const gs_texrender_t *texrender);

/* ---------------------------------------------------
 * graphics subsystem
 * --------------------------------------------------- */

#define GS_BUILD_MIPMAPS (1 << 0)
#define GS_DYNAMIC (1 << 1)
#define GS_RENDER_TARGET (1 << 2)
#define GS_GL_DUMMYTEX (1 << 3) /**<< texture with no allocated texture data */
#define GS_DUP_BUFFER \
	(1 << 4) /**<< do not pass buffer ownership when
				 *    creating a vertex/index buffer */
#define GS_SHARED_TEX (1 << 5)
#define GS_SHARED_KM_TEX (1 << 6)

/* ---------------- */
/* global functions */

#define GS_SUCCESS 0
#define GS_ERROR_FAIL -1
#define GS_ERROR_MODULE_NOT_FOUND -2
#define GS_ERROR_NOT_SUPPORTED -3

struct gs_window {
#if defined(_WIN32)
	void *hwnd;
#elif defined(__APPLE__)
	__unsafe_unretained id view;
#elif defined(__linux__) || defined(__FreeBSD__)
	/* I'm not sure how portable defining id to uint32_t is. */
	uint32_t id;
	void *display;
#endif
};

struct gs_init_data {
	struct gs_window window;
	uint32_t cx, cy;
	uint32_t num_backbuffers;
	enum gs_color_format format;
	enum gs_zstencil_format zsformat;
	uint32_t adapter;
};

#define GS_DEVICE_OPENGL 1
#define GS_DEVICE_DIRECT3D_11 2

EXPORT const char *gs_get_device_name(void);
EXPORT int gs_get_device_type(void);
EXPORT void gs_enum_adapters(bool (*callback)(void *param, const char *name,
					      uint32_t id),
			     void *param);

EXPORT int gs_create(graphics_t **graphics, const char *module,
		     uint32_t adapter);
EXPORT void gs_destroy(graphics_t *graphics);

EXPORT void gs_enter_context(graphics_t *graphics);
EXPORT void gs_leave_context(void);
EXPORT graphics_t *gs_get_context(void);
EXPORT void *gs_get_device_obj(void);

EXPORT void gs_matrix_push(void);
EXPORT void gs_matrix_pop(void);
EXPORT void gs_matrix_identity(void);
EXPORT void gs_matrix_transpose(void);
EXPORT void gs_matrix_set(const struct matrix4 *matrix);
EXPORT void gs_matrix_get(struct matrix4 *dst);
EXPORT void gs_matrix_mul(const struct matrix4 *matrix);
EXPORT void gs_matrix_rotquat(const struct quat *rot);
EXPORT void gs_matrix_rotaa(const struct axisang *rot);
EXPORT void gs_matrix_translate(const struct vec3 *pos);
EXPORT void gs_matrix_scale(const struct vec3 *scale);
EXPORT void gs_matrix_rotaa4f(float x, float y, float z, float angle);
EXPORT void gs_matrix_translate3f(float x, float y, float z);
EXPORT void gs_matrix_scale3f(float x, float y, float z);

EXPORT void gs_render_start(bool b_new);
EXPORT void gs_render_stop(enum gs_draw_mode mode);
EXPORT gs_vertbuffer_t *gs_render_save(void);
EXPORT void gs_vertex2f(float x, float y);
EXPORT void gs_vertex3f(float x, float y, float z);
EXPORT void gs_normal3f(float x, float y, float z);
EXPORT void gs_color(uint32_t color);
EXPORT void gs_texcoord(float x, float y, int unit);
EXPORT void gs_vertex2v(const struct vec2 *v);
EXPORT void gs_vertex3v(const struct vec3 *v);
EXPORT void gs_normal3v(const struct vec3 *v);
EXPORT void gs_color4v(const struct vec4 *v);
EXPORT void gs_texcoord2v(const struct vec2 *v, int unit);

EXPORT input_t *gs_get_input(void);
EXPORT gs_effect_t *gs_get_effect(void);

EXPORT gs_effect_t *gs_effect_create_from_file(const char *file,
					       char **error_string);
EXPORT gs_effect_t *gs_effect_create(const char *effect_string,
				     const char *filename, char **error_string);

EXPORT gs_shader_t *gs_vertexshader_create_from_file(const char *file,
						     char **error_string);
EXPORT gs_shader_t *gs_pixelshader_create_from_file(const char *file,
						    char **error_string);

EXPORT gs_texture_t *gs_texture_create_from_file(const char *file);
EXPORT uint8_t *gs_create_texture_file_data(const char *file,
					    enum gs_color_format *format,
					    uint32_t *cx, uint32_t *cy);

#define GS_FLIP_U (1 << 0)
#define GS_FLIP_V (1 << 1)

/**
 * Draws a 2D sprite
 *
 *   If width or height is 0, the width or height of the texture will be used.
 * The flip value specifies whether the texture should be flipped on the U or V
 * axis with GS_FLIP_U and GS_FLIP_V.
 */
EXPORT void gs_draw_sprite(gs_texture_t *tex, uint32_t flip, uint32_t width,
			   uint32_t height);

EXPORT void gs_draw_sprite_subregion(gs_texture_t *tex, uint32_t flip,
				     uint32_t x, uint32_t y, uint32_t cx,
				     uint32_t cy);

EXPORT void gs_draw_cube_backdrop(gs_texture_t *cubetex, const struct quat *rot,
				  float left, float right, float top,
				  float bottom, float znear);

/** sets the viewport to current swap chain size */
EXPORT void gs_reset_viewport(void);

/** sets default screen-sized orthographic mode */
EXPORT void gs_set_2d_mode(void);
/** sets default screen-sized perspective mode */
EXPORT void gs_set_3d_mode(double fovy, double znear, double zvar);

EXPORT void gs_viewport_push(void);
EXPORT void gs_viewport_pop(void);

EXPORT void gs_texture_set_image(gs_texture_t *tex, const uint8_t *data,
				 uint32_t linesize, bool invert);
EXPORT void gs_cubetexture_set_image(gs_texture_t *cubetex, uint32_t side,
				     const void *data, uint32_t linesize,
				     bool invert);

EXPORT void gs_perspective(float fovy, float aspect, float znear, float zfar);

EXPORT void gs_blend_state_push(void);
EXPORT void gs_blend_state_pop(void);
EXPORT void gs_reset_blend_state(void);

/* -------------------------- */
/* library-specific functions */

EXPORT gs_swapchain_t *gs_swapchain_create(const struct gs_init_data *data);

EXPORT void gs_resize(uint32_t x, uint32_t y);
EXPORT void gs_get_size(uint32_t *x, uint32_t *y);
EXPORT uint32_t gs_get_width(void);
EXPORT uint32_t gs_get_height(void);

EXPORT gs_texture_t *gs_texture_create(uint32_t width, uint32_t height,
				       enum gs_color_format color_format,
				       uint32_t levels, const uint8_t **data,
				       uint32_t flags);
EXPORT gs_texture_t *
gs_cubetexture_create(uint32_t size, enum gs_color_format color_format,
		      uint32_t levels, const uint8_t **data, uint32_t flags);
EXPORT gs_texture_t *gs_voltexture_create(uint32_t width, uint32_t height,
					  uint32_t depth,
					  enum gs_color_format color_format,
					  uint32_t levels, const uint8_t **data,
					  uint32_t flags);

EXPORT gs_zstencil_t *gs_zstencil_create(uint32_t width, uint32_t height,
					 enum gs_zstencil_format format);

EXPORT gs_stagesurf_t *
gs_stagesurface_create(uint32_t width, uint32_t height,
		       enum gs_color_format color_format);

EXPORT gs_samplerstate_t *
gs_samplerstate_create(const struct gs_sampler_info *info);

EXPORT gs_shader_t *gs_vertexshader_create(const char *shader, const char *file,
					   char **error_string);
EXPORT gs_shader_t *gs_pixelshader_create(const char *shader, const char *file,
					  char **error_string);

EXPORT gs_vertbuffer_t *gs_vertexbuffer_create(struct gs_vb_data *data,
					       uint32_t flags);
EXPORT gs_indexbuffer_t *gs_indexbuffer_create(enum gs_index_type type,
					       void *indices, size_t num,
					       uint32_t flags);

EXPORT gs_timer_t *gs_timer_create();
EXPORT gs_timer_range_t *gs_timer_range_create();

EXPORT enum gs_texture_type gs_get_texture_type(const gs_texture_t *texture);

EXPORT void gs_load_vertexbuffer(gs_vertbuffer_t *vertbuffer);
EXPORT void gs_load_indexbuffer(gs_indexbuffer_t *indexbuffer);
EXPORT void gs_load_texture(gs_texture_t *tex, int unit);
EXPORT void gs_load_samplerstate(gs_samplerstate_t *samplerstate, int unit);
EXPORT void gs_load_vertexshader(gs_shader_t *vertshader);
EXPORT void gs_load_pixelshader(gs_shader_t *pixelshader);

EXPORT void gs_load_default_samplerstate(bool b_3d, int unit);

EXPORT gs_shader_t *gs_get_vertex_shader(void);
EXPORT gs_shader_t *gs_get_pixel_shader(void);

EXPORT gs_texture_t *gs_get_render_target(void);
EXPORT gs_zstencil_t *gs_get_zstencil_target(void);

EXPORT void gs_set_render_target(gs_texture_t *tex, gs_zstencil_t *zstencil);
EXPORT void gs_set_cube_render_target(gs_texture_t *cubetex, int side,
				      gs_zstencil_t *zstencil);

EXPORT void gs_enable_framebuffer_srgb(bool enable);
EXPORT bool gs_framebuffer_srgb_enabled(void);

EXPORT bool gs_get_linear_srgb(void);
EXPORT bool gs_set_linear_srgb(bool linear_srgb);

EXPORT void gs_copy_texture(gs_texture_t *dst, gs_texture_t *src);
EXPORT void gs_copy_texture_region(gs_texture_t *dst, uint32_t dst_x,
				   uint32_t dst_y, gs_texture_t *src,
				   uint32_t src_x, uint32_t src_y,
				   uint32_t src_w, uint32_t src_h);
EXPORT void gs_stage_texture(gs_stagesurf_t *dst, gs_texture_t *src);

EXPORT void gs_begin_frame(void);
EXPORT void gs_begin_scene(void);
EXPORT void gs_draw(enum gs_draw_mode draw_mode, uint32_t start_vert,
		    uint32_t num_verts);
EXPORT void gs_end_scene(void);

#define GS_CLEAR_COLOR (1 << 0)
#define GS_CLEAR_DEPTH (1 << 1)
#define GS_CLEAR_STENCIL (1 << 2)

EXPORT void gs_load_swapchain(gs_swapchain_t *swapchain);
EXPORT void gs_clear(uint32_t clear_flags, const struct vec4 *color,
		     float depth, uint8_t stencil);
EXPORT void gs_present(void);
EXPORT void gs_flush(void);

EXPORT void gs_set_cull_mode(enum gs_cull_mode mode);
EXPORT enum gs_cull_mode gs_get_cull_mode(void);

EXPORT void gs_enable_blending(bool enable);
EXPORT void gs_enable_depth_test(bool enable);
EXPORT void gs_enable_stencil_test(bool enable);
EXPORT void gs_enable_stencil_write(bool enable);
EXPORT void gs_enable_color(bool red, bool green, bool blue, bool alpha);

EXPORT void gs_blend_function(enum gs_blend_type src, enum gs_blend_type dest);
EXPORT void gs_blend_function_separate(enum gs_blend_type src_c,
				       enum gs_blend_type dest_c,
				       enum gs_blend_type src_a,
				       enum gs_blend_type dest_a);
EXPORT void gs_depth_function(enum gs_depth_test test);

EXPORT void gs_stencil_function(enum gs_stencil_side side,
				enum gs_depth_test test);
EXPORT void gs_stencil_op(enum gs_stencil_side side,
			  enum gs_stencil_op_type fail,
			  enum gs_stencil_op_type zfail,
			  enum gs_stencil_op_type zpass);

EXPORT void gs_set_viewport(int x, int y, int width, int height);
EXPORT void gs_get_viewport(struct gs_rect *rect);
EXPORT void gs_set_scissor_rect(const struct gs_rect *rect);

EXPORT void gs_ortho(float left, float right, float top, float bottom,
		     float znear, float zfar);
EXPORT void gs_frustum(float left, float right, float top, float bottom,
		       float znear, float zfar);

EXPORT void gs_projection_push(void);
EXPORT void gs_projection_pop(void);

EXPORT void gs_swapchain_destroy(gs_swapchain_t *swapchain);

EXPORT void gs_texture_destroy(gs_texture_t *tex);
EXPORT uint32_t gs_texture_get_width(const gs_texture_t *tex);
EXPORT uint32_t gs_texture_get_height(const gs_texture_t *tex);
EXPORT enum gs_color_format
gs_texture_get_color_format(const gs_texture_t *tex);
EXPORT bool gs_texture_map(gs_texture_t *tex, uint8_t **ptr,
			   uint32_t *linesize);
EXPORT void gs_texture_unmap(gs_texture_t *tex);
/** special-case function (GL only) - specifies whether the texture is a
 * GL_TEXTURE_RECTANGLE type, which doesn't use normalized texture
 * coordinates, doesn't support mipmapping, and requires address clamping */
EXPORT bool gs_texture_is_rect(const gs_texture_t *tex);
/**
 * Gets a pointer to the context-specific object associated with the texture.
 * For example, for GL, this is a GLuint*.  For D3D11, ID3D11Texture2D*.
 */
EXPORT void *gs_texture_get_obj(gs_texture_t *tex);

EXPORT void gs_cubetexture_destroy(gs_texture_t *cubetex);
EXPORT uint32_t gs_cubetexture_get_size(const gs_texture_t *cubetex);
EXPORT enum gs_color_format
gs_cubetexture_get_color_format(const gs_texture_t *cubetex);

EXPORT void gs_voltexture_destroy(gs_texture_t *voltex);
EXPORT uint32_t gs_voltexture_get_width(const gs_texture_t *voltex);
EXPORT uint32_t gs_voltexture_get_height(const gs_texture_t *voltex);
EXPORT uint32_t gs_voltexture_get_depth(const gs_texture_t *voltex);
EXPORT enum gs_color_format
gs_voltexture_get_color_format(const gs_texture_t *voltex);

EXPORT void gs_stagesurface_destroy(gs_stagesurf_t *stagesurf);
EXPORT uint32_t gs_stagesurface_get_width(const gs_stagesurf_t *stagesurf);
EXPORT uint32_t gs_stagesurface_get_height(const gs_stagesurf_t *stagesurf);
EXPORT enum gs_color_format
gs_stagesurface_get_color_format(const gs_stagesurf_t *stagesurf);
EXPORT bool gs_stagesurface_map(gs_stagesurf_t *stagesurf, uint8_t **data,
				uint32_t *linesize);
EXPORT void gs_stagesurface_unmap(gs_stagesurf_t *stagesurf);

EXPORT void gs_zstencil_destroy(gs_zstencil_t *zstencil);

EXPORT void gs_samplerstate_destroy(gs_samplerstate_t *samplerstate);

EXPORT void gs_vertexbuffer_destroy(gs_vertbuffer_t *vertbuffer);
EXPORT void gs_vertexbuffer_flush(gs_vertbuffer_t *vertbuffer);
EXPORT void gs_vertexbuffer_flush_direct(gs_vertbuffer_t *vertbuffer,
					 const struct gs_vb_data *data);
EXPORT struct gs_vb_data *
gs_vertexbuffer_get_data(const gs_vertbuffer_t *vertbuffer);

EXPORT void gs_indexbuffer_destroy(gs_indexbuffer_t *indexbuffer);
EXPORT void gs_indexbuffer_flush(gs_indexbuffer_t *indexbuffer);
EXPORT void gs_indexbuffer_flush_direct(gs_indexbuffer_t *indexbuffer,
					const void *data);
EXPORT void *gs_indexbuffer_get_data(const gs_indexbuffer_t *indexbuffer);
EXPORT size_t
gs_indexbuffer_get_num_indices(const gs_indexbuffer_t *indexbuffer);
EXPORT enum gs_index_type
gs_indexbuffer_get_type(const gs_indexbuffer_t *indexbuffer);

EXPORT void gs_timer_destroy(gs_timer_t *timer);
EXPORT void gs_timer_begin(gs_timer_t *timer);
EXPORT void gs_timer_end(gs_timer_t *timer);
EXPORT bool gs_timer_get_data(gs_timer_t *timer, uint64_t *ticks);
EXPORT void gs_timer_range_destroy(gs_timer_range_t *timer);
EXPORT void gs_timer_range_begin(gs_timer_range_t *range);
EXPORT void gs_timer_range_end(gs_timer_range_t *range);
EXPORT bool gs_timer_range_get_data(gs_timer_range_t *range, bool *disjoint,
				    uint64_t *frequency);

EXPORT bool gs_nv12_available(void);

#define GS_USE_DEBUG_MARKERS 0
#if GS_USE_DEBUG_MARKERS
static const float GS_DEBUG_COLOR_DEFAULT[] = {0.5f, 0.5f, 0.5f, 1.0f};
static const float GS_DEBUG_COLOR_RENDER_VIDEO[] = {0.0f, 0.5f, 0.0f, 1.0f};
static const float GS_DEBUG_COLOR_MAIN_TEXTURE[] = {0.0f, 0.25f, 0.0f, 1.0f};
static const float GS_DEBUG_COLOR_DISPLAY[] = {0.0f, 0.5f, 0.5f, 1.0f};
static const float GS_DEBUG_COLOR_SOURCE[] = {0.0f, 0.5f, 5.0f, 1.0f};
static const float GS_DEBUG_COLOR_ITEM[] = {0.5f, 0.0f, 0.0f, 1.0f};
static const float GS_DEBUG_COLOR_ITEM_TEXTURE[] = {0.25f, 0.0f, 0.0f, 1.0f};
static const float GS_DEBUG_COLOR_CONVERT_FORMAT[] = {0.5f, 0.5f, 0.0f, 1.0f};
#define GS_DEBUG_MARKER_BEGIN(color, markername) \
	gs_debug_marker_begin(color, markername)
#define GS_DEBUG_MARKER_BEGIN_FORMAT(color, format, ...) \
	gs_debug_marker_begin_format(color, format, __VA_ARGS__)
#define GS_DEBUG_MARKER_END() gs_debug_marker_end()
#else
#define GS_DEBUG_MARKER_BEGIN(color, markername) ((void)0)
#define GS_DEBUG_MARKER_BEGIN_FORMAT(color, format, ...) ((void)0)
#define GS_DEBUG_MARKER_END() ((void)0)
#endif

EXPORT void gs_debug_marker_begin(const float color[4], const char *markername);
EXPORT void gs_debug_marker_begin_format(const float color[4],
					 const char *format, ...);
EXPORT void gs_debug_marker_end(void);

#ifdef __APPLE__

/** platform specific function for creating (GL_TEXTURE_RECTANGLE) textures
 * from shared surface resources */
EXPORT gs_texture_t *gs_texture_create_from_iosurface(void *iosurf);
EXPORT bool gs_texture_rebind_iosurface(gs_texture_t *texture, void *iosurf);
EXPORT gs_texture_t *gs_texture_open_shared(uint32_t handle);
EXPORT bool gs_shared_texture_available(void);

#elif _WIN32

EXPORT bool gs_gdi_texture_available(void);
EXPORT bool gs_shared_texture_available(void);

struct gs_duplicator;
typedef struct gs_duplicator gs_duplicator_t;

/**
 * Gets information about the monitor at the specific index, returns false
 * when there is no monitor at the specified index
 */
EXPORT bool
gs_get_duplicator_monitor_info(int monitor_idx,
			       struct gs_monitor_info *monitor_info);

EXPORT int gs_duplicator_get_monitor_index(void *monitor);

/** creates a windows 8+ output duplicator (monitor capture) */
EXPORT gs_duplicator_t *gs_duplicator_create(int monitor_idx);
EXPORT void gs_duplicator_destroy(gs_duplicator_t *duplicator);

EXPORT bool gs_duplicator_update_frame(gs_duplicator_t *duplicator);
EXPORT gs_texture_t *gs_duplicator_get_texture(gs_duplicator_t *duplicator);

/** creates a windows GDI-lockable texture */
EXPORT gs_texture_t *gs_texture_create_gdi(uint32_t width, uint32_t height);

EXPORT void *gs_texture_get_dc(gs_texture_t *gdi_tex);
EXPORT void gs_texture_release_dc(gs_texture_t *gdi_tex);

/** creates a windows shared texture from a texture handle */
EXPORT gs_texture_t *gs_texture_open_shared(uint32_t handle);

#define GS_INVALID_HANDLE (uint32_t) - 1
EXPORT uint32_t gs_texture_get_shared_handle(gs_texture_t *tex);

EXPORT gs_texture_t *gs_texture_wrap_obj(void *obj);

#define GS_WAIT_INFINITE (uint32_t) - 1

/**
 * acquires a lock on a keyed mutex texture.
 * returns -1 on generic failure, ETIMEDOUT if timed out
 */
EXPORT int gs_texture_acquire_sync(gs_texture_t *tex, uint64_t key,
				   uint32_t ms);

/**
 * releases a lock on a keyed mutex texture to another device.
 * return 0 on success, -1 on error
 */
EXPORT int gs_texture_release_sync(gs_texture_t *tex, uint64_t key);

EXPORT bool gs_texture_create_nv12(gs_texture_t **tex_y, gs_texture_t **tex_uv,
				   uint32_t width, uint32_t height,
				   uint32_t flags);

EXPORT gs_stagesurf_t *gs_stagesurface_create_nv12(uint32_t width,
						   uint32_t height);

EXPORT void gs_register_loss_callbacks(const struct gs_device_loss *callbacks);
EXPORT void gs_unregister_loss_callbacks(void *data);

#elif __linux__

EXPORT gs_texture_t *
gs_texture_create_from_dmabuf(unsigned int width, unsigned int height,
			      enum gs_color_format color_format,
			      uint32_t n_planes, const int *fds,
			      const uint32_t *strides, const uint32_t *offsets,
			      const uint64_t *modifiers);

#endif

/* inline functions used by modules */

static inline uint32_t gs_get_format_bpp(enum gs_color_format format)
{
	switch (format) {
	case GS_A8:
		return 8;
	case GS_R8:
		return 8;
	case GS_RGBA:
		return 32;
	case GS_BGRX:
		return 32;
	case GS_BGRA:
		return 32;
	case GS_R10G10B10A2:
		return 32;
	case GS_RGBA16:
		return 64;
	case GS_R16:
		return 16;
	case GS_RGBA16F:
		return 64;
	case GS_RGBA32F:
		return 128;
	case GS_RG16F:
		return 32;
	case GS_RG32F:
		return 64;
	case GS_R16F:
		return 16;
	case GS_R32F:
		return 32;
	case GS_DXT1:
		return 4;
	case GS_DXT3:
		return 8;
	case GS_DXT5:
		return 8;
	case GS_R8G8:
		return 16;
	case GS_RGBA_UNORM:
		return 32;
	case GS_BGRX_UNORM:
		return 32;
	case GS_BGRA_UNORM:
		return 32;
	case GS_UNKNOWN:
		return 0;
	}

	return 0;
}

static inline bool gs_is_compressed_format(enum gs_color_format format)
{
	return (format == GS_DXT1 || format == GS_DXT3 || format == GS_DXT5);
}

static inline bool gs_is_srgb_format(enum gs_color_format format)
{
	switch (format) {
	case GS_RGBA:
	case GS_BGRX:
	case GS_BGRA:
		return true;
	default:
		return false;
	}
}

static inline uint32_t gs_get_total_levels(uint32_t width, uint32_t height,
					   uint32_t depth)
{
	uint32_t size = width > height ? width : height;
	size = size > depth ? size : depth;
	uint32_t num_levels = 1;

	while (size > 1) {
		size /= 2;
		num_levels++;
	}

	return num_levels;
}

#ifdef __cplusplus
}
#endif
