/*
Copyright (C) 2020. Ka Ho Ng <khng300@gmail.com>
Copyright (C) 2020. Ed Maste <emaste@freebsd.org>

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
#include <obs-module.h>

#include <ctype.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include "oss-platform.h"

#define blog(level, msg, ...) blog(level, "oss-audio: " msg, ##__VA_ARGS__)

#define NSEC_PER_SEC 1000000000ULL

#define OSS_MAX_CHANNELS 8

#define OSS_DSP_DEFAULT "/dev/dsp"
#define OSS_SNDSTAT_PATH "/dev/sndstat"
#define OSS_RATE_DEFAULT 48000
#define OSS_CHANNELS_DEFAULT 2

#define OSS_DEVICE_BEGIN "Installed devices:"
#define OSS_USERDEVICE_BEGIN "Installed devices from userspace:"
#define OSS_FV_BEGIN "File Versions:"

/**
 * Control block of plugin instance
 */
struct oss_input_data {
	obs_source_t *source;

	char *device;
	int channels;
	int rate;
	int sample_fmt;

	pthread_t reader_thr;
	int notify_pipe[2];

	int dsp_fd;
	void *dsp_buf;
	size_t dsp_fragsize;
};
#define OBS_PROPS_DSP "dsp"
#define OBS_PROPS_CUSTOM_DSP "custom_dsp"
#define OBS_PATH_DSP_CUSTOM "/"
#define OBS_PROPS_CHANNELS "channels"
#define OBS_PROPS_RATE "rate"
#define OBS_PROPS_SAMPLE_FMT "sample_fmt"

/**
 * Common sampling rate table
 */
struct rate_option {
	int rate;
	char *desc;
} rate_table[] = {
	{8000, "8000 Hz"},     {11025, "11025 Hz"}, {16000, "16000 Hz"},
	{22050, "22050 Hz"},   {32000, "32000 Hz"}, {44100, "44100 Hz"},
	{48000, "48000 Hz"},   {96000, "96000 Hz"}, {192000, "192000 Hz"},
	{384000, "384000 Hz"},
};

static unsigned int oss_sample_size(unsigned int sample_fmt)
{
	switch (sample_fmt) {
	case AFMT_U8:
	case AFMT_S8:
		return 8;
	case AFMT_S16_LE:
	case AFMT_S16_BE:
	case AFMT_U16_LE:
	case AFMT_U16_BE:
		return 16;
	case AFMT_S32_LE:
	case AFMT_S32_BE:
	case AFMT_U32_LE:
	case AFMT_U32_BE:
	case AFMT_S24_LE:
	case AFMT_S24_BE:
	case AFMT_U24_LE:
	case AFMT_U24_BE:
		return 32;
	}
	return 0;
}

static size_t oss_calc_framesize(unsigned int channels, unsigned int sample_fmt)
{
	return oss_sample_size(sample_fmt) * channels / 8;
}

static enum audio_format oss_fmt_to_obs_audio_format(int fmt)
{
	switch (fmt) {
	case AFMT_U8:
		return AUDIO_FORMAT_U8BIT;
	case AFMT_S16_LE:
		return AUDIO_FORMAT_16BIT;
	case AFMT_S32_LE:
		return AUDIO_FORMAT_32BIT;
	}

	return AUDIO_FORMAT_UNKNOWN;
}

static enum speaker_layout oss_channels_to_obs_speakers(unsigned int channels)
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

