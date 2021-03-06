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

#include <pthread.h>

#include <pulse/thread-mainloop.h>

#include <util/base.h>
#include <obs.h>

#include "pulse-wrapper.h"

/* global data */
static uint_fast32_t pulse_refs = 0;
static pthread_mutex_t pulse_mutex = PTHREAD_MUTEX_INITIALIZER;
static pa_threaded_mainloop *pulse_mainloop = NULL;
static pa_context *pulse_context = NULL;

/**
 * context status change callback
 *
 * @todo this is currently a noop, we want to reconnect here if the connection
 *       is lost ...
 */
static void pulse_context_state_changed(pa_context *c, void *userdata)
{
	UNUSED_PARAMETER(userdata);
	UNUSED_PARAMETER(c);

	pulse_signal(0);
}

/**
 * get the default properties
 */
static pa_proplist *pulse_properties()
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
static void pulse_init_context()
{
	pulse_lock();

	pa_proplist *p = pulse_properties();
	pulse_context = pa_context_new_with_proplist(
		pa_threaded_mainloop_get_api(pulse_mainloop), "OBS", p);

	pa_context_set_state_callback(pulse_context,
				      pulse_context_state_changed, NULL);

	pa_context_connect(pulse_context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL);
	pa_proplist_free(p);

	pulse_unlock();
}

/**
 * wait for context to be ready
 */
static int_fast32_t pulse_context_ready()
{
	pulse_lock();

	if (!PA_CONTEXT_IS_GOOD(pa_context_get_state(pulse_context))) {
		pulse_unlock();
		return -1;
	}

	while (pa_context_get_state(pulse_context) != PA_CONTEXT_READY)
		pulse_wait();

	pulse_unlock();
	return 0;
}

int_fast32_t pulse_init()
{
	pthread_mutex_lock(&pulse_mutex);

	if (pulse_refs == 0) {
		pulse_mainloop = pa_threaded_mainloop_new();
		pa_threaded_mainloop_start(pulse_mainloop);

		pulse_init_context();
	}

	pulse_refs++;

	pthread_mutex_unlock(&pulse_mutex);

	return 0;
}

void pulse_unref()
{
	pthread_mutex_lock(&pulse_mutex);

	if (--pulse_refs == 0) {
		pulse_lock();
		if (pulse_context != NULL) {
			pa_context_disconnect(pulse_context);
			pa_context_unref(pulse_context);
			pulse_context = NULL;
		}
		pulse_unlock();

		if (pulse_mainloop != NULL) {
			pa_threaded_mainloop_stop(pulse_mainloop);
			pa_threaded_mainloop_free(pulse_mainloop);
			pulse_mainloop = NULL;
		}
	}

	pthread_mutex_unlock(&pulse_mutex);
}

void pulse_lock()
{
	pa_threaded_mainloop_lock(pulse_mainloop);
}

void pulse_unlock()
{
	pa_threaded_mainloop_unlock(pulse_mainloop);
}

void pulse_wait()
{
	pa_threaded_mainloop_wait(pulse_mainloop);
}

void pulse_signal(int wait_for_accept)
{
	pa_threaded_mainloop_signal(pulse_mainloop, wait_for_accept);
}

void pulse_accept()
{
	pa_threaded_mainloop_accept(pulse_mainloop);
}

int_fast32_t pulse_get_source_info_list(pa_source_info_cb_t cb, void *userdata)
{
	if (pulse_context_ready() < 0)
		return -1;

	pulse_lock();

	pa_operation *op =
		pa_context_get_source_info_list(pulse_context, cb, userdata);
	if (!op) {
		pulse_unlock();
		return -1;
	}
	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pulse_wait();
	pa_operation_unref(op);

	pulse_unlock();

	return 0;
}

int_fast32_t pulse_get_sink_info_list(pa_sink_info_cb_t cb, void *userdata)
{
	if (pulse_context_ready() < 0)
		return -1;

	pulse_lock();

	pa_operation *op =
		pa_context_get_sink_info_list(pulse_context, cb, userdata);
	if (!op) {
		pulse_unlock();
		return -1;
	}
	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pulse_wait();
	pa_operation_unref(op);

	pulse_unlock();

	return 0;
}

int_fast32_t pulse_get_source_info(pa_source_info_cb_t cb, const char *name,
				   void *userdata)
{
	if (pulse_context_ready() < 0)
		return -1;

	pulse_lock();

	pa_operation *op = pa_context_get_source_info_by_name(
		pulse_context, name, cb, userdata);
	if (!op) {
		pulse_unlock();
		return -1;
	}
	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pulse_wait();
	pa_operation_unref(op);

	pulse_unlock();

	return 0;
}

int_fast32_t pulse_get_server_info(pa_server_info_cb_t cb, void *userdata)
{
	if (pulse_context_ready() < 0)
		return -1;

	pulse_lock();

	pa_operation *op =
		pa_context_get_server_info(pulse_context, cb, userdata);
	if (!op) {
		pulse_unlock();
		return -1;
	}
	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pulse_wait();
	pa_operation_unref(op);

	pulse_unlock();
	return 0;
}

pa_stream *pulse_stream_new(const char *name, const pa_sample_spec *ss,
			    const pa_channel_map *map)
{
	if (pulse_context_ready() < 0)
		return NULL;

	pulse_lock();

	pa_proplist *p = pulse_properties();
	pa_stream *s =
		pa_stream_new_with_proplist(pulse_context, name, ss, map, p);
	pa_proplist_free(p);

	pulse_unlock();
	return s;
}
