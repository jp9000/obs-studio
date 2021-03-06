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
#include <sys/shm.h>
#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xinerama.h>

#include "xhelpers.h"

bool xinerama_is_active(xcb_connection_t *xcb)
{
	if (!xcb || !xcb_get_extension_data(xcb, &xcb_xinerama_id)->present)
		return false;

	bool active = true;
	xcb_xinerama_is_active_cookie_t xnr_c;
	xcb_xinerama_is_active_reply_t *xnr_r;

	xnr_c = xcb_xinerama_is_active_unchecked(xcb);
	xnr_r = xcb_xinerama_is_active_reply(xcb, xnr_c, NULL);
	if (!xnr_r || xnr_r->state == 0)
		active = false;
	free(xnr_r);

	return active;
}

int xinerama_screen_count(xcb_connection_t *xcb)
{
	if (!xcb)
		return 0;

	int screens = 0;
	xcb_xinerama_query_screens_cookie_t scr_c;
	xcb_xinerama_query_screens_reply_t *scr_r;

	scr_c = xcb_xinerama_query_screens_unchecked(xcb);
	scr_r = xcb_xinerama_query_screens_reply(xcb, scr_c, NULL);
	if (scr_r)
		screens = scr_r->number;
	free(scr_r);

	return screens;
}

int xinerama_screen_geo(xcb_connection_t *xcb, int_fast32_t screen,
			int_fast32_t *x, int_fast32_t *y, int_fast32_t *w,
			int_fast32_t *h)
{
	if (!xcb)
		goto fail;

	bool success = false;
	xcb_xinerama_query_screens_cookie_t scr_c;
	xcb_xinerama_query_screens_reply_t *scr_r;
	xcb_xinerama_screen_info_iterator_t iter;

	scr_c = xcb_xinerama_query_screens_unchecked(xcb);
	scr_r = xcb_xinerama_query_screens_reply(xcb, scr_c, NULL);
	if (!scr_r)
		goto fail;

	iter = xcb_xinerama_query_screens_screen_info_iterator(scr_r);
	for (; iter.rem; --screen, xcb_xinerama_screen_info_next(&iter)) {
		if (!screen) {
			*x = iter.data->x_org;
			*y = iter.data->y_org;
			*w = iter.data->width;
			*h = iter.data->height;
			success = true;
		}
	}
	free(scr_r);

	if (success)
		return 0;

fail:
	*x = *y = *w = *h = 0;
	return -1;
}

bool randr_is_active(xcb_connection_t *xcb)
{
	if (!xcb || !xcb_get_extension_data(xcb, &xcb_randr_id)->present)
		return false;

	return true;
}

static bool randr_has_monitors(xcb_connection_t *xcb)
{
	xcb_randr_query_version_cookie_t ver_c;
	xcb_randr_query_version_reply_t *ver_r;

	ver_c = xcb_randr_query_version(xcb, XCB_RANDR_MAJOR_VERSION,
					XCB_RANDR_MINOR_VERSION);
	ver_r = xcb_randr_query_version_reply(xcb, ver_c, 0);
	if (!ver_r)
		return 0;

	bool ret = ver_r->major_version > 1 || ver_r->minor_version >= 5;
	free(ver_r);
	return ret;
}

int randr_screen_count(xcb_connection_t *xcb)
{
	if (!xcb)
		return 0;
	xcb_screen_t *screen;
	screen = xcb_setup_roots_iterator(xcb_get_setup(xcb)).data;

	if (randr_has_monitors(xcb)) {
		xcb_randr_get_monitors_cookie_t mon_c;
		xcb_randr_get_monitors_reply_t *mon_r;

		mon_c = xcb_randr_get_monitors(xcb, screen->root, true);
		mon_r = xcb_randr_get_monitors_reply(xcb, mon_c, 0);
		if (!mon_r)
			return 0;

		int count = xcb_randr_get_monitors_monitors_length(mon_r);
		free(mon_r);
		return count;
	}
	xcb_randr_get_screen_resources_cookie_t res_c;
	xcb_randr_get_screen_resources_reply_t *res_r;

	res_c = xcb_randr_get_screen_resources(xcb, screen->root);
	res_r = xcb_randr_get_screen_resources_reply(xcb, res_c, 0);
	if (!res_r)
		return 0;

	return xcb_randr_get_screen_resources_crtcs_length(res_r);
}

