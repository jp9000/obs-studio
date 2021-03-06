/*
 * Copyright (c) 2015 John R. Bradley <jrb@turrettech.com>
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

#include <libavutil/avutil.h>
#include <pthread.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ff_timer_callback)(void *opaque);

struct ff_timer {
	ff_timer_callback callback;
	void *opaque;

	pthread_mutex_t mutex;
	pthread_mutexattr_t mutexattr;
	pthread_cond_t cond;

	pthread_t timer_thread;
	uint64_t next_wake;
	bool needs_wake;

	bool abort;
};

typedef struct ff_timer ff_timer_t;

bool ff_timer_init(struct ff_timer *timer, ff_timer_callback callback,
                   void *opaque);
void ff_timer_free(struct ff_timer *timer);
void ff_timer_schedule(struct ff_timer *timer, uint64_t microseconds);

#ifdef __cplusplus
}
#endif
