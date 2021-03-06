#include "obs-internal.h"
#include "pulseaudio-wrapper.h"

#define PULSE_DATA(voidptr) struct audio_monitor *data = voidptr;
#define blog(level, msg, ...) blog(level, "pulse-am: " msg, ##__VA_ARGS__)

struct audio_monitor {
	obs_source_t *source;
	pa_stream *stream;
	char *device;
	pa_buffer_attr attr;
	enum speaker_layout speakers;
	pa_sample_format_t format;
	uint_fast32_t samples_per_sec;
	uint_fast32_t bytes_per_frame;
	uint_fast8_t channels;

	uint_fast32_t packets;
	uint_fast64_t frames;

	struct circlebuf new_data;
	audio_resampler_t *resampler;
	size_t buffer_size;
	size_t bytesRemaining;
	size_t bytes_per_channel;

	bool ignore;
	pthread_mutex_t playback_mutex;
};

static enum speaker_layout
pulseaudio_channels_to_obs_speakers(uint_fast32_t channels)
{
	switch (channels) {
	case 0:
		return SPEAKERS_UNKNOWN;
	case 1:
		return SPEAKERS_MONO;
	case 2:
		return SPEAKERS_STEREO;
	case 3:
		return SPEAKERS_2POINT1;
	case 4:
		return SPEAKERS_4POINT0;
	case 5:
		return SPEAKERS_4POINT1;
	case 6:
		return SPEAKERS_5POINT1;
	case 8:
		return SPEAKERS_7POINT1;
	default:
		return SPEAKERS_UNKNOWN;
	}
}

static enum audio_format
pulseaudio_to_obs_audio_format(pa_sample_format_t format)
{
	switch (format) {
	case PA_SAMPLE_U8:
		return AUDIO_FORMAT_U8BIT;
	case PA_SAMPLE_S16LE:
		return AUDIO_FORMAT_16BIT;
	case PA_SAMPLE_S32LE:
		return AUDIO_FORMAT_32BIT;
	case PA_SAMPLE_FLOAT32LE:
		return AUDIO_FORMAT_FLOAT;
	default:
		return AUDIO_FORMAT_UNKNOWN;
	}
}

static pa_channel_map pulseaudio_channel_map(enum speaker_layout layout)
{
	pa_channel_map ret;

	ret.map[0] = PA_CHANNEL_POSITION_FRONT_LEFT;
	ret.map[1] = PA_CHANNEL_POSITION_FRONT_RIGHT;
	ret.map[2] = PA_CHANNEL_POSITION_FRONT_CENTER;
	ret.map[3] = PA_CHANNEL_POSITION_LFE;
	ret.map[4] = PA_CHANNEL_POSITION_REAR_LEFT;
	ret.map[5] = PA_CHANNEL_POSITION_REAR_RIGHT;
	ret.map[6] = PA_CHANNEL_POSITION_SIDE_LEFT;
	ret.map[7] = PA_CHANNEL_POSITION_SIDE_RIGHT;

	switch (layout) {
	case SPEAKERS_MONO:
		ret.channels = 1;
		ret.map[0] = PA_CHANNEL_POSITION_MONO;
		break;

	case SPEAKERS_STEREO:
		ret.channels = 2;
		break;

	case SPEAKERS_2POINT1:
		ret.channels = 3;
		ret.map[2] = PA_CHANNEL_POSITION_LFE;
		break;

	case SPEAKERS_4POINT0:
		ret.channels = 4;
		ret.map[3] = PA_CHANNEL_POSITION_REAR_CENTER;
		break;

	case SPEAKERS_4POINT1:
		ret.channels = 5;
		ret.map[4] = PA_CHANNEL_POSITION_REAR_CENTER;
		break;

	case SPEAKERS_5POINT1:
		ret.channels = 6;
		break;

	case SPEAKERS_7POINT1:
		ret.channels = 8;
		break;

	case SPEAKERS_UNKNOWN:
	default:
		ret.channels = 0;
		break;
	}

	return ret;
}

static void process_byte(void *p, size_t frames, size_t channels, float vol)
{
	register char *cur = (char *)p;
	register char *end = cur + frames * channels;

	while (cur < end)
		*(cur++) *= vol;
}