static int oss_setup_device(struct oss_input_data *handle)
{
	size_t dsp_fragsize;
	void *dsp_buf = NULL;
	int fd = -1, err;
	audio_buf_info bi;

	fd = open(handle->device, O_RDONLY);
	if (fd < 0) {
		blog(LOG_ERROR, "Failed to open device '%s'.", handle->device);
		return -1;
	}
	int val = handle->channels;
	err = ioctl(fd, SNDCTL_DSP_CHANNELS, &val);
	if (err) {
		blog(LOG_ERROR, "Failed to set number of channels on DSP '%s'.",
		     handle->device);
		goto failed_state;
	}
	val = handle->sample_fmt;
	err = ioctl(fd, SNDCTL_DSP_SETFMT, &val);
	if (err) {
		blog(LOG_ERROR, "Failed to set format on DSP '%s'.",
		     handle->device);
		goto failed_state;
	}
	val = handle->rate;
	err = ioctl(fd, SNDCTL_DSP_SPEED, &val);
	if (err) {
		blog(LOG_ERROR, "Failed to set sample rate on DSP '%s'.",
		     handle->device);
		goto failed_state;
	}
	err = ioctl(fd, SNDCTL_DSP_GETISPACE, &bi);
	if (err) {
		blog(LOG_ERROR, "Failed to get fragment size on DSP '%s'.",
		     handle->device);
		goto failed_state;
	}

	dsp_fragsize = bi.fragsize;
	dsp_buf = bmalloc(dsp_fragsize);
	if (dsp_buf == NULL)
		goto failed_state;

	handle->dsp_buf = dsp_buf;
	handle->dsp_fragsize = dsp_fragsize;
	handle->dsp_fd = fd;
	return 0;

failed_state:
	if (fd != -1)
		close(fd);
	bfree(dsp_buf);
	return -1;
}

static void oss_close_device(struct oss_input_data *handle)
{
	if (handle->dsp_fd != -1)
		close(handle->dsp_fd);
	bfree(handle->dsp_buf);
	handle->dsp_fd = -1;
	handle->dsp_buf = NULL;
	handle->dsp_fragsize = 0;
}

static void *oss_reader_thr(void *vptr)
{
	struct oss_input_data *handle = vptr;
	struct pollfd fds[2] = {0};
	size_t framesize;

	framesize = oss_calc_framesize(handle->channels, handle->sample_fmt);

	fds[0].fd = handle->dsp_fd;
	fds[0].events = POLLIN;
	fds[1].fd = handle->notify_pipe[0];
	fds[1].events = POLLIN;

	assert(handle->dsp_buf);

	while (poll(fds, 2, INFTIM) >= 0) {
		if (fds[0].revents & POLLIN) {
			/*
			 * Incoming audio frames
			 */

			struct obs_source_audio out;
			ssize_t nbytes;

			do {
				nbytes = read(handle->dsp_fd, handle->dsp_buf,
					      handle->dsp_fragsize);
			} while (nbytes < 0 && errno == EINTR);

			if (nbytes < 0) {
				blog(LOG_ERROR,
				     "%s: Failed to read buffer on DSP '%s'. Errno %d",
				     __func__, handle->device, errno);
				break;
			} else if (!nbytes) {
				blog(LOG_ERROR,
				     "%s: Unexpected EOF on DSP '%s'.",
				     __func__, handle->device);
				break;
			}

			out.data[0] = handle->dsp_buf;
			out.format =
				oss_fmt_to_obs_audio_format(handle->sample_fmt);
			out.speakers =
				oss_channels_to_obs_speakers(handle->channels);
			out.samples_per_sec = handle->rate;
			out.frames = nbytes / framesize;
			out.timestamp = os_gettime_ns() -
					util_mul_div64(out.frames, NSEC_PER_SEC,
						       handle->rate);
			obs_source_output_audio(handle->source, &out);
		}
		if (fds[1].revents & POLLIN) {
			char buf;
			ssize_t nbytes;

			do {
				nbytes = read(handle->notify_pipe[0], &buf, 1);
				assert(nbytes != 0);
			} while (nbytes < 0 && errno == EINTR);

			break;
		}
	}

	return NULL;
}

static int oss_start_reader(struct oss_input_data *handle)
{
	int pfd[2];
	int err;
	pthread_t thr;

	err = pipe(pfd);
	if (err)
		return -1;

	err = pthread_create(&thr, NULL, oss_reader_thr, handle);
	if (err) {
		close(pfd[0]);
		close(pfd[1]);
		return -1;
	}

	handle->notify_pipe[0] = pfd[0];
	handle->notify_pipe[1] = pfd[1];
	handle->reader_thr = thr;
	return 0;
}

static void oss_stop_reader(struct oss_input_data *handle)
{
	if (handle->reader_thr) {
		char buf = 0x0;

		write(handle->notify_pipe[1], &buf, 1);
		pthread_join(handle->reader_thr, NULL);
	}
	if (handle->notify_pipe[0] != -1) {
		close(handle->notify_pipe[0]);
		close(handle->notify_pipe[1]);
	}
	handle->notify_pipe[0] = -1;
	handle->notify_pipe[1] = -1;
	handle->reader_thr = NULL;
}

