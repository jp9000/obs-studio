/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>
    Copyright (C) 2014 by Zachary Lund <admin@computerquip.com>

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

#include <graphics/matrix3.h>
#include "gl-subsystem.h"

/* Goofy Windows.h macros need to be removed */
#undef far
#undef near

/* #define SHOW_ALL_GL_MESSAGES */

#ifdef _DEBUG

static void APIENTRY gl_debug_proc(GLenum source, GLenum type, GLuint id,
				   GLenum severity, GLsizei length,
				   const GLchar *message, const GLvoid *data)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(data);

	char *source_str, *type_str, *severity_str;

/* frames can get a bit too much spam with irrelevant/insignificant opengl
 * debug messages */
#ifndef SHOW_ALL_GL_MESSAGES
	if (type > GL_DEBUG_TYPE_PORTABILITY &&
	    severity != GL_DEBUG_SEVERITY_HIGH) {
		return;
	}
#endif

	switch (source) {
	case GL_DEBUG_SOURCE_API:
		source_str = "API";
		break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
		source_str = "Window System";
		break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER:
		source_str = "Shader Compiler";
		break;
	case GL_DEBUG_SOURCE_THIRD_PARTY:
		source_str = "Third Party";
		break;
	case GL_DEBUG_SOURCE_APPLICATION:
		source_str = "Application";
		break;
	case GL_DEBUG_SOURCE_OTHER:
		source_str = "Other";
		break;
	default:
		source_str = "Unknown";
	}

	switch (type) {
	case GL_DEBUG_TYPE_ERROR:
		type_str = "Error";
		break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
		type_str = "Deprecated Behavior";
		break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
		type_str = "Undefined Behavior";
		break;
	case GL_DEBUG_TYPE_PORTABILITY:
		type_str = "Portability";
		break;
	case GL_DEBUG_TYPE_PERFORMANCE:
		type_str = "Performance";
		break;
	case GL_DEBUG_TYPE_OTHER:
		type_str = "Other";
		break;
	default:
		type_str = "Unknown";
	}

	switch (severity) {
	case GL_DEBUG_SEVERITY_HIGH:
		severity_str = "High";
		break;
	case GL_DEBUG_SEVERITY_MEDIUM:
		severity_str = "Medium";
		break;
	case GL_DEBUG_SEVERITY_LOW:
		severity_str = "Low";
		break;
	case GL_DEBUG_SEVERITY_NOTIFICATION:
		severity_str = "Notification";
		break;
	default:
		severity_str = "Unknown";
	}

	blog(LOG_DEBUG, "[%s][%s]{%s}: %.*s", source_str, type_str,
	     severity_str, length, message);
}

static void gl_enable_debug()
{
	if (GLAD_GL_VERSION_4_3) {
		glDebugMessageCallback(gl_debug_proc, NULL);
		gl_enable(GL_DEBUG_OUTPUT);
	} else if (GLAD_GL_ARB_debug_output) {
		glDebugMessageCallbackARB(gl_debug_proc, NULL);
	} else {
		blog(LOG_DEBUG, "Failed to set GL debug callback as it is "
				"not supported.");
	}
}
#else
static void gl_enable_debug() {}
#endif

static bool gl_init_extensions(struct gs_device *device)
{
	if (!GLAD_GL_VERSION_3_3) {
		blog(LOG_ERROR,
		     "obs-studio requires OpenGL version 3.3 or higher.");
		return false;
	}

	gl_enable_debug();

	if (!GLAD_GL_EXT_texture_sRGB_decode) {
		blog(LOG_ERROR, "OpenGL extension EXT_texture_sRGB_decode "
				"is required.");
		return false;
	}

	gl_enable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	if (GLAD_GL_VERSION_4_3 || GLAD_GL_ARB_copy_image)
		device->copy_type = COPY_TYPE_ARB;
	else if (GLAD_GL_NV_copy_image)
		device->copy_type = COPY_TYPE_NV;
	else
		device->copy_type = COPY_TYPE_FBO_BLIT;

	return true;
}

static void clear_textures(struct gs_device *device)
{
	GLenum i;
	for (i = 0; i < GS_MAX_TEXTURES; i++) {
		if (device->cur_textures[i]) {
			gl_active_texture(GL_TEXTURE0 + i);
			gl_bind_texture(device->cur_textures[i]->gl_target, 0);
			device->cur_textures[i] = NULL;
		}
	}
}

void convert_sampler_info(struct gs_sampler_state *sampler,
			  const struct gs_sampler_info *info)
{
	GLint max_anisotropy_max;
	convert_filter(info->filter, &sampler->min_filter,
		       &sampler->mag_filter);
	sampler->address_u = convert_address_mode(info->address_u);
	sampler->address_v = convert_address_mode(info->address_v);
	sampler->address_w = convert_address_mode(info->address_w);
	sampler->max_anisotropy = info->max_anisotropy;

	max_anisotropy_max = 1;
	if (GLAD_GL_EXT_texture_filter_anisotropic) {
		glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,
			      &max_anisotropy_max);
		gl_success("glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT)");
	}

	if (1 <= sampler->max_anisotropy &&
	    sampler->max_anisotropy <= max_anisotropy_max)
		return;

	if (sampler->max_anisotropy < 1)
		sampler->max_anisotropy = 1;
	else if (sampler->max_anisotropy > max_anisotropy_max)
		sampler->max_anisotropy = max_anisotropy_max;

	blog(LOG_DEBUG,
	     "convert_sampler_info: 1 <= max_anisotropy <= "
	     "%d violated, selected: %d, set: %d",
	     max_anisotropy_max, info->max_anisotropy, sampler->max_anisotropy);
}

