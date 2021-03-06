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

#include "../util/base.h"
#include "../util/dstr.h"
#include "../util/platform.h"
#include "graphics-internal.h"

#define GRAPHICS_IMPORT(func)                                     \
	do {                                                      \
		exports->func = os_dlsym(module, #func);          \
		if (!exports->func) {                             \
			success = false;                          \
			blog(LOG_ERROR,                           \
			     "Could not load function '%s' from " \
			     "module '%s'",                       \
			     #func, module_name);                 \
		}                                                 \
	} while (false)

#define GRAPHICS_IMPORT_OPTIONAL(func)                   \
	do {                                             \
		exports->func = os_dlsym(module, #func); \
	} while (false)

bool load_graphics_imports(struct gs_exports *exports, void *module,
			   const char *module_name)
{
	bool success = true;

	GRAPHICS_IMPORT(device_get_name);
	GRAPHICS_IMPORT(device_get_type);
	GRAPHICS_IMPORT_OPTIONAL(device_enum_adapters);
	GRAPHICS_IMPORT(device_preprocessor_name);
	GRAPHICS_IMPORT(device_create);
	GRAPHICS_IMPORT(device_destroy);
	GRAPHICS_IMPORT(device_enter_context);
	GRAPHICS_IMPORT(device_leave_context);
	GRAPHICS_IMPORT(device_get_device_obj);
	GRAPHICS_IMPORT(device_swapchain_create);
	GRAPHICS_IMPORT(device_resize);
	GRAPHICS_IMPORT(device_get_size);
	GRAPHICS_IMPORT(device_get_width);
	GRAPHICS_IMPORT(device_get_height);
	GRAPHICS_IMPORT(device_texture_create);
	GRAPHICS_IMPORT(device_cubetexture_create);
	GRAPHICS_IMPORT(device_voltexture_create);
	GRAPHICS_IMPORT(device_zstencil_create);
	GRAPHICS_IMPORT(device_stagesurface_create);
	GRAPHICS_IMPORT(device_samplerstate_create);
	GRAPHICS_IMPORT(device_vertexshader_create);
	GRAPHICS_IMPORT(device_pixelshader_create);
	GRAPHICS_IMPORT(device_vertexbuffer_create);
	GRAPHICS_IMPORT(device_indexbuffer_create);
	GRAPHICS_IMPORT(device_timer_create);
	GRAPHICS_IMPORT(device_timer_range_create);
	GRAPHICS_IMPORT(device_get_texture_type);
	GRAPHICS_IMPORT(device_load_vertexbuffer);
	GRAPHICS_IMPORT(device_load_indexbuffer);
	GRAPHICS_IMPORT(device_load_texture);
	GRAPHICS_IMPORT(device_load_samplerstate);
	GRAPHICS_IMPORT(device_load_vertexshader);
	GRAPHICS_IMPORT(device_load_pixelshader);
	GRAPHICS_IMPORT(device_load_default_samplerstate);
	GRAPHICS_IMPORT(device_get_vertex_shader);
	GRAPHICS_IMPORT(device_get_pixel_shader);
	GRAPHICS_IMPORT(device_get_render_target);
	GRAPHICS_IMPORT(device_get_zstencil_target);
	GRAPHICS_IMPORT(device_set_render_target);
	GRAPHICS_IMPORT(device_set_cube_render_target);
	GRAPHICS_IMPORT(device_enable_framebuffer_srgb);
	GRAPHICS_IMPORT(device_framebuffer_srgb_enabled);
	GRAPHICS_IMPORT(device_copy_texture_region);
	GRAPHICS_IMPORT(device_copy_texture);
	GRAPHICS_IMPORT(device_stage_texture);
	GRAPHICS_IMPORT(device_begin_frame);
	GRAPHICS_IMPORT(device_begin_scene);
	GRAPHICS_IMPORT(device_draw);
	GRAPHICS_IMPORT(device_load_swapchain);
	GRAPHICS_IMPORT(device_end_scene);
	GRAPHICS_IMPORT(device_clear);
	GRAPHICS_IMPORT(device_present);
	GRAPHICS_IMPORT(device_flush);
	GRAPHICS_IMPORT(device_set_cull_mode);
	GRAPHICS_IMPORT(device_get_cull_mode);
	GRAPHICS_IMPORT(device_enable_blending);
	GRAPHICS_IMPORT(device_enable_depth_test);
	GRAPHICS_IMPORT(device_enable_stencil_test);
	GRAPHICS_IMPORT(device_enable_stencil_write);
	GRAPHICS_IMPORT(device_enable_color);
	GRAPHICS_IMPORT(device_blend_function);
	GRAPHICS_IMPORT(device_blend_function_separate);
	GRAPHICS_IMPORT(device_depth_function);
	GRAPHICS_IMPORT(device_stencil_function);
	GRAPHICS_IMPORT(device_stencil_op);
	GRAPHICS_IMPORT(device_set_viewport);
	GRAPHICS_IMPORT(device_get_viewport);
	GRAPHICS_IMPORT(device_set_scissor_rect);
	GRAPHICS_IMPORT(device_ortho);
	GRAPHICS_IMPORT(device_frustum);
	GRAPHICS_IMPORT(device_projection_push);
	GRAPHICS_IMPORT(device_projection_pop);

	GRAPHICS_IMPORT(gs_swapchain_destroy);

	GRAPHICS_IMPORT(gs_texture_destroy);
	GRAPHICS_IMPORT(gs_texture_get_width);
	GRAPHICS_IMPORT(gs_texture_get_height);
	GRAPHICS_IMPORT(gs_texture_get_color_format);
	GRAPHICS_IMPORT(gs_texture_map);
	GRAPHICS_IMPORT(gs_texture_unmap);
	GRAPHICS_IMPORT_OPTIONAL(gs_texture_is_rect);
	GRAPHICS_IMPORT(gs_texture_get_obj);

	GRAPHICS_IMPORT(gs_cubetexture_destroy);
	GRAPHICS_IMPORT(gs_cubetexture_get_size);
	GRAPHICS_IMPORT(gs_cubetexture_get_color_format);

	GRAPHICS_IMPORT(gs_voltexture_destroy);
	GRAPHICS_IMPORT(gs_voltexture_get_width);
	GRAPHICS_IMPORT(gs_voltexture_get_height);
	GRAPHICS_IMPORT(gs_voltexture_get_depth);
	GRAPHICS_IMPORT(gs_voltexture_get_color_format);

	GRAPHICS_IMPORT(gs_stagesurface_destroy);
	GRAPHICS_IMPORT(gs_stagesurface_get_width);
	GRAPHICS_IMPORT(gs_stagesurface_get_height);
	GRAPHICS_IMPORT(gs_stagesurface_get_color_format);
	GRAPHICS_IMPORT(gs_stagesurface_map);
	GRAPHICS_IMPORT(gs_stagesurface_unmap);

	GRAPHICS_IMPORT(gs_zstencil_destroy);

	GRAPHICS_IMPORT(gs_samplerstate_destroy);

	GRAPHICS_IMPORT(gs_vertexbuffer_destroy);
	GRAPHICS_IMPORT(gs_vertexbuffer_flush);
	GRAPHICS_IMPORT(gs_vertexbuffer_flush_direct);
	GRAPHICS_IMPORT(gs_vertexbuffer_get_data);

	GRAPHICS_IMPORT(gs_indexbuffer_destroy);
	GRAPHICS_IMPORT(gs_indexbuffer_flush);
	GRAPHICS_IMPORT(gs_indexbuffer_flush_direct);
	GRAPHICS_IMPORT(gs_indexbuffer_get_data);
	GRAPHICS_IMPORT(gs_indexbuffer_get_num_indices);
	GRAPHICS_IMPORT(gs_indexbuffer_get_type);

	GRAPHICS_IMPORT(gs_timer_destroy);
	GRAPHICS_IMPORT(gs_timer_begin);
	GRAPHICS_IMPORT(gs_timer_end);
	GRAPHICS_IMPORT(gs_timer_get_data);
	GRAPHICS_IMPORT(gs_timer_range_destroy);
	GRAPHICS_IMPORT(gs_timer_range_begin);
	GRAPHICS_IMPORT(gs_timer_range_end);
	GRAPHICS_IMPORT(gs_timer_range_get_data);

	GRAPHICS_IMPORT(gs_shader_destroy);
	GRAPHICS_IMPORT(gs_shader_get_num_params);
	GRAPHICS_IMPORT(gs_shader_get_param_by_idx);
	GRAPHICS_IMPORT(gs_shader_get_param_by_name);
	GRAPHICS_IMPORT(gs_shader_get_viewproj_matrix);
	GRAPHICS_IMPORT(gs_shader_get_world_matrix);
	GRAPHICS_IMPORT(gs_shader_get_param_info);
	GRAPHICS_IMPORT(gs_shader_set_bool);
	GRAPHICS_IMPORT(gs_shader_set_float);
	GRAPHICS_IMPORT(gs_shader_set_int);
	GRAPHICS_IMPORT(gs_shader_set_matrix3);
	GRAPHICS_IMPORT(gs_shader_set_matrix4);
	GRAPHICS_IMPORT(gs_shader_set_vec2);
	GRAPHICS_IMPORT(gs_shader_set_vec3);
	GRAPHICS_IMPORT(gs_shader_set_vec4);
	GRAPHICS_IMPORT(gs_shader_set_texture);
	GRAPHICS_IMPORT(gs_shader_set_val);
	GRAPHICS_IMPORT(gs_shader_set_default);
	GRAPHICS_IMPORT(gs_shader_set_next_sampler);

	GRAPHICS_IMPORT_OPTIONAL(device_nv12_available);

	GRAPHICS_IMPORT(device_debug_marker_begin);
	GRAPHICS_IMPORT(device_debug_marker_end);

	/* OSX/Cocoa specific functions */
#ifdef __APPLE__
	GRAPHICS_IMPORT(device_shared_texture_available);
	GRAPHICS_IMPORT_OPTIONAL(device_texture_open_shared);
	GRAPHICS_IMPORT_OPTIONAL(device_texture_create_from_iosurface);
	GRAPHICS_IMPORT_OPTIONAL(gs_texture_rebind_iosurface);

	/* win32 specific functions */
#elif _WIN32
	GRAPHICS_IMPORT(device_gdi_texture_available);
	GRAPHICS_IMPORT(device_shared_texture_available);
	GRAPHICS_IMPORT_OPTIONAL(device_get_duplicator_monitor_info);
	GRAPHICS_IMPORT_OPTIONAL(device_duplicator_get_monitor_index);
	GRAPHICS_IMPORT_OPTIONAL(device_duplicator_create);
	GRAPHICS_IMPORT_OPTIONAL(gs_duplicator_destroy);
	GRAPHICS_IMPORT_OPTIONAL(gs_duplicator_update_frame);
	GRAPHICS_IMPORT_OPTIONAL(gs_duplicator_get_texture);
	GRAPHICS_IMPORT_OPTIONAL(device_texture_create_gdi);
	GRAPHICS_IMPORT_OPTIONAL(gs_texture_get_dc);
	GRAPHICS_IMPORT_OPTIONAL(gs_texture_release_dc);
	GRAPHICS_IMPORT_OPTIONAL(device_texture_open_shared);
	GRAPHICS_IMPORT_OPTIONAL(device_texture_get_shared_handle);
	GRAPHICS_IMPORT_OPTIONAL(device_texture_wrap_obj);
	GRAPHICS_IMPORT_OPTIONAL(device_texture_acquire_sync);
	GRAPHICS_IMPORT_OPTIONAL(device_texture_release_sync);
	GRAPHICS_IMPORT_OPTIONAL(device_texture_create_nv12);
	GRAPHICS_IMPORT_OPTIONAL(device_stagesurface_create_nv12);
	GRAPHICS_IMPORT_OPTIONAL(device_register_loss_callbacks);
	GRAPHICS_IMPORT_OPTIONAL(device_unregister_loss_callbacks);
#elif __linux__
	GRAPHICS_IMPORT(device_texture_create_from_dmabuf);
#endif

	return success;
}