/**
 * Returns the name of the plugin
 */
static const char *oss_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("OSSInput");
}

/**
 * Create the plugin object
 */
static void *oss_create(obs_data_t *settings, obs_source_t *source)
{
	const char *dsp;
	const char *custom_dsp;
	struct oss_input_data *handle;

	dsp = obs_data_get_string(settings, OBS_PROPS_DSP);
	custom_dsp = obs_data_get_string(settings, OBS_PROPS_CUSTOM_DSP);
	handle = bmalloc(sizeof(struct oss_input_data));
	if (handle == NULL)
		return NULL;

	handle->source = source;
	handle->device = NULL;
	handle->channels = 0;
	handle->rate = 0;
	handle->sample_fmt = 0;

	handle->dsp_buf = NULL;
	handle->dsp_fragsize = 0;
	handle->dsp_fd = -1;

	handle->notify_pipe[0] = -1;
	handle->notify_pipe[1] = -1;
	handle->reader_thr = NULL;

	if (dsp == NULL)
		return handle;
	if (!strcmp(dsp, OBS_PATH_DSP_CUSTOM)) {
		if (custom_dsp == NULL)
			return handle;

		handle->device = bstrdup(custom_dsp);
	} else
		handle->device = bstrdup(dsp);
	if (handle->device == NULL)
		goto failed_state;

	handle->channels = obs_data_get_int(settings, OBS_PROPS_CHANNELS);
	handle->rate = obs_data_get_int(settings, OBS_PROPS_RATE);
	handle->sample_fmt = obs_data_get_int(settings, OBS_PROPS_SAMPLE_FMT);

	int err = oss_setup_device(handle);
	if (err)
		goto failed_state;
	err = oss_start_reader(handle);
	if (err) {
		oss_close_device(handle);
		goto failed_state;
	}

	return handle;

failed_state:
	bfree(handle);
	return NULL;
}

/**
 * Destroy the plugin object and free all memory
 */
static void oss_destroy(void *vptr)
{
	struct oss_input_data *handle = vptr;

	oss_stop_reader(handle);
	oss_close_device(handle);
	bfree(handle->device);
	bfree(handle);
}

/**
 * Update the input settings
 */
static void oss_update(void *vptr, obs_data_t *settings)
{
	struct oss_input_data *handle = vptr;

	oss_stop_reader(handle);
	oss_close_device(handle);

	const char *dsp = obs_data_get_string(settings, OBS_PROPS_DSP);
	const char *custom_dsp =
		obs_data_get_string(settings, OBS_PROPS_CUSTOM_DSP);
	if (dsp == NULL) {
		bfree(handle->device);
		handle->device = NULL;
		return;
	}

	bfree(handle->device);
	handle->device = NULL;
	if (!strcmp(dsp, OBS_PATH_DSP_CUSTOM)) {
		if (custom_dsp == NULL)
			return;
		handle->device = bstrdup(custom_dsp);
	} else
		handle->device = bstrdup(dsp);
	if (handle->device == NULL)
		return;

	handle->channels = obs_data_get_int(settings, OBS_PROPS_CHANNELS);
	handle->rate = obs_data_get_int(settings, OBS_PROPS_RATE);
	handle->sample_fmt = obs_data_get_int(settings, OBS_PROPS_SAMPLE_FMT);

	int err = oss_setup_device(handle);
	if (err) {
		goto failed_state;
		return;
	}
	err = oss_start_reader(handle);
	if (err) {
		oss_close_device(handle);
		goto failed_state;
	}

	return;

failed_state:
	bfree(handle->device);
	handle->device = NULL;
}

/**
 * Add audio devices to property
 */