const char *device_get_name(void)
{
	return "OpenGL";
}

int device_get_type(void)
{
	return GS_DEVICE_OPENGL;
}

const char *device_preprocessor_name(void)
{
	return "_OPENGL";
}

int device_create(gs_device_t **p_device, uint32_t adapter)
{
	struct gs_device *device = bzalloc(sizeof(struct gs_device));
	int errorcode = GS_ERROR_FAIL;

	blog(LOG_INFO, "---------------------------------");
	blog(LOG_INFO, "Initializing OpenGL...");

	device->plat = gl_platform_create(device, adapter);
	if (!device->plat)
		goto fail;

	const char *glVendor = (const char *)glGetString(GL_VENDOR);
	const char *glRenderer = (const char *)glGetString(GL_RENDERER);

	blog(LOG_INFO, "Loading up OpenGL on adapter %s %s", glVendor,
	     glRenderer);

	if (!gl_init_extensions(device)) {
		errorcode = GS_ERROR_NOT_SUPPORTED;
		goto fail;
	}

	const char *glVersion = (const char *)glGetString(GL_VERSION);
	const char *glShadingLanguage =
		(const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);

	blog(LOG_INFO,
	     "OpenGL loaded successfully, version %s, shading "
	     "language %s",
	     glVersion, glShadingLanguage);

	gl_enable(GL_CULL_FACE);
	gl_gen_vertex_arrays(1, &device->empty_vao);

	struct gs_sampler_info raw_load_info;
	raw_load_info.filter = GS_FILTER_POINT;
	raw_load_info.address_u = GS_ADDRESS_BORDER;
	raw_load_info.address_v = GS_ADDRESS_BORDER;
	raw_load_info.address_w = GS_ADDRESS_BORDER;
	raw_load_info.max_anisotropy = 1;
	raw_load_info.border_color = 0;
	device->raw_load_sampler =
		device_samplerstate_create(device, &raw_load_info);

	gl_clear_context(device);
	device->cur_swap = NULL;

#ifdef _WIN32
	blog(LOG_INFO, "Warning: The OpenGL renderer is currently in use.  "
		       "On windows, the OpenGL renderer can decrease "
		       "capture performance due to the lack of specific "
		       "features used to maximize capture performance.  "
		       "The Direct3D 11 renderer is recommended instead.");
#endif

	*p_device = device;
	return GS_SUCCESS;

fail:
	blog(LOG_ERROR, "device_create (GL) failed");
	bfree(device);

	*p_device = NULL;
	return errorcode;
}

void device_destroy(gs_device_t *device)
{
	if (device) {
		while (device->first_program)
			gs_program_destroy(device->first_program);

		samplerstate_release(device->raw_load_sampler);
		gl_delete_vertex_arrays(1, &device->empty_vao);

		da_free(device->proj_stack);
		gl_platform_destroy(device->plat);
		bfree(device);
	}
}

gs_swapchain_t *device_swapchain_create(gs_device_t *device,
					const struct gs_init_data *info)
{
	struct gs_swap_chain *swap = bzalloc(sizeof(struct gs_swap_chain));

	swap->device = device;
	swap->info = *info;
	swap->wi = gl_windowinfo_create(info);
	if (!swap->wi) {
		blog(LOG_ERROR, "device_swapchain_create (GL) failed");
		gs_swapchain_destroy(swap);
		return NULL;
	}

	if (!gl_platform_init_swapchain(swap)) {
		blog(LOG_ERROR, "gl_platform_init_swapchain  failed");
		gs_swapchain_destroy(swap);
		return NULL;
	}

	return swap;
}

void device_resize(gs_device_t *device, uint32_t cx, uint32_t cy)
{
	/* GL automatically resizes the device, so it doesn't do much */
	if (device->cur_swap) {
		device->cur_swap->info.cx = cx;
		device->cur_swap->info.cy = cy;
	} else {
		blog(LOG_WARNING, "device_resize (GL): No active swap");
	}

	gl_update(device);
}

void device_get_size(const gs_device_t *device, uint32_t *cx, uint32_t *cy)
{
	if (device->cur_swap) {
		*cx = device->cur_swap->info.cx;
		*cy = device->cur_swap->info.cy;
	} else {
		blog(LOG_WARNING, "device_get_size (GL): No active swap");
		*cx = 0;
		*cy = 0;
	}
}

uint32_t device_get_width(const gs_device_t *device)
{
	if (device->cur_swap) {
		return device->cur_swap->info.cx;
	} else {
		blog(LOG_WARNING, "device_get_width (GL): No active swap");
		return 0;
	}
}

uint32_t device_get_height(const gs_device_t *device)
{
	if (device->cur_swap) {
		return device->cur_swap->info.cy;
	} else {
		blog(LOG_WARNING, "device_get_height (GL): No active swap");
		return 0;
	}
}

gs_samplerstate_t *
device_samplerstate_create(gs_device_t *device,
			   const struct gs_sampler_info *info)
{
	struct gs_sampler_state *sampler;

	sampler = bzalloc(sizeof(struct gs_sampler_state));
	sampler->device = device;
	sampler->ref = 1;

	convert_sampler_info(sampler, info);
	return sampler;
}

gs_timer_t *device_timer_create(gs_device_t *device)
{
	UNUSED_PARAMETER(device);

	struct gs_timer *timer;

	GLuint queries[2];
	glGenQueries(2, queries);
	if (!gl_success("glGenQueries"))
		return NULL;

	timer = bzalloc(sizeof(struct gs_timer));
	timer->queries[0] = queries[0];
	timer->queries[1] = queries[1];

	return timer;
}

