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
#include <xcb/xfixes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	unsigned int last_serial;
	unsigned int last_width;
	unsigned int last_height;
	gs_texture_t *tex;

	int x;
	int y;
	int x_org;
	int y_org;
	float x_render;
	float y_render;
} xcb_xcursor_t;

/**
 * Initializes the xcursor object
 *
 * @return NULL on error
 */
xcb_xcursor_t *xcb_xcursor_init(xcb_connection_t *xcb);

/**
 * Destroys the xcursor object
 * @param data xcursor object
 */
void xcb_xcursor_destroy(xcb_xcursor_t *data);

/**
 * Update the cursor data
 * @param data xcursor object
 * @param xc xcb cursor image reply
 *
 * @note This needs to be executed within a valid render context
 *
 */
void xcb_xcursor_update(xcb_xcursor_t *data,
			xcb_xfixes_get_cursor_image_reply_t *xc);

/**
 * Draw the cursor
 *
 * This needs to be executed within a valid render context
 */
void xcb_xcursor_render(xcb_xcursor_t *data);

/**
 * Specify offset for the cursor
 */
void xcb_xcursor_offset(xcb_xcursor_t *data, const int x_org, const int y_org);

#ifdef __cplusplus
}
#endif
