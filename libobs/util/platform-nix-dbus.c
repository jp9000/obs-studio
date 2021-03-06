/*
 * Copyright (c) 2015 Hugh Bailey <obs.jim@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include <gio/gio.h>
#include "bmem.h"

/* NOTE: This is basically just the VLC implementation from its d-bus power
 * management inhibition code.  Credit is theirs for this. */

enum service_type {
	FREEDESKTOP_SS, /* freedesktop screensaver (KDE >= 4, GNOME >= 3.10) */
	FREEDESKTOP_PM, /* freedesktop power management (KDE, gnome <= 2.26) */
	MATE_SM,        /* MATE (>= 1.0) session manager */
	GNOME_SM,       /* GNOME 2.26 - 3.4 sessopm mamager */
};

struct service_info {
	const char *name;
	const char *path;
	const char *interface;
	const char *uninhibit;
};

static const struct service_info services[] = {
	[FREEDESKTOP_SS] =
		{
			.name = "org.freedesktop.ScreenSaver",
			.path = "/ScreenSaver",
			.interface = "org.freedesktop.ScreenSaver",
			.uninhibit = "UnInhibit",
		},
	[FREEDESKTOP_PM] =
		{
			.name = "org.freedesktop.PowerManagement.Inhibit",
			.path = "/org/freedesktop/PowerManagement",
			.interface = "org.freedesktop.PowerManagement.Inhibit",
			.uninhibit = "UnInhibit",
		},
	[MATE_SM] =
		{
			.name = "org.mate.SessionManager",
			.path = "/org/mate/SessionManager",
			.interface = "org.mate.SessionManager",
			.uninhibit = "Uninhibit",
		},
	[GNOME_SM] =
		{
			.name = "org.gnome.SessionManager",
			.path = "/org/gnome/SessionManager",
			.interface = "org.gnome.SessionManager",
			.uninhibit = "Uninhibit",
		},
};

static const size_t num_services =
	(sizeof(services) / sizeof(struct service_info));

struct dbus_sleep_info {
	const struct service_info *service;
	GDBusConnection *c;
	uint32_t cookie;
	enum service_type type;
};

void dbus_sleep_info_destroy(struct dbus_sleep_info *info)
{
	if (info) {
		g_clear_object(&info->c);
		bfree(info);
	}
}

struct dbus_sleep_info *dbus_sleep_info_create(void)
{
	struct dbus_sleep_info *info = bzalloc(sizeof(*info));
	g_autoptr(GError) error = NULL;

	info->c = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
	if (!info->c) {
		blog(LOG_ERROR, "Could not create dbus connection: %s",
		     error->message);
		bfree(info);
		return NULL;
	}

	for (size_t i = 0; i < num_services; i++) {
		const struct service_info *service = &services[i];
		g_autoptr(GVariant) reply = NULL;

		if (!service->name)
			continue;

		reply = g_dbus_connection_call_sync(
			info->c, "org.freedesktop.DBus",
			"/org/freedesktop/DBus", "org.freedesktop.DBus",
			"GetNameOwner", g_variant_new("(s)", service->name),
			NULL, G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL, NULL);

		if (reply != NULL) {
			blog(LOG_DEBUG, "Found dbus service: %s",
			     service->name);
			info->service = service;
			info->type = (enum service_type)i;
			return info;
		}
	}

	dbus_sleep_info_destroy(info);
	return NULL;
}

void dbus_inhibit_sleep(struct dbus_sleep_info *info, const char *reason,
			bool active)
{
	g_autoptr(GVariant) reply = NULL;
	g_autoptr(GError) error = NULL;
	const char *method;
	GVariant *params;

	if (active == !!info->cookie)
		return;

	method = active ? "Inhibit" : info->service->uninhibit;

	if (active) {
		const char *program = "libobs";
		uint32_t flags = 0xC;
		uint32_t xid = 0;

		assert(info->cookie == 0);

		switch (info->type) {
		case MATE_SM:
		case GNOME_SM:
			params = g_variant_new("(s@usu)", program,
					       g_variant_new_uint32(xid),
					       reason, flags);
			break;
		default:
			params = g_variant_new("(ss)", program, reason);
		}
	} else {
		assert(info->cookie != 0);
		params = g_variant_new("(u)", info->cookie);
	}

	reply = g_dbus_connection_call_sync(
		info->c, info->service->name, info->service->path,
		info->service->interface, method, params, NULL,
		G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

	if (error != NULL) {
		blog(LOG_ERROR, "Failed to call %s: %s", method,
		     error->message);
		return;
	}

	if (active)
		g_variant_get(reply, "(u)", &info->cookie);
	else
		info->cookie = 0;
}
