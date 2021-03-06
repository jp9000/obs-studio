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

#include <libavutil/time.h>
#include <libswscale/swscale.h>

#include <assert.h>

#include "ff-compat.h"

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

	// Check if we need to communicate a different format has been received
	// to any callbacks
	AVCodecContext *codec = decoder->codec;
	call_initialize = (queue_frame->frame == NULL ||
	                   queue_frame->frame->width != codec->width ||
	                   queue_frame->frame->height != codec->height ||
	                   queue_frame->frame->format != codec->pix_fmt);

	if (queue_frame->frame != NULL) {
		// This shouldn't happen any more, the frames are freed in
		// ff_decoder_refresh.
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

void *ff_video_decoder_thread(void *opaque_video_decoder)
{
	struct ff_decoder *decoder = (struct ff_decoder *)opaque_video_decoder;

	struct ff_packet packet = {0};
	int complete;
	AVFrame *frame = av_frame_alloc();
	int ret;
	bool key_frame;

	while (!decoder->abort) {
		if (decoder->eof)
			ret = packet_queue_get(&decoder->packet_queue, &packet,
			                       0);
		else
			ret = packet_queue_get(&decoder->packet_queue, &packet,
			                       1);

		if (ret == FF_PACKET_EMPTY || ret == FF_PACKET_FAIL) {
			// should we just use abort here?
			break;
		}

		if (packet.base.data ==
		    decoder->packet_queue.flush_packet.base.data) {
			avcodec_flush_buffers(decoder->codec);
			continue;
		}

		// We received a reset packet with a new clock
		if (packet.clock != NULL) {
			if (decoder->clock != NULL)
				ff_clock_release(&decoder->clock);
			decoder->clock = ff_clock_move(&packet.clock);
			av_free_packet(&packet.base);
			continue;
		}

		int64_t start_time = ff_clock_start_time(decoder->clock);
		key_frame = packet.base.flags & AV_PKT_FLAG_KEY;

		// We can only make decisions on keyframes for
		// hw decoders (maybe just OSX?)
		// For now, always make drop decisions on keyframes
		bool frame_drop_check = key_frame;
		// Must have a proper packet pts to drop frames here
		frame_drop_check &= start_time != AV_NOPTS_VALUE;

		if (frame_drop_check)
			ff_decoder_set_frame_drop_state(decoder, start_time,
			                                packet.base.pts);

		avcodec_decode_video2(decoder->codec, frame, &complete,
		                      &packet.base);

		// Did we get an entire video frame?  This doesn't guarantee
		// there is a picture to show for some codecs, but we still want
		// to adjust our various internal clocks for the next frame
		if (complete) {
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
