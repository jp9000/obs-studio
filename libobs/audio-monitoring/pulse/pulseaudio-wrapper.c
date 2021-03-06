/*
Copyright (C) 2014 by Leonhard Oelke <leonhard@in-verted.de>
Copyright (C) 2017 by Fabio Madia <admshao@gmail.com>

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

#include <pthread.h>

#include <pulse/thread-mainloop.h>

#include <util/base.h>
#include <obs.h>

#include "pulseaudio-wrapper.h"

/* global data */
static uint_fast32_t pulseaudio_refs = 0;
static pthread_mutex_t pulseaudio_mutex = PTHREAD_MUTEX_INITIALIZER;
static pa_threaded_mainloop *pulseaudio_mainloop = NULL;
static pa_context *pulseaudio_context = NULL;

static void pulseaudio_default_devices(pa_context *c, const pa_server_info *i,
				       void *userdata)
{
	UNUSED_PARAMETER(c);
	struct pulseaudio_default_output *d =
		(struct pulseaudio_default_output *)userdata;
	d->default_sink_name = bstrdup(i->default_sink_name);
	pulseaudio_signal(0);
}

void get_default_id(char **id)
{
	pulseaudio_init();
	struct pulseaudio_default_output *pdo =
		bzalloc(sizeof(struct pulseaudio_default_output));
	pulseaudio_get_server_info(
		(pa_server_info_cb_t)pulseaudio_default_devices, (void *)pdo);

	if (!pdo->default_sink_name || !*pdo->default_sink_name) {
		*id = NULL;
	} else {
		*id = bzalloc(strlen(pdo->default_sink_name) + 9);
		strcat(*id, pdo->default_sink_name);
		strcat(*id, ".monitor");
		bfree(pdo->default_sink_name);
	}

	bfree(pdo);
	pulseaudio_unref();
}

bool devices_match(const char *id1, const char *id2)
{
	bool match;
	char *name1 = NULL;
	char *name2 = NULL;

	if (!id1 || !id2)
		return false;

	if (strcmp(id1, "default") == 0) {
		get_default_id(&name1);
		id1 = name1;
	}
	if (strcmp(id2, "default") == 0) {
		get_default_id(&name2);
		id2 = name2;
	}

	match = strcmp(id1, id2) == 0;
	bfree(name1);
	bfree(name2);
	return match;
}

/**
 * context status change callback
 *
 * @todo this is currently a noop, we want to reconnect here if the connection
 *       is lost ...
 */
static void pulseaudio_context_state_changed(pa_context *c, void *userdata)
{
	UNUSED_PARAMETER(userdata);
	UNUSED_PARAMETER(c);

	pulseaudio_signal(0);
}

/**
 * get the default properties
 */
static pa_proplist *pulseaudio_properties()
{
	pa_proplist *p = pa_proplist_new();

	pa_proplist_sets(p, PA_PROP_APPLICATION_NAME, "OBS");
	pa_proplist_sets(p, PA_PROP_APPLICATION_ICON_NAME, "obs");
	pa_proplist_sets(p, PA_PROP_MEDIA_ROLE, "production");

	return p;
}

/**
 * Initialize the pulse audio context with properties and callback
 */
static void pulseaudio_init_context()
{
	pulseaudio_lock();

	pa_proplist *p = pulseaudio_properties();
	pulseaudio_context = pa_context_new_with_proplist(
		pa_threaded_mainloop_get_api(pulseaudio_mainloop),
		"OBS-Monitor", p);

	pa_context_set_state_callback(pulseaudio_context,
				      pulseaudio_context_state_changed, NULL);

	pa_context_connect(pulseaudio_context, NULL, PA_CONTEXT_NOAUTOSPAWN,
			   NULL);
	pa_proplist_free(p);

	pulseaudio_unlock();
}

/**
 * wait for context to be ready
 */
static int_fast32_t pulseaudio_context_ready()
{
	pulseaudio_lock();

	if (!PA_CONTEXT_IS_GOOD(pa_context_get_state(pulseaudio_context))) {
		pulseaudio_unlock();
		return -1;
	}

	while (pa_context_get_state(pulseaudio_context) != PA_CONTEXT_READY)
		pulseaudio_wait();

	pulseaudio_unlock();
	return 0;
}

int_fast32_t pulseaudio_init()
{
	pthread_mutex_lock(&pulseaudio_mutex);

	if (pulseaudio_refs == 0) {
		pulseaudio_mainloop = pa_threaded_mainloop_new();
		pa_threaded_mainloop_start(pulseaudio_mainloop);

		pulseaudio_init_context();
	}

	pulseaudio_refs++;

	pthread_mutex_unlock(&pulseaudio_mutex);

	return 0;
}

