#include <math.h>
#include <util/bmem.h>
#include <util/threading.h>
#include <util/platform.h>
#include <util/util_uint64.h>
#include <obs.h>

struct sync_pair_aud {
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

extern uint64_t starting_time;

static inline bool whitelist_time(uint64_t ts, uint64_t interval,
				  uint64_t fps_num, uint64_t fps_den)
{
	if (!starting_time)
		return false;

	uint64_t count = (ts - starting_time) / interval;
	uint64_t sec = count * fps_den / fps_num;
	return sec % 2 == 1;
}

static void *sync_pair_aud_thread(void *pdata)
{
	struct sync_pair_aud *spa = pdata;
	uint32_t sample_rate = audio_output_get_sample_rate(obs_get_audio());
	uint32_t frames = sample_rate / 100;
	uint64_t last_time = obs_get_video_frame_time();
	double cos_val = 0.0;
	float *samples = malloc(frames * sizeof(float));

	uint64_t interval = video_output_get_frame_time(obs_get_video());
	const struct video_output_info *voi =
		video_output_get_info(obs_get_video());
	uint64_t fps_num = voi->fps_num;
	uint64_t fps_den = voi->fps_den;

	while (os_event_try(spa->event) == EAGAIN) {
		if (!os_sleepto_ns(last_time += 10000000))
			last_time = obs_get_video_frame_time();

		for (uint64_t i = 0; i < frames; i++) {
			uint64_t ts =
				last_time +
				util_mul_div64(i, 1000000000ULL, sample_rate);

			if (whitelist_time(ts, interval, fps_num, fps_den)) {
				cos_val += rate * M_PI_X2;
				if (cos_val > M_PI_X2)
					cos_val -= M_PI_X2;

				samples[i] = (float)(cos(cos_val) * 0.5);
			} else {
				samples[i] = 0.0f;
			}
		}

		struct obs_source_audio data;
		data.data[0] = (uint8_t *)samples;
		data.frames = frames;
		data.speakers = SPEAKERS_MONO;
		data.samples_per_sec = sample_rate;
		data.timestamp = last_time;
		data.format = AUDIO_FORMAT_FLOAT;
		obs_source_output_audio(spa->source, &data);
	}

	free(samples);

	return NULL;
}

/* ------------------------------------------------------------------------- */

static const char *sync_pair_aud_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Sync Test Pair (Audio)";
}

static void sync_pair_aud_destroy(void *data)
{
	struct sync_pair_aud *spa = data;

	if (spa) {
		if (spa->initialized_thread) {
			void *ret;
			os_event_signal(spa->event);
			pthread_join(spa->thread, &ret);
		}

		os_event_destroy(spa->event);
		bfree(spa);
	}
}

static void *sync_pair_aud_create(obs_data_t *settings, obs_source_t *source)
{
	struct sync_pair_aud *spa = bzalloc(sizeof(struct sync_pair_aud));
	spa->source = source;

	if (os_event_init(&spa->event, OS_EVENT_TYPE_MANUAL) != 0)
		goto fail;
	if (pthread_create(&spa->thread, NULL, sync_pair_aud_thread, spa) != 0)
		goto fail;

	spa->initialized_thread = true;

	UNUSED_PARAMETER(settings);
	return spa;

fail:
	sync_pair_aud_destroy(spa);
	return NULL;
}

struct obs_source_info sync_audio = {
	.id = "sync_audio",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = sync_pair_aud_getname,
	.create = sync_pair_aud_create,
	.destroy = sync_pair_aud_destroy,
};
