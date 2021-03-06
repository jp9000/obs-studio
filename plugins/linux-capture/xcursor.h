/*
Copyright (C) 2014 by Leonhard Oelke <leonhard@in-verted.de>

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
*/

#pragma once

#include <obs.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	Display *dpy;
	float render_x;
	float render_y;
	unsigned long last_serial;
	uint_fast32_t last_width;
	uint_fast32_t last_height;
	gs_texture_t *tex;

	int_fast32_t x, y;
	int_fast32_t x_org;
	int_fast32_t y_org;
} xcursor_t;

/**
 * Initializes the xcursor object
 *
 * This needs to be executed within a valid render context
 */
xcursor_t *xcursor_init(Display *dpy);

/**
 * Destroys the xcursor object
 */
void xcursor_destroy(xcursor_t *data);

/**
 * Update the cursor texture
 *
 * This needs to be executed within a valid render context
 */
void xcursor_tick(xcursor_t *data);

/**
 * Draw the cursor
 *
 * This needs to be executed within a valid render context
 */
void xcursor_render(xcursor_t *data, int x_offset, int y_offset);

/**
 * Specify offset for the cursor
 */
void xcursor_offset(xcursor_t *data, int_fast32_t x_org, int_fast32_t y_org);

#ifdef __cplusplus
}
#endif
