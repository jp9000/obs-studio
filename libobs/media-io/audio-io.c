/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>

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

#include <math.h>
#include <inttypes.h>

#include "../util/threading.h"
#include "../util/darray.h"
#include "../util/circlebuf.h"
#include "../util/platform.h"
#include "../util/profiler.h"
#include "../util/util_uint64.h"

#include "audio-io.h"
#include "audio-resampler.h"

extern profiler_name_store_t *obs_get_profiler_name_store(void);

/* #define DEBUG_AUDIO */

#define nop()                    \
	do {                     \
		int invalid = 0; \
	} while (0)

struct audio_input {
	struct audio_convert_info conversion;
	audio_resampler_t *resampler;

	audio_output_callback_t callback;
	void *param;
};

static inline void audio_input_free(struct audio_input *input)
{
	audio_resampler_destroy(input->resampler);
}

struct audio_mix {
	DARRAY(struct audio_input) inputs;
	float buffer[MAX_AUDIO_CHANNELS][AUDIO_OUTPUT_FRAMES];
};

struct audio_output {
	struct audio_output_info info;
	size_t block_size;
	size_t channels;
	size_t planes;

	pthread_t thread;
	os_event_t *stop_event;

	bool initialized;

	audio_input_callback_t input_cb;
	void *input_param;
	pthread_mutex_t input_mutex;
	struct audio_mix mixes[MAX_AUDIO_MIXES];
};

/* ------------------------------------------------------------------------- */

static bool resample_audio_output(struct audio_input *input,
				  struct audio_data *data)
{
	bool success = true;

	if (input->resampler) {
		uint8_t *output[MAX_AV_PLANES];
		uint32_t frames;
		uint64_t offset;

		memset(output, 0, sizeof(output));

		success = audio_resampler_resample(
			input->resampler, output, &frames, &offset,
			(const uint8_t *const *)data->data, data->frames);

		for (size_t i = 0; i < MAX_AV_PLANES; i++)
			data->data[i] = output[i];
		data->frames = frames;
		data->timestamp -= offset;
	}

	return success;
}

static inline void do_audio_output(struct audio_output *audio, size_t mix_idx,
				   uint64_t timestamp, uint32_t frames)
{
	struct audio_mix *mix = &audio->mixes[mix_idx];
	struct audio_data data;

	pthread_mutex_lock(&audio->input_mutex);

	for (size_t i = mix->inputs.num; i > 0; i--) {
		struct audio_input *input = mix->inputs.array + (i - 1);

		for (size_t i = 0; i < audio->planes; i++)
			data.data[i] = (uint8_t *)mix->buffer[i];
		data.frames = frames;
		data.timestamp = timestamp;

		if (resample_audio_output(input, &data))
			input->callback(input->param, mix_idx, &data);
	}

	pthread_mutex_unlock(&audio->input_mutex);
}

static inline void clamp_audio_output(struct audio_output *audio, size_t bytes)
{
	size_t float_size = bytes / sizeof(float);

	for (size_t mix_idx = 0; mix_idx < MAX_AUDIO_MIXES; mix_idx++) {
		struct audio_mix *mix = &audio->mixes[mix_idx];

		/* do not process mixing if a specific mix is inactive */
		if (!mix->inputs.num)
			continue;

		for (size_t plane = 0; plane < audio->planes; plane++) {
			float *mix_data = mix->buffer[plane];
			float *mix_end = &mix_data[float_size];

			while (mix_data < mix_end) {
				float val = *mix_data;
				val = (val > 1.0f) ? 1.0f : val;
				val = (val < -1.0f) ? -1.0f : val;
				*(mix_data++) = val;
			}
		}
	}
}

