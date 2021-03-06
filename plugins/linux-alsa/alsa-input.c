/*
Copyright (C) 2015. Guillermo A. Amaral B. <g@maral.me>

Based on Pulse Input plugin by Leonhard Oelke.

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
*/

#include <util/bmem.h>
#include <util/platform.h>
#include <util/threading.h>
#include <util/util_uint64.h>
#include <obs-module.h>

#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

#include <pthread.h>

#define blog(level, msg, ...) blog(level, "alsa-input: " msg, ##__VA_ARGS__)

#define NSEC_PER_SEC 1000000000LL
#define NSEC_PER_MSEC 1000000L
#define STARTUP_TIMEOUT_NS (500 * NSEC_PER_MSEC)
#define REOPEN_TIMEOUT 1000UL
#define SHUTDOWN_ON_DEACTIVATE false

struct alsa_data {
	obs_source_t *source;
#if SHUTDOWN_ON_DEACTIVATE
	bool active;
#endif

	/* user settings */
	char *device;

	/* pthread */
	pthread_t listen_thread;
	pthread_t reopen_thread;
	os_event_t *abort_event;
	volatile bool listen;
	volatile bool reopen;

	/* alsa */
	snd_pcm_t *handle;
	snd_pcm_format_t format;
	snd_pcm_uframes_t period_size;

	unsigned int channels;
	unsigned int rate;
	unsigned int sample_size;
	uint8_t *buffer;
	uint64_t first_ts;
};

static const char *alsa_get_name(void *);
static bool alsa_devices_changed(obs_properties_t *props, obs_property_t *p,
				 obs_data_t *settings);
static obs_properties_t *alsa_get_properties(void *);
static void *alsa_create(obs_data_t *, obs_source_t *);
static void alsa_destroy(void *);
static void alsa_activate(void *);
static void alsa_deactivate(void *);
static void alsa_get_defaults(obs_data_t *);
static void alsa_update(void *, obs_data_t *);

struct obs_source_info alsa_input_capture = {
	.id = "alsa_input_capture",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_AUDIO,
	.create = alsa_create,
	.destroy = alsa_destroy,
#if SHUTDOWN_ON_DEACTIVATE
	.activate = alsa_activate,
	.deactivate = alsa_deactivate,
#endif
	.update = alsa_update,
	.get_defaults = alsa_get_defaults,
	.get_name = alsa_get_name,
	.get_properties = alsa_get_properties,
	.icon_type = OBS_ICON_TYPE_AUDIO_INPUT,
};

static bool _alsa_try_open(struct alsa_data *);
static bool _alsa_open(struct alsa_data *);
static void _alsa_close(struct alsa_data *);
static bool _alsa_configure(struct alsa_data *);
static void _alsa_start_reopen(struct alsa_data *);
static void _alsa_stop_reopen(struct alsa_data *);
static void *_alsa_listen(void *);
static void *_alsa_reopen(void *);

static enum audio_format _alsa_to_obs_audio_format(snd_pcm_format_t);
static enum speaker_layout _alsa_channels_to_obs_speakers(unsigned int);

/*****************************************************************************/

void *alsa_create(obs_data_t *settings, obs_source_t *source)
{
	struct alsa_data *data = bzalloc(sizeof(struct alsa_data));

	data->source = source;
#if SHUTDOWN_ON_DEACTIVATE
	data->active = false;
#endif
	data->buffer = NULL;
	data->device = NULL;
	data->first_ts = 0;
	data->handle = NULL;
	data->listen = false;
	data->reopen = false;
	data->listen_thread = 0;
	data->reopen_thread = 0;

	const char *device = obs_data_get_string(settings, "device_id");

	if (strcmp(device, "__custom__") == 0)
		device = obs_data_get_string(settings, "custom_pcm");

	data->device = bstrdup(device);
	data->rate = obs_data_get_int(settings, "rate");

	if (os_event_init(&data->abort_event, OS_EVENT_TYPE_MANUAL) != 0) {
		blog(LOG_ERROR, "Abort event creation failed!");
		goto cleanup;
	}

#if !SHUTDOWN_ON_DEACTIVATE
	_alsa_try_open(data);
#endif
	return data;

cleanup:
	if (data->device)
		bfree(data->device);

	bfree(data);
	return NULL;
}

void alsa_destroy(void *vptr)
{
	struct alsa_data *data = vptr;

	if (data->handle)
		_alsa_close(data);

	os_event_destroy(data->abort_event);
	bfree(data->device);
	bfree(data);
}