int randr_screen_geo(xcb_connection_t *xcb, int_fast32_t screen,
		     int_fast32_t *x, int_fast32_t *y, int_fast32_t *w,
		     int_fast32_t *h, xcb_screen_t **rscreen, char **name)
{
	xcb_screen_t *xscreen;
	xscreen = xcb_setup_roots_iterator(xcb_get_setup(xcb)).data;

	if (randr_has_monitors(xcb)) {
		xcb_randr_get_monitors_cookie_t mon_c;
		xcb_randr_get_monitors_reply_t *mon_r;

		mon_c = xcb_randr_get_monitors(xcb, xscreen->root, true);
		mon_r = xcb_randr_get_monitors_reply(xcb, mon_c, 0);
		if (!mon_r)
			return 0;

		int monitors = xcb_randr_get_monitors_monitors_length(mon_r);
		if (screen < 0 || screen >= monitors) {
			free(mon_r);
			goto fail;
		}

		xcb_randr_monitor_info_iterator_t mon_i;
		mon_i = xcb_randr_get_monitors_monitors_iterator(mon_r);

		int s;
		for (s = 0; s < screen; s++)
			xcb_randr_monitor_info_next(&mon_i);

		xcb_randr_monitor_info_t *mon = mon_i.data;

		*x = mon->x;
		*y = mon->y;
		*w = mon->width;
		*h = mon->height;
		if (rscreen)
			*rscreen = xscreen;

		if (mon->name && name) {
			xcb_get_atom_name_cookie_t atom_c;
			xcb_get_atom_name_reply_t *atom_r;

			atom_c = xcb_get_atom_name(xcb, mon->name);
			atom_r = xcb_get_atom_name_reply(xcb, atom_c, 0);
			if (atom_r) {
				*name = strndup(
					xcb_get_atom_name_name(atom_r),
					xcb_get_atom_name_name_length(atom_r));
				free(atom_r);
			}
		}
		free(mon_r);
		return 0;
	}
	xcb_randr_get_screen_resources_cookie_t res_c;
	xcb_randr_get_screen_resources_reply_t *res_r;

	res_c = xcb_randr_get_screen_resources(xcb, xscreen->root);
	res_r = xcb_randr_get_screen_resources_reply(xcb, res_c, 0);
	if (!res_r)
		goto fail;

	int screens = xcb_randr_get_screen_resources_crtcs_length(res_r);
	if (screen < 0 || screen >= screens)
		goto fail;

	xcb_randr_crtc_t *crtc = xcb_randr_get_screen_resources_crtcs(res_r);

	xcb_randr_get_crtc_info_cookie_t crtc_c;
	xcb_randr_get_crtc_info_reply_t *crtc_r;

	crtc_c = xcb_randr_get_crtc_info(xcb, *(crtc + screen), 0);
	crtc_r = xcb_randr_get_crtc_info_reply(xcb, crtc_c, 0);
	if (!crtc_r)
		goto fail;

	*x = crtc_r->x;
	*y = crtc_r->y;
	*w = crtc_r->width;
	*h = crtc_r->height;

	if (rscreen)
		*rscreen = xscreen;

	return 0;

fail:
	*x = *y = *w = *h = 0;
	return -1;
}

int x11_screen_geo(xcb_connection_t *xcb, int_fast32_t screen, int_fast32_t *w,
		   int_fast32_t *h)
{
	if (!xcb)
		goto fail;

	bool success = false;
	xcb_screen_iterator_t iter;

	iter = xcb_setup_roots_iterator(xcb_get_setup(xcb));
	for (; iter.rem; --screen, xcb_screen_next(&iter)) {
		if (!screen) {
			*w = iter.data->width_in_pixels;
			*h = iter.data->height_in_pixels;
			success = true;
		}
	}

	if (success)
		return 0;

fail:
	*w = *h = 0;
	return -1;
}

xcb_shm_t *xshm_xcb_attach(xcb_connection_t *xcb, const int w, const int h)
{
	if (!xcb)
		return NULL;

	xcb_shm_t *shm = bzalloc(sizeof(xcb_shm_t));
	shm->xcb = xcb;
	shm->seg = xcb_generate_id(shm->xcb);

	shm->shmid = shmget(IPC_PRIVATE, w * h * 4, IPC_CREAT | 0777);
	if (shm->shmid == -1)
		goto fail;

	xcb_shm_attach(shm->xcb, shm->seg, shm->shmid, false);

	shm->data = shmat(shm->shmid, NULL, 0);

	return shm;
fail:
	xshm_xcb_detach(shm);
	return NULL;
}

void xshm_xcb_detach(xcb_shm_t *shm)
{
	if (!shm)
		return;

	xcb_shm_detach(shm->xcb, shm->seg);

	if ((char *)shm->data != (char *)-1)
		shmdt(shm->data);

	if (shm->shmid != -1)
		shmctl(shm->shmid, IPC_RMID, NULL);

	bfree(shm);
}

xcb_screen_t *xcb_get_screen(xcb_connection_t *xcb, int screen)
{
	xcb_screen_iterator_t iter;

	iter = xcb_setup_roots_iterator(xcb_get_setup(xcb));
	for (; iter.rem; --screen, xcb_screen_next(&iter)) {
		if (screen == 0)
			return iter.data;
	}

	return NULL;
}
