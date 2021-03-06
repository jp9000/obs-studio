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

#include <util/darray.h>
#include <util/threading.h>
#include <graphics/graphics.h>
#include <graphics/device-exports.h>
#include <graphics/matrix4.h>

#include <glad/glad.h>

#include "gl-helpers.h"

struct gl_platform;
struct gl_windowinfo;

enum copy_type { COPY_TYPE_ARB, COPY_TYPE_NV, COPY_TYPE_FBO_BLIT };

static inline GLenum convert_gs_format(enum gs_color_format format)
{
	switch (format) {
	case GS_A8:
		return GL_RED;
	case GS_R8:
		return GL_RED;
	case GS_RGBA:
		return GL_RGBA;
	case GS_BGRX:
		return GL_BGRA;
	case GS_BGRA:
		return GL_BGRA;
	case GS_R10G10B10A2:
		return GL_RGBA;
	case GS_RGBA16:
		return GL_RGBA;
	case GS_R16:
		return GL_RED;
	case GS_RGBA16F:
		return GL_RGBA;
	case GS_RGBA32F:
		return GL_RGBA;
	case GS_RG16F:
		return GL_RG;
	case GS_RG32F:
		return GL_RG;
	case GS_R8G8:
		return GL_RG;
	case GS_R16F:
		return GL_RED;
	case GS_R32F:
		return GL_RED;
	case GS_DXT1:
		return GL_RGB;
	case GS_DXT3:
		return GL_RGBA;
	case GS_DXT5:
		return GL_RGBA;
	case GS_RGBA_UNORM:
		return GL_RGBA;
	case GS_BGRX_UNORM:
		return GL_BGRA;
	case GS_BGRA_UNORM:
		return GL_BGRA;
	case GS_UNKNOWN:
		return 0;
	}

	return 0;
}

static inline GLenum convert_gs_internal_format(enum gs_color_format format)
{
	switch (format) {
	case GS_A8:
		return GL_R8; /* NOTE: use GL_TEXTURE_SWIZZLE_x */
	case GS_R8:
		return GL_R8;
	case GS_RGBA:
		return GL_SRGB8_ALPHA8;
	case GS_BGRX:
		return GL_SRGB8;
	case GS_BGRA:
		return GL_SRGB8_ALPHA8;
	case GS_R10G10B10A2:
		return GL_RGB10_A2;
	case GS_RGBA16:
		return GL_RGBA16;
	case GS_R16:
		return GL_R16;
	case GS_RGBA16F:
		return GL_RGBA16F;
	case GS_RGBA32F:
		return GL_RGBA32F;
	case GS_RG16F:
		return GL_RG16F;
	case GS_RG32F:
		return GL_RG32F;
	case GS_R8G8:
		return GL_RG8;
	case GS_R16F:
		return GL_R16F;
	case GS_R32F:
		return GL_R32F;
	case GS_DXT1:
		return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
	case GS_DXT3:
		return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
	case GS_DXT5:
		return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
	case GS_RGBA_UNORM:
		return GL_RGBA;
	case GS_BGRX_UNORM:
		return GL_RGB;
	case GS_BGRA_UNORM:
		return GL_RGBA;
	case GS_UNKNOWN:
		return 0;
	}

	return 0;
}

static inline GLenum get_gl_format_type(enum gs_color_format format)
{
	switch (format) {
	case GS_A8:
		return GL_UNSIGNED_BYTE;
	case GS_R8:
		return GL_UNSIGNED_BYTE;
	case GS_RGBA:
		return GL_UNSIGNED_BYTE;
	case GS_BGRX:
		return GL_UNSIGNED_BYTE;
	case GS_BGRA:
		return GL_UNSIGNED_BYTE;
	case GS_R10G10B10A2:
		return GL_UNSIGNED_INT_10_10_10_2;
	case GS_RGBA16:
		return GL_UNSIGNED_SHORT;
	case GS_R16:
		return GL_UNSIGNED_SHORT;
	case GS_RGBA16F:
		return GL_HALF_FLOAT;
	case GS_RGBA32F:
		return GL_FLOAT;
	case GS_RG16F:
		return GL_HALF_FLOAT;
	case GS_RG32F:
		return GL_FLOAT;
	case GS_R8G8:
		return GL_UNSIGNED_BYTE;
	case GS_R16F:
		return GL_HALF_FLOAT;
	case GS_R32F:
		return GL_FLOAT;
	case GS_DXT1:
		return GL_UNSIGNED_BYTE;
	case GS_DXT3:
		return GL_UNSIGNED_BYTE;
	case GS_DXT5:
		return GL_UNSIGNED_BYTE;
	case GS_RGBA_UNORM:
		return GL_UNSIGNED_BYTE;
	case GS_BGRX_UNORM:
		return GL_UNSIGNED_BYTE;
	case GS_BGRA_UNORM:
		return GL_UNSIGNED_BYTE;
	case GS_UNKNOWN:
		return 0;
	}

	return GL_UNSIGNED_BYTE;
}

