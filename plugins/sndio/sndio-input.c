/*
Copyright (C) 2020 by Vadim Zhukov <zhuk@openbsd.org>

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

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <sndio.h>
#include <string.h>
#include <unistd.h>

#include <util/bmem.h>
#include <util/platform.h>
#include <util/threading.h>
#include <util/util_uint64.h>
#include <obs.h>
#include <obs-module.h>

#include "sndio-input.h"

#define blog(level, msg, ...) \
	blog(level, "sndio-input: %s: " msg, __func__, ##__VA_ARGS__);

#define berr(level, msg, ...)                                   \
	do {                                                    \
		const char *errstr = strerror(errno);           \
		blog(level, msg ": %s", ##__VA_ARGS__, errstr); \
	} while (0)

#define NSEC_PER_SEC 1000000000LL

/**
 * Returns the name of the plugin
 */
static const char *sndio_input_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("SndioInput");
}

static enum speaker_layout sndio_channels_to_obs_speakers(int channels)
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

static enum audio_format sndio_to_obs_audio_format(const struct sio_par *par)
{
	switch (par->bits) {
	case 8:
		return AUDIO_FORMAT_U8BIT;
	case 16:
		return AUDIO_FORMAT_16BIT;
	case 32:
		return AUDIO_FORMAT_32BIT;
	}
	return AUDIO_FORMAT_UNKNOWN;
}

/**
 * Runs sndio tasks on a pre-opened audio device.
 * Whenever user chooses another device, this thread gets signalled to exit,
 * and another one starts immediately, without waiting for the previous.
 */
