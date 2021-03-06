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

#pragma once

/*
 *   Allows posix thread usage on windows as well as other operating systems.
 * Use this header if you want to make your code more platform independent.
 *
 *   Also provides a custom platform-independent "event" handler via
 * pthread conditional waits.
 */

#include "c99defs.h"

#ifdef _MSC_VER
#include "../../deps/w32-pthreads/pthread.h"
#else
#include <errno.h>
#include <pthread.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#include "threading-windows.h"
#else
#include "threading-posix.h"
#endif

/* this may seem strange, but you can't use it unless it's an initializer */
static inline void pthread_mutex_init_value(pthread_mutex_t *mutex)
{
	pthread_mutex_t init_val = PTHREAD_MUTEX_INITIALIZER;
	if (!mutex)
		return;

	*mutex = init_val;
}

enum os_event_type {
	OS_EVENT_TYPE_AUTO,
	OS_EVENT_TYPE_MANUAL,
};

struct os_event_data;
struct os_sem_data;
typedef struct os_event_data os_event_t;
typedef struct os_sem_data os_sem_t;

EXPORT int os_event_init(os_event_t **event, enum os_event_type type);
EXPORT void os_event_destroy(os_event_t *event);
EXPORT int os_event_wait(os_event_t *event);
EXPORT int os_event_timedwait(os_event_t *event, unsigned long milliseconds);
EXPORT int os_event_try(os_event_t *event);
EXPORT int os_event_signal(os_event_t *event);
EXPORT void os_event_reset(os_event_t *event);

EXPORT int os_sem_init(os_sem_t **sem, int value);
EXPORT void os_sem_destroy(os_sem_t *sem);
EXPORT int os_sem_post(os_sem_t *sem);
EXPORT int os_sem_wait(os_sem_t *sem);

EXPORT void os_set_thread_name(const char *name);

#ifdef _MSC_VER
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL __thread
#endif

#ifdef __cplusplus
}
#endif