gs_timer_range_t *device_timer_range_create(gs_device_t *device)
{
	UNUSED_PARAMETER(device);

	return NULL;
}

enum gs_texture_type device_get_texture_type(const gs_texture_t *texture)
{
	return texture->type;
}

static void strip_mipmap_filter(GLint *filter)
{
	switch (*filter) {
	case GL_NEAREST:
	case GL_LINEAR:
		return;
	case GL_NEAREST_MIPMAP_NEAREST:
	case GL_NEAREST_MIPMAP_LINEAR:
		*filter = GL_NEAREST;
		return;
	case GL_LINEAR_MIPMAP_NEAREST:
	case GL_LINEAR_MIPMAP_LINEAR:
		*filter = GL_LINEAR;
		return;
	}
	*filter = GL_NEAREST;
}

static inline void apply_swizzle(struct gs_texture *tex)
{
	if (tex->format == GS_A8) {
		gl_tex_param_i(tex->gl_target, GL_TEXTURE_SWIZZLE_R, GL_ONE);
		gl_tex_param_i(tex->gl_target, GL_TEXTURE_SWIZZLE_G, GL_ONE);
		gl_tex_param_i(tex->gl_target, GL_TEXTURE_SWIZZLE_B, GL_ONE);
		gl_tex_param_i(tex->gl_target, GL_TEXTURE_SWIZZLE_A, GL_RED);
	}
}

static bool load_texture_sampler(gs_texture_t *tex, gs_samplerstate_t *ss)
{
	bool success = true;
	GLint min_filter;

	if (tex->cur_sampler == ss)
		return true;

	if (tex->cur_sampler)
		samplerstate_release(tex->cur_sampler);
	tex->cur_sampler = ss;
	if (!ss)
		return true;

	samplerstate_addref(ss);

	min_filter = ss->min_filter;
	if (gs_texture_is_rect(tex))
		strip_mipmap_filter(&min_filter);

	if (!gl_tex_param_i(tex->gl_target, GL_TEXTURE_MIN_FILTER, min_filter))
		success = false;
	if (!gl_tex_param_i(tex->gl_target, GL_TEXTURE_MAG_FILTER,
			    ss->mag_filter))
		success = false;
	if (!gl_tex_param_i(tex->gl_target, GL_TEXTURE_WRAP_S, ss->address_u))
		success = false;
	if (!gl_tex_param_i(tex->gl_target, GL_TEXTURE_WRAP_T, ss->address_v))
		success = false;
	if (!gl_tex_param_i(tex->gl_target, GL_TEXTURE_WRAP_R, ss->address_w))
		success = false;
	if (GLAD_GL_EXT_texture_filter_anisotropic) {
		if (!gl_tex_param_i(tex->gl_target,
				    GL_TEXTURE_MAX_ANISOTROPY_EXT,
				    ss->max_anisotropy))
			success = false;
	}

	apply_swizzle(tex);

	return success;
}

static inline struct gs_shader_param *get_texture_param(gs_device_t *device,
							int unit)
{
	struct gs_shader *shader = device->cur_pixel_shader;
	size_t i;

	for (i = 0; i < shader->params.num; i++) {
		struct gs_shader_param *param = shader->params.array + i;
		if (param->type == GS_SHADER_PARAM_TEXTURE) {
			if (param->texture_id == unit)
				return param;
		}
	}

	return NULL;
}

static void device_load_texture_internal(gs_device_t *device, gs_texture_t *tex,
					 int unit, GLint decode)
{
	struct gs_shader_param *param;
	struct gs_sampler_state *sampler;
	struct gs_texture *cur_tex = device->cur_textures[unit];

	/* need a pixel shader to properly bind textures */
	if (!device->cur_pixel_shader)
		goto fail;

	if (cur_tex == tex)
		return;

	if (!gl_active_texture(GL_TEXTURE0 + unit))
		goto fail;

	/* the target for the previous text may not be the same as the
	 * next texture, so unbind the previous texture first to be safe */
	if (cur_tex && (!tex || cur_tex->gl_target != tex->gl_target))
		gl_bind_texture(cur_tex->gl_target, 0);

	device->cur_textures[unit] = tex;
	param = get_texture_param(device, unit);
	if (!param)
		return;

	param->texture = tex;

	if (!tex)
		return;

	if (param->sampler_id != (size_t)-1)
		sampler = device->cur_samplers[param->sampler_id];
	else
		sampler = device->raw_load_sampler;

	if (!gl_bind_texture(tex->gl_target, tex->texture))
		goto fail;

	if (!gl_tex_param_i(tex->gl_target, GL_TEXTURE_SRGB_DECODE_EXT, decode))
		goto fail;

	if (sampler && !load_texture_sampler(tex, sampler))
		goto fail;

	return;

fail:
	blog(LOG_ERROR, "device_load_texture (GL) failed");
}

void device_load_texture(gs_device_t *device, gs_texture_t *tex, int unit)
{
	device_load_texture_internal(device, tex, unit, GL_SKIP_DECODE_EXT);
}

void device_load_texture_srgb(gs_device_t *device, gs_texture_t *tex, int unit)
{
	device_load_texture_internal(device, tex, unit, GL_DECODE_EXT);
}

static bool load_sampler_on_textures(gs_device_t *device, gs_samplerstate_t *ss,
				     int sampler_unit)
{
	struct gs_shader *shader = device->cur_pixel_shader;
	size_t i;

	for (i = 0; i < shader->params.num; i++) {
		struct gs_shader_param *param = shader->params.array + i;

		if (param->type == GS_SHADER_PARAM_TEXTURE &&
		    param->sampler_id == (uint32_t)sampler_unit &&
		    param->texture) {
			if (!gl_active_texture(GL_TEXTURE0 + param->texture_id))
				return false;
			if (!load_texture_sampler(param->texture, ss))
				return false;
		}
	}

	return true;
}

