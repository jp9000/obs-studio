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

#include "ff-callbacks.h"
#include "ff-circular-queue.h"
#include "ff-clock.h"
#include "ff-decoder.h"
#include "ff-frame.h"
#include "ff-packet-queue.h"
#include "ff-timer.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>

#include <assert.h>

#include "ff-compat.h"

static inline void shrink_packet(struct ff_packet *packet, int packet_length)
{
	if (packet_length <= packet->base.size) {
		int remaining = packet->base.size - packet_length;

		memmove(packet->base.data, &packet->base.data[packet_length],
		        remaining);
		av_shrink_packet(&packet->base, remaining);
	}
}

static bool handle_reset_packet(struct ff_decoder *decoder,
                                struct ff_packet *packet)
{
	if (decoder->clock != NULL)
		ff_clock_release(&decoder->clock);
	decoder->clock = packet->clock;
	av_free_packet(&packet->base);

	return true;
}

static void drop_late_packets(struct ff_decoder *decoder,
                              struct ff_packet *packet)
{
	int64_t start_time = ff_clock_start_time(decoder->clock);
	if (start_time != AV_NOPTS_VALUE) {
		if (ff_decoder_set_frame_drop_state(decoder, start_time,
		                                    packet->base.pts))
			shrink_packet(packet, packet->base.size);
	}
}

static int decode_frame(struct ff_decoder *decoder, struct ff_packet *packet,
                        AVFrame *frame, bool *frame_complete)
{
	int packet_length;
	int ret;

	while (true) {
		if (decoder->eof)
			ret = packet_queue_get(&decoder->packet_queue, packet,
			                       0);
		else
			ret = packet_queue_get(&decoder->packet_queue, packet,
			                       1);

		if (ret == FF_PACKET_EMPTY) {
			return 0;
		} else if (ret == FF_PACKET_FAIL) {
			return -1;
		}

		if (packet->base.data ==
		    decoder->packet_queue.flush_packet.base.data) {
			avcodec_flush_buffers(decoder->codec);
			continue;
		}

		// Packet has a new clock (reset packet)
		if (packet->clock != NULL)
			if (!handle_reset_packet(decoder, packet))
				return -1;

		while (packet->base.size > 0) {
			int complete;

			drop_late_packets(decoder, packet);

			packet_length = avcodec_decode_audio4(decoder->codec,
			                                      frame, &complete,
			                                      &packet->base);

			if (packet_length < 0)
				break;

			shrink_packet(packet, packet_length);

			if (complete == 0 || frame->nb_samples <= 0)
				continue;

			*frame_complete = complete != 0;

			return frame->nb_samples *
			       av_get_bytes_per_sample(frame->format);
		}

		if (packet->base.data != NULL)
			av_packet_unref(&packet->base);
	}
}

static bool queue_frame(struct ff_decoder *decoder, AVFrame *frame,
                        double best_effort_pts)
{
	struct ff_frame *queue_frame;
	bool call_initialize;

	ff_circular_queue_wait_write(&decoder->frame_queue);

	if (decoder->abort) {
		return false;
	}

	queue_frame = ff_circular_queue_peek_write(&decoder->frame_queue);

	AVCodecContext *codec = decoder->codec;
	call_initialize =
	        (queue_frame->frame == NULL ||
	         queue_frame->frame->channels != codec->channels ||
	         queue_frame->frame->sample_rate != codec->sample_rate ||
	         queue_frame->frame->format != codec->sample_fmt);

	if (queue_frame->frame != NULL) {
		//FIXME: this shouldn't happen any more!
		av_frame_free(&queue_frame->frame);
	}

	queue_frame->frame = av_frame_clone(frame);
	queue_frame->clock = ff_clock_retain(decoder->clock);

	if (call_initialize)
		ff_callbacks_frame_initialize(queue_frame, decoder->callbacks);

	queue_frame->pts = best_effort_pts;

	ff_circular_queue_advance_write(&decoder->frame_queue, queue_frame);

	return true;
}

void *ff_audio_decoder_thread(void *opaque_audio_decoder)
{
	struct ff_decoder *decoder = opaque_audio_decoder;

	struct ff_packet packet = {0};
	bool frame_complete;
	AVFrame *frame = av_frame_alloc();
	int ret;

	while (!decoder->abort) {
		ret = decode_frame(decoder, &packet, frame, &frame_complete);
		if (ret == 0) {
			break;
		} else if (ret < 0) {
			av_free_packet(&packet.base);
			continue;
		}

		// Did we get a audio frame?
		if (frame_complete) {
			// If we don't have a good PTS, try to guess based
			// on last received PTS provided plus prediction
			// This function returns a pts scaled to stream
			// time base
			double best_effort_pts =
			        ff_decoder_get_best_effort_pts(decoder, frame);
			queue_frame(decoder, frame, best_effort_pts);
			av_frame_unref(frame);
		}

		av_free_packet(&packet.base);
	}

	if (decoder->clock != NULL)
		ff_clock_release(&decoder->clock);

	av_frame_free(&frame);

	decoder->finished = true;

	return NULL;
}