static inline GLenum convert_zstencil_format(enum gs_zstencil_format format)
{
	switch (format) {
	case GS_Z16:
		return GL_DEPTH_COMPONENT16;
	case GS_Z24_S8:
		return GL_DEPTH24_STENCIL8;
	case GS_Z32F:
		return GL_DEPTH_COMPONENT32F;
	case GS_Z32F_S8X24:
		return GL_DEPTH32F_STENCIL8;
	case GS_ZS_NONE:
		return 0;
	}

	return 0;
}

static inline GLenum convert_gs_depth_test(enum gs_depth_test test)
{
	switch (test) {
	case GS_NEVER:
		return GL_NEVER;
	case GS_LESS:
		return GL_LESS;
	case GS_LEQUAL:
		return GL_LEQUAL;
	case GS_EQUAL:
		return GL_EQUAL;
	case GS_GEQUAL:
		return GL_GEQUAL;
	case GS_GREATER:
		return GL_GREATER;
	case GS_NOTEQUAL:
		return GL_NOTEQUAL;
	case GS_ALWAYS:
		return GL_ALWAYS;
	}

	return GL_NEVER;
}

static inline GLenum convert_gs_stencil_op(enum gs_stencil_op_type op)
{
	switch (op) {
	case GS_KEEP:
		return GL_KEEP;
	case GS_ZERO:
		return GL_ZERO;
	case GS_REPLACE:
		return GL_REPLACE;
	case GS_INCR:
		return GL_INCR;
	case GS_DECR:
		return GL_DECR;
	case GS_INVERT:
		return GL_INVERT;
	}

	return GL_KEEP;
}

static inline GLenum convert_gs_stencil_side(enum gs_stencil_side side)
{
	switch (side) {
	case GS_STENCIL_FRONT:
		return GL_FRONT;
	case GS_STENCIL_BACK:
		return GL_BACK;
	case GS_STENCIL_BOTH:
		return GL_FRONT_AND_BACK;
	}

	return GL_FRONT;
}

static inline GLenum convert_gs_blend_type(enum gs_blend_type type)
{
	switch (type) {
	case GS_BLEND_ZERO:
		return GL_ZERO;
	case GS_BLEND_ONE:
		return GL_ONE;
	case GS_BLEND_SRCCOLOR:
		return GL_SRC_COLOR;
	case GS_BLEND_INVSRCCOLOR:
		return GL_ONE_MINUS_SRC_COLOR;
	case GS_BLEND_SRCALPHA:
		return GL_SRC_ALPHA;
	case GS_BLEND_INVSRCALPHA:
		return GL_ONE_MINUS_SRC_ALPHA;
	case GS_BLEND_DSTCOLOR:
		return GL_DST_COLOR;
	case GS_BLEND_INVDSTCOLOR:
		return GL_ONE_MINUS_DST_COLOR;
	case GS_BLEND_DSTALPHA:
		return GL_DST_ALPHA;
	case GS_BLEND_INVDSTALPHA:
		return GL_ONE_MINUS_DST_ALPHA;
	case GS_BLEND_SRCALPHASAT:
		return GL_SRC_ALPHA_SATURATE;
	}

	return GL_ONE;
}

static inline GLenum convert_shader_type(enum gs_shader_type type)
{
	switch (type) {
	case GS_SHADER_VERTEX:
		return GL_VERTEX_SHADER;
	case GS_SHADER_PIXEL:
		return GL_FRAGMENT_SHADER;
	}

	return GL_VERTEX_SHADER;
}

