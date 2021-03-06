#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioQueue.h>
#include <CoreFoundation/CFString.h>
#include <CoreAudio/CoreAudio.h>

#include "../../media-io/audio-resampler.h"
#include "../../util/circlebuf.h"
#include "../../util/threading.h"
#include "../../util/platform.h"
#include "../../obs-internal.h"
#include "../../util/darray.h"

#include "mac-helpers.h"

struct audio_monitor {
	obs_source_t *source;
	AudioQueueRef queue;
	AudioQueueBufferRef buffers[3];

	pthread_mutex_t mutex;
	struct circlebuf empty_buffers;
	struct circlebuf new_data;
	audio_resampler_t *resampler;
	size_t buffer_size;
	size_t wait_size;
	uint32_t channels;

	volatile bool active;
	bool paused;
	bool ignore;
};

static inline bool fill_buffer(struct audio_monitor *monitor)
{
	AudioQueueBufferRef buf;
	OSStatus stat;

	if (monitor->new_data.size < monitor->buffer_size) {
		return false;
	}

	circlebuf_pop_front(&monitor->empty_buffers, &buf, sizeof(buf));
	circlebuf_pop_front(&monitor->new_data, buf->mAudioData,
			    monitor->buffer_size);

	buf->mAudioDataByteSize = monitor->buffer_size;

	stat = AudioQueueEnqueueBuffer(monitor->queue, buf, 0, NULL);
	if (!success(stat, "AudioQueueEnqueueBuffer")) {
		blog(LOG_WARNING, "%s: %s", __FUNCTION__,
		     "Failed to enqueue buffer");
		AudioQueueStop(monitor->queue, false);
	}
	return true;
}

static void on_audio_playback(void *param, obs_source_t *source,
			      const struct audio_data *audio_data, bool muted)
{
	struct audio_monitor *monitor = param;
	float vol = source->user_volume;
	uint32_t bytes;

	UNUSED_PARAMETER(source);

	if (!os_atomic_load_bool(&monitor->active)) {
		return;
	}

	uint8_t *resample_data[MAX_AV_PLANES];
	uint32_t resample_frames;
	uint64_t ts_offset;
	bool success;

	success = audio_resampler_resample(
		monitor->resampler, resample_data, &resample_frames, &ts_offset,
		(const uint8_t *const *)audio_data->data,
		(uint32_t)audio_data->frames);
	if (!success) {
		return;
	}

	bytes = sizeof(float) * monitor->channels * resample_frames;

	if (muted) {
		memset(resample_data[0], 0, bytes);
	} else {
		/* apply volume */
		if (!close_float(vol, 1.0f, EPSILON)) {
			register float *cur = (float *)resample_data[0];
			register float *end =
				cur + resample_frames * monitor->channels;

			while (cur < end)
				*(cur++) *= vol;
		}
	}

	pthread_mutex_lock(&monitor->mutex);
	circlebuf_push_back(&monitor->new_data, resample_data[0], bytes);

	if (monitor->new_data.size >= monitor->wait_size) {
		monitor->wait_size = 0;

		while (monitor->empty_buffers.size > 0) {
			if (!fill_buffer(monitor)) {
				break;
			}
		}

		if (monitor->paused) {
			AudioQueueStart(monitor->queue, NULL);
			monitor->paused = false;
		}
	}

	pthread_mutex_unlock(&monitor->mutex);
}

static void buffer_audio(void *data, AudioQueueRef aq, AudioQueueBufferRef buf)
{
	struct audio_monitor *monitor = data;

	pthread_mutex_lock(&monitor->mutex);
	circlebuf_push_back(&monitor->empty_buffers, &buf, sizeof(buf));
	while (monitor->empty_buffers.size > 0) {
		if (!fill_buffer(monitor)) {
			break;
		}
	}
	if (monitor->empty_buffers.size == sizeof(buf) * 3) {
		monitor->paused = true;
		monitor->wait_size = monitor->buffer_size * 3;
		AudioQueuePause(monitor->queue);
	}
	pthread_mutex_unlock(&monitor->mutex);

	UNUSED_PARAMETER(aq);
}

extern bool devices_match(const char *id1, const char *id2);