static void oss_prop_add_devices(obs_property_t *p)
{
#if defined(__FreeBSD__) || defined(__DragonFly__)
	char *line = NULL;
	size_t linecap = 0;
	FILE *fp;
	bool ud_matching = false;
	bool skipall = false;

	fp = fopen(OSS_SNDSTAT_PATH, "r");
	if (fp == NULL) {
		blog(LOG_ERROR, "Failed to open sndstat at '%s'.",
		     OSS_SNDSTAT_PATH);
		return;
	}

	while (getline(&line, &linecap, fp) > 0) {
		int pcm;
		char *ptr, *pdesc, *pmode;
		char *descr = NULL, *devname = NULL;
		char *udname = NULL;

		if (!strncmp(line, OSS_FV_BEGIN, strlen(OSS_FV_BEGIN))) {
			skipall = true;
			continue;
		}
		if (!strncmp(line, OSS_DEVICE_BEGIN,
			     strlen(OSS_DEVICE_BEGIN))) {
			ud_matching = false;
			skipall = false;
			continue;
		}
		if (!strncmp(line, OSS_USERDEVICE_BEGIN,
			     strlen(OSS_USERDEVICE_BEGIN))) {
			ud_matching = true;
			skipall = false;
			continue;
		}
		if (skipall || isblank(line[0]))
			continue;

		if (!ud_matching) {
			if (sscanf(line, "pcm%i: ", &pcm) != 1)
				continue;
		} else {
			char *end = strchr(line, ':');
			if (end == NULL || end - line == 0)
				continue;

			udname = strndup(line, end - line);
			if (udname == NULL)
				continue;
		}
		if ((ptr = strchr(line, '<')) == NULL)
			goto free_all_str;
		pdesc = ptr + 1;
		if ((ptr = strrchr(pdesc, '>')) == NULL)
			goto free_all_str;
		*ptr++ = '\0';
		if ((pmode = strchr(ptr, '(')) == NULL)
			goto free_all_str;
		pmode++;
		if ((ptr = strrchr(pmode, ')')) == NULL)
			goto free_all_str;
		*ptr++ = '\0';
		if (!isdigit(pmode[0])) {
			if (strcmp(pmode, "rec") != 0 &&
			    strcmp(pmode, "play/rec") != 0)
				goto free_all_str;
		} else {
			int npcs, nrcs;
			if (sscanf(pmode, "%dp:%*dv/%dr:%*dv", &npcs, &nrcs) !=
			    2)
				goto free_all_str;
			if (nrcs < 1)
				goto free_all_str;
		}
		if (!ud_matching) {
			if (asprintf(&descr, "pcm%i: %s", pcm, pdesc) == -1)
				goto free_all_str;
			if (asprintf(&devname, "/dev/dsp%i", pcm) == -1)
				goto free_all_str;
		} else {
			if (asprintf(&descr, "%s: %s", udname, pdesc) == -1)
				goto free_all_str;
			if (asprintf(&devname, "/dev/%s", udname) == -1)
				goto free_all_str;
		}

		obs_property_list_add_string(p, descr, devname);

	free_all_str:
		free(descr);
		free(devname);
		free(udname);
	}
	free(line);

	fclose(fp);
#endif /* defined(__FreeBSD__) || defined(__DragonFly__) */
}

/**
 * Get plugin defaults
 */
static void oss_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, OBS_PROPS_CHANNELS,
				 OSS_CHANNELS_DEFAULT);
	obs_data_set_default_int(settings, OBS_PROPS_RATE, OSS_RATE_DEFAULT);
	obs_data_set_default_int(settings, OBS_PROPS_SAMPLE_FMT, AFMT_S16_LE);
	obs_data_set_default_string(settings, OBS_PROPS_DSP, OSS_DSP_DEFAULT);
}

/**
 * Get plugin properties:
 *
 * Fetch the engine information of the corresponding DSP
 */
static bool oss_fill_device_info(obs_property_t *rate, obs_property_t *channels,
				 const char *device)
{
	oss_audioinfo ai;
	int fd = -1;
	int err;

	obs_property_list_clear(rate);
	obs_property_list_clear(channels);

	if (!strcmp(device, OBS_PATH_DSP_CUSTOM))
		goto cleanup;

	fd = open(device, O_RDONLY);
	if (fd < 0) {
		blog(LOG_ERROR, "Failed to open device '%s'.", device);
		goto cleanup;
	}

	ai.dev = -1;
	err = ioctl(fd, SNDCTL_ENGINEINFO, &ai);
	if (err) {
		blog(LOG_ERROR,
		     "Failed to issue ioctl(SNDCTL_ENGINEINFO) on device '%s'. Errno: %d",
		     device, errno);
		goto cleanup;
	}

	for (int i = ai.min_channels;
	     i <= ai.max_channels && i <= OSS_MAX_CHANNELS; i++) {
		enum speaker_layout layout = oss_channels_to_obs_speakers(i);

		if (layout != SPEAKERS_UNKNOWN) {
			char name[] = "xxx";
			snprintf(name, sizeof(name), "%d", i);
			obs_property_list_add_int(channels, name, i);
		}
	}

	for (size_t i = 0; i < sizeof(rate_table) / sizeof(rate_table[0]);
	     i++) {
		if (ai.min_rate <= rate_table[i].rate &&
		    ai.max_rate >= rate_table[i].rate)
			obs_property_list_add_int(rate, rate_table[i].desc,
						  rate_table[i].rate);
	}

cleanup:
	if (!obs_property_list_item_count(rate))
		obs_property_list_add_int(rate, "48000 Hz", OSS_RATE_DEFAULT);
	if (!obs_property_list_item_count(channels))
		obs_property_list_add_int(channels, "2", OSS_CHANNELS_DEFAULT);
	if (fd != -1)
		close(fd);
	return true;
}