void pulseaudio_unref()
{
	pthread_mutex_lock(&pulseaudio_mutex);

	if (--pulseaudio_refs == 0) {
		pulseaudio_lock();
		if (pulseaudio_context != NULL) {
			pa_context_disconnect(pulseaudio_context);
			pa_context_unref(pulseaudio_context);
			pulseaudio_context = NULL;
		}
		pulseaudio_unlock();

		if (pulseaudio_mainloop != NULL) {
			pa_threaded_mainloop_stop(pulseaudio_mainloop);
			pa_threaded_mainloop_free(pulseaudio_mainloop);
			pulseaudio_mainloop = NULL;
		}
	}

	pthread_mutex_unlock(&pulseaudio_mutex);
}

void pulseaudio_lock()
{
	pa_threaded_mainloop_lock(pulseaudio_mainloop);
}

void pulseaudio_unlock()
{
	pa_threaded_mainloop_unlock(pulseaudio_mainloop);
}

void pulseaudio_wait()
{
	pa_threaded_mainloop_wait(pulseaudio_mainloop);
}

void pulseaudio_signal(int wait_for_accept)
{
	pa_threaded_mainloop_signal(pulseaudio_mainloop, wait_for_accept);
}

void pulseaudio_accept()
{
	pa_threaded_mainloop_accept(pulseaudio_mainloop);
}

int_fast32_t pulseaudio_get_source_info_list(pa_source_info_cb_t cb,
					     void *userdata)
{
	if (pulseaudio_context_ready() < 0)
		return -1;

	pulseaudio_lock();

	pa_operation *op = pa_context_get_source_info_list(pulseaudio_context,
							   cb, userdata);
	if (!op) {
		pulseaudio_unlock();
		return -1;
	}
	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pulseaudio_wait();
	pa_operation_unref(op);

	pulseaudio_unlock();

	return 0;
}

int_fast32_t pulseaudio_get_source_info(pa_source_info_cb_t cb,
					const char *name, void *userdata)
{
	if (pulseaudio_context_ready() < 0)
		return -1;

	pulseaudio_lock();

	pa_operation *op = pa_context_get_source_info_by_name(
		pulseaudio_context, name, cb, userdata);
	if (!op) {
		pulseaudio_unlock();
		return -1;
	}
	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pulseaudio_wait();
	pa_operation_unref(op);

	pulseaudio_unlock();

	return 0;
}

int_fast32_t pulseaudio_get_server_info(pa_server_info_cb_t cb, void *userdata)
{
	if (pulseaudio_context_ready() < 0)
		return -1;

	pulseaudio_lock();

	pa_operation *op =
		pa_context_get_server_info(pulseaudio_context, cb, userdata);
	if (!op) {
		pulseaudio_unlock();
		return -1;
	}
	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pulseaudio_wait();
	pa_operation_unref(op);

	pulseaudio_unlock();
	return 0;
}

pa_stream *pulseaudio_stream_new(const char *name, const pa_sample_spec *ss,
				 const pa_channel_map *map)
{
	if (pulseaudio_context_ready() < 0)
		return NULL;

	pulseaudio_lock();

	pa_proplist *p = pulseaudio_properties();
	pa_stream *s = pa_stream_new_with_proplist(pulseaudio_context, name, ss,
						   map, p);
	pa_proplist_free(p);

	pulseaudio_unlock();
	return s;
}

int_fast32_t pulseaudio_connect_playback(pa_stream *s, const char *name,
					 const pa_buffer_attr *attr,
					 pa_stream_flags_t flags)
{
	if (pulseaudio_context_ready() < 0)
		return -1;

	size_t dev_len = strlen(name) - 8;
	char *device = bzalloc(dev_len + 1);
	memcpy(device, name, dev_len);

	pulseaudio_lock();
	int_fast32_t ret =
		pa_stream_connect_playback(s, device, attr, flags, NULL, NULL);
	pulseaudio_unlock();

	bfree(device);
	return ret;
}

void pulseaudio_write_callback(pa_stream *p, pa_stream_request_cb_t cb,
			       void *userdata)
{
	if (pulseaudio_context_ready() < 0)
		return;

	pulseaudio_lock();
	pa_stream_set_write_callback(p, cb, userdata);
	pulseaudio_unlock();
}

void pulseaudio_set_underflow_callback(pa_stream *p, pa_stream_notify_cb_t cb,
				       void *userdata)
{
	if (pulseaudio_context_ready() < 0)
		return;

	pulseaudio_lock();
	pa_stream_set_underflow_callback(p, cb, userdata);
	pulseaudio_unlock();
}
