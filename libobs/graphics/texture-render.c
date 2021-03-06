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

/*
 *   This is a set of helper functions to more easily render to textures
 * without having to duplicate too much code.
 */

#include <assert.h>
#include "graphics.h"

struct gs_texture_render {
	gs_texture_t *target, *prev_target;
	gs_zstencil_t *zs, *prev_zs;

	uint32_t cx, cy;

	enum gs_color_format format;
	enum gs_zstencil_format zsformat;

	bool rendered;
};

gs_texrender_t *gs_texrender_create(enum gs_color_format format,
				    enum gs_zstencil_format zsformat)
{
	struct gs_texture_render *texrender;
	texrender = bzalloc(sizeof(struct gs_texture_render));
	texrender->format = format;
	texrender->zsformat = zsformat;

	return texrender;
}

void gs_texrender_destroy(gs_texrender_t *texrender)
{
	if (texrender) {
		gs_texture_destroy(texrender->target);
		gs_zstencil_destroy(texrender->zs);
		bfree(texrender);
	}
}

static bool texrender_resetbuffer(gs_texrender_t *texrender, uint32_t cx,
				  uint32_t cy)
{
	if (!texrender)
		return false;

	gs_texture_destroy(texrender->target);
	gs_zstencil_destroy(texrender->zs);

	texrender->target = NULL;
	texrender->zs = NULL;
	texrender->cx = cx;
	texrender->cy = cy;

	texrender->target = gs_texture_create(cx, cy, texrender->format, 1,
					      NULL, GS_RENDER_TARGET);
	if (!texrender->target)
		return false;

	if (texrender->zsformat != GS_ZS_NONE) {
		texrender->zs = gs_zstencil_create(cx, cy, texrender->zsformat);
		if (!texrender->zs) {
			gs_texture_destroy(texrender->target);
			texrender->target = NULL;

			return false;
		}
	}

	return true;
}

bool gs_texrender_begin(gs_texrender_t *texrender, uint32_t cx, uint32_t cy)
{
	if (!texrender || texrender->rendered)
		return false;

	if (!cx || !cy)
		return false;

	if (texrender->cx != cx || texrender->cy != cy)
		if (!texrender_resetbuffer(texrender, cx, cy))
			return false;

	if (!texrender->target)
		return false;

	gs_viewport_push();
	gs_projection_push();
	gs_matrix_push();
	gs_matrix_identity();

	texrender->prev_target = gs_get_render_target();
	texrender->prev_zs = gs_get_zstencil_target();
	gs_set_render_target(texrender->target, texrender->zs);

	gs_set_viewport(0, 0, texrender->cx, texrender->cy);

	return true;
}

void gs_texrender_end(gs_texrender_t *texrender)
{
	if (!texrender)
		return;

	gs_set_render_target(texrender->prev_target, texrender->prev_zs);

	gs_matrix_pop();
	gs_projection_pop();
	gs_viewport_pop();

	texrender->rendered = true;
}

void gs_texrender_reset(gs_texrender_t *texrender)
{
	if (texrender)
		texrender->rendered = false;
}

gs_texture_t *gs_texrender_get_texture(const gs_texrender_t *texrender)
{
	return texrender ? texrender->target : NULL;
}
