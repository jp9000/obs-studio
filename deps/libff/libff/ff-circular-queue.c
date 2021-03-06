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

#include "ff-circular-queue.h"

static void *queue_fetch_or_alloc(struct ff_circular_queue *cq, int index)
{
	if (cq->slots[index] == NULL)
		cq->slots[index] = av_mallocz(cq->item_size);

	return cq->slots[index];
}

static void queue_lock(struct ff_circular_queue *cq)
{
	pthread_mutex_lock(&cq->mutex);
}

static void queue_unlock(struct ff_circular_queue *cq)
{
	pthread_mutex_unlock(&cq->mutex);
}

static void queue_signal(struct ff_circular_queue *cq)
{
	pthread_cond_signal(&cq->cond);
}

static void queue_wait(struct ff_circular_queue *cq)
{
	pthread_cond_wait(&cq->cond, &cq->mutex);
}

bool ff_circular_queue_init(struct ff_circular_queue *cq, int item_size,
                            int capacity)
{
	memset(cq, 0, sizeof(struct ff_circular_queue));

	cq->item_size = item_size;
	cq->capacity = capacity;
	cq->abort = false;

	cq->slots = av_mallocz(capacity * sizeof(void *));

	if (cq->slots == NULL)
		goto fail;

	cq->size = 0;
	cq->write_index = 0;
	cq->read_index = 0;

	if (pthread_mutex_init(&cq->mutex, NULL) != 0)
		goto fail1;

	if (pthread_cond_init(&cq->cond, NULL) != 0)
		goto fail2;

	return true;

fail2:
	pthread_mutex_destroy(&cq->mutex);
fail1:
	av_free(cq->slots);
fail:
	return false;
}

void ff_circular_queue_abort(struct ff_circular_queue *cq)
{
	queue_lock(cq);
	cq->abort = true;
	queue_signal(cq);
	queue_unlock(cq);
}

void ff_circular_queue_free(struct ff_circular_queue *cq)
{
	ff_circular_queue_abort(cq);

	if (cq->slots != NULL)
		av_free(cq->slots);

	pthread_mutex_destroy(&cq->mutex);
	pthread_cond_destroy(&cq->cond);
}

void ff_circular_queue_wait_write(struct ff_circular_queue *cq)
{
	queue_lock(cq);

	while (cq->size >= cq->capacity && !cq->abort)
		queue_wait(cq);

	queue_unlock(cq);
}

void *ff_circular_queue_peek_write(struct ff_circular_queue *cq)
{
	return queue_fetch_or_alloc(cq, cq->write_index);
}

void ff_circular_queue_advance_write(struct ff_circular_queue *cq, void *item)
{
	cq->slots[cq->write_index] = item;
	cq->write_index = (cq->write_index + 1) % cq->capacity;

	queue_lock(cq);
	++cq->size;
	queue_unlock(cq);
}

void *ff_circular_queue_peek_read(struct ff_circular_queue *cq)
{
	return queue_fetch_or_alloc(cq, cq->read_index);
}

void ff_circular_queue_advance_read(struct ff_circular_queue *cq)
{
	cq->read_index = (cq->read_index + 1) % cq->capacity;
	queue_lock(cq);
	--cq->size;
	queue_signal(cq);
	queue_unlock(cq);
}
