/******************************************************************************
    Copyright (C) 2015 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <inttypes.h>
#include "obs-internal.h"
#include "util/util_uint64.h"

struct ts_info {
	uint64_t start;
	uint64_t end;
};

#define DEBUG_AUDIO 0
#define DEBUG_LAGGED_AUDIO 0
#define MAX_BUFFERING_TICKS 45

static void push_audio_tree(obs_source_t *parent, obs_source_t *source, void *p)
{
	struct obs_core_audio *audio = p;

	if (da_find(audio->render_order, &source, 0) == DARRAY_INVALID) {
		obs_source_t *s = obs_source_get_ref(source);
		if (s)
			da_push_back(audio->render_order, &s);
	}

	UNUSED_PARAMETER(parent);
}

static inline size_t convert_time_to_frames(size_t sample_rate, uint64_t t)
{
	return (size_t)util_mul_div64(t, sample_rate, 1000000000ULL);
}

static inline void mix_audio(struct audio_output_data *mixes,
			     obs_source_t *source, size_t channels,
			     size_t sample_rate, struct ts_info *ts)
{
	size_t total_floats = AUDIO_OUTPUT_FRAMES;
	size_t start_point = 0;

	if (source->audio_ts < ts->start || ts->end <= source->audio_ts)
		return;

	if (source->audio_ts != ts->start) {
		start_point = convert_time_to_frames(
			sample_rate, source->audio_ts - ts->start);
		if (start_point == AUDIO_OUTPUT_FRAMES)
			return;

		total_floats -= start_point;
	}

	for (size_t mix_idx = 0; mix_idx < MAX_AUDIO_MIXES; mix_idx++) {
		for (size_t ch = 0; ch < channels; ch++) {
			register float *mix = mixes[mix_idx].data[ch];
			register float *aud =
				source->audio_output_buf[mix_idx][ch];
			register float *end;

			mix += start_point;
			end = aud + total_floats;

			while (aud < end)
				*(mix++) += *(aud++);
		}
	}
}

static bool ignore_audio(obs_source_t *source, size_t channels,
			 size_t sample_rate, uint64_t start_ts)
{
	size_t num_floats = source->audio_input_buf[0].size / sizeof(float);
	const char *name = obs_source_get_name(source);

	if (!source->audio_ts && num_floats) {
#if DEBUG_LAGGED_AUDIO == 1
		blog(LOG_DEBUG, "[src: %s] no timestamp, but audio available?",
		     name);
#endif
		for (size_t ch = 0; ch < channels; ch++)
			circlebuf_pop_front(&source->audio_input_buf[ch], NULL,
					    source->audio_input_buf[0].size);
		source->last_audio_input_buf_size = 0;
		return false;
	}

	if (num_floats) {
		/* round up the number of samples to drop */
		size_t drop = util_mul_div64(start_ts - source->audio_ts - 1,
					     sample_rate, 1000000000ULL) +
			      1;
		if (drop > num_floats)
			drop = num_floats;

#if DEBUG_LAGGED_AUDIO == 1
		blog(LOG_DEBUG,
		     "[src: %s] ignored %" PRIu64 "/%" PRIu64 " samples", name,
		     (uint64_t)drop, (uint64_t)num_floats);
#endif
		for (size_t ch = 0; ch < channels; ch++)
			circlebuf_pop_front(&source->audio_input_buf[ch], NULL,
					    drop * sizeof(float));

		source->last_audio_input_buf_size = 0;
		source->audio_ts +=
			util_mul_div64(drop, 1000000000ULL, sample_rate);
		blog(LOG_DEBUG, "[src: %s] ts lag after ignoring: %" PRIu64,
		     name, start_ts - source->audio_ts);

		/* rounding error, adjust */
		if (source->audio_ts == (start_ts - 1))
			source->audio_ts = start_ts;

		/* source is back in sync */
		if (source->audio_ts >= start_ts)
			return true;
	} else {
#if DEBUG_LAGGED_AUDIO == 1
		blog(LOG_DEBUG, "[src: %s] no samples to ignore! ts = %" PRIu64,
		     name, source->audio_ts);
#endif
	}

	if (!source->audio_pending || num_floats) {
		blog(LOG_WARNING,
		     "Source %s audio is lagging (over by %.02f ms) "
		     "at max audio buffering. Restarting source audio.",
		     name, (start_ts - source->audio_ts) / 1000000.);
	}

	source->audio_pending = true;
	source->audio_ts = 0;
	/* tell the timestamp adjustment code in source_output_audio_data to
	 * reset everything, and hopefully fix the timestamps */
	source->timing_set = false;
	return false;
}