static void process_short(void *p, size_t frames, size_t channels, float vol)
{
	register short *cur = (short *)p;
	register short *end = cur + frames * channels;

	while (cur < end)
		*(cur++) *= vol;
}

static void process_float(void *p, size_t frames, size_t channels, float vol)
{
	register float *cur = (float *)p;
	register float *end = cur + frames * channels;

	while (cur < end)
		*(cur++) *= vol;
}

void process_volume(const struct audio_monitor *monitor, float vol,
		    uint8_t *const *resample_data, uint32_t resample_frames)
{
	switch (monitor->bytes_per_channel) {
	case 1:
		process_byte(resample_data[0], resample_frames,
			     monitor->channels, vol);
		break;
	case 2:
		process_short(resample_data[0], resample_frames,
			      monitor->channels, vol);
		break;
	default:
		process_float(resample_data[0], resample_frames,
			      monitor->channels, vol);
		break;
	}
}

static void do_stream_write(void *param)
{
	PULSE_DATA(param);
	uint8_t *buffer = NULL;

	while (data->new_data.size >= data->buffer_size &&
	       data->bytesRemaining > 0) {
		size_t bytesToFill = data->buffer_size;

		if (bytesToFill > data->bytesRemaining)
			bytesToFill = data->bytesRemaining;

		pulseaudio_lock();
		pa_stream_begin_write(data->stream, (void **)&buffer,
				      &bytesToFill);
		pulseaudio_unlock();

		circlebuf_pop_front(&data->new_data, buffer, bytesToFill);

		pulseaudio_lock();
		pa_stream_write(data->stream, buffer, bytesToFill, NULL, 0LL,
				PA_SEEK_RELATIVE);
		pulseaudio_unlock();

		data->bytesRemaining -= bytesToFill;
	}
}

static void on_audio_playback(void *param, obs_source_t *source,
			      const struct audio_data *audio_data, bool muted)
{
	struct audio_monitor *monitor = param;
	float vol = source->user_volume;
	size_t bytes;

	uint8_t *resample_data[MAX_AV_PLANES];
	uint32_t resample_frames;
	uint64_t ts_offset;
	bool success;

	if (pthread_mutex_trylock(&monitor->playback_mutex) != 0)
		return;

	if (os_atomic_load_long(&source->activate_refs) == 0)
		goto unlock;

	success = audio_resampler_resample(
		monitor->resampler, resample_data, &resample_frames, &ts_offset,
		(const uint8_t *const *)audio_data->data,
		(uint32_t)audio_data->frames);

	if (!success)
		goto unlock;

	bytes = monitor->bytes_per_frame * resample_frames;

	if (muted) {
		memset(resample_data[0], 0, bytes);
	} else {
		if (!close_float(vol, 1.0f, EPSILON)) {
			process_volume(monitor, vol, resample_data,
				       resample_frames);
		}
	}

	circlebuf_push_back(&monitor->new_data, resample_data[0], bytes);
	monitor->packets++;
	monitor->frames += resample_frames;

unlock:
	pthread_mutex_unlock(&monitor->playback_mutex);
	do_stream_write(param);
}

static void pulseaudio_stream_write(pa_stream *p, size_t nbytes, void *userdata)
{
	UNUSED_PARAMETER(p);
	PULSE_DATA(userdata);

	pthread_mutex_lock(&data->playback_mutex);
	data->bytesRemaining += nbytes;
	pthread_mutex_unlock(&data->playback_mutex);

	pulseaudio_signal(0);
}

static void pulseaudio_underflow(pa_stream *p, void *userdata)
{
	UNUSED_PARAMETER(p);
	PULSE_DATA(userdata);

	pthread_mutex_lock(&data->playback_mutex);
	if (obs_source_active(data->source))
		data->attr.tlength = (data->attr.tlength * 3) / 2;

	pa_stream_set_buffer_attr(data->stream, &data->attr, NULL, NULL);
	pthread_mutex_unlock(&data->playback_mutex);

	pulseaudio_signal(0);
}

static void pulseaudio_server_info(pa_context *c, const pa_server_info *i,
				   void *userdata)
{
	UNUSED_PARAMETER(c);
	UNUSED_PARAMETER(userdata);

	blog(LOG_INFO, "Server name: '%s %s'", i->server_name,
	     i->server_version);

	pulseaudio_signal(0);
}