void device_load_samplerstate(gs_device_t *device, gs_samplerstate_t *ss,
			      int unit)
{
	/* need a pixel shader to properly bind samplers */
	if (!device->cur_pixel_shader)
		ss = NULL;

	if (device->cur_samplers[unit] == ss)
		return;

	device->cur_samplers[unit] = ss;

	if (!ss)
		return;

	if (!load_sampler_on_textures(device, ss, unit))
		blog(LOG_ERROR, "device_load_samplerstate (GL) failed");

	return;
}

void device_load_vertexshader(gs_device_t *device, gs_shader_t *vertshader)
{
	if (device->cur_vertex_shader == vertshader)
		return;

	if (vertshader && vertshader->type != GS_SHADER_VERTEX) {
		blog(LOG_ERROR, "Specified shader is not a vertex shader");
		blog(LOG_ERROR, "device_load_vertexshader (GL) failed");
		return;
	}

	device->cur_vertex_shader = vertshader;
}

static void load_default_pixelshader_samplers(struct gs_device *device,
					      struct gs_shader *ps)
{
	size_t i;
	if (!ps)
		return;

	for (i = 0; i < ps->samplers.num; i++) {
		struct gs_sampler_state *ss = ps->samplers.array[i];
		device->cur_samplers[i] = ss;
	}

	for (; i < GS_MAX_TEXTURES; i++)
		device->cur_samplers[i] = NULL;
}

void device_load_pixelshader(gs_device_t *device, gs_shader_t *pixelshader)
{
	if (device->cur_pixel_shader == pixelshader)
		return;

	if (pixelshader && pixelshader->type != GS_SHADER_PIXEL) {
		blog(LOG_ERROR, "Specified shader is not a pixel shader");
		goto fail;
	}

	device->cur_pixel_shader = pixelshader;

	clear_textures(device);

	if (pixelshader)
		load_default_pixelshader_samplers(device, pixelshader);
	return;

fail:
	blog(LOG_ERROR, "device_load_pixelshader (GL) failed");
}

void device_load_default_samplerstate(gs_device_t *device, bool b_3d, int unit)
{
	/* TODO */
	UNUSED_PARAMETER(device);
	UNUSED_PARAMETER(b_3d);
	UNUSED_PARAMETER(unit);
}

gs_shader_t *device_get_vertex_shader(const gs_device_t *device)
{
	return device->cur_vertex_shader;
}

gs_shader_t *device_get_pixel_shader(const gs_device_t *device)
{
	return device->cur_pixel_shader;
}

gs_texture_t *device_get_render_target(const gs_device_t *device)
{
	return device->cur_render_target;
}

gs_zstencil_t *device_get_zstencil_target(const gs_device_t *device)
{
	return device->cur_zstencil_buffer;
}

static bool get_tex_dimensions(gs_texture_t *tex, uint32_t *width,
			       uint32_t *height)
{
	if (tex->type == GS_TEXTURE_2D) {
		struct gs_texture_2d *tex2d = (struct gs_texture_2d *)tex;
		*width = tex2d->width;
		*height = tex2d->height;
		return true;

	} else if (tex->type == GS_TEXTURE_CUBE) {
		struct gs_texture_cube *cube = (struct gs_texture_cube *)tex;
		*width = cube->size;
		*height = cube->size;
		return true;
	}

	blog(LOG_ERROR, "Texture must be 2D or cubemap");
	return false;
}

/*
 * This automatically manages FBOs so that render targets are always given
 * an FBO that matches their width/height/format to maximize optimization
 */
struct fbo_info *get_fbo(gs_texture_t *tex, uint32_t width, uint32_t height)
{
	if (tex->fbo && tex->fbo->width == width &&
	    tex->fbo->height == height && tex->fbo->format == tex->format)
		return tex->fbo;

	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	if (!gl_success("glGenFramebuffers"))
		return NULL;

	tex->fbo = bmalloc(sizeof(struct fbo_info));
	tex->fbo->fbo = fbo;
	tex->fbo->width = width;
	tex->fbo->height = height;
	tex->fbo->format = tex->format;
	tex->fbo->cur_render_target = NULL;
	tex->fbo->cur_render_side = 0;
	tex->fbo->cur_zstencil_buffer = NULL;

	return tex->fbo;
}

static inline struct fbo_info *get_fbo_by_tex(gs_texture_t *tex)
{
	uint32_t width, height;
	if (!get_tex_dimensions(tex, &width, &height))
		return NULL;

	return get_fbo(tex, width, height);
}

static bool set_current_fbo(gs_device_t *device, struct fbo_info *fbo)
{
	if (device->cur_fbo != fbo) {
		GLuint fbo_obj = fbo ? fbo->fbo : 0;
		if (!gl_bind_framebuffer(GL_DRAW_FRAMEBUFFER, fbo_obj))
			return false;

		if (device->cur_fbo) {
			device->cur_fbo->cur_render_target = NULL;
			device->cur_fbo->cur_zstencil_buffer = NULL;
		}
	}

	device->cur_fbo = fbo;
	return true;
}

static bool attach_rendertarget(struct fbo_info *fbo, gs_texture_t *tex,
				int side)
{
	if (fbo->cur_render_target == tex)
		return true;

	fbo->cur_render_target = tex;

	if (tex->type == GS_TEXTURE_2D) {
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
				       GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
				       tex->texture, 0);

	} else if (tex->type == GS_TEXTURE_CUBE) {
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
				       GL_COLOR_ATTACHMENT0,
				       GL_TEXTURE_CUBE_MAP_POSITIVE_X + side,
				       tex->texture, 0);

	} else {
		return false;
	}

	return gl_success("glFramebufferTexture2D");
}

