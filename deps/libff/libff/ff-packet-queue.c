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

#include "ff-packet-queue.h"
#include "ff-compat.h"

bool packet_queue_init(struct ff_packet_queue *q)
{
	memset(q, 0, sizeof(struct ff_packet_queue));

	if (pthread_mutex_init(&q->mutex, NULL) != 0)
		goto fail;

	if (pthread_cond_init(&q->cond, NULL) != 0)
		goto fail1;

	av_init_packet(&q->flush_packet.base);
	q->flush_packet.base.data = (uint8_t *)"FLUSH";

	return true;

fail1:
	pthread_mutex_destroy(&q->mutex);
fail:
	return false;
}

void packet_queue_abort(struct ff_packet_queue *q)
{
	pthread_mutex_lock(&q->mutex);
	q->abort = true;
	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex);
}

void packet_queue_free(struct ff_packet_queue *q)
{
	packet_queue_flush(q);

	pthread_mutex_destroy(&q->mutex);
	pthread_cond_destroy(&q->cond);

	av_free_packet(&q->flush_packet.base);
}

int packet_queue_put(struct ff_packet_queue *q, struct ff_packet *packet)
{
	struct ff_packet_list *new_packet;

	new_packet = av_malloc(sizeof(struct ff_packet_list));

	if (new_packet == NULL)
		return FF_PACKET_FAIL;

	new_packet->packet = *packet;
	new_packet->next = NULL;

	pthread_mutex_lock(&q->mutex);

	if (q->last_packet == NULL)
		q->first_packet = new_packet;
	else
		q->last_packet->next = new_packet;

	q->last_packet = new_packet;

	q->count++;
	q->total_size += new_packet->packet.base.size;

	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex);

	return FF_PACKET_SUCCESS;
}

int packet_queue_put_flush_packet(struct ff_packet_queue *q)
{
	return packet_queue_put(q, &q->flush_packet);
}

int packet_queue_get(struct ff_packet_queue *q, struct ff_packet *packet,
                     bool block)
{
	struct ff_packet_list *potential_packet;
	int return_status;

	pthread_mutex_lock(&q->mutex);

	while (true) {
		potential_packet = q->first_packet;

		if (potential_packet != NULL) {
			q->first_packet = potential_packet->next;

			if (q->first_packet == NULL)
				q->last_packet = NULL;

			q->count--;
			q->total_size -= potential_packet->packet.base.size;
			*packet = potential_packet->packet;
			av_free(potential_packet);
			return_status = FF_PACKET_SUCCESS;
			break;

		} else if (!block) {
			return_status = FF_PACKET_EMPTY;
			break;

		} else {
			pthread_cond_wait(&q->cond, &q->mutex);
			if (q->abort) {
				return_status = FF_PACKET_FAIL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&q->mutex);

	return return_status;
}

void packet_queue_flush(struct ff_packet_queue *q)
{
	struct ff_packet_list *packet;

	pthread_mutex_lock(&q->mutex);

	for (packet = q->first_packet; packet != NULL;
	     packet = q->first_packet) {
		q->first_packet = packet->next;
		av_free_packet(&packet->packet.base);
		if (packet->packet.clock != NULL)
			ff_clock_release(&packet->packet.clock);
		av_freep(&packet);
	}

	q->last_packet = q->first_packet = NULL;
	q->count = 0;
	q->total_size = 0;

	pthread_mutex_unlock(&q->mutex);
}
