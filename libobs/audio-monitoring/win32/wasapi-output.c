#include "../../media-io/audio-resampler.h"
#include "../../util/circlebuf.h"
#include "../../util/platform.h"
#include "../../util/darray.h"
#include "../../util/util_uint64.h"
#include "../../obs-internal.h"

#include "wasapi-output.h"

#define ACTUALLY_DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
	EXTERN_C const GUID DECLSPEC_SELECTANY name = {                       \
		l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}

#define do_log(level, format, ...)                      \
	blog(level, "[audio monitoring: '%s'] " format, \
	     obs_source_get_name(monitor->source), ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)

ACTUALLY_DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xBCDE0395, 0xE52F, 0x467C, 0x8E,
		     0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
ACTUALLY_DEFINE_GUID(IID_IMMDeviceEnumerator, 0xA95664D2, 0x9614, 0x4F35, 0xA7,
		     0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
ACTUALLY_DEFINE_GUID(IID_IAudioClient, 0x1CB9AD4C, 0xDBFA, 0x4C32, 0xB1, 0x78,
		     0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2);
ACTUALLY_DEFINE_GUID(IID_IAudioRenderClient, 0xF294ACFC, 0x3146, 0x4483, 0xA7,
		     0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2);

struct audio_monitor {
	obs_source_t *source;
	IMMDevice *device;
	IAudioClient *client;
	IAudioRenderClient *render;

	uint64_t last_recv_time;
	uint64_t prev_video_ts;
	uint64_t time_since_prev;
	audio_resampler_t *resampler;
	uint32_t sample_rate;
	uint32_t channels;
	bool source_has_video;
	bool ignore;

	int64_t lowest_audio_offset;
	struct circlebuf delay_buffer;
	uint32_t delay_size;

	DARRAY(float) buf;
	pthread_mutex_t playback_mutex;
};

/* #define DEBUG_AUDIO */

static bool process_audio_delay(struct audio_monitor *monitor, float **data,
				uint32_t *frames, uint64_t ts, uint32_t pad)
{
	obs_source_t *s = monitor->source;
	uint64_t last_frame_ts = s->last_frame_ts;
	uint64_t cur_time = os_gettime_ns();
	uint64_t front_ts;
	uint64_t cur_ts;
	int64_t diff;
	uint32_t blocksize = monitor->channels * sizeof(float);

	/* cut off audio if long-since leftover audio in delay buffer */
	if (cur_time - monitor->last_recv_time > 1000000000)
		circlebuf_free(&monitor->delay_buffer);
	monitor->last_recv_time = cur_time;

	ts += monitor->source->sync_offset;

	circlebuf_push_back(&monitor->delay_buffer, &ts, sizeof(ts));
	circlebuf_push_back(&monitor->delay_buffer, frames, sizeof(*frames));
	circlebuf_push_back(&monitor->delay_buffer, *data, *frames * blocksize);

	if (!monitor->prev_video_ts) {
		monitor->prev_video_ts = last_frame_ts;

	} else if (monitor->prev_video_ts == last_frame_ts) {
		monitor->time_since_prev += util_mul_div64(
			*frames, 1000000000ULL, monitor->sample_rate);
	} else {
		monitor->time_since_prev = 0;
	}

	while (monitor->delay_buffer.size != 0) {
		size_t size;
		bool bad_diff;

		circlebuf_peek_front(&monitor->delay_buffer, &cur_ts,
				     sizeof(ts));
		front_ts = cur_ts - util_mul_div64(pad, 1000000000ULL,
						   monitor->sample_rate);
		diff = (int64_t)front_ts - (int64_t)last_frame_ts;
		bad_diff = !last_frame_ts || llabs(diff) > 5000000000 ||
			   monitor->time_since_prev > 100000000ULL;

		/* delay audio if rushing */
		if (!bad_diff && diff > 75000000) {
#ifdef DEBUG_AUDIO
			blog(LOG_INFO,
			     "audio rushing, cutting audio, "
			     "diff: %lld, delay buffer size: %lu, "
			     "v: %llu: a: %llu",
			     diff, (int)monitor->delay_buffer.size,
			     last_frame_ts, front_ts);
#endif
			return false;
		}

		circlebuf_pop_front(&monitor->delay_buffer, NULL, sizeof(ts));
		circlebuf_pop_front(&monitor->delay_buffer, frames,
				    sizeof(*frames));

		size = *frames * blocksize;
		da_resize(monitor->buf, size);
		circlebuf_pop_front(&monitor->delay_buffer, monitor->buf.array,
				    size);

		/* cut audio if dragging */
		if (!bad_diff && diff < -75000000 &&
		    monitor->delay_buffer.size > 0) {
#ifdef DEBUG_AUDIO
			blog(LOG_INFO,
			     "audio dragging, cutting audio, "
			     "diff: %lld, delay buffer size: %lu, "
			     "v: %llu: a: %llu",
			     diff, (int)monitor->delay_buffer.size,
			     last_frame_ts, front_ts);
#endif
			continue;
		}

		*data = monitor->buf.array;
		return true;
	}

	return false;
}

static void on_audio_playback(void *param, obs_source_t *source,
			      const struct audio_data *audio_data, bool muted)
{
	struct audio_monitor *monitor = param;
	IAudioRenderClient *render = monitor->render;
	uint8_t *resample_data[MAX_AV_PLANES];
	float vol = source->user_volume;
	uint32_t resample_frames;
	uint64_t ts_offset;
	bool success;
	BYTE *output;

	if (pthread_mutex_trylock(&monitor->playback_mutex) != 0) {
		return;
	}
	if (os_atomic_load_long(&source->activate_refs) == 0) {
		goto unlock;
	}

	success = audio_resampler_resample(
		monitor->resampler, resample_data, &resample_frames, &ts_offset,
		(const uint8_t *const *)audio_data->data,
		(uint32_t)audio_data->frames);
	if (!success) {
		goto unlock;
	}

	UINT32 pad = 0;
	monitor->client->lpVtbl->GetCurrentPadding(monitor->client, &pad);

	bool decouple_audio = source->async_unbuffered &&
			      source->async_decoupled;

	if (monitor->source_has_video && !decouple_audio) {
		uint64_t ts = audio_data->timestamp - ts_offset;

		if (!process_audio_delay(monitor, (float **)(&resample_data[0]),
					 &resample_frames, ts, pad)) {
			goto unlock;
		}
	}

	HRESULT hr =
		render->lpVtbl->GetBuffer(render, resample_frames, &output);
	if (FAILED(hr)) {
		goto unlock;
	}

	if (!muted) {
		/* apply volume */
		if (!close_float(vol, 1.0f, EPSILON)) {
			register float *cur = (float *)resample_data[0];
			register float *end =
				cur + resample_frames * monitor->channels;

			while (cur < end)
				*(cur++) *= vol;
		}
		memcpy(output, resample_data[0],
		       resample_frames * monitor->channels * sizeof(float));
	}

	render->lpVtbl->ReleaseBuffer(render, resample_frames,
				      muted ? AUDCLNT_BUFFERFLAGS_SILENT : 0);

unlock:
	pthread_mutex_unlock(&monitor->playback_mutex);
}

static inline void audio_monitor_free(struct audio_monitor *monitor)
{
	if (monitor->ignore)
		return;

	if (monitor->source) {
		obs_source_remove_audio_capture_callback(
			monitor->source, on_audio_playback, monitor);
	}

	if (monitor->client)
		monitor->client->lpVtbl->Stop(monitor->client);

	safe_release(monitor->device);
	safe_release(monitor->client);
	safe_release(monitor->render);
	audio_resampler_destroy(monitor->resampler);
	circlebuf_free(&monitor->delay_buffer);
	da_free(monitor->buf);
}

static enum speaker_layout convert_speaker_layout(DWORD layout, WORD channels)
{
	switch (layout) {
	case KSAUDIO_SPEAKER_2POINT1:
		return SPEAKERS_2POINT1;
	case KSAUDIO_SPEAKER_SURROUND:
		return SPEAKERS_4POINT0;
	case KSAUDIO_SPEAKER_4POINT1:
		return SPEAKERS_4POINT1;
	case KSAUDIO_SPEAKER_5POINT1:
		return SPEAKERS_5POINT1;
	case KSAUDIO_SPEAKER_7POINT1:
		return SPEAKERS_7POINT1;
	}

	return (enum speaker_layout)channels;
}

extern bool devices_match(const char *id1, const char *id2);

static bool audio_monitor_init(struct audio_monitor *monitor,
			       obs_source_t *source)
{
	IMMDeviceEnumerator *immde = NULL;
	WAVEFORMATEX *wfex = NULL;
	bool success = false;
	UINT32 frames;
	HRESULT hr;

	pthread_mutex_init_value(&monitor->playback_mutex);

	monitor->source = source;

	const char *id = obs->audio.monitoring_device_id;
	if (!id) {
		warn("%s: No device ID set", __FUNCTION__);
		return false;
	}

	if (source->info.output_flags & OBS_SOURCE_DO_NOT_SELF_MONITOR) {
		obs_data_t *s = obs_source_get_settings(source);
		const char *s_dev_id = obs_data_get_string(s, "device_id");
		bool match = devices_match(s_dev_id, id);
		obs_data_release(s);

		if (match) {
			monitor->ignore = true;
			return true;
		}
	}

	/* ------------------------------------------ *
	 * Init device                                */

	hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
			      &IID_IMMDeviceEnumerator, (void **)&immde);
	if (FAILED(hr)) {
		warn("%s: Failed to create IMMDeviceEnumerator: %08lX",
		     __FUNCTION__, hr);
		return false;
	}

	if (strcmp(id, "default") == 0) {
		hr = immde->lpVtbl->GetDefaultAudioEndpoint(
			immde, eRender, eConsole, &monitor->device);
	} else {
		wchar_t w_id[512];
		os_utf8_to_wcs(id, 0, w_id, 512);

		hr = immde->lpVtbl->GetDevice(immde, w_id, &monitor->device);
	}

	if (FAILED(hr)) {
		warn("%s: Failed to get device: %08lX", __FUNCTION__, hr);
		goto fail;
	}

	/* ------------------------------------------ *
	 * Init client                                */

	hr = monitor->device->lpVtbl->Activate(monitor->device,
					       &IID_IAudioClient, CLSCTX_ALL,
					       NULL, (void **)&monitor->client);
	if (FAILED(hr)) {
		warn("%s: Failed to activate device: %08lX", __FUNCTION__, hr);
		goto fail;
	}

	hr = monitor->client->lpVtbl->GetMixFormat(monitor->client, &wfex);
	if (FAILED(hr)) {
		warn("%s: Failed to get mix format: %08lX", __FUNCTION__, hr);
		goto fail;
	}

	hr = monitor->client->lpVtbl->Initialize(monitor->client,
						 AUDCLNT_SHAREMODE_SHARED, 0,
						 10000000, 0, wfex, NULL);
	if (FAILED(hr)) {
		warn("%s: Failed to initialize: %08lX", __FUNCTION__, hr);
		goto fail;
	}

	/* ------------------------------------------ *
	 * Init resampler                             */

	const struct audio_output_info *info =
		audio_output_get_info(obs->audio.audio);
	WAVEFORMATEXTENSIBLE *ext = (WAVEFORMATEXTENSIBLE *)wfex;
	struct resample_info from;
	struct resample_info to;

	from.samples_per_sec = info->samples_per_sec;
	from.speakers = info->speakers;
	from.format = AUDIO_FORMAT_FLOAT_PLANAR;

	to.samples_per_sec = (uint32_t)wfex->nSamplesPerSec;
	to.speakers =
		convert_speaker_layout(ext->dwChannelMask, wfex->nChannels);
	to.format = AUDIO_FORMAT_FLOAT;

	monitor->sample_rate = (uint32_t)wfex->nSamplesPerSec;
	monitor->channels = wfex->nChannels;
	monitor->resampler = audio_resampler_create(&to, &from);
	if (!monitor->resampler) {
		goto fail;
	}

	/* ------------------------------------------ *
	 * Init client                                */

	hr = monitor->client->lpVtbl->GetBufferSize(monitor->client, &frames);
	if (FAILED(hr)) {
		warn("%s: Failed to get buffer size: %08lX", __FUNCTION__, hr);
		goto fail;
	}

	hr = monitor->client->lpVtbl->GetService(monitor->client,
						 &IID_IAudioRenderClient,
						 (void **)&monitor->render);
	if (FAILED(hr)) {
		warn("%s: Failed to get IAudioRenderClient: %08lX",
		     __FUNCTION__, hr);
		goto fail;
	}

	if (pthread_mutex_init(&monitor->playback_mutex, NULL) != 0) {
		warn("%s: Failed to initialize mutex", __FUNCTION__);
		goto fail;
	}

	hr = monitor->client->lpVtbl->Start(monitor->client);
	if (FAILED(hr)) {
		warn("%s: Failed to start audio: %08lX", __FUNCTION__, hr);
		goto fail;
	}

	success = true;

fail:
	safe_release(immde);
	if (wfex)
		CoTaskMemFree(wfex);
	return success;
}

