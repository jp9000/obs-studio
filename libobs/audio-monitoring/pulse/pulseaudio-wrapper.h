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

#include <inttypes.h>
#include <pulse/stream.h>
#include <pulse/context.h>
#include <pulse/introspect.h>

#pragma once

struct pulseaudio_default_output {
	char *default_sink_name;
};

struct enum_cb {
	obs_enum_audio_device_cb cb;
	void *data;
	int cont;
};

void get_default_id(char **id);

bool devices_match(const char *id1, const char *id2);

/**
 * Initialize the pulseaudio mainloop and increase the reference count
 */
int_fast32_t pulseaudio_init();

/**
 * Unreference the pulseaudio mainloop, when the reference count reaches
 * zero the mainloop will automatically be destroyed
 */
void pulseaudio_unref();

/**
 * Lock the mainloop
 *
 * In order to allow for multiple threads to use the same mainloop pulseaudio
 * provides it's own locking mechanism. This function should be called before
 * using any pulseaudio function that is in any way related to the mainloop or
 * context.
 *
 * @note use of this function may cause deadlocks
 *
 * @warning do not use with pulseaudio_ wrapper functions
 */
void pulseaudio_lock();

/**
 * Unlock the mainloop
 *
 * @see pulseaudio_lock()
 */
void pulseaudio_unlock();

/**
 * Wait for events to happen
 *
 * This function should be called when waiting for an event to happen.
 */
void pulseaudio_wait();

/**
 * Wait for accept signal from calling thread
 *
 * This function tells the pulseaudio mainloop whether the data provided to
 * the callback should be retained until the calling thread executes
 * pulseaudio_accept()
 *
 * If wait_for_accept is 0 the function returns and the data is freed.
 */
void pulseaudio_signal(int wait_for_accept);

/**
 * Signal the waiting callback to return
 *
 * This function is used in conjunction with pulseaudio_signal()
 */
void pulseaudio_accept();

/**
 * Request source information
 *
 * The function will block until the operation was executed and the mainloop
 * called the provided callback function.
 *
 * @return negative on error
 *
 * @note The function will block until the server context is ready.
 *
 * @warning call without active locks
 */
int_fast32_t pulseaudio_get_source_info_list(pa_source_info_cb_t cb,
					     void *userdata);

/**
 * Request source information from a specific source
 *
 * The function will block until the operation was executed and the mainloop
 * called the provided callback function.
 *
 * @param cb pointer to the callback function
 * @param name the source name to get information for
 * @param userdata pointer to userdata the callback will be called with
 *
 * @return negative on error
 *
 * @note The function will block until the server context is ready.
 *
 * @warning call without active locks
 */
int_fast32_t pulseaudio_get_source_info(pa_source_info_cb_t cb,
					const char *name, void *userdata);

/**
 * Request server information
 *
 * The function will block until the operation was executed and the mainloop
 * called the provided callback function.
 *
 * @return negative on error
 *
 * @note The function will block until the server context is ready.
 *
 * @warning call without active locks
 */
int_fast32_t pulseaudio_get_server_info(pa_server_info_cb_t cb, void *userdata);

/**
 * Create a new stream with the default properties
 *
 * @note The function will block until the server context is ready.
 *
 * @warning call without active locks
 */
pa_stream *pulseaudio_stream_new(const char *name, const pa_sample_spec *ss,
				 const pa_channel_map *map);

/**
 * Connect to a pulseaudio playback stream
 *
 * @param s pa_stream to connect to. NULL for default
 * @param attr pa_buffer_attr
 * @param name Device name. NULL for default device
 * @param flags pa_stream_flags_t
 * @return negative on error
 */
int_fast32_t pulseaudio_connect_playback(pa_stream *s, const char *name,
					 const pa_buffer_attr *attr,
					 pa_stream_flags_t flags);

/**
 * Sets a callback function for when data can be written to the stream
 *
 * @param p pa_stream to connect to. NULL for default
 * @param cb pa_stream_request_cb_t
 * @param userdata pointer to userdata the callback will be called with
 */
void pulseaudio_write_callback(pa_stream *p, pa_stream_request_cb_t cb,
			       void *userdata);

/**
 * Sets a callback function for when an underflow happen
 *
 * @param p pa_stream to connect to. NULL for default
 * @param cb pa_stream_notify_cb_t
 * @param userdata pointer to userdata the callback will be called with
 */
void pulseaudio_set_underflow_callback(pa_stream *p, pa_stream_notify_cb_t cb,
				       void *userdata);