static bool attach_zstencil(struct fbo_info *fbo, gs_zstencil_t *zs)
{
	GLuint zsbuffer = 0;
	GLenum zs_attachment = GL_DEPTH_STENCIL_ATTACHMENT;

	if (fbo->cur_zstencil_buffer == zs)
		return true;

	fbo->cur_zstencil_buffer = zs;

	if (zs) {
		zsbuffer = zs->buffer;
		zs_attachment = zs->attachment;
	}

	glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, zs_attachment,
				  GL_RENDERBUFFER, zsbuffer);
	if (!gl_success("glFramebufferRenderbuffer"))
		return false;

	return true;
}

static bool set_target(gs_device_t *device, gs_texture_t *tex, int side,
		       gs_zstencil_t *zs)
{
	struct fbo_info *fbo;

	if (device->cur_render_target == tex &&
	    device->cur_zstencil_buffer == zs &&
	    device->cur_render_side == side)
		return true;

	device->cur_render_target = tex;
	device->cur_render_side = side;
	device->cur_zstencil_buffer = zs;

	if (!tex)
		return set_current_fbo(device, NULL);

	fbo = get_fbo_by_tex(tex);
	if (!fbo)
		return false;

	set_current_fbo(device, fbo);

	if (!attach_rendertarget(fbo, tex, side))
		return false;
	if (!attach_zstencil(fbo, zs))
		return false;

	return true;
}

void device_set_render_target(gs_device_t *device, gs_texture_t *tex,
			      gs_zstencil_t *zstencil)
{
	if (tex) {
		if (tex->type != GS_TEXTURE_2D) {
			blog(LOG_ERROR, "Texture is not a 2D texture");
			goto fail;
		}

		if (!tex->is_render_target) {
			blog(LOG_ERROR, "Texture is not a render target");
			goto fail;
		}
	}

	if (!set_target(device, tex, 0, zstencil))
		goto fail;

	return;

fail:
	blog(LOG_ERROR, "device_set_render_target (GL) failed");
}

void device_set_cube_render_target(gs_device_t *device, gs_texture_t *cubetex,
				   int side, gs_zstencil_t *zstencil)
{
	if (cubetex) {
		if (cubetex->type != GS_TEXTURE_CUBE) {
			blog(LOG_ERROR, "Texture is not a cube texture");
			goto fail;
		}

		if (!cubetex->is_render_target) {
			blog(LOG_ERROR, "Texture is not a render target");
			goto fail;
		}
	}

	if (!set_target(device, cubetex, side, zstencil))
		goto fail;

	return;

fail:
	blog(LOG_ERROR, "device_set_cube_render_target (GL) failed");
}

void device_enable_framebuffer_srgb(gs_device_t *device, bool enable)
{
	UNUSED_PARAMETER(device);

	if (enable)
		gl_enable(GL_FRAMEBUFFER_SRGB);
	else
		gl_disable(GL_FRAMEBUFFER_SRGB);
}

bool device_framebuffer_srgb_enabled(gs_device_t *device)
{
	UNUSED_PARAMETER(device);

	const GLboolean enabled = glIsEnabled(GL_FRAMEBUFFER_SRGB);
	gl_success("glIsEnabled");
	return enabled == GL_TRUE;
}

void device_copy_texture_region(gs_device_t *device, gs_texture_t *dst,
				uint32_t dst_x, uint32_t dst_y,
				gs_texture_t *src, uint32_t src_x,
				uint32_t src_y, uint32_t src_w, uint32_t src_h)
{
	struct gs_texture_2d *src2d = (struct gs_texture_2d *)src;
	struct gs_texture_2d *dst2d = (struct gs_texture_2d *)dst;

	if (!src) {
		blog(LOG_ERROR, "Source texture is NULL");
		goto fail;
	}

	if (!dst) {
		blog(LOG_ERROR, "Destination texture is NULL");
		goto fail;
	}

	if (dst->type != GS_TEXTURE_2D || src->type != GS_TEXTURE_2D) {
		blog(LOG_ERROR, "Source and destination textures must be 2D "
				"textures");
		goto fail;
	}

	if (dst->format != src->format) {
		blog(LOG_ERROR, "Source and destination formats do not match");
		goto fail;
	}

	uint32_t nw = (uint32_t)src_w ? (uint32_t)src_w
				      : (src2d->width - src_x);
	uint32_t nh = (uint32_t)src_h ? (uint32_t)src_h
				      : (src2d->height - src_y);

	if (dst2d->width - dst_x < nw || dst2d->height - dst_y < nh) {
		blog(LOG_ERROR, "Destination texture region is not big "
				"enough to hold the source region");
		goto fail;
	}

	if (!gl_copy_texture(device, dst, dst_x, dst_y, src, src_x, src_y, nw,
			     nh))
		goto fail;

	return;

fail:
	blog(LOG_ERROR, "device_copy_texture (GL) failed");
}

void device_copy_texture(gs_device_t *device, gs_texture_t *dst,
			 gs_texture_t *src)
{
	device_copy_texture_region(device, dst, 0, 0, src, 0, 0, 0, 0);
}

void device_begin_frame(gs_device_t *device)
{
	/* does nothing */
	UNUSED_PARAMETER(device);
}

void device_begin_scene(gs_device_t *device)
{
	clear_textures(device);
}

