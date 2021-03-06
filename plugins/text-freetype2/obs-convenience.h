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

#pragma once

#include <obs-module.h>

gs_vertbuffer_t *create_uv_vbuffer(uint32_t num_verts, bool add_color);
void draw_uv_vbuffer(gs_vertbuffer_t *vbuf, gs_texture_t *tex,
		     gs_effect_t *effect, uint32_t num_verts);

#define set_v3_rect(a, x, y, w, h)       \
	vec3_set(a, x, y, 0.0f);         \
	vec3_set(a + 1, x + w, y, 0.0f); \
	vec3_set(a + 2, x, y + h, 0.0f); \
	vec3_set(a + 3, x, y + h, 0.0f); \
	vec3_set(a + 4, x + w, y, 0.0f); \
	vec3_set(a + 5, x + w, y + h, 0.0f);

#define set_v2_uv(a, u, v, u2, v2) \
	vec2_set(a, u, v);         \
	vec2_set(a + 1, u2, v);    \
	vec2_set(a + 2, u, v2);    \
	vec2_set(a + 3, u, v2);    \
	vec2_set(a + 4, u2, v);    \
	vec2_set(a + 5, u2, v2);

#define set_rect_colors2(a, c1, c2) \
	uint32_t *b = a;            \
	b[0] = b[1] = b[4] = c1;    \
	b[2] = b[3] = b[5] = c2;