static inline void convert_filter(enum gs_sample_filter filter,
				  GLint *min_filter, GLint *mag_filter)
{
	switch (filter) {
	case GS_FILTER_POINT:
		*min_filter = GL_NEAREST_MIPMAP_NEAREST;
		*mag_filter = GL_NEAREST;
		return;
	case GS_FILTER_LINEAR:
		*min_filter = GL_LINEAR_MIPMAP_LINEAR;
		*mag_filter = GL_LINEAR;
		return;
	case GS_FILTER_MIN_MAG_POINT_MIP_LINEAR:
		*min_filter = GL_NEAREST_MIPMAP_LINEAR;
		*mag_filter = GL_NEAREST;
		return;
	case GS_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT:
		*min_filter = GL_NEAREST_MIPMAP_NEAREST;
		*mag_filter = GL_LINEAR;
		return;
	case GS_FILTER_MIN_POINT_MAG_MIP_LINEAR:
		*min_filter = GL_NEAREST_MIPMAP_LINEAR;
		*mag_filter = GL_LINEAR;
		return;
	case GS_FILTER_MIN_LINEAR_MAG_MIP_POINT:
		*min_filter = GL_LINEAR_MIPMAP_NEAREST;
		*mag_filter = GL_NEAREST;
		return;
	case GS_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
		*min_filter = GL_LINEAR_MIPMAP_LINEAR;
		*mag_filter = GL_NEAREST;
		return;
	case GS_FILTER_MIN_MAG_LINEAR_MIP_POINT:
		*min_filter = GL_LINEAR_MIPMAP_NEAREST;
		*mag_filter = GL_LINEAR;
		return;
	case GS_FILTER_ANISOTROPIC:
		*min_filter = GL_LINEAR_MIPMAP_LINEAR;
		*mag_filter = GL_LINEAR;
		return;
	}

	*min_filter = GL_NEAREST_MIPMAP_NEAREST;
	*mag_filter = GL_NEAREST;
}

static inline GLint convert_address_mode(enum gs_address_mode mode)
{
	switch (mode) {
	case GS_ADDRESS_WRAP:
		return GL_REPEAT;
	case GS_ADDRESS_CLAMP:
		return GL_CLAMP_TO_EDGE;
	case GS_ADDRESS_MIRROR:
		return GL_MIRRORED_REPEAT;
	case GS_ADDRESS_BORDER:
		return GL_CLAMP_TO_BORDER;
	case GS_ADDRESS_MIRRORONCE:
		return GL_MIRROR_CLAMP_EXT;
	}

	return GL_REPEAT;
}

static inline GLenum convert_gs_topology(enum gs_draw_mode mode)
{
	switch (mode) {
	case GS_POINTS:
		return GL_POINTS;
	case GS_LINES:
		return GL_LINES;
	case GS_LINESTRIP:
		return GL_LINE_STRIP;
	case GS_TRIS:
		return GL_TRIANGLES;
	case GS_TRISTRIP:
		return GL_TRIANGLE_STRIP;
	}

	return GL_POINTS;
}

extern void convert_sampler_info(struct gs_sampler_state *sampler,
				 const struct gs_sampler_info *info);

struct gs_sampler_state {
	gs_device_t *device;
	volatile long ref;

	GLint min_filter;
	GLint mag_filter;
	GLint address_u;
	GLint address_v;
	GLint address_w;
	GLint max_anisotropy;
};

static inline void samplerstate_addref(gs_samplerstate_t *ss)
{
	os_atomic_inc_long(&ss->ref);
}

static inline void samplerstate_release(gs_samplerstate_t *ss)
{
	if (os_atomic_dec_long(&ss->ref) == 0)
		bfree(ss);
}

struct gs_timer {
	GLuint queries[2];
};

struct gs_shader_param {
	enum gs_shader_param_type type;

	char *name;
	gs_shader_t *shader;
	gs_samplerstate_t *next_sampler;
	GLint texture_id;
	size_t sampler_id;
	int array_count;

	struct gs_texture *texture;
	bool srgb;

	DARRAY(uint8_t) cur_value;
	DARRAY(uint8_t) def_value;
	bool changed;
};

enum attrib_type {
	ATTRIB_POSITION,
	ATTRIB_NORMAL,
	ATTRIB_TANGENT,
	ATTRIB_COLOR,
	ATTRIB_TEXCOORD,
	ATTRIB_TARGET
};

struct shader_attrib {
	char *name;
	size_t index;
	enum attrib_type type;
};

struct gs_shader {
	gs_device_t *device;
	enum gs_shader_type type;
	GLuint obj;

	struct gs_shader_param *viewproj;
	struct gs_shader_param *world;

	DARRAY(struct shader_attrib) attribs;
	DARRAY(struct gs_shader_param) params;
	DARRAY(gs_samplerstate_t *) samplers;
};

struct program_param {
	GLint obj;
	struct gs_shader_param *param;
};

struct gs_program {
	gs_device_t *device;
	GLuint obj;
	struct gs_shader *vertex_shader;
	struct gs_shader *pixel_shader;

	DARRAY(struct program_param) params;
	DARRAY(GLint) attribs;

	struct gs_program **prev_next;
	struct gs_program *next;
};

extern struct gs_program *gs_program_create(struct gs_device *device);
extern void gs_program_destroy(struct gs_program *program);
extern void program_update_params(struct gs_program *shader);

