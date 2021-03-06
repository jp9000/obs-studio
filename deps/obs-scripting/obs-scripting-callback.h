/******************************************************************************
    Copyright (C) 2017 by Hugh Bailey <jim@obsproject.com>

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

#include <callback/calldata.h>
#include <util/threading.h>
#include <util/bmem.h>
#include "obs-scripting-internal.h"

extern pthread_mutex_t detach_mutex;
extern struct script_callback *detached_callbacks;

struct script_callback {
	struct script_callback *next;
	struct script_callback **p_prev_next;

	void (*on_remove)(void *p_cb);
	obs_script_t *script;
	calldata_t extra;

	bool removed;
};

static inline void *add_script_callback(struct script_callback **first,
					obs_script_t *script, size_t extra_size)
{
	struct script_callback *cb = bzalloc(sizeof(*cb) + extra_size);
	cb->script = script;

	struct script_callback *next = *first;
	cb->next = next;
	cb->p_prev_next = first;
	if (next)
		next->p_prev_next = &cb->next;
	*first = cb;

	return cb;
}

static inline void remove_script_callback(struct script_callback *cb)
{
	cb->removed = true;

	struct script_callback *next = cb->next;
	if (next)
		next->p_prev_next = cb->p_prev_next;
	*cb->p_prev_next = cb->next;

	pthread_mutex_lock(&detach_mutex);
	next = detached_callbacks;
	cb->next = next;
	if (next)
		next->p_prev_next = &cb->next;
	cb->p_prev_next = &detached_callbacks;
	detached_callbacks = cb;
	pthread_mutex_unlock(&detach_mutex);

	if (cb->on_remove)
		cb->on_remove(cb);
}

static inline void just_free_script_callback(struct script_callback *cb)
{
	calldata_free(&cb->extra);
	bfree(cb);
}

static inline void free_script_callback(struct script_callback *cb)
{
	pthread_mutex_lock(&detach_mutex);
	struct script_callback *next = cb->next;
	if (next)
		next->p_prev_next = cb->p_prev_next;
	*cb->p_prev_next = cb->next;
	pthread_mutex_unlock(&detach_mutex);

	just_free_script_callback(cb);
}