#if SHUTDOWN_ON_DEACTIVATE
void alsa_activate(void *vptr)
{
	struct alsa_data *data = vptr;

	data->active = true;
	_alsa_try_open(data);
}

void alsa_deactivate(void *vptr)
{
	struct alsa_data *data = vptr;

	_alsa_stop_reopen(data);
	_alsa_close(data);
	data->active = false;
}
#endif

void alsa_update(void *vptr, obs_data_t *settings)
{
	struct alsa_data *data = vptr;
	const char *device;
	unsigned int rate;
	bool reset = false;

	device = obs_data_get_string(settings, "device_id");

	if (strcmp(device, "__custom__") == 0)
		device = obs_data_get_string(settings, "custom_pcm");

	if (strcmp(data->device, device) != 0) {
		bfree(data->device);
		data->device = bstrdup(device);
		reset = true;
	}

	rate = obs_data_get_int(settings, "rate");
	if (data->rate != rate) {
		data->rate = rate;
		reset = true;
	}

#if SHUTDOWN_ON_DEACTIVATE
	if (reset && data->handle)
		_alsa_close(data);

	if (data->active && !data->handle)
		_alsa_try_open(data);
#else
	if (reset) {
		if (data->handle)
			_alsa_close(data);
		_alsa_try_open(data);
	}
#endif
}

const char *alsa_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("AlsaInput");
}

void alsa_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "device_id", "default");
	obs_data_set_default_string(settings, "custom_pcm", "default");
	obs_data_set_default_int(settings, "rate", 44100);
}

static bool alsa_devices_changed(obs_properties_t *props, obs_property_t *p,
				 obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	bool visible = false;
	const char *device_id = obs_data_get_string(settings, "device_id");

	if (strcmp(device_id, "__custom__") == 0)
		visible = true;

	obs_property_t *custom_pcm = obs_properties_get(props, "custom_pcm");

	obs_property_set_visible(custom_pcm, visible);
	obs_property_modified(custom_pcm, settings);

	return true;
}

obs_properties_t *alsa_get_properties(void *unused)
{
	void **hints;
	void **hint;
	char *name = NULL;
	char *descr = NULL;
	char *io = NULL;
	char *descr_i;
	obs_properties_t *props;
	obs_property_t *devices;
	obs_property_t *rate;

	UNUSED_PARAMETER(unused);

	props = obs_properties_create();

	devices = obs_properties_add_list(props, "device_id",
					  obs_module_text("Device"),
					  OBS_COMBO_TYPE_LIST,
					  OBS_COMBO_FORMAT_STRING);

	obs_property_list_add_string(devices, "Default", "default");

	obs_properties_add_text(props, "custom_pcm", obs_module_text("PCM"),
				OBS_TEXT_DEFAULT);

	rate = obs_properties_add_list(props, "rate", obs_module_text("Rate"),
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_INT);

	obs_property_set_modified_callback(devices, alsa_devices_changed);

	obs_property_list_add_int(rate, "32000 Hz", 32000);
	obs_property_list_add_int(rate, "44100 Hz", 44100);
	obs_property_list_add_int(rate, "48000 Hz", 48000);

	if (snd_device_name_hint(-1, "pcm", &hints) < 0)
		return props;

	hint = hints;
	while (*hint != NULL) {
		/* check if we're dealing with an Input */
		io = snd_device_name_get_hint(*hint, "IOID");
		if (io != NULL && strcmp(io, "Input") != 0)
			goto next;

		name = snd_device_name_get_hint(*hint, "NAME");
		if (name == NULL || strstr(name, "front:") == NULL)
			goto next;

		descr = snd_device_name_get_hint(*hint, "DESC");
		if (!descr)
			goto next;

		descr_i = descr;
		while (*descr_i) {
			if (*descr_i == '\n') {
				*descr_i = '\0';
				break;
			} else
				++descr_i;
		}

		obs_property_list_add_string(devices, descr, name);

	next:
		if (name != NULL)
			free(name), name = NULL;

		if (descr != NULL)
			free(descr), descr = NULL;

		if (io != NULL)
			free(io), io = NULL;

		++hint;
	}
	obs_property_list_add_string(devices, "Custom", "__custom__");

	snd_device_name_free_hint(hints);

	return props;
}

/*****************************************************************************/

