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

static bool gl_init_volume(GLenum type, uint32_t num_levels, GLenum format,
			   GLint internal_format, bool compressed,
			   uint32_t width, uint32_t height, uint32_t depth,
			   const uint8_t *const **p_data)
{
	bool success = true;
	const uint8_t *const *data = p_data ? *p_data : NULL;
	uint32_t i;
	const uint32_t bpp = gs_get_format_bpp(format);

	for (i = 0; i < num_levels; i++) {
		if (compressed) {
			uint32_t mip_size = width * height * depth * bpp / 8;
			glCompressedTexImage3D(GL_TEXTURE_3D, i,
					       internal_format, width, height,
					       depth, 0, mip_size,
					       data ? *data : NULL);
			if (!gl_success("glCompressedTexImage3D"))
				success = false;

		} else {
			glTexImage3D(GL_TEXTURE_3D, i, internal_format, width,
				     height, depth, 0, format, type,
				     data ? *data : NULL);
			if (!gl_success("glTexImage3D"))
				success = false;
		}

		if (data)
			data++;

		if (width > 1)
			width /= 2;
		if (height > 1)
			height /= 2;
		if (depth > 1)
			depth /= 2;
	}

	if (data)
		*p_data = data;
	return success;
}

static bool upload_texture_3d(struct gs_texture_3d *tex,
			      const uint8_t *const *data)
{
	uint32_t num_levels = tex->base.levels;
	bool compressed = gs_is_compressed_format(tex->base.format);
	bool success;

	if (!num_levels)
		num_levels = gs_get_total_levels(tex->width, tex->height,
						 tex->depth);

	if (!gl_bind_texture(GL_TEXTURE_3D, tex->base.texture))
		return false;

	success = gl_init_volume(tex->base.gl_type, num_levels,
				 tex->base.gl_format,
				 tex->base.gl_internal_format, compressed,
				 tex->width, tex->height, tex->depth, &data);

	if (!gl_tex_param_i(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL,
			    num_levels - 1))
		success = false;
	if (!gl_bind_texture(GL_TEXTURE_3D, 0))
		success = false;

	return success;
}

static bool create_pixel_unpack_buffer(struct gs_texture_3d *tex)
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
		size *= tex->depth;
	} else {
		size *= tex->height;
		size *= tex->depth;
		size /= 8;
	}

	glBufferData(GL_PIXEL_UNPACK_BUFFER, size, 0, GL_DYNAMIC_DRAW);
	if (!gl_success("glBufferData"))
		success = false;

	if (!gl_bind_buffer(GL_PIXEL_UNPACK_BUFFER, 0))
		success = false;

	return success;
}

gs_texture_t *device_voltexture_create(gs_device_t *device, uint32_t width,
				       uint32_t height, uint32_t depth,
				       enum gs_color_format color_format,
				       uint32_t levels,
				       const uint8_t *const *data,
				       uint32_t flags)
{
	struct gs_texture_3d *tex = bzalloc(sizeof(struct gs_texture_3d));
	tex->base.device = device;
	tex->base.type = GS_TEXTURE_3D;
	tex->base.format = color_format;
	tex->base.levels = levels;
	tex->base.gl_format = convert_gs_format(color_format);
	tex->base.gl_internal_format = convert_gs_internal_format(color_format);
	tex->base.gl_type = get_gl_format_type(color_format);
	tex->base.gl_target = GL_TEXTURE_3D;
	tex->base.is_dynamic = (flags & GS_DYNAMIC) != 0;
	tex->base.is_render_target = false;
	tex->base.is_dummy = (flags & GS_GL_DUMMYTEX) != 0;
	tex->base.gen_mipmaps = (flags & GS_BUILD_MIPMAPS) != 0;
	tex->width = width;
	tex->height = height;
	tex->depth = depth;

	if (!gl_gen_textures(1, &tex->base.texture))
		goto fail;

	if (!tex->base.is_dummy) {
		if (tex->base.is_dynamic && !create_pixel_unpack_buffer(tex))
			goto fail;
		if (!upload_texture_3d(tex, data))
			goto fail;
	} else {
		if (!gl_bind_texture(GL_TEXTURE_3D, tex->base.texture))
			goto fail;

		bool compressed = gs_is_compressed_format(tex->base.format);
		bool did_init = gl_init_volume(tex->base.gl_type, 1,
					       tex->base.gl_format,
					       tex->base.gl_internal_format,
					       compressed, tex->width,
					       tex->height, tex->depth, NULL);
		did_init =
			gl_tex_param_i(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0);

		bool did_unbind = gl_bind_texture(GL_TEXTURE_3D, 0);
		if (!did_init || !did_unbind)
			goto fail;
	}

	return (gs_texture_t *)tex;

fail:
	gs_texture_destroy((gs_texture_t *)tex);
	blog(LOG_ERROR, "device_voltexture_create (GL) failed");
	return NULL;
}

void gs_voltexture_destroy(gs_texture_t *voltex)
{
	gs_texture_destroy(voltex);
}
