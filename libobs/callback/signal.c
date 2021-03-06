/*
 * Copyright (c) 2013-2014 Hugh Bailey <obs.jim@gmail.com>
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

#include "../util/darray.h"
#include "../util/threading.h"

#include "decl.h"
#include "signal.h"

struct signal_callback {
	signal_callback_t callback;
	void *data;
	bool remove;
	bool keep_ref;
};

struct signal_info {
	struct decl_info func;
	DARRAY(struct signal_callback) callbacks;
	pthread_mutex_t mutex;
	bool signalling;

	struct signal_info *next;
};

static inline struct signal_info *signal_info_create(struct decl_info *info)
{
	pthread_mutexattr_t attr;
	struct signal_info *si;

	if (pthread_mutexattr_init(&attr) != 0)
		return NULL;
	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
		return NULL;

	si = bmalloc(sizeof(struct signal_info));

	si->func = *info;
	si->next = NULL;
	si->signalling = false;
	da_init(si->callbacks);

	if (pthread_mutex_init(&si->mutex, &attr) != 0) {
		blog(LOG_ERROR, "Could not create signal");

		decl_info_free(&si->func);
		bfree(si);
		return NULL;
	}

	return si;
}

static inline void signal_info_destroy(struct signal_info *si)
{
	if (si) {
		pthread_mutex_destroy(&si->mutex);
		decl_info_free(&si->func);
		da_free(si->callbacks);
		bfree(si);
	}
}

static inline size_t signal_get_callback_idx(struct signal_info *si,
					     signal_callback_t callback,
					     void *data)
{
	for (size_t i = 0; i < si->callbacks.num; i++) {
		struct signal_callback *sc = si->callbacks.array + i;

		if (sc->callback == callback && sc->data == data)
			return i;
	}

	return DARRAY_INVALID;
}

struct global_callback_info {
	global_signal_callback_t callback;
	void *data;
	long signaling;
	bool remove;
};

struct signal_handler {
	struct signal_info *first;
	pthread_mutex_t mutex;
	volatile long refs;

	DARRAY(struct global_callback_info) global_callbacks;
	pthread_mutex_t global_callbacks_mutex;
};

static struct signal_info *getsignal(signal_handler_t *handler,
				     const char *name,
				     struct signal_info **p_last)
{
	struct signal_info *signal, *last = NULL;

	signal = handler->first;
	while (signal != NULL) {
		if (strcmp(signal->func.name, name) == 0)
			break;

		last = signal;
		signal = signal->next;
	}

	if (p_last)
		*p_last = last;
	return signal;
}

/* ------------------------------------------------------------------------- */

signal_handler_t *signal_handler_create(void)
{
	struct signal_handler *handler = bzalloc(sizeof(struct signal_handler));
	handler->first = NULL;
	handler->refs = 1;

	pthread_mutexattr_t attr;
	if (pthread_mutexattr_init(&attr) != 0)
		return NULL;
	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
		return NULL;

	if (pthread_mutex_init(&handler->mutex, NULL) != 0) {
		blog(LOG_ERROR, "Couldn't create signal handler mutex!");
		bfree(handler);
		return NULL;
	}
	if (pthread_mutex_init(&handler->global_callbacks_mutex, &attr) != 0) {
		blog(LOG_ERROR, "Couldn't create signal handler global "
				"callbacks mutex!");
		pthread_mutex_destroy(&handler->mutex);
		bfree(handler);
		return NULL;
	}

	return handler;
}

static void signal_handler_actually_destroy(signal_handler_t *handler)
{
	struct signal_info *sig = handler->first;
	while (sig != NULL) {
		struct signal_info *next = sig->next;
		signal_info_destroy(sig);
		sig = next;
	}

	da_free(handler->global_callbacks);
	pthread_mutex_destroy(&handler->global_callbacks_mutex);
	pthread_mutex_destroy(&handler->mutex);
	bfree(handler);
}

void signal_handler_destroy(signal_handler_t *handler)
{
	if (handler && os_atomic_dec_long(&handler->refs) == 0) {
		signal_handler_actually_destroy(handler);
	}
}

bool signal_handler_add(signal_handler_t *handler, const char *signal_decl)
{
	struct decl_info func = {0};
	struct signal_info *sig, *last;
	bool success = true;

	if (!parse_decl_string(&func, signal_decl)) {
		blog(LOG_ERROR, "Signal declaration invalid: %s", signal_decl);
		return false;
	}

	pthread_mutex_lock(&handler->mutex);

	sig = getsignal(handler, func.name, &last);
	if (sig) {
		blog(LOG_WARNING, "Signal declaration '%s' exists", func.name);
		decl_info_free(&func);
		success = false;
	} else {
		sig = signal_info_create(&func);
		if (!last)
			handler->first = sig;
		else
			last->next = sig;
	}

	pthread_mutex_unlock(&handler->mutex);

	return success;
}

static void signal_handler_connect_internal(signal_handler_t *handler,
					    const char *signal,
					    signal_callback_t callback,
					    void *data, bool keep_ref)
{
	struct signal_info *sig, *last;
	struct signal_callback cb_data = {callback, data, false, keep_ref};
	size_t idx;

	if (!handler)
		return;

	pthread_mutex_lock(&handler->mutex);
	sig = getsignal(handler, signal, &last);
	pthread_mutex_unlock(&handler->mutex);

	if (!sig) {
		blog(LOG_WARNING,
		     "signal_handler_connect: "
		     "signal '%s' not found",
		     signal);
		return;
	}

	/* -------------- */

	pthread_mutex_lock(&sig->mutex);

	if (keep_ref)
		os_atomic_inc_long(&handler->refs);

	idx = signal_get_callback_idx(sig, callback, data);
	if (keep_ref || idx == DARRAY_INVALID)
		da_push_back(sig->callbacks, &cb_data);

	pthread_mutex_unlock(&sig->mutex);
}

