/******************************************************************************
    Copyright (C) 2013-2014 by Hugh Bailey <obs.jim@gmail.com>

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

#include <util/c99defs.h>

#ifdef __cplusplus
extern "C" {
#endif

EXPORT const char *device_get_name(void);
EXPORT int device_get_type(void);
EXPORT bool device_enum_adapters(bool (*callback)(void *param, const char *name,
						  uint32_t id),
				 void *param);
EXPORT const char *device_preprocessor_name(void);
EXPORT int device_create(gs_device_t **device, uint32_t adapter);
EXPORT void device_destroy(gs_device_t *device);
EXPORT void device_enter_context(gs_device_t *device);
EXPORT void device_leave_context(gs_device_t *device);
EXPORT void *device_get_device_obj(gs_device_t *device);
EXPORT gs_swapchain_t *device_swapchain_create(gs_device_t *device,
					       const struct gs_init_data *data);
EXPORT void device_resize(gs_device_t *device, uint32_t x, uint32_t y);
EXPORT void device_get_size(const gs_device_t *device, uint32_t *x,
			    uint32_t *y);
EXPORT uint32_t device_get_width(const gs_device_t *device);
EXPORT uint32_t device_get_height(const gs_device_t *device);
EXPORT gs_texture_t *
device_texture_create(gs_device_t *device, uint32_t width, uint32_t height,
		      enum gs_color_format color_format, uint32_t levels,
		      const uint8_t **data, uint32_t flags);
EXPORT gs_texture_t *
device_cubetexture_create(gs_device_t *device, uint32_t size,
			  enum gs_color_format color_format, uint32_t levels,
			  const uint8_t **data, uint32_t flags);
EXPORT gs_texture_t *
device_voltexture_create(gs_device_t *device, uint32_t width, uint32_t height,
			 uint32_t depth, enum gs_color_format color_format,
			 uint32_t levels, const uint8_t *const *data,
			 uint32_t flags);
EXPORT gs_zstencil_t *device_zstencil_create(gs_device_t *device,
					     uint32_t width, uint32_t height,
					     enum gs_zstencil_format format);
EXPORT gs_stagesurf_t *
device_stagesurface_create(gs_device_t *device, uint32_t width, uint32_t height,
			   enum gs_color_format color_format);
EXPORT gs_samplerstate_t *
device_samplerstate_create(gs_device_t *device,
			   const struct gs_sampler_info *info);
EXPORT gs_shader_t *device_vertexshader_create(gs_device_t *device,
					       const char *shader,
					       const char *file,
					       char **error_string);
EXPORT gs_shader_t *device_pixelshader_create(gs_device_t *device,
					      const char *shader,
					      const char *file,
					      char **error_string);
EXPORT gs_vertbuffer_t *device_vertexbuffer_create(gs_device_t *device,
						   struct gs_vb_data *data,
						   uint32_t flags);
EXPORT gs_indexbuffer_t *device_indexbuffer_create(gs_device_t *device,
						   enum gs_index_type type,
						   void *indices, size_t num,
						   uint32_t flags);
EXPORT gs_timer_t *device_timer_create(gs_device_t *device);
EXPORT gs_timer_range_t *device_timer_range_create(gs_device_t *device);
EXPORT enum gs_texture_type
device_get_texture_type(const gs_texture_t *texture);
EXPORT void device_load_vertexbuffer(gs_device_t *device,
				     gs_vertbuffer_t *vertbuffer);
EXPORT void device_load_indexbuffer(gs_device_t *device,
				    gs_indexbuffer_t *indexbuffer);
EXPORT void device_load_texture(gs_device_t *device, gs_texture_t *tex,
				int unit);
EXPORT void device_load_texture_srgb(gs_device_t *device, gs_texture_t *tex,
				     int unit);
EXPORT void device_load_samplerstate(gs_device_t *device,
				     gs_samplerstate_t *samplerstate, int unit);
EXPORT void device_load_vertexshader(gs_device_t *device,
				     gs_shader_t *vertshader);
EXPORT void device_load_pixelshader(gs_device_t *device,
				    gs_shader_t *pixelshader);
EXPORT void device_load_default_samplerstate(gs_device_t *device, bool b_3d,
					     int unit);
EXPORT gs_shader_t *device_get_vertex_shader(const gs_device_t *device);
EXPORT gs_shader_t *device_get_pixel_shader(const gs_device_t *device);
EXPORT gs_texture_t *device_get_render_target(const gs_device_t *device);
EXPORT gs_zstencil_t *device_get_zstencil_target(const gs_device_t *device);
EXPORT void device_set_render_target(gs_device_t *device, gs_texture_t *tex,
				     gs_zstencil_t *zstencil);
EXPORT void device_set_cube_render_target(gs_device_t *device,
					  gs_texture_t *cubetex, int side,
					  gs_zstencil_t *zstencil);
EXPORT void device_enable_framebuffer_srgb(gs_device_t *device, bool enable);
EXPORT bool device_framebuffer_srgb_enabled(gs_device_t *device);
EXPORT void device_copy_texture(gs_device_t *device, gs_texture_t *dst,
				gs_texture_t *src);
EXPORT void device_copy_texture_region(gs_device_t *device, gs_texture_t *dst,
				       uint32_t dst_x, uint32_t dst_y,
				       gs_texture_t *src, uint32_t src_x,
				       uint32_t src_y, uint32_t src_w,
				       uint32_t src_h);
EXPORT void device_stage_texture(gs_device_t *device, gs_stagesurf_t *dst,
				 gs_texture_t *src);
EXPORT void device_begin_frame(gs_device_t *device);
EXPORT void device_begin_scene(gs_device_t *device);
EXPORT void device_draw(gs_device_t *device, enum gs_draw_mode draw_mode,
			uint32_t start_vert, uint32_t num_verts);
EXPORT void device_end_scene(gs_device_t *device);
EXPORT void device_load_swapchain(gs_device_t *device,
				  gs_swapchain_t *swapchain);
EXPORT void device_clear(gs_device_t *device, uint32_t clear_flags,
			 const struct vec4 *color, float depth,
			 uint8_t stencil);
EXPORT void device_present(gs_device_t *device);
EXPORT void device_flush(gs_device_t *device);
EXPORT void device_set_cull_mode(gs_device_t *device, enum gs_cull_mode mode);
EXPORT enum gs_cull_mode device_get_cull_mode(const gs_device_t *device);
EXPORT void device_enable_blending(gs_device_t *device, bool enable);
EXPORT void device_enable_depth_test(gs_device_t *device, bool enable);
EXPORT void device_enable_stencil_test(gs_device_t *device, bool enable);
EXPORT void device_enable_stencil_write(gs_device_t *device, bool enable);
EXPORT void device_enable_color(gs_device_t *device, bool red, bool green,
				bool blue, bool alpha);
EXPORT void device_blend_function(gs_device_t *device, enum gs_blend_type src,
				  enum gs_blend_type dest);
EXPORT void device_blend_function_separate(gs_device_t *device,
					   enum gs_blend_type src_c,
					   enum gs_blend_type dest_c,
					   enum gs_blend_type src_a,
					   enum gs_blend_type dest_a);
EXPORT void device_depth_function(gs_device_t *device, enum gs_depth_test test);
EXPORT void device_stencil_function(gs_device_t *device,
				    enum gs_stencil_side side,
				    enum gs_depth_test test);
EXPORT void device_stencil_op(gs_device_t *device, enum gs_stencil_side side,
			      enum gs_stencil_op_type fail,
			      enum gs_stencil_op_type zfail,
			      enum gs_stencil_op_type zpass);
EXPORT void device_set_viewport(gs_device_t *device, int x, int y, int width,
				int height);
EXPORT void device_get_viewport(const gs_device_t *device,
				struct gs_rect *rect);
EXPORT void device_set_scissor_rect(gs_device_t *device,
				    const struct gs_rect *rect);
EXPORT void device_ortho(gs_device_t *device, float left, float right,
			 float top, float bottom, float znear, float zfar);
EXPORT void device_frustum(gs_device_t *device, float left, float right,
			   float top, float bottom, float znear, float zfar);
EXPORT void device_projection_push(gs_device_t *device);
EXPORT void device_projection_pop(gs_device_t *device);
EXPORT void device_debug_marker_begin(gs_device_t *device,
				      const char *markername,
				      const float color[4]);
EXPORT void device_debug_marker_end(gs_device_t *device);

#if __linux__

EXPORT gs_texture_t *device_texture_create_from_dmabuf(
	gs_device_t *device, unsigned int width, unsigned int height,
	enum gs_color_format color_format, uint32_t n_planes, const int *fds,
	const uint32_t *strides, const uint32_t *offsets,
	const uint64_t *modifiers);

#endif

#ifdef __cplusplus
}
#endif