static void *sndio_thread(void *attr)
{
	struct sndio_thr_data *thrdata = attr;
	size_t msgread = 0; // msg bytes read from socket
	size_t nsiofds = sio_nfds(thrdata->hdl);
	struct pollfd pfd[1 + nsiofds];
	struct sio_par par;
	uint64_t ts;
	ssize_t nread;
	int pollres;
	uint8_t *buf;
	size_t bufsz;

	ts = os_gettime_ns();

	bufsz = thrdata->par.appbufsz * thrdata->par.bps * 2;
	if ((buf = bmalloc(bufsz * 2)) == NULL) {
		blog(LOG_ERROR, "could not allocate record buffer of %zu bytes",
		     bufsz);
		goto finish;
	}

	memset(pfd, 0, sizeof(pfd));
	pfd[0].fd = thrdata->sock;

	for (;;) {
		for (size_t i = 0; i <= nsiofds; i++)
			pfd[i].revents = 0;
		pfd[0].events = POLLIN;
		sio_pollfd(thrdata->hdl, pfd + 1, POLLIN);

		if (poll(pfd, 1 + nsiofds, /*INFTIM*/ -1) == -1) {
			if (errno == EINTR)
				continue;
			berr(LOG_ERROR, "exiting due to poll error");
			goto finish;
		}

		if ((pfd[0].revents & POLLHUP) == POLLHUP) {
			blog(LOG_INFO,
			     "exiting upon receiving EOF at IPC socket");
			goto finish;
		}
		if ((pfd[0].revents & POLLIN) == POLLIN) {
			nread = read(pfd[0].fd, ((uint8_t *)&par) + msgread,
				     sizeof(par) - msgread);
			switch (nread) {
			case -1:
				if (errno == EAGAIN)
					goto proceed_sio;
				berr(LOG_ERROR,
				     "reading from IPC socket failed");
				goto finish;

			case 0:
				blog(LOG_INFO,
				     "exiting upon receiving EOF at IPC socket");
				goto finish;

			default:
				msgread += (size_t)nread;
				if (msgread == sizeof(struct sio_par)) {
					size_t tbufsz;
					uint8_t *tbuf;

					msgread = 0;
					sio_stop(thrdata->hdl);
					if (!sio_setpar(thrdata->hdl, &par)) {
						blog(LOG_WARNING,
						     "sio_setpar failed, keeping old params");
					}
					blog(LOG_INFO,
					     "after sio_setpar(): appbufsz=%u bps=%u",
					     par.appbufsz, par.bps);
					memcpy(&thrdata->par, &par,
					       sizeof(struct sio_par));

					tbufsz = thrdata->par.appbufsz *
						 thrdata->par.bps * 2;
					if ((tbuf = brealloc(buf, tbufsz)) ==
					    NULL) {
						blog(LOG_ERROR,
						     "could not reallocate record buffer of %zu bytes",
						     tbufsz);
						goto finish;
					}
					buf = tbuf;
					bufsz = tbufsz;

					if (!sio_start(thrdata->hdl)) {
						blog(LOG_ERROR,
						     "sio_start failed, exiting");
						goto finish;
					}
					ts = os_gettime_ns();
					// Since we restarted recording,
					// do not try to handle events we lost.
					continue;
				}
			}
		}

	proceed_sio:
		pollres = sio_revents(thrdata->hdl, pfd + 1);
		if ((pollres & POLLHUP) == POLLHUP) {
			blog(LOG_ERROR, "sndio device error happened, exiting");
			goto finish;
		}
		if ((pollres & POLLIN) == POLLIN) {
			struct obs_source_audio out;
			unsigned int nframes;

			nread = (ssize_t)sio_read(thrdata->hdl, buf, bufsz);
			if (nread == 0) {
				if (sio_eof(thrdata->hdl)) {
					blog(LOG_ERROR,
					     "sndio device EOF happened, exiting");
					goto finish;
				}
				continue;
			}
			nframes = (unsigned int)nread / thrdata->par.bps;
			//blog(LOG_INFO, "sio_read returned %u, nframes = %u", nread, nframes);

			memset(&out, 0, sizeof(struct obs_source_audio));
			out.data[0] = buf;
			out.frames = nframes;
			out.format = sndio_to_obs_audio_format(&thrdata->par);
			out.speakers = sndio_channels_to_obs_speakers(
				thrdata->par.rchan);
			out.samples_per_sec = thrdata->par.rate;
			out.timestamp = ts;

			ts += util_mul_div64(nframes, NSEC_PER_SEC,
					     thrdata->par.rate);

			obs_source_output_audio(thrdata->source, &out);
		}
	}

finish:
	sio_close(thrdata->hdl);
	close(thrdata->sock);
	bfree(buf);
	bfree(thrdata);
	return NULL;
}

/**
 * Destroy the plugin object and free all memory
 */
static void sndio_destroy(void *vptr)
{
	struct sndio_data *data = vptr;

	if (!data)
		return;
	close(data->sock);
	data->sock = -1;
	pthread_attr_destroy(&data->attr);
	bfree(data);
}

/**
 * Tries to apply the input settings.
 * Must be called on stopped device.
 */
