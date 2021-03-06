/******************************************************************************
Copyright (C) 2014 by Nibbles

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

#include <obs-module.h>
#include <graphics/vec2.h>
#include <graphics/vec3.h>
#include <graphics/vec4.h>
#include "obs-convenience.h"

gs_vertbuffer_t *create_uv_vbuffer(uint32_t num_verts, bool add_color)
{
	obs_enter_graphics();

	gs_vertbuffer_t *tmp = NULL;
	struct gs_vb_data *vrect = NULL;

	vrect = gs_vbdata_create();
	vrect->num = num_verts;
	vrect->points = (struct vec3 *)bmalloc(sizeof(struct vec3) * num_verts);
	vrect->num_tex = 1;
	vrect->tvarray =
		(struct gs_tvertarray *)bmalloc(sizeof(struct gs_tvertarray));
	vrect->tvarray[0].width = 2;
	vrect->tvarray[0].array = bmalloc(sizeof(struct vec2) * num_verts);
	if (add_color)
		vrect->colors =
			(uint32_t *)bmalloc(sizeof(uint32_t) * num_verts);

	memset(vrect->points, 0, sizeof(struct vec3) * num_verts);
	memset(vrect->tvarray[0].array, 0, sizeof(struct vec2) * num_verts);
	if (add_color)
		memset(vrect->colors, 0, sizeof(uint32_t) * num_verts);

	tmp = gs_vertexbuffer_create(vrect, GS_DYNAMIC);

	if (tmp == NULL) {
		blog(LOG_WARNING, "Couldn't create UV vertex buffer.");
	}

	obs_leave_graphics();

	return tmp;
}

void draw_uv_vbuffer(gs_vertbuffer_t *vbuf, gs_texture_t *tex,
		     gs_effect_t *effect, uint32_t num_verts)
{
	gs_texture_t *texture = tex;
	gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	size_t passes;

	if (vbuf == NULL || tex == NULL)
		return;

	const bool linear_srgb = gs_get_linear_srgb();

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(linear_srgb);

	gs_vertexbuffer_flush(vbuf);
	gs_load_vertexbuffer(vbuf);
	gs_load_indexbuffer(NULL);

	passes = gs_technique_begin(tech);

	for (size_t i = 0; i < passes; i++) {
		if (gs_technique_begin_pass(tech, i)) {
			if (linear_srgb)
				gs_effect_set_texture_srgb(image, texture);
			else
				gs_effect_set_texture(image, texture);

			gs_draw(GS_TRIS, 0, num_verts);

			gs_technique_end_pass(tech);
		}
	}

	gs_technique_end(tech);

	gs_enable_framebuffer_srgb(previous);
}
