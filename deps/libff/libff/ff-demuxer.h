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

#include "ff-circular-queue.h"
#include "ff-decoder.h"
#include "ff-packet-queue.h"

#include <libavformat/avformat.h>

#define FF_DEMUXER_FAIL -1
#define FF_DEMUXER_SUCCESS 1

#include "ff-callbacks.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ff_demuxer_options {
	int audio_packet_queue_size;
	int video_packet_queue_size;
	int audio_frame_queue_size;
	int video_frame_queue_size;
	bool is_hw_decoding;
	bool is_looping;
	enum AVDiscard frame_drop;
	AVDictionary *custom_options;
};

typedef struct ff_demuxer_options ff_demuxer_options_t;

struct ff_demuxer {
	AVIOContext *io_context;
	AVFormatContext *format_context;

	struct ff_clock clock;

	struct ff_demuxer_options options;

	struct ff_decoder *audio_decoder;
	struct ff_callbacks audio_callbacks;

	struct ff_decoder *video_decoder;
	struct ff_callbacks video_callbacks;

	pthread_t demuxer_thread;

	int64_t seek_pos;
	bool seek_request;
	int seek_flags;
	bool seek_flush;

	bool abort;

	char *input;
	char *input_format;
};

typedef struct ff_demuxer ff_demuxer_t;

struct ff_demuxer *ff_demuxer_init();
bool ff_demuxer_open(struct ff_demuxer *demuxer, char *input,
                     char *input_format);
void ff_demuxer_free(struct ff_demuxer *demuxer);

void ff_demuxer_set_callbacks(struct ff_callbacks *callbacks,
                              ff_callback_frame frame,
                              ff_callback_format format,
                              ff_callback_initialize initialize,
                              ff_callback_frame frame_initialize,
                              ff_callback_frame frame_free, void *opaque);

void ff_demuxer_flush(struct ff_demuxer *demuxer);

#ifdef __cplusplus
}
#endif
