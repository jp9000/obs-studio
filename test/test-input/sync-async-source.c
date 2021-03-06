#include <stdlib.h>
#include <util/threading.h>
#include <util/platform.h>
#include <obs.h>

struct async_sync_test {
	obs_source_t *source;
	os_event_t *stop_signal;
	pthread_t thread;
	bool initialized;
};

/* middle C */
static const double rate = 261.63 / 48000.0;

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif

#define M_PI_X2 M_PI * 2

static const char *ast_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Sync Test (Async Video/Audio Source)";
}

static void ast_destroy(void *data)
{
	struct async_sync_test *ast = data;

	if (ast->initialized) {
		os_event_signal(ast->stop_signal);
		pthread_join(ast->thread, NULL);
	}

	os_event_destroy(ast->stop_signal);
	bfree(ast);
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
	struct async_sync_test *ast = data;

	uint32_t sample_rate = audio_output_get_sample_rate(obs_get_audio());

	uint32_t *pixels = bmalloc(20 * 20 * sizeof(uint32_t));
	float *samples = bmalloc(sample_rate * sizeof(float));
	uint64_t cur_time = os_gettime_ns();
	bool whitelist = false;
	double cos_val = 0.0;
	uint64_t start_time = cur_time;

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
		.frames = sample_rate,
		.format = AUDIO_FORMAT_FLOAT,
	};

	while (os_event_try(ast->stop_signal) == EAGAIN) {
		fill_texture(pixels, whitelist ? 0xFFFFFFFF : 0xFF000000);

		frame.timestamp = cur_time - start_time;
		audio.timestamp = cur_time - start_time;

		if (whitelist) {
			for (size_t i = 0; i < sample_rate; i++) {
				cos_val += rate * M_PI_X2;
				if (cos_val > M_PI_X2)
					cos_val -= M_PI_X2;

				samples[i] = (float)(cos(cos_val) * 0.5);
			}
		} else {
			for (size_t i = 0; i < sample_rate; i++)
				samples[i] = 0.0f;
		}

		obs_source_output_video(ast->source, &frame);
		obs_source_output_audio(ast->source, &audio);

		os_sleepto_ns(cur_time += 1000000000);

		whitelist = !whitelist;
	}

	bfree(pixels);
	bfree(samples);

	return NULL;
}

static void *ast_create(obs_data_t *settings, obs_source_t *source)
{
	struct async_sync_test *ast = bzalloc(sizeof(struct async_sync_test));
	ast->source = source;

	if (os_event_init(&ast->stop_signal, OS_EVENT_TYPE_MANUAL) != 0) {
		ast_destroy(ast);
		return NULL;
	}

	if (pthread_create(&ast->thread, NULL, video_thread, ast) != 0) {
		ast_destroy(ast);
		return NULL;
	}

	ast->initialized = true;

	UNUSED_PARAMETER(settings);
	UNUSED_PARAMETER(source);
	return ast;
}

struct obs_source_info async_sync_test = {
	.id = "async_sync_test",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO,
	.get_name = ast_getname,
	.create = ast_create,
	.destroy = ast_destroy,
};