bool _alsa_try_open(struct alsa_data *data)
{
	_alsa_stop_reopen(data);

	if (_alsa_open(data))
		return true;

	_alsa_start_reopen(data);

	return false;
}

bool _alsa_open(struct alsa_data *data)
{
	pthread_attr_t attr;
	int err;

	err = snd_pcm_open(&data->handle, data->device, SND_PCM_STREAM_CAPTURE,
			   0);
	if (err < 0) {
		blog(LOG_ERROR, "Failed to open '%s': %s", data->device,
		     snd_strerror(err));
		return false;
	}

	if (!_alsa_configure(data))
		goto cleanup;

	if (snd_pcm_state(data->handle) != SND_PCM_STATE_PREPARED) {
		blog(LOG_ERROR, "Device not prepared: '%s'", data->device);
		goto cleanup;
	}

	/* start listening */

	err = snd_pcm_start(data->handle);
	if (err < 0) {
		blog(LOG_ERROR, "Failed to start '%s': %s", data->device,
		     snd_strerror(err));
		goto cleanup;
	}

	/* create capture thread */

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	err = pthread_create(&data->listen_thread, &attr, _alsa_listen, data);
	if (err) {
		pthread_attr_destroy(&attr);
		blog(LOG_ERROR,
		     "Failed to create capture thread for device '%s'.",
		     data->device);
		goto cleanup;
	}

	pthread_attr_destroy(&attr);
	return true;

cleanup:
	_alsa_close(data);
	return false;
}

void _alsa_close(struct alsa_data *data)
{
	if (data->listen_thread) {
		os_atomic_set_bool(&data->listen, false);
		pthread_join(data->listen_thread, NULL);
		data->listen_thread = 0;
	}

	if (data->handle) {
		snd_pcm_drop(data->handle);
		snd_pcm_close(data->handle), data->handle = NULL;
	}

	if (data->buffer)
		bfree(data->buffer), data->buffer = NULL;
}

bool _alsa_configure(struct alsa_data *data)
{
	snd_pcm_hw_params_t *hwparams;
	int err;
	int dir;

	snd_pcm_hw_params_alloca(&hwparams);

	err = snd_pcm_hw_params_any(data->handle, hwparams);
	if (err < 0) {
		blog(LOG_ERROR, "snd_pcm_hw_params_any failed: %s",
		     snd_strerror(err));
		return false;
	}

	err = snd_pcm_hw_params_set_access(data->handle, hwparams,
					   SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		blog(LOG_ERROR, "snd_pcm_hw_params_set_access failed: %s",
		     snd_strerror(err));
		return false;
	}

#define FORMAT_SIZE 4
	snd_pcm_format_t formats[FORMAT_SIZE] = {SND_PCM_FORMAT_S16_LE,
						 SND_PCM_FORMAT_S32_LE,
						 SND_PCM_FORMAT_FLOAT_LE,
						 SND_PCM_FORMAT_U8};
	bool format_found = false;
	for (int i = 0; i < FORMAT_SIZE; ++i) {
		data->format = formats[i];
		err = snd_pcm_hw_params_test_format(data->handle, hwparams,
						    data->format);
		if (err == 0) {
			format_found = true;
			break;
		}
	}
#undef FORMAT_SIZE
	if (!format_found) {
		blog(LOG_ERROR, "device doesnt support any OBS formats");
		return false;
	}
	snd_pcm_hw_params_set_format(data->handle, hwparams, data->format);
	if (err < 0) {
		blog(LOG_ERROR, "snd_pcm_hw_params_set_format failed: %s",
		     snd_strerror(err));
		return false;
	}

	err = snd_pcm_hw_params_set_rate_near(data->handle, hwparams,
					      &data->rate, 0);
	if (err < 0) {
		blog(LOG_ERROR, "snd_pcm_hw_params_set_rate_near failed: %s",
		     snd_strerror(err));
		return false;
	}
	blog(LOG_INFO, "PCM '%s' rate set to %d", data->device, data->rate);

	err = snd_pcm_hw_params_get_channels(hwparams, &data->channels);
	if (err < 0)
		data->channels = 2;

	err = snd_pcm_hw_params_set_channels_near(data->handle, hwparams,
						  &data->channels);
	if (err < 0) {
		blog(LOG_ERROR,
		     "snd_pcm_hw_params_set_channels_near failed: %s",
		     snd_strerror(err));
		return false;
	}
	blog(LOG_INFO, "PCM '%s' channels set to %d", data->device,
	     data->channels);

	err = snd_pcm_hw_params(data->handle, hwparams);
	if (err < 0) {
		blog(LOG_ERROR, "snd_pcm_hw_params failed: %s",
		     snd_strerror(err));
		return false;
	}

	err = snd_pcm_hw_params_get_period_size(hwparams, &data->period_size,
						&dir);
	if (err < 0) {
		blog(LOG_ERROR, "snd_pcm_hw_params_get_period_size failed: %s",
		     snd_strerror(err));
		return false;
	}

	data->sample_size =
		(data->channels * snd_pcm_format_physical_width(data->format)) /
		8;

	if (data->buffer)
		bfree(data->buffer);
	data->buffer = bzalloc(data->period_size * data->sample_size);

	return true;
}