static bool discard_if_stopped(obs_source_t *source, size_t channels)
{
	size_t last_size;
	size_t size;

	last_size = source->last_audio_input_buf_size;
	size = source->audio_input_buf[0].size;

	if (!size)
		return false;

	/* if perpetually pending data, it means the audio has stopped,
	 * so clear the audio data */
	if (last_size == size) {
		if (!source->pending_stop) {
			source->pending_stop = true;
#if DEBUG_AUDIO == 1
			blog(LOG_DEBUG, "doing pending stop trick: '%s'",
			     source->context.name);
#endif
			return false;
		}

		for (size_t ch = 0; ch < channels; ch++)
			circlebuf_pop_front(&source->audio_input_buf[ch], NULL,
					    source->audio_input_buf[ch].size);

		source->pending_stop = false;
		source->audio_ts = 0;
		source->last_audio_input_buf_size = 0;
#if DEBUG_AUDIO == 1
		blog(LOG_DEBUG, "source audio data appears to have "
				"stopped, clearing");
#endif
		return true;
	} else {
		source->last_audio_input_buf_size = size;
		return false;
	}
}

#define MAX_AUDIO_SIZE (AUDIO_OUTPUT_FRAMES * sizeof(float))

static inline void discard_audio(struct obs_core_audio *audio,
				 obs_source_t *source, size_t channels,
				 size_t sample_rate, struct ts_info *ts)
{
	size_t total_floats = AUDIO_OUTPUT_FRAMES;
	size_t size;
	/* debug assert only */
	UNUSED_PARAMETER(audio);

#if DEBUG_AUDIO == 1
	bool is_audio_source = source->info.output_flags & OBS_SOURCE_AUDIO;
#endif

	if (source->info.audio_render) {
		source->audio_ts = 0;
		return;
	}

	if (ts->end <= source->audio_ts) {
#if DEBUG_AUDIO == 1
		blog(LOG_DEBUG,
		     "can't discard, source "
		     "timestamp (%" PRIu64 ") >= "
		     "end timestamp (%" PRIu64 ")",
		     source->audio_ts, ts->end);
#endif
		return;
	}

	if (source->audio_ts < (ts->start - 1)) {
		if (source->audio_pending &&
		    source->audio_input_buf[0].size < MAX_AUDIO_SIZE &&
		    discard_if_stopped(source, channels))
			return;

#if DEBUG_AUDIO == 1
		if (is_audio_source) {
			blog(LOG_DEBUG,
			     "can't discard, source "
			     "timestamp (%" PRIu64 ") < "
			     "start timestamp (%" PRIu64 ")",
			     source->audio_ts, ts->start);
		}

		/* ignore_audio should have already run and marked this source
		 * pending, unless we *just* added buffering */
		assert(audio->total_buffering_ticks < MAX_BUFFERING_TICKS ||
		       source->audio_pending || !source->audio_ts ||
		       audio->buffering_wait_ticks);
#endif
		return;
	}

	if (source->audio_ts != ts->start &&
	    source->audio_ts != (ts->start - 1)) {
		size_t start_point = convert_time_to_frames(
			sample_rate, source->audio_ts - ts->start);
		if (start_point == AUDIO_OUTPUT_FRAMES) {
#if DEBUG_AUDIO == 1
			if (is_audio_source)
				blog(LOG_DEBUG, "can't discard, start point is "
						"at audio frame count");
#endif
			return;
		}

		total_floats -= start_point;
	}

	size = total_floats * sizeof(float);

	if (source->audio_input_buf[0].size < size) {
		if (discard_if_stopped(source, channels))
			return;

#if DEBUG_AUDIO == 1
		if (is_audio_source)
			blog(LOG_DEBUG, "can't discard, data still pending");
#endif
		source->audio_ts = ts->end;
		return;
	}

	for (size_t ch = 0; ch < channels; ch++)
		circlebuf_pop_front(&source->audio_input_buf[ch], NULL, size);

	source->last_audio_input_buf_size = 0;

#if DEBUG_AUDIO == 1
	if (is_audio_source)
		blog(LOG_DEBUG, "audio discarded, new ts: %" PRIu64, ts->end);
#endif

	source->pending_stop = false;
	source->audio_ts = ts->end;
}