static void audio_monitor_init_final(struct audio_monitor *monitor)
{
	if (monitor->ignore)
		return;

	monitor->source_has_video =
		(monitor->source->info.output_flags & OBS_SOURCE_VIDEO) != 0;
	obs_source_add_audio_capture_callback(monitor->source,
					      on_audio_playback, monitor);
}

struct audio_monitor *audio_monitor_create(obs_source_t *source)
{
	struct audio_monitor monitor = {0};
	struct audio_monitor *out;

	if (!audio_monitor_init(&monitor, source)) {
		goto fail;
	}

	out = bmemdup(&monitor, sizeof(monitor));

	pthread_mutex_lock(&obs->audio.monitoring_mutex);
	da_push_back(obs->audio.monitors, &out);
	pthread_mutex_unlock(&obs->audio.monitoring_mutex);

	audio_monitor_init_final(out);
	return out;

fail:
	audio_monitor_free(&monitor);
	return NULL;
}

void audio_monitor_reset(struct audio_monitor *monitor)
{
	struct audio_monitor new_monitor = {0};
	bool success;

	pthread_mutex_lock(&monitor->playback_mutex);
	success = audio_monitor_init(&new_monitor, monitor->source);
	pthread_mutex_unlock(&monitor->playback_mutex);

	if (success) {
		obs_source_t *source = monitor->source;
		audio_monitor_free(monitor);
		*monitor = new_monitor;
		audio_monitor_init_final(monitor);
	} else {
		audio_monitor_free(&new_monitor);
	}
}

void audio_monitor_destroy(struct audio_monitor *monitor)
{
	if (monitor) {
		audio_monitor_free(monitor);

		pthread_mutex_lock(&obs->audio.monitoring_mutex);
		da_erase_item(obs->audio.monitors, &monitor);
		pthread_mutex_unlock(&obs->audio.monitoring_mutex);

		bfree(monitor);
	}
}
