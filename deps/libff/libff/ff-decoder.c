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

#include "ff-decoder.h"

#include <libavutil/time.h>
#include <assert.h>

typedef void *(*ff_decoder_thread_t)(void *opaque_decoder);

extern void *ff_audio_decoder_thread(void *opaque_audio_decoder);
extern void *ff_video_decoder_thread(void *opaque_video_decoder);

struct ff_decoder *ff_decoder_init(AVCodecContext *codec_context,
                                   AVStream *stream,
                                   unsigned int packet_queue_size,
                                   unsigned int frame_queue_size)
{
	bool success;

	assert(codec_context != NULL);
	assert(stream != NULL);

	struct ff_decoder *decoder = av_mallocz(sizeof(struct ff_decoder));

	if (decoder == NULL)
		goto fail;

	decoder->codec = codec_context;
	decoder->codec->opaque = decoder;
	decoder->stream = stream;
	decoder->abort = false;
	decoder->finished = false;

	decoder->packet_queue_size = packet_queue_size;
	if (!packet_queue_init(&decoder->packet_queue))
		goto fail1;

	decoder->timer_next_wake = (double)av_gettime() / 1000000.0;
	decoder->previous_pts_diff = 40e-3;
	decoder->current_pts_time = av_gettime();
	decoder->start_pts = 0;
	decoder->predicted_pts = 0;
	decoder->first_frame = true;

	success = ff_timer_init(&decoder->refresh_timer, ff_decoder_refresh,
	                        decoder);
	if (!success)
		goto fail2;

	success = ff_circular_queue_init(&decoder->frame_queue,
	                                 sizeof(struct ff_frame),
	                                 frame_queue_size);
	if (!success)
		goto fail3;

	return decoder;

fail3:
	ff_timer_free(&decoder->refresh_timer);
fail2:
	packet_queue_free(&decoder->packet_queue);
fail1:
	av_free(decoder);
fail:
	return NULL;
}

bool ff_decoder_start(struct ff_decoder *decoder)
{
	assert(decoder != NULL);

	ff_decoder_thread_t decoder_thread;
	if (decoder->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
		decoder_thread = ff_audio_decoder_thread;
	} else if (decoder->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
		decoder_thread = ff_video_decoder_thread;
	} else {
		av_log(NULL, AV_LOG_ERROR, "no decoder found for type %d",
		       decoder->codec->codec_type);
		return false;
	}

	ff_decoder_schedule_refresh(decoder, 40);

	return (pthread_create(&decoder->decoder_thread, NULL, decoder_thread,
	                       decoder) != 0);
}

void ff_decoder_free(struct ff_decoder *decoder)
{
	void *decoder_thread_result;
	int i;

	assert(decoder != NULL);

	decoder->abort = true;

	ff_circular_queue_abort(&decoder->frame_queue);
	packet_queue_abort(&decoder->packet_queue);

	ff_timer_free(&decoder->refresh_timer);

	pthread_join(decoder->decoder_thread, &decoder_thread_result);

	for (i = 0; i < decoder->frame_queue.capacity; i++) {
		void *item = decoder->frame_queue.slots[i];
		struct ff_frame *frame = (struct ff_frame *)item;

		ff_callbacks_frame_free(frame, decoder->callbacks);

		if (frame != NULL) {
			if (frame->frame != NULL)
				av_frame_unref(frame->frame);
			if (frame->clock != NULL)
				ff_clock_release(&frame->clock);
			av_free(frame);
		}
	}

	packet_queue_free(&decoder->packet_queue);
	ff_circular_queue_free(&decoder->frame_queue);

	avcodec_close(decoder->codec);

	av_free(decoder);
}

void ff_decoder_schedule_refresh(struct ff_decoder *decoder, int delay)
{
	ff_timer_schedule(&decoder->refresh_timer, 1000 * delay);
}

double ff_decoder_clock(void *opaque)
{
	struct ff_decoder *decoder = opaque;
	double delta = (av_gettime() - decoder->current_pts_time) / 1000000.0;
	return decoder->current_pts + delta;
}

static double get_sync_adjusted_pts_diff(struct ff_clock *clock, double pts,
                                         double pts_diff)
{
	double new_pts_diff = pts_diff;
	double sync_time = ff_get_sync_clock(clock);
	double diff = pts - sync_time;
	double sync_threshold;

	sync_threshold = (pts_diff > AV_SYNC_THRESHOLD) ? pts_diff
	                                                : AV_SYNC_THRESHOLD;

	if (fabs(diff) < AV_NOSYNC_THRESHOLD) {
		if (diff <= -sync_threshold) {
			new_pts_diff = 0;

		} else if (diff >= sync_threshold) {
			new_pts_diff = 2 * pts_diff;
		}
	}

	return new_pts_diff;
}