static void input_and_output(struct audio_output *audio, uint64_t audio_time,
			     uint64_t prev_time)
{
	size_t bytes = AUDIO_OUTPUT_FRAMES * audio->block_size;
	struct audio_output_data data[MAX_AUDIO_MIXES];
	uint32_t active_mixes = 0;
	uint64_t new_ts = 0;
	bool success;

	memset(data, 0, sizeof(data));

#ifdef DEBUG_AUDIO
	blog(LOG_DEBUG, "audio_time: %llu, prev_time: %llu, bytes: %lu",
	     audio_time, prev_time, bytes);
#endif

	/* get mixers */
	pthread_mutex_lock(&audio->input_mutex);
	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
		if (audio->mixes[i].inputs.num)
			active_mixes |= (1 << i);
	}
	pthread_mutex_unlock(&audio->input_mutex);

	/* clear mix buffers */
	for (size_t mix_idx = 0; mix_idx < MAX_AUDIO_MIXES; mix_idx++) {
		struct audio_mix *mix = &audio->mixes[mix_idx];

		memset(mix->buffer, 0, sizeof(mix->buffer));

		for (size_t i = 0; i < audio->planes; i++)
			data[mix_idx].data[i] = mix->buffer[i];
	}

	/* get new audio data */
	success = audio->input_cb(audio->input_param, prev_time, audio_time,
				  &new_ts, active_mixes, data);
	if (!success)
		return;

	/* clamps audio data to -1.0..1.0 */
	clamp_audio_output(audio, bytes);

	/* output */
	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++)
		do_audio_output(audio, i, new_ts, AUDIO_OUTPUT_FRAMES);
}

static void *audio_thread(void *param)
{
	struct audio_output *audio = param;
	size_t rate = audio->info.samples_per_sec;
	uint64_t samples = 0;
	uint64_t start_time = os_gettime_ns();
	uint64_t prev_time = start_time;
	uint64_t audio_time = prev_time;
	uint32_t audio_wait_time = (uint32_t)(
		audio_frames_to_ns(rate, AUDIO_OUTPUT_FRAMES) / 1000000);

	os_set_thread_name("audio-io: audio thread");

	const char *audio_thread_name =
		profile_store_name(obs_get_profiler_name_store(),
				   "audio_thread(%s)", audio->info.name);

	while (os_event_try(audio->stop_event) == EAGAIN) {
		uint64_t cur_time;

		os_sleep_ms(audio_wait_time);

		profile_start(audio_thread_name);

		cur_time = os_gettime_ns();
		while (audio_time <= cur_time) {
			samples += AUDIO_OUTPUT_FRAMES;
			audio_time =
				start_time + audio_frames_to_ns(rate, samples);

			input_and_output(audio, audio_time, prev_time);
			prev_time = audio_time;
		}

		profile_end(audio_thread_name);

		profile_reenable_thread();
	}

	return NULL;
}

/* ------------------------------------------------------------------------- */

static size_t audio_get_input_idx(const audio_t *audio, size_t mix_idx,
				  audio_output_callback_t callback, void *param)
{
	const struct audio_mix *mix = &audio->mixes[mix_idx];

	for (size_t i = 0; i < mix->inputs.num; i++) {
		struct audio_input *input = mix->inputs.array + i;

		if (input->callback == callback && input->param == param)
			return i;
	}

	return DARRAY_INVALID;
}

static inline bool audio_input_init(struct audio_input *input,
				    struct audio_output *audio)
{
	if (input->conversion.format != audio->info.format ||
	    input->conversion.samples_per_sec != audio->info.samples_per_sec ||
	    input->conversion.speakers != audio->info.speakers) {
		struct resample_info from = {
			.format = audio->info.format,
			.samples_per_sec = audio->info.samples_per_sec,
			.speakers = audio->info.speakers};

		struct resample_info to = {
			.format = input->conversion.format,
			.samples_per_sec = input->conversion.samples_per_sec,
			.speakers = input->conversion.speakers};

		input->resampler = audio_resampler_create(&to, &from);
		if (!input->resampler) {
			blog(LOG_ERROR, "audio_input_init: Failed to "
					"create resampler");
			return false;
		}
	} else {
		input->resampler = NULL;
	}

	return true;
}

bool audio_output_connect(audio_t *audio, size_t mi,
			  const struct audio_convert_info *conversion,
			  audio_output_callback_t callback, void *param)
{
	bool success = false;

	if (!audio || mi >= MAX_AUDIO_MIXES)
		return false;

	pthread_mutex_lock(&audio->input_mutex);

	if (audio_get_input_idx(audio, mi, callback, param) == DARRAY_INVALID) {
		struct audio_mix *mix = &audio->mixes[mi];
		struct audio_input input;
		input.callback = callback;
		input.param = param;

		if (conversion) {
			input.conversion = *conversion;
		} else {
			input.conversion.format = audio->info.format;
			input.conversion.speakers = audio->info.speakers;
			input.conversion.samples_per_sec =
				audio->info.samples_per_sec;
		}

		if (input.conversion.format == AUDIO_FORMAT_UNKNOWN)
			input.conversion.format = audio->info.format;
		if (input.conversion.speakers == SPEAKERS_UNKNOWN)
			input.conversion.speakers = audio->info.speakers;
		if (input.conversion.samples_per_sec == 0)
			input.conversion.samples_per_sec =
				audio->info.samples_per_sec;

		success = audio_input_init(&input, audio);
		if (success)
			da_push_back(mix->inputs, &input);
	}

	pthread_mutex_unlock(&audio->input_mutex);

	return success;
}