/**
 * Get plugin properties
 */
static bool oss_on_devices_changed(obs_properties_t *props, obs_property_t *p,
				   obs_data_t *settings)
{
	obs_property_t *rate, *channels;
	obs_property_t *custom_dsp;
	const char *device;

	UNUSED_PARAMETER(p);

	device = obs_data_get_string(settings, OBS_PROPS_DSP);
	custom_dsp = obs_properties_get(props, OBS_PROPS_CUSTOM_DSP);
	rate = obs_properties_get(props, OBS_PROPS_RATE);
	channels = obs_properties_get(props, OBS_PROPS_CHANNELS);

	if (!strcmp(device, OBS_PATH_DSP_CUSTOM))
		obs_property_set_visible(custom_dsp, true);
	else
		obs_property_set_visible(custom_dsp, false);
	oss_fill_device_info(rate, channels, device);
	obs_property_modified(rate, settings);
	obs_property_modified(channels, settings);

	obs_property_modified(custom_dsp, settings);

	return true;
}

/**
 * Get plugin properties
 */
static obs_properties_t *oss_properties(void *unused)
{
	obs_properties_t *props;
	obs_property_t *devices;
	obs_property_t *rate;
	obs_property_t *sample_fmt;
	obs_property_t *channels;

	UNUSED_PARAMETER(unused);

	props = obs_properties_create();

	devices = obs_properties_add_list(props, OBS_PROPS_DSP,
					  obs_module_text("DSP"),
					  OBS_COMBO_TYPE_LIST,
					  OBS_COMBO_FORMAT_STRING);

	obs_property_list_add_string(devices, obs_module_text("Default"),
				     OSS_DSP_DEFAULT);
	obs_property_list_add_string(devices, obs_module_text("Custom"),
				     OBS_PATH_DSP_CUSTOM);
	obs_property_set_modified_callback(devices, oss_on_devices_changed);

	obs_properties_add_text(props, OBS_PROPS_CUSTOM_DSP,
				obs_module_text("CustomDSPPath"),
				OBS_TEXT_DEFAULT);

	rate = obs_properties_add_list(props, OBS_PROPS_RATE,
				       obs_module_text("SampleRate"),
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_INT);
	channels = obs_properties_add_list(props, OBS_PROPS_CHANNELS,
					   obs_module_text("Channels"),
					   OBS_COMBO_TYPE_LIST,
					   OBS_COMBO_FORMAT_INT);
	oss_fill_device_info(rate, channels, OSS_DSP_DEFAULT);

	sample_fmt = obs_properties_add_list(props, OBS_PROPS_SAMPLE_FMT,
					     obs_module_text("SampleFormat"),
					     OBS_COMBO_TYPE_LIST,
					     OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(sample_fmt, "pcm8", AFMT_U8);
	obs_property_list_add_int(sample_fmt, "pcm16le", AFMT_S16_LE);
	obs_property_list_add_int(sample_fmt, "pcm32le", AFMT_S32_LE);

	oss_prop_add_devices(devices);

	return props;
}

struct obs_source_info oss_input_capture = {
	.id = "oss_input_capture",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = oss_getname,
	.create = oss_create,
	.destroy = oss_destroy,
	.update = oss_update,
	.get_defaults = oss_defaults,
	.get_properties = oss_properties,
	.icon_type = OBS_ICON_TYPE_AUDIO_INPUT,
};