struct gs_vertex_buffer {
	GLuint vao;
	GLuint vertex_buffer;
	GLuint normal_buffer;
	GLuint tangent_buffer;
	GLuint color_buffer;
	DARRAY(GLuint) uv_buffers;
	DARRAY(size_t) uv_sizes;

	gs_device_t *device;
	size_t num;
	bool dynamic;
	struct gs_vb_data *data;
};

extern bool load_vb_buffers(struct gs_program *program,
			    struct gs_vertex_buffer *vb,
			    struct gs_index_buffer *ib);

struct gs_index_buffer {
	GLuint buffer;
	enum gs_index_type type;
	GLuint gl_type;

	gs_device_t *device;
	void *data;
	size_t num;
	size_t width;
	size_t size;
	bool dynamic;
};

struct gs_texture {
	gs_device_t *device;
	enum gs_texture_type type;
	enum gs_color_format format;
	GLenum gl_format;
	GLenum gl_target;
	GLenum gl_internal_format;
	GLenum gl_type;
	GLuint texture;
	uint32_t levels;
	bool is_dynamic;
	bool is_render_target;
	bool is_dummy;
	bool gen_mipmaps;

	gs_samplerstate_t *cur_sampler;
	struct fbo_info *fbo;
};

struct gs_texture_2d {
	struct gs_texture base;

	uint32_t width;
	uint32_t height;
	bool gen_mipmaps;
	GLuint unpack_buffer;
};

struct gs_texture_3d {
	struct gs_texture base;

	uint32_t width;
	uint32_t height;
	uint32_t depth;
	bool gen_mipmaps;
	GLuint unpack_buffer;
};

struct gs_texture_cube {
	struct gs_texture base;

	uint32_t size;
};

struct gs_stage_surface {
	gs_device_t *device;

	enum gs_color_format format;
	uint32_t width;
	uint32_t height;

	uint32_t bytes_per_pixel;
	GLenum gl_format;
	GLint gl_internal_format;
	GLenum gl_type;
	GLuint pack_buffer;
};

struct gs_zstencil_buffer {
	gs_device_t *device;
	GLuint buffer;
	GLuint attachment;
	GLenum format;
};

struct gs_swap_chain {
	gs_device_t *device;
	struct gl_windowinfo *wi;
	struct gs_init_data info;
};

struct fbo_info {
	GLuint fbo;
	uint32_t width;
	uint32_t height;
	enum gs_color_format format;

	gs_texture_t *cur_render_target;
	int cur_render_side;
	gs_zstencil_t *cur_zstencil_buffer;
};

static inline void fbo_info_destroy(struct fbo_info *fbo)
{
	if (fbo) {
		glDeleteFramebuffers(1, &fbo->fbo);
		gl_success("glDeleteFramebuffers");

		bfree(fbo);
	}
}

struct gs_device {
	struct gl_platform *plat;
	enum copy_type copy_type;

	GLuint empty_vao;
	gs_samplerstate_t *raw_load_sampler;

	gs_texture_t *cur_render_target;
	gs_zstencil_t *cur_zstencil_buffer;
	int cur_render_side;
	gs_texture_t *cur_textures[GS_MAX_TEXTURES];
	gs_samplerstate_t *cur_samplers[GS_MAX_TEXTURES];
	gs_vertbuffer_t *cur_vertex_buffer;
	gs_indexbuffer_t *cur_index_buffer;
	gs_shader_t *cur_vertex_shader;
	gs_shader_t *cur_pixel_shader;
	gs_swapchain_t *cur_swap;
	struct gs_program *cur_program;

	struct gs_program *first_program;

	enum gs_cull_mode cur_cull_mode;
	struct gs_rect cur_viewport;

	struct matrix4 cur_proj;
	struct matrix4 cur_view;
	struct matrix4 cur_viewproj;

	DARRAY(struct matrix4) proj_stack;

	struct fbo_info *cur_fbo;
};

extern struct fbo_info *get_fbo(gs_texture_t *tex, uint32_t width,
				uint32_t height);

extern void gl_update(gs_device_t *device);
extern void gl_clear_context(gs_device_t *device);

extern struct gl_platform *gl_platform_create(gs_device_t *device,
					      uint32_t adapter);
extern void gl_platform_destroy(struct gl_platform *platform);

extern bool gl_platform_init_swapchain(struct gs_swap_chain *swap);
extern void gl_platform_cleanup_swapchain(struct gs_swap_chain *swap);

extern struct gl_windowinfo *
gl_windowinfo_create(const struct gs_init_data *info);
extern void gl_windowinfo_destroy(struct gl_windowinfo *wi);

extern void gl_getclientsize(const struct gs_swap_chain *swap, uint32_t *width,
			     uint32_t *height);
