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

#include "ff-callbacks.h"
#include "ff-circular-queue.h"
#include "ff-clock.h"
#include "ff-packet-queue.h"
#include "ff-timer.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ff_decoder {
	AVCodecContext *codec;
	AVStream *stream;

	pthread_t decoder_thread;
	struct ff_timer refresh_timer;

	struct ff_callbacks *callbacks;
	struct ff_packet_queue packet_queue;
	struct ff_circular_queue frame_queue;
	unsigned int packet_queue_size;

	double timer_next_wake;
	double previous_pts;      // previous decoded frame's pts
	double previous_pts_diff; // previous decoded frame pts delay
	double predicted_pts;     // predicted pts of next frame
	double current_pts;       // pts of the most recently dispatched frame
	int64_t current_pts_time; // clock time when current_pts was set
	int64_t start_pts;

	bool hwaccel_decoder;
	enum AVDiscard frame_drop;
	struct ff_clock *clock;
	enum ff_av_sync_type natural_sync_clock;

	bool first_frame;
	bool eof;
	bool abort;
	bool finished;
};

typedef struct ff_decoder ff_decoder_t;

struct ff_decoder *ff_decoder_init(AVCodecContext *codec_context,
                                   AVStream *stream,
                                   unsigned int packet_queue_size,
                                   unsigned int frame_queue_size);
bool ff_decoder_start(struct ff_decoder *decoder);
void ff_decoder_free(struct ff_decoder *decoder);

bool ff_decoder_full(struct ff_decoder *decoder);
bool ff_decoder_accept(struct ff_decoder *decoder, struct ff_packet *packet);

double ff_decoder_clock(void *opaque);

void ff_decoder_schedule_refresh(struct ff_decoder *decoder, int delay);
void ff_decoder_refresh(void *opaque);

double ff_decoder_get_best_effort_pts(struct ff_decoder *decoder,
                                      AVFrame *frame);

bool ff_decoder_set_frame_drop_state(struct ff_decoder *decoder,
                                     int64_t start_time, int64_t pts);

#ifdef __cplusplus
}
#endif