static inline bool can_render(const gs_device_t *device, uint32_t num_verts)
{
	if (!device->cur_vertex_shader) {
		blog(LOG_ERROR, "No vertex shader specified");
		return false;
	}

	if (!device->cur_pixel_shader) {
		blog(LOG_ERROR, "No pixel shader specified");
		return false;
	}

	if (!device->cur_vertex_buffer && (num_verts == 0)) {
		blog(LOG_ERROR, "No vertex buffer specified");
		return false;
	}

	if (!device->cur_swap && !device->cur_render_target) {
		blog(LOG_ERROR, "No active swap chain or render target");
		return false;
	}

	return true;
}

static void update_viewproj_matrix(struct gs_device *device)
{
	struct gs_shader *vs = device->cur_vertex_shader;
	struct matrix4 cur_proj;

	gs_matrix_get(&device->cur_view);
	matrix4_copy(&cur_proj, &device->cur_proj);

	if (device->cur_fbo) {
		cur_proj.x.y = -cur_proj.x.y;
		cur_proj.y.y = -cur_proj.y.y;
		cur_proj.z.y = -cur_proj.z.y;
		cur_proj.t.y = -cur_proj.t.y;

		glFrontFace(GL_CW);
	} else {
		glFrontFace(GL_CCW);
	}

	gl_success("glFrontFace");

	matrix4_mul(&device->cur_viewproj, &device->cur_view, &cur_proj);
	matrix4_transpose(&device->cur_viewproj, &device->cur_viewproj);

	if (vs->viewproj)
		gs_shader_set_matrix4(vs->viewproj, &device->cur_viewproj);
}

static inline struct gs_program *find_program(const struct gs_device *device)
{
	struct gs_program *program = device->first_program;

	while (program) {
		if (program->vertex_shader == device->cur_vertex_shader &&
		    program->pixel_shader == device->cur_pixel_shader)
			return program;

		program = program->next;
	}

	return NULL;
}

static inline struct gs_program *get_shader_program(struct gs_device *device)
{
	struct gs_program *program = find_program(device);

	if (!program)
		program = gs_program_create(device);

	return program;
}

void device_draw(gs_device_t *device, enum gs_draw_mode draw_mode,
		 uint32_t start_vert, uint32_t num_verts)
{
	struct gs_vertex_buffer *vb = device->cur_vertex_buffer;
	struct gs_index_buffer *ib = device->cur_index_buffer;
	GLenum topology = convert_gs_topology(draw_mode);
	gs_effect_t *effect = gs_get_effect();
	struct gs_program *program;

	if (!can_render(device, num_verts))
		goto fail;

	if (effect)
		gs_effect_update_params(effect);

	program = get_shader_program(device);
	if (!program)
		goto fail;

	if (vb)
		load_vb_buffers(program, vb, ib);
	else
		gl_bind_vertex_array(device->empty_vao);

	if (program != device->cur_program && device->cur_program) {
		glUseProgram(0);
		gl_success("glUseProgram (zero)");
	}

	if (program != device->cur_program) {
		device->cur_program = program;

		glUseProgram(program->obj);
		if (!gl_success("glUseProgram"))
			goto fail;
	}

	update_viewproj_matrix(device);

	program_update_params(program);

	if (ib) {
		if (num_verts == 0)
			num_verts = (uint32_t)device->cur_index_buffer->num;
		glDrawElements(topology, num_verts, ib->gl_type,
			       (const GLvoid *)(start_vert * ib->width));
		if (!gl_success("glDrawElements"))
			goto fail;

	} else {
		if (num_verts == 0)
			num_verts = (uint32_t)device->cur_vertex_buffer->num;
		glDrawArrays(topology, start_vert, num_verts);
		if (!gl_success("glDrawArrays"))
			goto fail;
	}

	return;

fail:
	blog(LOG_ERROR, "device_draw (GL) failed");
}

void device_end_scene(gs_device_t *device)
{
	/* does nothing */
	UNUSED_PARAMETER(device);
}

void device_clear(gs_device_t *device, uint32_t clear_flags,
		  const struct vec4 *color, float depth, uint8_t stencil)
{
	GLbitfield gl_flags = 0;

	if (clear_flags & GS_CLEAR_COLOR) {
		glClearColor(color->x, color->y, color->z, color->w);
		gl_flags |= GL_COLOR_BUFFER_BIT;
	}

	if (clear_flags & GS_CLEAR_DEPTH) {
		glClearDepth(depth);
		gl_flags |= GL_DEPTH_BUFFER_BIT;
	}

	if (clear_flags & GS_CLEAR_STENCIL) {
		glClearStencil(stencil);
		gl_flags |= GL_STENCIL_BUFFER_BIT;
	}

	glClear(gl_flags);
	if (!gl_success("glClear"))
		blog(LOG_ERROR, "device_clear (GL) failed");

	UNUSED_PARAMETER(device);
}

void device_flush(gs_device_t *device)
{
#ifdef __APPLE__
	if (!device->cur_swap)
		glFlush();
#else
	glFlush();
#endif

	UNUSED_PARAMETER(device);
}

void device_set_cull_mode(gs_device_t *device, enum gs_cull_mode mode)
{
	if (device->cur_cull_mode == mode)
		return;

	if (device->cur_cull_mode == GS_NEITHER)
		gl_enable(GL_CULL_FACE);

	device->cur_cull_mode = mode;

	if (mode == GS_BACK)
		gl_cull_face(GL_BACK);
	else if (mode == GS_FRONT)
		gl_cull_face(GL_FRONT);
	else
		gl_disable(GL_CULL_FACE);
}

enum gs_cull_mode device_get_cull_mode(const gs_device_t *device)
{
	return device->cur_cull_mode;
}

void device_enable_blending(gs_device_t *device, bool enable)
{
	if (enable)
		gl_enable(GL_BLEND);
	else
		gl_disable(GL_BLEND);

	UNUSED_PARAMETER(device);
}