void _alsa_start_reopen(struct alsa_data *data)
{
	pthread_attr_t attr;
	int err;

	if (os_atomic_load_bool(&data->reopen))
		return;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	err = pthread_create(&data->reopen_thread, &attr, _alsa_reopen, data);
	if (err) {
		blog(LOG_ERROR,
		     "Failed to create reopen thread for device '%s'.",
		     data->device);
	}

	pthread_attr_destroy(&attr);
}

void _alsa_stop_reopen(struct alsa_data *data)
{
	if (os_atomic_load_bool(&data->reopen))
		os_event_signal(data->abort_event);

	if (data->reopen_thread) {
		pthread_join(data->reopen_thread, NULL);
		data->reopen_thread = 0;
	}

	os_event_reset(data->abort_event);
}

void *_alsa_listen(void *attr)
{
	struct alsa_data *data = attr;
	struct obs_source_audio out;

	blog(LOG_DEBUG, "Capture thread started.");

	out.data[0] = data->buffer;
	out.format = _alsa_to_obs_audio_format(data->format);
	out.speakers = _alsa_channels_to_obs_speakers(data->channels);
	out.samples_per_sec = data->rate;

	os_atomic_set_bool(&data->listen, true);

	do {
		snd_pcm_sframes_t frames = snd_pcm_readi(
			data->handle, data->buffer, data->period_size);

		if (!os_atomic_load_bool(&data->listen))
			break;

		if (frames <= 0) {
			frames = snd_pcm_recover(data->handle, frames, 0);
			if (frames <= 0) {
				snd_pcm_wait(data->handle, 100);
				continue;
			}
		}

		out.frames = frames;
		out.timestamp =
			os_gettime_ns() -
			util_mul_div64(frames, NSEC_PER_SEC, data->rate);

		if (!data->first_ts)
			data->first_ts = out.timestamp + STARTUP_TIMEOUT_NS;

		if (out.timestamp > data->first_ts)
			obs_source_output_audio(data->source, &out);
	} while (os_atomic_load_bool(&data->listen));

	blog(LOG_DEBUG, "Capture thread is about to exit.");

	pthread_exit(NULL);
	return NULL;
}

void *_alsa_reopen(void *attr)
{
	struct alsa_data *data = attr;
	unsigned long timeout = REOPEN_TIMEOUT;

	blog(LOG_DEBUG, "Reopen thread started.");

	os_atomic_set_bool(&data->reopen, true);

	while (os_event_timedwait(data->abort_event, timeout) == ETIMEDOUT) {
		if (_alsa_open(data))
			break;

		if (timeout < (REOPEN_TIMEOUT * 5))
			timeout += REOPEN_TIMEOUT;
	}

	os_atomic_set_bool(&data->reopen, false);

	blog(LOG_DEBUG, "Reopen thread is about to exit.");

	pthread_exit(NULL);
	return NULL;
}

enum audio_format _alsa_to_obs_audio_format(snd_pcm_format_t format)
{
	switch (format) {
	case SND_PCM_FORMAT_U8:
		return AUDIO_FORMAT_U8BIT;
	case SND_PCM_FORMAT_S16_LE:
		return AUDIO_FORMAT_16BIT;
	case SND_PCM_FORMAT_S32_LE:
		return AUDIO_FORMAT_32BIT;
	case SND_PCM_FORMAT_FLOAT_LE:
		return AUDIO_FORMAT_FLOAT;
	default:
		break;
	}

	return AUDIO_FORMAT_UNKNOWN;
}

enum speaker_layout _alsa_channels_to_obs_speakers(unsigned int channels)
{
	switch (channels) {
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
	}

	return SPEAKERS_UNKNOWN;
}