void ff_decoder_refresh(void *opaque)
{
	struct ff_decoder *decoder = (struct ff_decoder *)opaque;

	struct ff_frame *frame;

	if (decoder->stream) {
		if (decoder->frame_queue.size == 0) {
			if (!decoder->eof || !decoder->finished) {
				// We expected a frame, but there were none
				// available

				// Schedule another call as soon as possible
				ff_decoder_schedule_refresh(decoder, 1);
			} else {
				ff_callbacks_frame(decoder->callbacks, NULL);
				decoder->refresh_timer.abort = true;
				// no more refreshes, we are at the eof
				av_log(NULL, AV_LOG_INFO,
				       "refresh timer stopping; eof");
				return;
			}
		} else {
			double pts_diff;
			double delay_until_next_wake;
			bool late_first_frame = false;

			frame = ff_circular_queue_peek_read(
			        &decoder->frame_queue);

			// Get frame clock and start it if needed
			ff_clock_t *clock = ff_clock_move(&frame->clock);
			if (!ff_clock_start(clock, decoder->natural_sync_clock,
			                    &decoder->refresh_timer.abort)) {
				ff_clock_release(&clock);

				// Our clock was never started and deleted or
				// aborted

				if (decoder->refresh_timer.abort) {
					av_log(NULL, AV_LOG_INFO,
					       "refresh timer aborted");
					return;
				}

				// Drop this frame? The only way this can happen
				// is if one stream finishes before another and
				// the input is looping or canceled.  Until we
				// get another clock we will unable to continue

				ff_decoder_schedule_refresh(decoder, 100);

				// Drop this frame as we have no way of timing
				// it
				ff_circular_queue_advance_read(
				        &decoder->frame_queue);
				return;
			}

			decoder->current_pts = frame->pts;
			decoder->current_pts_time = av_gettime();

			// the amount of time until we need to display this
			// frame
			pts_diff = frame->pts - decoder->previous_pts;

			// if the first frame is a very large value, we've most
			// likely started mid-stream, and the initial diff
			// should be ignored.
			if (decoder->first_frame) {
				late_first_frame = pts_diff >= 1.0;
				decoder->first_frame = false;
			}

			if (pts_diff <= 0 || late_first_frame) {
				// if diff is invalid, use previous
				pts_diff = decoder->previous_pts_diff;
			}

			// save for next time
			decoder->previous_pts_diff = pts_diff;
			decoder->previous_pts = frame->pts;

			// if not synced against natural clock
			if (clock->sync_type != decoder->natural_sync_clock) {
				pts_diff = get_sync_adjusted_pts_diff(
				        clock, frame->pts, pts_diff);
			}

			decoder->timer_next_wake += pts_diff;

			// compute the amount of time until next refresh
			delay_until_next_wake = decoder->timer_next_wake -
			                        (av_gettime() / 1000000.0L);
			if (delay_until_next_wake < 0.010L) {
				delay_until_next_wake = 0.010L;
			}

			if (delay_until_next_wake > pts_diff)
				delay_until_next_wake = pts_diff;

			ff_clock_release(&clock);
			ff_callbacks_frame(decoder->callbacks, frame);

			ff_decoder_schedule_refresh(
			        decoder,
			        (int)(delay_until_next_wake * 1000 + 0.5L));

			av_frame_free(&frame->frame);

			ff_circular_queue_advance_read(&decoder->frame_queue);
		}
	} else {
		ff_decoder_schedule_refresh(decoder, 100);
	}
}

bool ff_decoder_full(struct ff_decoder *decoder)
{
	if (decoder == NULL)
		return false;

	return (decoder->packet_queue.total_size > decoder->packet_queue_size);
}

bool ff_decoder_accept(struct ff_decoder *decoder, struct ff_packet *packet)
{
	if (decoder && packet->base.stream_index == decoder->stream->index) {
		packet_queue_put(&decoder->packet_queue, packet);
		return true;
	}

	return false;
}

double ff_decoder_get_best_effort_pts(struct ff_decoder *decoder,
                                      AVFrame *frame)
{
	// this is how long each frame is added to the amount of repeated frames
	// according to the codec
	double estimated_frame_delay;
	int64_t best_effort_pts;
	double d_pts;

	// This function is ffmpeg only, replace with frame->pkt_pts
	// if you are trying to compile for libav as a temporary
	// measure
	best_effort_pts = av_frame_get_best_effort_timestamp(frame);

	if (best_effort_pts != AV_NOPTS_VALUE) {
		// Fix the first pts if less than start_pts
		if (best_effort_pts < decoder->start_pts) {
			if (decoder->first_frame) {
				best_effort_pts = decoder->start_pts;
			} else {
				av_log(NULL, AV_LOG_WARNING,
				       "multiple pts < "
				       "start_pts; setting start pts "
				       "to 0");
				decoder->start_pts = 0;
			}
		}

		best_effort_pts -= decoder->start_pts;

		// Since the best effort pts came from the stream we use his
		// time base
		d_pts = best_effort_pts * av_q2d(decoder->stream->time_base);
		decoder->predicted_pts = d_pts;
	} else {
		d_pts = decoder->predicted_pts;
	}

	// Update our predicted pts to include the repeated picture count
	// Our predicted pts clock is based on the codecs time base
	estimated_frame_delay = av_frame_get_pkt_duration(frame) *
	                        av_q2d(decoder->codec->time_base);
	// Add repeat frame delay
	estimated_frame_delay +=
	        frame->repeat_pict / (1.0L / estimated_frame_delay);

	decoder->predicted_pts += estimated_frame_delay;

	return d_pts;
}

bool ff_decoder_set_frame_drop_state(struct ff_decoder *decoder,
                                     int64_t start_time, int64_t pts)
{
	if (pts != AV_NOPTS_VALUE) {
		int64_t rescaled_pts = av_rescale_q(
		        pts, decoder->stream->time_base, AV_TIME_BASE_Q);
		int64_t master_clock = av_gettime() - start_time;

		int64_t diff = master_clock - rescaled_pts;

		if (diff > (AV_TIME_BASE / 2)) {
			decoder->codec->skip_frame = decoder->frame_drop;
			decoder->codec->skip_idct = decoder->frame_drop;
			decoder->codec->skip_loop_filter = decoder->frame_drop;
			return true;
		} else {
			decoder->codec->skip_frame = AVDISCARD_DEFAULT;
			decoder->codec->skip_idct = AVDISCARD_DEFAULT;
			decoder->codec->skip_loop_filter = AVDISCARD_DEFAULT;
			return false;
		}
	}

	return false;
}