static void pulseaudio_source_info(pa_context *c, const pa_source_info *i,
				   int eol, void *userdata)
{
	UNUSED_PARAMETER(c);
	PULSE_DATA(userdata);
	// An error occurred
	if (eol < 0) {
		data->format = PA_SAMPLE_INVALID;
		goto skip;
	}
	// Terminating call for multi instance callbacks
	if (eol > 0)
		goto skip;

	blog(LOG_INFO, "Audio format: %s, %" PRIu32 " Hz, %" PRIu8 " channels",
	     pa_sample_format_to_string(i->sample_spec.format),
	     i->sample_spec.rate, i->sample_spec.channels);

	pa_sample_format_t format = i->sample_spec.format;
	if (pulseaudio_to_obs_audio_format(format) == AUDIO_FORMAT_UNKNOWN) {
		format = PA_SAMPLE_FLOAT32LE;

		blog(LOG_INFO,
		     "Sample format %s not supported by OBS,"
		     "using %s instead for recording",
		     pa_sample_format_to_string(i->sample_spec.format),
		     pa_sample_format_to_string(format));
	}

	uint8_t channels = i->sample_spec.channels;
	if (pulseaudio_channels_to_obs_speakers(channels) == SPEAKERS_UNKNOWN) {
		channels = 2;

		blog(LOG_INFO,
		     "%c channels not supported by OBS,"
		     "using %c instead for recording",
		     i->sample_spec.channels, channels);
	}

	data->format = format;
	data->samples_per_sec = i->sample_spec.rate;
	data->channels = channels;
skip:
	pulseaudio_signal(0);
}

static void pulseaudio_stop_playback(struct audio_monitor *monitor)
{
	if (monitor->stream) {
		/* Stop the stream */
		pulseaudio_lock();
		pa_stream_disconnect(monitor->stream);
		pulseaudio_unlock();

		/* Remove the callbacks, to ensure we no longer try to do anything
		 * with this stream object */
		pulseaudio_write_callback(monitor->stream, NULL, NULL);
		pulseaudio_set_underflow_callback(monitor->stream, NULL, NULL);

		/* Unreference the stream and drop it. PA will free it when it can. */
		pulseaudio_lock();
		pa_stream_unref(monitor->stream);
		pulseaudio_unlock();
		monitor->stream = NULL;
	}

	blog(LOG_INFO, "Stopped Monitoring in '%s'", monitor->device);
	blog(LOG_INFO,
	     "Got %" PRIuFAST32 " packets with %" PRIuFAST64 " frames",
	     monitor->packets, monitor->frames);

	monitor->packets = 0;
	monitor->frames = 0;
}