void device_enable_depth_test(gs_device_t *device, bool enable)
{
	if (enable)
		gl_enable(GL_DEPTH_TEST);
	else
		gl_disable(GL_DEPTH_TEST);

	UNUSED_PARAMETER(device);
}

void device_enable_stencil_test(gs_device_t *device, bool enable)
{
	if (enable)
		gl_enable(GL_STENCIL_TEST);
	else
		gl_disable(GL_STENCIL_TEST);

	UNUSED_PARAMETER(device);
}

void device_enable_stencil_write(gs_device_t *device, bool enable)
{
	if (enable)
		glStencilMask(0xFFFFFFFF);
	else
		glStencilMask(0);

	UNUSED_PARAMETER(device);
}

void device_enable_color(gs_device_t *device, bool red, bool green, bool blue,
			 bool alpha)
{
	glColorMask(red, green, blue, alpha);

	UNUSED_PARAMETER(device);
}

void device_blend_function(gs_device_t *device, enum gs_blend_type src,
			   enum gs_blend_type dest)
{
	GLenum gl_src = convert_gs_blend_type(src);
	GLenum gl_dst = convert_gs_blend_type(dest);

	glBlendFunc(gl_src, gl_dst);
	if (!gl_success("glBlendFunc"))
		blog(LOG_ERROR, "device_blend_function (GL) failed");

	UNUSED_PARAMETER(device);
}

void device_blend_function_separate(gs_device_t *device,
				    enum gs_blend_type src_c,
				    enum gs_blend_type dest_c,
				    enum gs_blend_type src_a,
				    enum gs_blend_type dest_a)
{
	GLenum gl_src_c = convert_gs_blend_type(src_c);
	GLenum gl_dst_c = convert_gs_blend_type(dest_c);
	GLenum gl_src_a = convert_gs_blend_type(src_a);
	GLenum gl_dst_a = convert_gs_blend_type(dest_a);

	glBlendFuncSeparate(gl_src_c, gl_dst_c, gl_src_a, gl_dst_a);
	if (!gl_success("glBlendFuncSeparate"))
		blog(LOG_ERROR, "device_blend_function_separate (GL) failed");

	UNUSED_PARAMETER(device);
}

void device_depth_function(gs_device_t *device, enum gs_depth_test test)
{
	GLenum gl_test = convert_gs_depth_test(test);

	glDepthFunc(gl_test);
	if (!gl_success("glDepthFunc"))
		blog(LOG_ERROR, "device_depth_function (GL) failed");

	UNUSED_PARAMETER(device);
}

void device_stencil_function(gs_device_t *device, enum gs_stencil_side side,
			     enum gs_depth_test test)
{
	GLenum gl_side = convert_gs_stencil_side(side);
	GLenum gl_test = convert_gs_depth_test(test);

	glStencilFuncSeparate(gl_side, gl_test, 0, 0xFFFFFFFF);
	if (!gl_success("glStencilFuncSeparate"))
		blog(LOG_ERROR, "device_stencil_function (GL) failed");

	UNUSED_PARAMETER(device);
}

void device_stencil_op(gs_device_t *device, enum gs_stencil_side side,
		       enum gs_stencil_op_type fail,
		       enum gs_stencil_op_type zfail,
		       enum gs_stencil_op_type zpass)
{
	GLenum gl_side = convert_gs_stencil_side(side);
	GLenum gl_fail = convert_gs_stencil_op(fail);
	GLenum gl_zfail = convert_gs_stencil_op(zfail);
	GLenum gl_zpass = convert_gs_stencil_op(zpass);

	glStencilOpSeparate(gl_side, gl_fail, gl_zfail, gl_zpass);
	if (!gl_success("glStencilOpSeparate"))
		blog(LOG_ERROR, "device_stencil_op (GL) failed");

	UNUSED_PARAMETER(device);
}

static inline uint32_t get_target_height(const struct gs_device *device)
{
	if (!device->cur_render_target)
		return device_get_height(device);

	if (device->cur_render_target->type == GS_TEXTURE_2D)
		return gs_texture_get_height(device->cur_render_target);
	else /* cube map */
		return gs_cubetexture_get_size(device->cur_render_target);
}

void device_set_viewport(gs_device_t *device, int x, int y, int width,
			 int height)
{
	uint32_t base_height = 0;

	/* GL uses bottom-up coordinates for viewports.  We want top-down */
	if (device->cur_render_target) {
		base_height = get_target_height(device);
	} else if (device->cur_swap) {
		uint32_t dw;
		gl_getclientsize(device->cur_swap, &dw, &base_height);
	}

	GLint gl_y = y;
	if (base_height && !device->cur_fbo)
		gl_y = base_height - y - height;

	glViewport(x, gl_y, width, height);
	if (!gl_success("glViewport"))
		blog(LOG_ERROR, "device_set_viewport (GL) failed");

	device->cur_viewport.x = x;
	device->cur_viewport.y = y;
	device->cur_viewport.cx = width;
	device->cur_viewport.cy = height;
}

void device_get_viewport(const gs_device_t *device, struct gs_rect *rect)
{
	*rect = device->cur_viewport;
}

void device_set_scissor_rect(gs_device_t *device, const struct gs_rect *rect)
{
	UNUSED_PARAMETER(device);

	if (rect != NULL) {
		glScissor(rect->x, rect->y, rect->cx, rect->cy);
		if (gl_success("glScissor") && gl_enable(GL_SCISSOR_TEST))
			return;

	} else if (gl_disable(GL_SCISSOR_TEST)) {
		return;
	}

	blog(LOG_ERROR, "device_set_scissor_rect (GL) failed");
}