void signal_handler_connect(signal_handler_t *handler, const char *signal,
			    signal_callback_t callback, void *data)
{
	signal_handler_connect_internal(handler, signal, callback, data, false);
}

void signal_handler_connect_ref(signal_handler_t *handler, const char *signal,
				signal_callback_t callback, void *data)
{
	signal_handler_connect_internal(handler, signal, callback, data, true);
}

static inline struct signal_info *getsignal_locked(signal_handler_t *handler,
						   const char *name)
{
	struct signal_info *sig;

	if (!handler)
		return NULL;

	pthread_mutex_lock(&handler->mutex);
	sig = getsignal(handler, name, NULL);
	pthread_mutex_unlock(&handler->mutex);

	return sig;
}

void signal_handler_disconnect(signal_handler_t *handler, const char *signal,
			       signal_callback_t callback, void *data)
{
	struct signal_info *sig = getsignal_locked(handler, signal);
	bool keep_ref = false;
	size_t idx;

	if (!sig)
		return;

	pthread_mutex_lock(&sig->mutex);

	idx = signal_get_callback_idx(sig, callback, data);
	if (idx != DARRAY_INVALID) {
		if (sig->signalling) {
			sig->callbacks.array[idx].remove = true;
		} else {
			keep_ref = sig->callbacks.array[idx].keep_ref;
			da_erase(sig->callbacks, idx);
		}
	}

	pthread_mutex_unlock(&sig->mutex);

	if (keep_ref && os_atomic_dec_long(&handler->refs) == 0) {
		signal_handler_actually_destroy(handler);
	}
}

static THREAD_LOCAL struct signal_callback *current_signal_cb = NULL;
static THREAD_LOCAL struct global_callback_info *current_global_cb = NULL;

void signal_handler_remove_current(void)
{
	if (current_signal_cb)
		current_signal_cb->remove = true;
	else if (current_global_cb)
		current_global_cb->remove = true;
}

void signal_handler_signal(signal_handler_t *handler, const char *signal,
			   calldata_t *params)
{
	struct signal_info *sig = getsignal_locked(handler, signal);
	long remove_refs = 0;

	if (!sig)
		return;

	pthread_mutex_lock(&sig->mutex);
	sig->signalling = true;

	for (size_t i = 0; i < sig->callbacks.num; i++) {
		struct signal_callback *cb = sig->callbacks.array + i;
		if (!cb->remove) {
			current_signal_cb = cb;
			cb->callback(cb->data, params);
			current_signal_cb = NULL;
		}
	}

	for (size_t i = sig->callbacks.num; i > 0; i--) {
		struct signal_callback *cb = sig->callbacks.array + i - 1;
		if (cb->remove) {
			if (cb->keep_ref)
				remove_refs++;

			da_erase(sig->callbacks, i - 1);
		}
	}

	sig->signalling = false;
	pthread_mutex_unlock(&sig->mutex);

	pthread_mutex_lock(&handler->global_callbacks_mutex);

	if (handler->global_callbacks.num) {
		for (size_t i = 0; i < handler->global_callbacks.num; i++) {
			struct global_callback_info *cb =
				handler->global_callbacks.array + i;

			if (!cb->remove) {
				cb->signaling++;
				current_global_cb = cb;
				cb->callback(cb->data, signal, params);
				current_global_cb = NULL;
				cb->signaling--;
			}
		}

		for (size_t i = handler->global_callbacks.num; i > 0; i--) {
			struct global_callback_info *cb =
				handler->global_callbacks.array + (i - 1);

			if (cb->remove && !cb->signaling)
				da_erase(handler->global_callbacks, i - 1);
		}
	}

	pthread_mutex_unlock(&handler->global_callbacks_mutex);

	if (remove_refs) {
		os_atomic_set_long(&handler->refs,
				   os_atomic_load_long(&handler->refs) -
					   remove_refs);
	}
}

void signal_handler_connect_global(signal_handler_t *handler,
				   global_signal_callback_t callback,
				   void *data)
{
	struct global_callback_info cb_data = {callback, data, 0, false};
	size_t idx;

	if (!handler || !callback)
		return;

	pthread_mutex_lock(&handler->global_callbacks_mutex);

	idx = da_find(handler->global_callbacks, &cb_data, 0);
	if (idx == DARRAY_INVALID)
		da_push_back(handler->global_callbacks, &cb_data);

	pthread_mutex_unlock(&handler->global_callbacks_mutex);
}

void signal_handler_disconnect_global(signal_handler_t *handler,
				      global_signal_callback_t callback,
				      void *data)
{
	struct global_callback_info cb_data = {callback, data, false};
	size_t idx;

	if (!handler || !callback)
		return;

	pthread_mutex_lock(&handler->global_callbacks_mutex);

	idx = da_find(handler->global_callbacks, &cb_data, 0);
	if (idx != DARRAY_INVALID) {
		struct global_callback_info *cb =
			handler->global_callbacks.array + idx;

		if (cb->signaling)
			cb->remove = true;
		else
			da_erase(handler->global_callbacks, idx);
	}

	pthread_mutex_unlock(&handler->global_callbacks_mutex);
}