static bool audio_monitor_init(struct audio_monitor *monitor,
			       obs_source_t *source)
{
	const struct audio_output_info *info =
		audio_output_get_info(obs->audio.audio);
	uint32_t channels = get_audio_channels(info->speakers);
	OSStatus stat;

	AudioStreamBasicDescription desc = {
		.mSampleRate = (Float64)info->samples_per_sec,
		.mFormatID = kAudioFormatLinearPCM,
		.mFormatFlags = kAudioFormatFlagIsFloat |
				kAudioFormatFlagIsPacked,
		.mBytesPerPacket = sizeof(float) * channels,
		.mFramesPerPacket = 1,
		.mBytesPerFrame = sizeof(float) * channels,
		.mChannelsPerFrame = channels,
		.mBitsPerChannel = sizeof(float) * 8};

	monitor->source = source;

	monitor->channels = channels;
	monitor->buffer_size =
		channels * sizeof(float) * info->samples_per_sec / 100 * 3;
	monitor->wait_size = monitor->buffer_size * 3;

	pthread_mutex_init_value(&monitor->mutex);

	const char *uid = obs->audio.monitoring_device_id;
	if (!uid || !*uid) {
		return false;
	}

	if (source->info.output_flags & OBS_SOURCE_DO_NOT_SELF_MONITOR) {
		obs_data_t *s = obs_source_get_settings(source);
		const char *s_dev_id = obs_data_get_string(s, "device_id");
		bool match = devices_match(s_dev_id, uid);
		obs_data_release(s);

		if (match) {
			monitor->ignore = true;
			return true;
		}
	}

	stat = AudioQueueNewOutput(&desc, buffer_audio, monitor, NULL, NULL, 0,
				   &monitor->queue);
	if (!success(stat, "AudioStreamBasicDescription")) {
		return false;
	}

	if (strcmp(uid, "default") != 0) {
		CFStringRef cf_uid = CFStringCreateWithBytes(
			NULL, (const UInt8 *)uid, strlen(uid),
			kCFStringEncodingUTF8, false);

		stat = AudioQueueSetProperty(monitor->queue,
					     kAudioQueueProperty_CurrentDevice,
					     &cf_uid, sizeof(cf_uid));
		CFRelease(cf_uid);

		if (!success(stat, "set current device")) {
			return false;
		}
	}

	stat = AudioQueueSetParameter(monitor->queue, kAudioQueueParam_Volume,
				      1.0);
	if (!success(stat, "set volume")) {
		return false;
	}

	for (size_t i = 0; i < 3; i++) {
		stat = AudioQueueAllocateBuffer(monitor->queue,
						monitor->buffer_size,
						&monitor->buffers[i]);
		if (!success(stat, "allocation of buffer")) {
			return false;
		}

		circlebuf_push_back(&monitor->empty_buffers,
				    &monitor->buffers[i],
				    sizeof(monitor->buffers[i]));
	}

	if (pthread_mutex_init(&monitor->mutex, NULL) != 0) {
		blog(LOG_WARNING, "%s: %s", __FUNCTION__,
		     "Failed to init mutex");
		return false;
	}

	struct resample_info from = {.samples_per_sec = info->samples_per_sec,
				     .speakers = info->speakers,
				     .format = AUDIO_FORMAT_FLOAT_PLANAR};
	struct resample_info to = {.samples_per_sec = info->samples_per_sec,
				   .speakers = info->speakers,
				   .format = AUDIO_FORMAT_FLOAT};

	monitor->resampler = audio_resampler_create(&to, &from);
	if (!monitor->resampler) {
		blog(LOG_WARNING, "%s: %s", __FUNCTION__,
		     "Failed to create resampler");
		return false;
	}

	stat = AudioQueueStart(monitor->queue, NULL);
	if (!success(stat, "start")) {
		return false;
	}

	monitor->active = true;
	return true;
}

static void audio_monitor_free(struct audio_monitor *monitor)
{
	if (monitor->source) {
		obs_source_remove_audio_capture_callback(
			monitor->source, on_audio_playback, monitor);
	}
	if (monitor->active) {
		AudioQueueStop(monitor->queue, true);
	}
	for (size_t i = 0; i < 3; i++) {
		if (monitor->buffers[i]) {
			AudioQueueFreeBuffer(monitor->queue,
					     monitor->buffers[i]);
		}
	}
	if (monitor->queue) {
		AudioQueueDispose(monitor->queue, true);
	}

	audio_resampler_destroy(monitor->resampler);
	circlebuf_free(&monitor->empty_buffers);
	circlebuf_free(&monitor->new_data);
	pthread_mutex_destroy(&monitor->mutex);
}

static void audio_monitor_init_final(struct audio_monitor *monitor)
{
	if (monitor->ignore)
		return;

	obs_source_add_audio_capture_callback(monitor->source,
					      on_audio_playback, monitor);
}

struct audio_monitor *audio_monitor_create(obs_source_t *source)
{
	struct audio_monitor *monitor = bzalloc(sizeof(*monitor));

	if (!audio_monitor_init(monitor, source)) {
		goto fail;
	}

	pthread_mutex_lock(&obs->audio.monitoring_mutex);
	da_push_back(obs->audio.monitors, &monitor);
	pthread_mutex_unlock(&obs->audio.monitoring_mutex);

	audio_monitor_init_final(monitor);
	return monitor;

fail:
	audio_monitor_free(monitor);
	bfree(monitor);
	return NULL;
}

void audio_monitor_reset(struct audio_monitor *monitor)
{
	bool success;

	obs_source_t *source = monitor->source;
	audio_monitor_free(monitor);
	memset(monitor, 0, sizeof(*monitor));

	success = audio_monitor_init(monitor, source);
	if (success)
		audio_monitor_init_final(monitor);
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
