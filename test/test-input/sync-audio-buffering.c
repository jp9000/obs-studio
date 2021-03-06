#include <stdlib.h>
#include <util/threading.h>
#include <util/platform.h>
#include <obs.h>

struct buffering_async_sync_test {
	obs_source_t *source;
	os_event_t *stop_signal;
	pthread_t thread;
	bool initialized;
	bool buffer_audio;
};

#define CYCLE_COUNT 7

static const double aud_rates[CYCLE_COUNT] = {
	220.00 / 48000.0, /* A */
	233.08 / 48000.0, /* A# */
	246.94 / 48000.0, /* B */
	261.63 / 48000.0, /* C */
	277.18 / 48000.0, /* C# */
	293.67 / 48000.0, /* D */
	311.13 / 48000.0, /* D# */
};

#define MAKE_COL_CHAN(x, y) (((0xFF / 7) * x) << (y * 8))
#define MAKE_GRAYSCALE(x) \
	(MAKE_COL_CHAN(x, 0) | MAKE_COL_CHAN(x, 1) | MAKE_COL_CHAN(x, 2))

static const uint32_t vid_colors[CYCLE_COUNT] = {
	0xFF000000,
	0xFF000000 + MAKE_GRAYSCALE(1),
	0xFF000000 + MAKE_GRAYSCALE(2),
	0xFF000000 + MAKE_GRAYSCALE(3),
	0xFF000000 + MAKE_GRAYSCALE(4),
	0xFF000000 + MAKE_GRAYSCALE(5),
	0xFF000000 + MAKE_GRAYSCALE(6),
};

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif

#define M_PI_X2 M_PI * 2

static const char *bast_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Audio Buffering Sync Test (Async Video/Audio Source)";
}

static void bast_destroy(void *data)
{
	struct buffering_async_sync_test *bast = data;

	if (bast->initialized) {
		os_event_signal(bast->stop_signal);
		pthread_join(bast->thread, NULL);
	}

	os_event_destroy(bast->stop_signal);
	bfree(bast);
}

static inline void fill_texture(uint32_t *pixels, uint32_t color)
{
	size_t x, y;

	for (y = 0; y < 20; y++) {
		for (x = 0; x < 20; x++) {
			pixels[y * 20 + x] = color;
		}
	}
}

static void *video_thread(void *data)
{
	struct buffering_async_sync_test *bast = data;

	uint32_t sample_rate = audio_output_get_sample_rate(obs_get_audio());

	uint32_t *pixels = bmalloc(20 * 20 * sizeof(uint32_t));
	float *samples = bmalloc(sample_rate * sizeof(float));
	uint64_t cur_time = os_gettime_ns();
	int cur_vid_pos = 0;
	int cur_aud_pos = 0;
	double cos_val = 0.0;
	uint64_t start_time = cur_time;
	bool audio_buffering_enabled = false;

	struct obs_source_frame frame = {
		.data = {[0] = (uint8_t *)pixels},
		.linesize = {[0] = 20 * 4},
		.width = 20,
		.height = 20,
		.format = VIDEO_FORMAT_BGRX,
	};
	struct obs_source_audio audio = {
		.speakers = SPEAKERS_MONO,
		.data = {[0] = (uint8_t *)samples},
		.samples_per_sec = sample_rate,
		.frames = sample_rate / 4,
		.format = AUDIO_FORMAT_FLOAT,
	};

	while (os_event_try(bast->stop_signal) == EAGAIN) {
		fill_texture(pixels, vid_colors[cur_vid_pos]);

		if (!audio_buffering_enabled && bast->buffer_audio) {
			audio_buffering_enabled = true;
			blog(LOG_DEBUG, "okay, buffering audio: now");

			/* 1 second = 4 cycles when running at
			 *            250ms per cycle */
			cur_aud_pos -= 4;
			if (cur_aud_pos < 0)
				cur_aud_pos += CYCLE_COUNT;
		}

		/* should cause approximately 750 milliseconds of audio
		 * buffering */
		frame.timestamp = cur_time - start_time;
		audio.timestamp = cur_time - start_time -
				  (audio_buffering_enabled ? 1000000000 : 0);

		const double rate = aud_rates[cur_aud_pos];

		for (size_t i = 0; i < sample_rate / 4; i++) {
			cos_val += rate * M_PI_X2;
			if (cos_val > M_PI_X2)
				cos_val -= M_PI_X2;

			samples[i] = (float)(cos(cos_val) * 0.5);
		}

		obs_source_output_video(bast->source, &frame);
		obs_source_output_audio(bast->source, &audio);

		os_sleepto_ns(cur_time += 250000000);

		if (++cur_vid_pos == CYCLE_COUNT)
			cur_vid_pos = 0;
		if (++cur_aud_pos == CYCLE_COUNT)
			cur_aud_pos = 0;
	}

	bfree(pixels);
	bfree(samples);

	return NULL;
}

static void bast_buffer_audio(void *data, obs_hotkey_id id,
			      obs_hotkey_t *hotkey, bool pressed)
{
	struct buffering_async_sync_test *bast = data;

	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (pressed)
		bast->buffer_audio = true;
}

static void *bast_create(obs_data_t *settings, obs_source_t *source)
{
	struct buffering_async_sync_test *bast = bzalloc(sizeof(*bast));
	bast->source = source;

	if (os_event_init(&bast->stop_signal, OS_EVENT_TYPE_MANUAL) != 0) {
		bast_destroy(bast);
		return NULL;
	}

	if (pthread_create(&bast->thread, NULL, video_thread, bast) != 0) {
		bast_destroy(bast);
		return NULL;
	}

	obs_hotkey_register_source(source, "AudioBufferingSyncTest.Buffer",
				   "Buffer Audio", bast_buffer_audio, bast);

	bast->initialized = true;

	UNUSED_PARAMETER(settings);
	UNUSED_PARAMETER(source);
	return bast;
}

struct obs_source_info buffering_async_sync_test = {
	.id = "buffering_async_sync_test",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO,
	.get_name = bast_getname,
	.create = bast_create,
	.destroy = bast_destroy,
};
