#include <stdlib.h>
#include <util/threading.h>
#include <util/platform.h>
#include <obs.h>

struct random_tex {
	obs_source_t *source;
	os_event_t *stop_signal;
	pthread_t thread;
	bool initialized;
};

static const char *random_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "20x20 Random Pixel Texture Source (Test)";
}

static void random_destroy(void *data)
{
	struct random_tex *rt = data;

	if (rt) {
		if (rt->initialized) {
			os_event_signal(rt->stop_signal);
			pthread_join(rt->thread, NULL);
		}

		os_event_destroy(rt->stop_signal);
		bfree(rt);
	}
}

static inline void fill_texture(uint32_t *pixels)
{
	size_t x, y;

	for (y = 0; y < 20; y++) {
		for (x = 0; x < 20; x++) {
			uint32_t pixel = 0;
			pixel |= (rand() % 256);
			pixel |= (rand() % 256) << 8;
			pixel |= (rand() % 256) << 16;
			//pixel |= (rand()%256) << 24;
			//pixel |= 0xFFFFFFFF;
			pixels[y * 20 + x] = pixel;
		}
	}
}

static void *video_thread(void *data)
{
	struct random_tex *rt = data;
	uint32_t pixels[20 * 20];
	uint64_t cur_time = os_gettime_ns();

	struct obs_source_frame frame = {
		.data = {[0] = (uint8_t *)pixels},
		.linesize = {[0] = 20 * 4},
		.width = 20,
		.height = 20,
		.format = VIDEO_FORMAT_BGRX,
	};

	while (os_event_try(rt->stop_signal) == EAGAIN) {
		fill_texture(pixels);

		frame.timestamp = cur_time;

		obs_source_output_video(rt->source, &frame);

		os_sleepto_ns(cur_time += 250000000);
	}

	return NULL;
}

static void *random_create(obs_data_t *settings, obs_source_t *source)
{
	struct random_tex *rt = bzalloc(sizeof(struct random_tex));
	rt->source = source;

	if (os_event_init(&rt->stop_signal, OS_EVENT_TYPE_MANUAL) != 0) {
		random_destroy(rt);
		return NULL;
	}

	if (pthread_create(&rt->thread, NULL, video_thread, rt) != 0) {
		random_destroy(rt);
		return NULL;
	}

	rt->initialized = true;

	UNUSED_PARAMETER(settings);
	UNUSED_PARAMETER(source);
	return rt;
}

struct obs_source_info test_random = {
	.id = "random",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO,
	.get_name = random_getname,
	.create = random_create,
	.destroy = random_destroy,
};
