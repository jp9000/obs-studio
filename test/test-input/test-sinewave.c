#include <math.h>
#include <util/bmem.h>
#include <util/threading.h>
#include <util/platform.h>
#include <obs.h>

struct sinewave_data {
	bool initialized_thread;
	pthread_t thread;
	os_event_t *event;
	obs_source_t *source;
};

/* middle C */
static const double rate = 261.63 / 48000.0;

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif

#define M_PI_X2 M_PI * 2

static void *sinewave_thread(void *pdata)
{
	struct sinewave_data *swd = pdata;
	uint64_t last_time = os_gettime_ns();
	uint64_t ts = 0;
	double cos_val = 0.0;
	uint8_t bytes[480];

	while (os_event_try(swd->event) == EAGAIN) {
		if (!os_sleepto_ns(last_time += 10000000))
			last_time = os_gettime_ns();

		for (size_t i = 0; i < 480; i++) {
			cos_val += rate * M_PI_X2;
			if (cos_val > M_PI_X2)
				cos_val -= M_PI_X2;

			double wave = cos(cos_val) * 0.5;
			bytes[i] = (uint8_t)((wave + 1.0) * 0.5 * 255.0);
		}

		struct obs_source_audio data;
		data.data[0] = bytes;
		data.frames = 480;
		data.speakers = SPEAKERS_MONO;
		data.samples_per_sec = 48000;
		data.timestamp = ts;
		data.format = AUDIO_FORMAT_U8BIT;
		obs_source_output_audio(swd->source, &data);

		ts += 10000000;
	}

	return NULL;
}

/* ------------------------------------------------------------------------- */

static const char *sinewave_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Sinewave Sound Source (Test)";
}

static void sinewave_destroy(void *data)
{
	struct sinewave_data *swd = data;

	if (swd) {
		if (swd->initialized_thread) {
			void *ret;
			os_event_signal(swd->event);
			pthread_join(swd->thread, &ret);
		}

		os_event_destroy(swd->event);
		bfree(swd);
	}
}

static void *sinewave_create(obs_data_t *settings, obs_source_t *source)
{
	struct sinewave_data *swd = bzalloc(sizeof(struct sinewave_data));
	swd->source = source;

	if (os_event_init(&swd->event, OS_EVENT_TYPE_MANUAL) != 0)
		goto fail;
	if (pthread_create(&swd->thread, NULL, sinewave_thread, swd) != 0)
		goto fail;

	swd->initialized_thread = true;

	UNUSED_PARAMETER(settings);
	return swd;

fail:
	sinewave_destroy(swd);
	return NULL;
}

struct obs_source_info test_sinewave = {
	.id = "test_sinewave",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = sinewave_getname,
	.create = sinewave_create,
	.destroy = sinewave_destroy,
};
