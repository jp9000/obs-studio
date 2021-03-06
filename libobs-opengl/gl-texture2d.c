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

#include "gl-subsystem.h"

static bool upload_texture_2d(struct gs_texture_2d *tex, const uint8_t **data)
{
	uint32_t row_size = tex->width * gs_get_format_bpp(tex->base.format);
	uint32_t tex_size = tex->height * row_size / 8;
	uint32_t num_levels = tex->base.levels;
	bool compressed = gs_is_compressed_format(tex->base.format);
	bool success;

	if (!num_levels)
		num_levels = gs_get_total_levels(tex->width, tex->height, 1);

	if (!gl_bind_texture(GL_TEXTURE_2D, tex->base.texture))
		return false;

	success = gl_init_face(GL_TEXTURE_2D, tex->base.gl_type, num_levels,
			       tex->base.gl_format,
			       tex->base.gl_internal_format, compressed,
			       tex->width, tex->height, tex_size, &data);

	if (!gl_tex_param_i(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,
			    num_levels - 1))
		success = false;
	if (!gl_bind_texture(GL_TEXTURE_2D, 0))
		success = false;

	return success;
}

static bool create_pixel_unpack_buffer(struct gs_texture_2d *tex)
{
	GLsizeiptr size;
	bool success = true;

	if (!gl_gen_buffers(1, &tex->unpack_buffer))
		return false;

	if (!gl_bind_buffer(GL_PIXEL_UNPACK_BUFFER, tex->unpack_buffer))
		return false;

	size = tex->width * gs_get_format_bpp(tex->base.format);
	if (!gs_is_compressed_format(tex->base.format)) {
		size /= 8;
		size = (size + 3) & 0xFFFFFFFC;
		size *= tex->height;
	} else {
		size *= tex->height;
		size /= 8;
	}

	glBufferData(GL_PIXEL_UNPACK_BUFFER, size, 0, GL_DYNAMIC_DRAW);
	if (!gl_success("glBufferData"))
		success = false;

	if (!gl_bind_buffer(GL_PIXEL_UNPACK_BUFFER, 0))
		success = false;

	return success;
}