static void add_audio_buffering(struct obs_core_audio *audio,
				size_t sample_rate, struct ts_info *ts,
				uint64_t min_ts, const char *buffering_name)
{
	struct ts_info new_ts;
	uint64_t offset;
	uint64_t frames;
	size_t total_ms;
	size_t ms;
	int ticks;

	if (audio->total_buffering_ticks == MAX_BUFFERING_TICKS)
		return;

	if (!audio->buffering_wait_ticks)
		audio->buffered_ts = ts->start;

	offset = ts->start - min_ts;
	frames = ns_to_audio_frames(sample_rate, offset);
	ticks = (int)((frames + AUDIO_OUTPUT_FRAMES - 1) / AUDIO_OUTPUT_FRAMES);

	audio->total_buffering_ticks += ticks;

	if (audio->total_buffering_ticks >= MAX_BUFFERING_TICKS) {
		ticks -= audio->total_buffering_ticks - MAX_BUFFERING_TICKS;
		audio->total_buffering_ticks = MAX_BUFFERING_TICKS;
		blog(LOG_WARNING, "Max audio buffering reached!");
	}

	ms = ticks * AUDIO_OUTPUT_FRAMES * 1000 / sample_rate;
	total_ms = audio->total_buffering_ticks * AUDIO_OUTPUT_FRAMES * 1000 /
		   sample_rate;

	blog(LOG_INFO,
	     "adding %d milliseconds of audio buffering, total "
	     "audio buffering is now %d milliseconds"
	     " (source: %s)\n",
	     (int)ms, (int)total_ms, buffering_name);
#if DEBUG_AUDIO == 1
	blog(LOG_DEBUG,
	     "min_ts (%" PRIu64 ") < start timestamp "
	     "(%" PRIu64 ")",
	     min_ts, ts->start);
	blog(LOG_DEBUG, "old buffered ts: %" PRIu64 "-%" PRIu64, ts->start,
	     ts->end);
#endif

	new_ts.start =
		audio->buffered_ts -
		audio_frames_to_ns(sample_rate, audio->buffering_wait_ticks *
							AUDIO_OUTPUT_FRAMES);

	while (ticks--) {
		int cur_ticks = ++audio->buffering_wait_ticks;

		new_ts.end = new_ts.start;
		new_ts.start =
			audio->buffered_ts -
			audio_frames_to_ns(sample_rate,
					   cur_ticks * AUDIO_OUTPUT_FRAMES);

#if DEBUG_AUDIO == 1
		blog(LOG_DEBUG, "add buffered ts: %" PRIu64 "-%" PRIu64,
		     new_ts.start, new_ts.end);
#endif

		circlebuf_push_front(&audio->buffered_timestamps, &new_ts,
				     sizeof(new_ts));
	}

	*ts = new_ts;
}

static bool audio_buffer_insuffient(struct obs_source *source,
				    size_t sample_rate, uint64_t min_ts)
{
	size_t total_floats = AUDIO_OUTPUT_FRAMES;
	size_t size;

	if (source->info.audio_render || source->audio_pending ||
	    !source->audio_ts) {
		return false;
	}

	if (source->audio_ts != min_ts && source->audio_ts != (min_ts - 1)) {
		size_t start_point = convert_time_to_frames(
			sample_rate, source->audio_ts - min_ts);
		if (start_point >= AUDIO_OUTPUT_FRAMES)
			return false;

		total_floats -= start_point;
	}

	size = total_floats * sizeof(float);

	if (source->audio_input_buf[0].size < size) {
		source->audio_pending = true;
		return true;
	}

	return false;
}

static inline const char *find_min_ts(struct obs_core_data *data,
				      uint64_t *min_ts)
{
	obs_source_t *buffering_source = NULL;
	struct obs_source *source = data->first_audio_source;
	while (source) {
		if (!source->audio_pending && source->audio_ts &&
		    source->audio_ts < *min_ts) {
			*min_ts = source->audio_ts;
			buffering_source = source;
		}

		source = (struct obs_source *)source->next_audio_source;
	}
	return buffering_source ? obs_source_get_name(buffering_source) : NULL;
}

static inline bool mark_invalid_sources(struct obs_core_data *data,
					size_t sample_rate, uint64_t min_ts)
{
	bool recalculate = false;

	struct obs_source *source = data->first_audio_source;
	while (source) {
		recalculate |=
			audio_buffer_insuffient(source, sample_rate, min_ts);
		source = (struct obs_source *)source->next_audio_source;
	}

	return recalculate;
}

static inline const char *calc_min_ts(struct obs_core_data *data,
				      size_t sample_rate, uint64_t *min_ts)
{
	const char *buffering_name = find_min_ts(data, min_ts);
	if (mark_invalid_sources(data, sample_rate, *min_ts))
		buffering_name = find_min_ts(data, min_ts);
	return buffering_name;
}

static inline void release_audio_sources(struct obs_core_audio *audio)
{
	for (size_t i = 0; i < audio->render_order.num; i++)
		obs_source_release(audio->render_order.array[i]);
}

bool audio_callback(void *param, uint64_t start_ts_in, uint64_t end_ts_in,
		    uint64_t *out_ts, uint32_t mixers,
		    struct audio_output_data *mixes)
{
	struct obs_core_data *data = &obs->data;
	struct obs_core_audio *audio = &obs->audio;
	struct obs_source *source;
	size_t sample_rate = audio_output_get_sample_rate(audio->audio);
	size_t channels = audio_output_get_channels(audio->audio);
	struct ts_info ts = {start_ts_in, end_ts_in};
	size_t audio_size;
	uint64_t min_ts;