static bool audio_monitor_init(struct audio_monitor *monitor,
			       obs_source_t *source)
{
	pthread_mutex_init_value(&monitor->playback_mutex);

	monitor->source = source;

	const char *id = obs->audio.monitoring_device_id;
	if (!id)
		return false;

	if (source->info.output_flags & OBS_SOURCE_DO_NOT_SELF_MONITOR) {
		obs_data_t *s = obs_source_get_settings(source);
		const char *s_dev_id = obs_data_get_string(s, "device_id");
		bool match = devices_match(s_dev_id, id);
		obs_data_release(s);

		if (match) {
			monitor->ignore = true;
			blog(LOG_INFO, "Prevented feedback-loop in '%s'",
			     s_dev_id);
			return true;
		}
	}

	pulseaudio_init();

	if (strcmp(id, "default") == 0)
		get_default_id(&monitor->device);
	else
		monitor->device = bstrdup(id);

	if (!monitor->device)
		return false;

	if (pulseaudio_get_server_info(pulseaudio_server_info,
				       (void *)monitor) < 0) {
		blog(LOG_ERROR, "Unable to get server info !");
		return false;
	}

	if (pulseaudio_get_source_info(pulseaudio_source_info, monitor->device,
				       (void *)monitor) < 0) {
		blog(LOG_ERROR, "Unable to get source info !");
		return false;
	}
	if (monitor->format == PA_SAMPLE_INVALID) {
		blog(LOG_ERROR,
		     "An error occurred while getting the source info!");
		return false;
	}

	pa_sample_spec spec;
	spec.format = monitor->format;
	spec.rate = (uint32_t)monitor->samples_per_sec;
	spec.channels = monitor->channels;

	if (!pa_sample_spec_valid(&spec)) {
		blog(LOG_ERROR, "Sample spec is not valid");
		return false;
	}

	const struct audio_output_info *info =
		audio_output_get_info(obs->audio.audio);

	struct resample_info from = {.samples_per_sec = info->samples_per_sec,
				     .speakers = info->speakers,
				     .format = AUDIO_FORMAT_FLOAT_PLANAR};
	struct resample_info to = {
		.samples_per_sec = (uint32_t)monitor->samples_per_sec,
		.speakers =
			pulseaudio_channels_to_obs_speakers(monitor->channels),
		.format = pulseaudio_to_obs_audio_format(monitor->format)};

	monitor->resampler = audio_resampler_create(&to, &from);
	if (!monitor->resampler) {
		blog(LOG_WARNING, "%s: %s", __FUNCTION__,
		     "Failed to create resampler");
		return false;
	}

	monitor->bytes_per_channel = get_audio_bytes_per_channel(
		pulseaudio_to_obs_audio_format(monitor->format));
	monitor->speakers = pulseaudio_channels_to_obs_speakers(spec.channels);
	monitor->bytes_per_frame = pa_frame_size(&spec);

	pa_channel_map channel_map = pulseaudio_channel_map(monitor->speakers);

	monitor->stream = pulseaudio_stream_new(
		obs_source_get_name(monitor->source), &spec, &channel_map);
	if (!monitor->stream) {
		blog(LOG_ERROR, "Unable to create stream");
		return false;
	}

	monitor->attr.fragsize = (uint32_t)-1;
	monitor->attr.maxlength = (uint32_t)-1;
	monitor->attr.minreq = (uint32_t)-1;
	monitor->attr.prebuf = (uint32_t)-1;
	monitor->attr.tlength = pa_usec_to_bytes(25000, &spec);

	monitor->buffer_size =
		monitor->bytes_per_frame * pa_usec_to_bytes(5000, &spec);

	pa_stream_flags_t flags = PA_STREAM_INTERPOLATE_TIMING |
				  PA_STREAM_AUTO_TIMING_UPDATE;

	if (pthread_mutex_init(&monitor->playback_mutex, NULL) != 0) {
		blog(LOG_WARNING, "%s: %s", __FUNCTION__,
		     "Failed to init mutex");
		return false;
	}

	int_fast32_t ret = pulseaudio_connect_playback(
		monitor->stream, monitor->device, &monitor->attr, flags);
	if (ret < 0) {
		pulseaudio_stop_playback(monitor);
		blog(LOG_ERROR, "Unable to connect to stream");
		return false;
	}

	blog(LOG_INFO, "Started Monitoring in '%s'", monitor->device);
	return true;
}

static void audio_monitor_init_final(struct audio_monitor *monitor)
{
	if (monitor->ignore)
		return;

	obs_source_add_audio_capture_callback(monitor->source,
					      on_audio_playback, monitor);

	pulseaudio_write_callback(monitor->stream, pulseaudio_stream_write,
				  (void *)monitor);

	pulseaudio_set_underflow_callback(monitor->stream, pulseaudio_underflow,
					  (void *)monitor);
}

static inline void audio_monitor_free(struct audio_monitor *monitor)
{
	if (monitor->ignore)
		return;

	if (monitor->source)
		obs_source_remove_audio_capture_callback(
			monitor->source, on_audio_playback, monitor);

	audio_resampler_destroy(monitor->resampler);
	circlebuf_free(&monitor->new_data);

	if (monitor->stream)
		pulseaudio_stop_playback(monitor);
	pulseaudio_unref();

	bfree(monitor->device);
}

struct audio_monitor *audio_monitor_create(obs_source_t *source)
{
	struct audio_monitor monitor = {0};
	struct audio_monitor *out;

	if (!audio_monitor_init(&monitor, source))
		goto fail;

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
	audio_monitor_free(monitor);

	pthread_mutex_lock(&monitor->playback_mutex);
	success = audio_monitor_init(&new_monitor, monitor->source);
	pthread_mutex_unlock(&monitor->playback_mutex);

	if (success) {
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