void audio_output_disconnect(audio_t *audio, size_t mix_idx,
			     audio_output_callback_t callback, void *param)
{
	if (!audio || mix_idx >= MAX_AUDIO_MIXES)
		return;

	pthread_mutex_lock(&audio->input_mutex);

	size_t idx = audio_get_input_idx(audio, mix_idx, callback, param);
	if (idx != DARRAY_INVALID) {
		struct audio_mix *mix = &audio->mixes[mix_idx];
		audio_input_free(mix->inputs.array + idx);
		da_erase(mix->inputs, idx);
	}

	pthread_mutex_unlock(&audio->input_mutex);
}

static inline bool valid_audio_params(const struct audio_output_info *info)
{
	return info->format && info->name && info->samples_per_sec > 0 &&
	       info->speakers > 0;
}

int audio_output_open(audio_t **audio, struct audio_output_info *info)
{
	struct audio_output *out;
	pthread_mutexattr_t attr;
	bool planar = is_audio_planar(info->format);

	if (!valid_audio_params(info))
		return AUDIO_OUTPUT_INVALIDPARAM;

	out = bzalloc(sizeof(struct audio_output));
	if (!out)
		goto fail;

	memcpy(&out->info, info, sizeof(struct audio_output_info));
	out->channels = get_audio_channels(info->speakers);
	out->planes = planar ? out->channels : 1;
	out->input_cb = info->input_callback;
	out->input_param = info->input_param;
	out->block_size = (planar ? 1 : out->channels) *
			  get_audio_bytes_per_channel(info->format);

	if (pthread_mutexattr_init(&attr) != 0)
		goto fail;
	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
		goto fail;
	if (pthread_mutex_init(&out->input_mutex, &attr) != 0)
		goto fail;
	if (os_event_init(&out->stop_event, OS_EVENT_TYPE_MANUAL) != 0)
		goto fail;
	if (pthread_create(&out->thread, NULL, audio_thread, out) != 0)
		goto fail;

	out->initialized = true;
	*audio = out;
	return AUDIO_OUTPUT_SUCCESS;

fail:
	audio_output_close(out);
	return AUDIO_OUTPUT_FAIL;
}

void audio_output_close(audio_t *audio)
{
	void *thread_ret;

	if (!audio)
		return;

	if (audio->initialized) {
		os_event_signal(audio->stop_event);
		pthread_join(audio->thread, &thread_ret);
	}

	for (size_t mix_idx = 0; mix_idx < MAX_AUDIO_MIXES; mix_idx++) {
		struct audio_mix *mix = &audio->mixes[mix_idx];

		for (size_t i = 0; i < mix->inputs.num; i++)
			audio_input_free(mix->inputs.array + i);

		da_free(mix->inputs);
	}

	os_event_destroy(audio->stop_event);
	bfree(audio);
}

const struct audio_output_info *audio_output_get_info(const audio_t *audio)
{
	return audio ? &audio->info : NULL;
}

bool audio_output_active(const audio_t *audio)
{
	if (!audio)
		return false;

	for (size_t mix_idx = 0; mix_idx < MAX_AUDIO_MIXES; mix_idx++) {
		const struct audio_mix *mix = &audio->mixes[mix_idx];

		if (mix->inputs.num != 0)
			return true;
	}

	return false;
}

size_t audio_output_get_block_size(const audio_t *audio)
{
	return audio ? audio->block_size : 0;
}

size_t audio_output_get_planes(const audio_t *audio)
{
	return audio ? audio->planes : 0;
}

size_t audio_output_get_channels(const audio_t *audio)
{
	return audio ? audio->channels : 0;
}

uint32_t audio_output_get_sample_rate(const audio_t *audio)
{
	return audio ? audio->info.samples_per_sec : 0;
}
