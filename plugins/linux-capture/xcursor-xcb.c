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

#include <stdint.h>
#include <xcb/xfixes.h>

#include <util/bmem.h>
#include "xcursor-xcb.h"

/*
 * Create the cursor texture, either by updating if the new cursor has the same
 * size or by creating a new texture if the size is different
 */
static void xcb_xcursor_create(xcb_xcursor_t *data,
			       xcb_xfixes_get_cursor_image_reply_t *xc)
{
	uint32_t *pixels = xcb_xfixes_get_cursor_image_cursor_image(xc);
	if (!pixels)
		return;

	if (data->tex && data->last_height == xc->width &&
	    data->last_width == xc->height) {
		gs_texture_set_image(data->tex, (const uint8_t *)pixels,
				     xc->width * sizeof(uint32_t), false);
	} else {
		if (data->tex)
			gs_texture_destroy(data->tex);

		data->tex = gs_texture_create(xc->width, xc->height, GS_BGRA, 1,
					      (const uint8_t **)&pixels,
					      GS_DYNAMIC);
	}

	data->last_serial = xc->cursor_serial;
	data->last_width = xc->width;
	data->last_height = xc->height;
}

/**
 * We need to check for the xfixes version in order to initialize it ?
 */
xcb_xcursor_t *xcb_xcursor_init(xcb_connection_t *xcb)
{
	xcb_xcursor_t *data = bzalloc(sizeof(xcb_xcursor_t));

	xcb_xfixes_query_version_cookie_t xfix_c;

	xfix_c = xcb_xfixes_query_version_unchecked(
		xcb, XCB_XFIXES_MAJOR_VERSION, XCB_XFIXES_MINOR_VERSION);
	free(xcb_xfixes_query_version_reply(xcb, xfix_c, NULL));

	return data;
}

void xcb_xcursor_destroy(xcb_xcursor_t *data)
{
	if (data->tex)
		gs_texture_destroy(data->tex);
	bfree(data);
}

void xcb_xcursor_update(xcb_xcursor_t *data,
			xcb_xfixes_get_cursor_image_reply_t *xc)
{
	if (!data || !xc)
		return;

	if (!data->tex || data->last_serial != xc->cursor_serial)
		xcb_xcursor_create(data, xc);

	data->x = xc->x - data->x_org;
	data->y = xc->y - data->y_org;
	data->x_render = data->x - xc->xhot;
	data->y_render = data->y - xc->yhot;
}

void xcb_xcursor_render(xcb_xcursor_t *data)
{
	if (!data->tex)
		return;

	const bool linear_srgb = gs_get_linear_srgb();

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(linear_srgb);

	gs_effect_t *effect = gs_get_effect();
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	if (linear_srgb)
		gs_effect_set_texture_srgb(image, data->tex);
	else
		gs_effect_set_texture(image, data->tex);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);
	gs_enable_color(true, true, true, false);

	gs_matrix_push();
	gs_matrix_translate3f(data->x_render, data->y_render, 0.0f);
	gs_draw_sprite(data->tex, 0, 0, 0);
	gs_matrix_pop();

	gs_enable_color(true, true, true, true);
	gs_blend_state_pop();

	gs_enable_framebuffer_srgb(previous);
}

void xcb_xcursor_offset(xcb_xcursor_t *data, const int x_org, const int y_org)
{
	data->x_org = x_org;
	data->y_org = y_org;
}