	da_resize(audio->render_order, 0);
	da_resize(audio->root_nodes, 0);

	circlebuf_push_back(&audio->buffered_timestamps, &ts, sizeof(ts));
	circlebuf_peek_front(&audio->buffered_timestamps, &ts, sizeof(ts));
	min_ts = ts.start;

	audio_size = AUDIO_OUTPUT_FRAMES * sizeof(float);

#if DEBUG_AUDIO == 1
	blog(LOG_DEBUG, "ts %llu-%llu", ts.start, ts.end);
#endif

	/* ------------------------------------------------ */
	/* build audio render order
	 * NOTE: these are source channels, not audio channels */
	for (uint32_t i = 0; i < MAX_CHANNELS; i++) {
		obs_source_t *source = obs_get_output_source(i);
		if (source) {
			obs_source_enum_active_tree(source, push_audio_tree,
						    audio);
			push_audio_tree(NULL, source, audio);
			da_push_back(audio->root_nodes, &source);
			obs_source_release(source);
		}
	}

	pthread_mutex_lock(&data->audio_sources_mutex);

	source = data->first_audio_source;
	while (source) {
		push_audio_tree(NULL, source, audio);
		source = (struct obs_source *)source->next_audio_source;
	}

	pthread_mutex_unlock(&data->audio_sources_mutex);

	/* ------------------------------------------------ */
	/* render audio data */
	for (size_t i = 0; i < audio->render_order.num; i++) {
		obs_source_t *source = audio->render_order.array[i];
		obs_source_audio_render(source, mixers, channels, sample_rate,
					audio_size);

		/* if a source has gone backward in time and we can no
		 * longer buffer, drop some or all of its audio */
		if (audio->total_buffering_ticks == MAX_BUFFERING_TICKS &&
		    source->audio_ts < ts.start) {
			if (source->info.audio_render) {
				blog(LOG_DEBUG,
				     "render audio source %s timestamp has "
				     "gone backwards",
				     obs_source_get_name(source));

				/* just avoid further damage */
				source->audio_pending = true;
#if DEBUG_AUDIO == 1
				/* this should really be fixed */
				assert(false);
#endif
			} else {
				pthread_mutex_lock(&source->audio_buf_mutex);
				bool rerender = ignore_audio(source, channels,
							     sample_rate,
							     ts.start);
				pthread_mutex_unlock(&source->audio_buf_mutex);

				/* if we (potentially) recovered, re-render */
				if (rerender)
					obs_source_audio_render(source, mixers,
								channels,
								sample_rate,
								audio_size);
			}
		}
	}

	/* ------------------------------------------------ */
	/* get minimum audio timestamp */
	pthread_mutex_lock(&data->audio_sources_mutex);
	const char *buffering_name = calc_min_ts(data, sample_rate, &min_ts);
	pthread_mutex_unlock(&data->audio_sources_mutex);

	/* ------------------------------------------------ */
	/* if a source has gone backward in time, buffer */
	if (min_ts < ts.start)
		add_audio_buffering(audio, sample_rate, &ts, min_ts,
				    buffering_name);

	/* ------------------------------------------------ */
	/* mix audio */
	if (!audio->buffering_wait_ticks) {
		for (size_t i = 0; i < audio->root_nodes.num; i++) {
			obs_source_t *source = audio->root_nodes.array[i];

			if (source->audio_pending)
				continue;

			pthread_mutex_lock(&source->audio_buf_mutex);

			if (source->audio_output_buf[0][0] && source->audio_ts)
				mix_audio(mixes, source, channels, sample_rate,
					  &ts);

			pthread_mutex_unlock(&source->audio_buf_mutex);
		}
	}

	/* ------------------------------------------------ */
	/* discard audio */
	pthread_mutex_lock(&data->audio_sources_mutex);

	source = data->first_audio_source;
	while (source) {
		pthread_mutex_lock(&source->audio_buf_mutex);
		discard_audio(audio, source, channels, sample_rate, &ts);
		pthread_mutex_unlock(&source->audio_buf_mutex);

		source = (struct obs_source *)source->next_audio_source;
	}

	pthread_mutex_unlock(&data->audio_sources_mutex);

	/* ------------------------------------------------ */
	/* release audio sources */
	release_audio_sources(audio);

	circlebuf_pop_front(&audio->buffered_timestamps, NULL, sizeof(ts));

	*out_ts = ts.start;

	if (audio->buffering_wait_ticks) {
		audio->buffering_wait_ticks--;
		return false;
	}

	UNUSED_PARAMETER(param);
	return true;
}