gs_texture_t *device_texture_create(gs_device_t *device, uint32_t width,
				    uint32_t height,
				    enum gs_color_format color_format,
				    uint32_t levels, const uint8_t **data,
				    uint32_t flags)
{
	struct gs_texture_2d *tex = bzalloc(sizeof(struct gs_texture_2d));
	tex->base.device = device;
	tex->base.type = GS_TEXTURE_2D;
	tex->base.format = color_format;
	tex->base.levels = levels;
	tex->base.gl_format = convert_gs_format(color_format);
	tex->base.gl_internal_format = convert_gs_internal_format(color_format);
	tex->base.gl_type = get_gl_format_type(color_format);
	tex->base.gl_target = GL_TEXTURE_2D;
	tex->base.is_dynamic = (flags & GS_DYNAMIC) != 0;
	tex->base.is_render_target = (flags & GS_RENDER_TARGET) != 0;
	tex->base.is_dummy = (flags & GS_GL_DUMMYTEX) != 0;
	tex->base.gen_mipmaps = (flags & GS_BUILD_MIPMAPS) != 0;
	tex->width = width;
	tex->height = height;

	if (!gl_gen_textures(1, &tex->base.texture))
		goto fail;

	if (!tex->base.is_dummy) {
		if (tex->base.is_dynamic && !create_pixel_unpack_buffer(tex))
			goto fail;
		if (!upload_texture_2d(tex, data))
			goto fail;
	} else {
		if (!gl_bind_texture(GL_TEXTURE_2D, tex->base.texture))
			goto fail;

		uint32_t row_size =
			tex->width * gs_get_format_bpp(tex->base.format);
		uint32_t tex_size = tex->height * row_size / 8;
		bool compressed = gs_is_compressed_format(tex->base.format);
		bool did_init = gl_init_face(GL_TEXTURE_2D, tex->base.gl_type,
					     1, tex->base.gl_format,
					     tex->base.gl_internal_format,
					     compressed, tex->width,
					     tex->height, tex_size, NULL);
		did_init =
			gl_tex_param_i(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

		bool did_unbind = gl_bind_texture(GL_TEXTURE_2D, 0);
		if (!did_init || !did_unbind)
			goto fail;
	}

	return (gs_texture_t *)tex;

fail:
	gs_texture_destroy((gs_texture_t *)tex);
	blog(LOG_ERROR, "device_texture_create (GL) failed");
	return NULL;
}

static inline bool is_texture_2d(const gs_texture_t *tex, const char *func)
{
	bool is_tex2d = tex->type == GS_TEXTURE_2D;
	if (!is_tex2d)
		blog(LOG_ERROR, "%s (GL) failed:  Not a 2D texture", func);
	return is_tex2d;
}

void gs_texture_destroy(gs_texture_t *tex)
{
	if (!tex)
		return;

	if (tex->cur_sampler)
		gs_samplerstate_destroy(tex->cur_sampler);

	if (!tex->is_dummy && tex->is_dynamic) {
		if (tex->type == GS_TEXTURE_2D) {
			struct gs_texture_2d *tex2d =
				(struct gs_texture_2d *)tex;
			if (tex2d->unpack_buffer)
				gl_delete_buffers(1, &tex2d->unpack_buffer);
		} else if (tex->type == GS_TEXTURE_3D) {
			struct gs_texture_3d *tex3d =
				(struct gs_texture_3d *)tex;
			if (tex3d->unpack_buffer)
				gl_delete_buffers(1, &tex3d->unpack_buffer);
		}
	}

	if (tex->texture)
		gl_delete_textures(1, &tex->texture);

	if (tex->fbo)
		fbo_info_destroy(tex->fbo);

	bfree(tex);
}

uint32_t gs_texture_get_width(const gs_texture_t *tex)
{
	const struct gs_texture_2d *tex2d = (const struct gs_texture_2d *)tex;
	if (!is_texture_2d(tex, "gs_texture_get_width"))
		return 0;

	return tex2d->width;
}

uint32_t gs_texture_get_height(const gs_texture_t *tex)
{
	const struct gs_texture_2d *tex2d = (const struct gs_texture_2d *)tex;
	if (!is_texture_2d(tex, "gs_texture_get_height"))
		return 0;

	return tex2d->height;
}

enum gs_color_format gs_texture_get_color_format(const gs_texture_t *tex)
{
	return tex->format;
}

bool gs_texture_map(gs_texture_t *tex, uint8_t **ptr, uint32_t *linesize)
{
	struct gs_texture_2d *tex2d = (struct gs_texture_2d *)tex;

	if (!is_texture_2d(tex, "gs_texture_map"))
		goto fail;

	if (!tex2d->base.is_dynamic) {
		blog(LOG_ERROR, "Texture is not dynamic");
		goto fail;
	}

	if (!gl_bind_buffer(GL_PIXEL_UNPACK_BUFFER, tex2d->unpack_buffer))
		goto fail;

	*ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
	if (!gl_success("glMapBuffer"))
		goto fail;

	gl_bind_buffer(GL_PIXEL_UNPACK_BUFFER, 0);

	*linesize = tex2d->width * gs_get_format_bpp(tex->format) / 8;
	*linesize = (*linesize + 3) & 0xFFFFFFFC;
	return true;

fail:
	blog(LOG_ERROR, "gs_texture_map (GL) failed");
	return false;
}

void gs_texture_unmap(gs_texture_t *tex)
{
	struct gs_texture_2d *tex2d = (struct gs_texture_2d *)tex;
	if (!is_texture_2d(tex, "gs_texture_unmap"))
		goto failed;

	if (!gl_bind_buffer(GL_PIXEL_UNPACK_BUFFER, tex2d->unpack_buffer))
		goto failed;

	glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
	if (!gl_success("glUnmapBuffer"))
		goto failed;

	if (!gl_bind_texture(GL_TEXTURE_2D, tex2d->base.texture))
		goto failed;

	glTexImage2D(GL_TEXTURE_2D, 0, tex->gl_internal_format, tex2d->width,
		     tex2d->height, 0, tex->gl_format, tex->gl_type, 0);
	if (!gl_success("glTexImage2D"))
		goto failed;

	gl_bind_buffer(GL_PIXEL_UNPACK_BUFFER, 0);
	gl_bind_texture(GL_TEXTURE_2D, 0);
	return;

failed:
	gl_bind_buffer(GL_PIXEL_UNPACK_BUFFER, 0);
	gl_bind_texture(GL_TEXTURE_2D, 0);
	blog(LOG_ERROR, "gs_texture_unmap (GL) failed");
}

bool gs_texture_is_rect(const gs_texture_t *tex)
{
	if (tex->type == GS_TEXTURE_3D)
		return false;

	if (!is_texture_2d(tex, "gs_texture_is_rect")) {
		blog(LOG_ERROR, "gs_texture_is_rect (GL) failed");
		return false;
	}

	const struct gs_texture_2d *tex2d = (const struct gs_texture_2d *)tex;
	return tex2d->base.gl_target == GL_TEXTURE_RECTANGLE;
}

void *gs_texture_get_obj(gs_texture_t *tex)
{
	struct gs_texture_2d *tex2d = (struct gs_texture_2d *)tex;
	if (!is_texture_2d(tex, "gs_texture_get_obj")) {
		blog(LOG_ERROR, "gs_texture_get_obj (GL) failed");
		return NULL;
	}

	return &tex2d->base.texture;
}