void device_ortho(gs_device_t *device, float left, float right, float top,
		  float bottom, float near, float far)
{
	struct matrix4 *dst = &device->cur_proj;

	float rml = right - left;
	float bmt = bottom - top;
	float fmn = far - near;

	vec4_zero(&dst->x);
	vec4_zero(&dst->y);
	vec4_zero(&dst->z);
	vec4_zero(&dst->t);

	dst->x.x = 2.0f / rml;
	dst->t.x = (left + right) / -rml;

	dst->y.y = 2.0f / -bmt;
	dst->t.y = (bottom + top) / bmt;

	dst->z.z = -2.0f / fmn;
	dst->t.z = (far + near) / -fmn;

	dst->t.w = 1.0f;
}

void device_frustum(gs_device_t *device, float left, float right, float top,
		    float bottom, float near, float far)
{
	struct matrix4 *dst = &device->cur_proj;

	float rml = right - left;
	float tmb = top - bottom;
	float nmf = near - far;
	float nearx2 = 2.0f * near;

	vec4_zero(&dst->x);
	vec4_zero(&dst->y);
	vec4_zero(&dst->z);
	vec4_zero(&dst->t);

	dst->x.x = nearx2 / rml;
	dst->z.x = (left + right) / rml;

	dst->y.y = nearx2 / tmb;
	dst->z.y = (bottom + top) / tmb;

	dst->z.z = (far + near) / nmf;
	dst->t.z = 2.0f * (near * far) / nmf;

	dst->z.w = -1.0f;
}

void device_projection_push(gs_device_t *device)
{
	da_push_back(device->proj_stack, &device->cur_proj);
}

void device_projection_pop(gs_device_t *device)
{
	struct matrix4 *end;
	if (!device->proj_stack.num)
		return;

	end = da_end(device->proj_stack);
	device->cur_proj = *end;
	da_pop_back(device->proj_stack);
}

void device_debug_marker_begin(gs_device_t *device, const char *markername,
			       const float color[4])
{
	UNUSED_PARAMETER(device);
	UNUSED_PARAMETER(color);

	glPushDebugGroupKHR(GL_DEBUG_SOURCE_APPLICATION, 0, -1, markername);
}

void device_debug_marker_end(gs_device_t *device)
{
	UNUSED_PARAMETER(device);

	glPopDebugGroupKHR();
}

void gs_swapchain_destroy(gs_swapchain_t *swapchain)
{
	if (!swapchain)
		return;

	if (swapchain->device->cur_swap == swapchain)
		device_load_swapchain(swapchain->device, NULL);

	gl_platform_cleanup_swapchain(swapchain);

	gl_windowinfo_destroy(swapchain->wi);
	bfree(swapchain);
}

uint32_t gs_voltexture_get_width(const gs_texture_t *voltex)
{
	/* TODO */
	UNUSED_PARAMETER(voltex);
	return 0;
}

uint32_t gs_voltexture_get_height(const gs_texture_t *voltex)
{
	/* TODO */
	UNUSED_PARAMETER(voltex);
	return 0;
}

uint32_t gs_voltexture_get_depth(const gs_texture_t *voltex)
{
	/* TODO */
	UNUSED_PARAMETER(voltex);
	return 0;
}

enum gs_color_format gs_voltexture_get_color_format(const gs_texture_t *voltex)
{
	/* TODO */
	UNUSED_PARAMETER(voltex);
	return GS_UNKNOWN;
}

void gs_samplerstate_destroy(gs_samplerstate_t *samplerstate)
{
	if (!samplerstate)
		return;

	if (samplerstate->device)
		for (int i = 0; i < GS_MAX_TEXTURES; i++)
			if (samplerstate->device->cur_samplers[i] ==
			    samplerstate)
				samplerstate->device->cur_samplers[i] = NULL;

	samplerstate_release(samplerstate);
}

void gs_timer_destroy(gs_timer_t *timer)
{
	if (!timer)
		return;

	glDeleteQueries(2, timer->queries);
	gl_success("glDeleteQueries");

	bfree(timer);
}

void gs_timer_begin(gs_timer_t *timer)
{
	glQueryCounter(timer->queries[0], GL_TIMESTAMP);
	gl_success("glQueryCounter");
}

void gs_timer_end(gs_timer_t *timer)
{
	glQueryCounter(timer->queries[1], GL_TIMESTAMP);
	gl_success("glQueryCounter");
}

bool gs_timer_get_data(gs_timer_t *timer, uint64_t *ticks)
{
	GLint available = 0;
	glGetQueryObjectiv(timer->queries[1], GL_QUERY_RESULT_AVAILABLE,
			   &available);

	GLuint64 begin, end;
	glGetQueryObjectui64v(timer->queries[0], GL_QUERY_RESULT, &begin);
	gl_success("glGetQueryObjectui64v");
	glGetQueryObjectui64v(timer->queries[1], GL_QUERY_RESULT, &end);
	gl_success("glGetQueryObjectui64v");

	*ticks = end - begin;
	return true;
}

void gs_timer_range_destroy(gs_timer_range_t *range)
{
	UNUSED_PARAMETER(range);
}

void gs_timer_range_begin(gs_timer_range_t *range)
{
	UNUSED_PARAMETER(range);
}

void gs_timer_range_end(gs_timer_range_t *range)
{
	UNUSED_PARAMETER(range);
}

bool gs_timer_range_get_data(gs_timer_range_t *range, bool *disjoint,
			     uint64_t *frequency)
{
	UNUSED_PARAMETER(range);

	*disjoint = false;
	*frequency = 1000000000;
	return true;
}
