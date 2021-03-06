/*
 * Copyright (c) 2014 Hugh Bailey <obs.jim@gmail.com>
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

#if defined(__APPLE__) || defined(__MINGW32__)
#include <sys/time.h>
#endif
#ifdef __APPLE__
#include <mach/semaphore.h>
#include <mach/task.h>
#include <mach/mach_init.h>
#else
#define _GNU_SOURCE
#include <semaphore.h>
#endif

#if defined(__FreeBSD__)
#include <pthread_np.h>
#endif

#include "bmem.h"
#include "threading.h"

struct os_event_data {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	volatile bool signalled;
	bool manual;
};

int os_event_init(os_event_t **event, enum os_event_type type)
{
	int code = 0;

	struct os_event_data *data = bzalloc(sizeof(struct os_event_data));

	if ((code = pthread_mutex_init(&data->mutex, NULL)) < 0) {
		bfree(data);
		return code;
	}

	if ((code = pthread_cond_init(&data->cond, NULL)) < 0) {
		pthread_mutex_destroy(&data->mutex);
		bfree(data);
		return code;
	}

	data->manual = (type == OS_EVENT_TYPE_MANUAL);
	data->signalled = false;
	*event = data;

	return 0;
}

void os_event_destroy(os_event_t *event)
{
	if (event) {
		pthread_mutex_destroy(&event->mutex);
		pthread_cond_destroy(&event->cond);
		bfree(event);
	}
}

int os_event_wait(os_event_t *event)
{
	int code = 0;
	pthread_mutex_lock(&event->mutex);
	while (!event->signalled) {
		code = pthread_cond_wait(&event->cond, &event->mutex);
		if (code != 0)
			break;
	}

	if (code == 0) {
		if (!event->manual)
			event->signalled = false;
	}

	pthread_mutex_unlock(&event->mutex);

	return code;
}

static inline void add_ms_to_ts(struct timespec *ts, unsigned long milliseconds)
{
	ts->tv_sec += milliseconds / 1000;
	ts->tv_nsec += (milliseconds % 1000) * 1000000;
	if (ts->tv_nsec > 1000000000) {
		ts->tv_sec += 1;
		ts->tv_nsec -= 1000000000;
	}
}

int os_event_timedwait(os_event_t *event, unsigned long milliseconds)
{
	int code = 0;
	pthread_mutex_lock(&event->mutex);
	while (!event->signalled) {
		struct timespec ts;
#if defined(__APPLE__) || defined(__MINGW32__)
		struct timeval tv;
		gettimeofday(&tv, NULL);
		ts.tv_sec = tv.tv_sec;
		ts.tv_nsec = tv.tv_usec * 1000;
#else
		clock_gettime(CLOCK_REALTIME, &ts);
#endif
		add_ms_to_ts(&ts, milliseconds);
		code = pthread_cond_timedwait(&event->cond, &event->mutex, &ts);
		if (code != 0)
			break;
	}

	if (code == 0) {
		if (!event->manual)
			event->signalled = false;
	}

	pthread_mutex_unlock(&event->mutex);

	return code;
}

int os_event_try(os_event_t *event)
{
	int ret = EAGAIN;

	pthread_mutex_lock(&event->mutex);
	if (event->signalled) {
		if (!event->manual)
			event->signalled = false;
		ret = 0;
	}
	pthread_mutex_unlock(&event->mutex);

	return ret;
}

int os_event_signal(os_event_t *event)
{
	int code = 0;

	pthread_mutex_lock(&event->mutex);
	code = pthread_cond_signal(&event->cond);
	event->signalled = true;
	pthread_mutex_unlock(&event->mutex);

	return code;
}

void os_event_reset(os_event_t *event)
{
	pthread_mutex_lock(&event->mutex);
	event->signalled = false;
	pthread_mutex_unlock(&event->mutex);
}

#ifdef __APPLE__

struct os_sem_data {
	semaphore_t sem;
	task_t task;
};

int os_sem_init(os_sem_t **sem, int value)
{
	semaphore_t new_sem;
	task_t task = mach_task_self();

	if (semaphore_create(task, &new_sem, 0, value) != KERN_SUCCESS)
		return -1;

	*sem = bzalloc(sizeof(struct os_sem_data));
	if (!*sem)
		return -2;

	(*sem)->sem = new_sem;
	(*sem)->task = task;
	return 0;
}

void os_sem_destroy(os_sem_t *sem)
{
	if (sem) {
		semaphore_destroy(sem->task, sem->sem);
		bfree(sem);
	}
}

int os_sem_post(os_sem_t *sem)
{
	if (!sem)
		return -1;
	return (semaphore_signal(sem->sem) == KERN_SUCCESS) ? 0 : -1;
}

int os_sem_wait(os_sem_t *sem)
{
	if (!sem)
		return -1;
	return (semaphore_wait(sem->sem) == KERN_SUCCESS) ? 0 : -1;
}

#else

struct os_sem_data {
	sem_t sem;
};

int os_sem_init(os_sem_t **sem, int value)
{
	sem_t new_sem;
	int ret = sem_init(&new_sem, 0, value);
	if (ret != 0)
		return ret;

	*sem = bzalloc(sizeof(struct os_sem_data));
	(*sem)->sem = new_sem;
	return 0;
}

void os_sem_destroy(os_sem_t *sem)
{
	if (sem) {
		sem_destroy(&sem->sem);
		bfree(sem);
	}
}

int os_sem_post(os_sem_t *sem)
{
	if (!sem)
		return -1;
	return sem_post(&sem->sem);
}

int os_sem_wait(os_sem_t *sem)
{
	if (!sem)
		return -1;
	return sem_wait(&sem->sem);
}

#endif

void os_set_thread_name(const char *name)
{
#if defined(__APPLE__)
	pthread_setname_np(name);
#elif defined(__FreeBSD__)
	pthread_set_name_np(pthread_self(), name);
#elif defined(__GLIBC__) && !defined(__MINGW32__)
	if (strlen(name) <= 15) {
		pthread_setname_np(pthread_self(), name);
	} else {
		char *thread_name = bstrdup_n(name, 15);
		pthread_setname_np(pthread_self(), thread_name);
		bfree(thread_name);
	}
#endif
}