static void sndio_apply(struct sndio_data *data, obs_data_t *settings)
{
	struct sndio_thr_data *thrdata;
	pthread_t thread;
	int ec;
	int socks[2] = {-1, -1};
	const char *devname = obs_data_get_string(settings, "device");

	if ((thrdata = bzalloc(sizeof(struct sndio_thr_data))) == NULL) {
		blog(LOG_ERROR, "malloc");
		return;
	}
	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0,
		       socks) == -1) {
		berr(LOG_ERROR, "socketpair");
		goto error;
	}

	if (data->sock != -1)
		close(data->sock);
	data->sock = socks[0];
	thrdata->sock = socks[1];
	thrdata->source = data->source;

	thrdata->hdl = sio_open(devname, SIO_REC, 0);
	if (!thrdata->hdl) {
		berr(LOG_ERROR, "could not open %s sndio device", devname);
		goto error;
	}

	sio_initpar(&thrdata->par);
	thrdata->par.bits = obs_data_get_int(settings, "bits");
	thrdata->par.bps = SIO_BPS(thrdata->par.bits);
	thrdata->par.sig = thrdata->par.bits > 8;
	thrdata->par.le = 1;
	thrdata->par.rate = obs_data_get_int(settings, "rate");
	thrdata->par.rchan = obs_data_get_int(settings, "channels");
	thrdata->par.xrun = SIO_SYNC; // makes timestamping easy
	if (!sio_setpar(thrdata->hdl, &thrdata->par)) {
		berr(LOG_ERROR, "could not set parameters for %s sndio device",
		     devname);
		goto error;
	}
	blog(LOG_INFO, "after initial sio_setpar(): appbufsz=%u bps=%u",
	     thrdata->par.appbufsz, thrdata->par.bps);

	if (!sio_start(thrdata->hdl)) {
		berr(LOG_ERROR, "could not start recording on %s sndio device",
		     devname);
		goto error;
	}

	ec = pthread_create(&thread, &data->attr, sndio_thread, thrdata);
	if (ec != 0) {
		blog(LOG_ERROR, "could not start thread");
		goto error;
	}

	return;

error:
	if (thrdata->hdl != NULL)
		sio_close(thrdata->hdl);
	close(socks[0]);
	close(socks[1]);
	bfree(thrdata);
}

/**
 * Update the input settings.
 */
static void sndio_update(void *vptr, obs_data_t *settings)
{
	struct sndio_data *data = vptr;
	sndio_apply(data, settings);
}

/**
 * Create the plugin object
 */
static void *sndio_create(obs_data_t *settings, obs_source_t *source)
{
	struct sndio_data *data = bzalloc(sizeof(struct sndio_data));
	pthread_attr_init(&data->attr);
	pthread_attr_setdetachstate(&data->attr, PTHREAD_CREATE_DETACHED);
	data->source = source;
	data->sock = -1;
	sndio_apply(data, settings);
	return data;
}

/**
 * Get plugin defaults
 */
static void sndio_input_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "device", SIO_DEVANY);
	obs_data_set_default_int(settings, "rate", 48000);
	obs_data_set_default_int(settings, "bits", 16);
	obs_data_set_default_int(settings, "channels", 2);
}

/**
 * Get plugin properties
 */
static obs_properties_t *sndio_input_properties(void *unused)
{
	obs_property_t *rate, *bits;

	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_text(props, "device", obs_module_text("Device"),
				OBS_TEXT_DEFAULT);

	rate = obs_properties_add_list(props, "rate", obs_module_text("Rate"),
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(rate, "11025 Hz", 11025);
	obs_property_list_add_int(rate, "22050 Hz", 22050);
	obs_property_list_add_int(rate, "32000 Hz", 32000);
	obs_property_list_add_int(rate, "44100 Hz", 44100);
	obs_property_list_add_int(rate, "48000 Hz", 48000);
	obs_property_list_add_int(rate, "96000 Hz", 96000);
	obs_property_list_add_int(rate, "192000 Hz", 192000);

	bits = obs_properties_add_list(props, "bits",
				       obs_module_text("BitsPerSample"),
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(bits, "8", 8);
	obs_property_list_add_int(bits, "16", 16);
	obs_property_list_add_int(bits, "32", 32);

	obs_properties_add_int(props, "channels", obs_module_text("Channels"),
			       1, 8, 1);

	return props;
}

struct obs_source_info sndio_output_capture = {
	.id = "sndio_output_capture",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = sndio_input_getname,
	.create = sndio_create,
	.destroy = sndio_destroy,
#if SHUTDOWN_ON_DEACTIVATE
	.activate = sndio_activate,
	.deactivate = sndio_deactivate,
#endif
	.update = sndio_update,
	.get_defaults = sndio_input_defaults,
	.get_properties = sndio_input_properties,
	.icon_type = OBS_ICON_TYPE_AUDIO_OUTPUT,
};
