#include <stdlib.h>
#include <util/threading.h>
#include <util/platform.h>
#include <graphics/graphics.h>
#include <obs.h>

struct sync_pair_vid {
	obs_source_t *source;
	gs_texture_t *tex;
	gs_texture_t *white;
	gs_texture_t *black;
};

uint64_t starting_time = 0;
uint64_t last_frame = 0;

static const char *sync_pair_vid_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Sync Test Pair (Video)";
}

static void sync_pair_vid_destroy(void *data)
{
	struct sync_pair_vid *spv = data;

	obs_enter_graphics();
	gs_texture_destroy(spv->tex);
	gs_texture_destroy(spv->white);
	gs_texture_destroy(spv->black);
	obs_leave_graphics();

	bfree(spv);
}

static inline void fill_texture(uint32_t *pixels, uint32_t pixel)
{
	size_t x, y;

	for (y = 0; y < 32; y++) {
		for (x = 0; x < 32; x++) {
			pixels[y * 32 + x] = pixel;
		}
	}
}

static void *sync_pair_vid_create(obs_data_t *settings, obs_source_t *source)
{
	struct sync_pair_vid *spv = bzalloc(sizeof(struct sync_pair_vid));
	spv->source = source;

	obs_enter_graphics();
	spv->tex = gs_texture_create(32, 32, GS_RGBA, 1, NULL, GS_DYNAMIC);
	spv->white = gs_texture_create(32, 32, GS_RGBA, 1, NULL, GS_DYNAMIC);
	spv->black = gs_texture_create(32, 32, GS_RGBA, 1, NULL, GS_DYNAMIC);

	uint8_t *ptr;
	uint32_t linesize;
	if (gs_texture_map(spv->white, &ptr, &linesize)) {
		fill_texture((uint32_t *)ptr, 0xFFFFFFFF);
		gs_texture_unmap(spv->white);
	}
	if (gs_texture_map(spv->black, &ptr, &linesize)) {
		fill_texture((uint32_t *)ptr, 0xFF000000);
		gs_texture_unmap(spv->black);
	}

	obs_leave_graphics();

	return spv;
}

static inline bool whitelist_time(uint64_t ts, uint64_t interval,
				  uint64_t fps_num, uint64_t fps_den)
{
	if (!starting_time)
		return false;

	uint64_t count = (ts - starting_time) / interval;
	uint64_t sec = count * fps_den / fps_num;
	return sec % 2 == 1;
}

static void sync_pair_vid_render(void *data, gs_effect_t *effect)
{
	struct sync_pair_vid *spv = data;

	uint64_t ts = obs_get_video_frame_time();
	if (!starting_time)
		starting_time = ts;

	uint64_t interval = video_output_get_frame_time(obs_get_video());
	const struct video_output_info *voi =
		video_output_get_info(obs_get_video());
	uint64_t fps_num = voi->fps_num;
	uint64_t fps_den = voi->fps_den;

	bool whitelist = whitelist_time(ts, interval, fps_num, fps_den);

#if 0
	if (last_frame != ts) {
		uint8_t *ptr;
		uint32_t linesize;
		if (gs_texture_map(spv->tex, &ptr, &linesize)) {
			fill_texture((uint32_t*)ptr, whitelist ? 0xFFFFFFFF : 0xFF000000);
			gs_texture_unmap(spv->tex);
		}
		last_frame = ts;
	}

	obs_source_draw(spv->tex, 0, 0, 0, 0, 0);
#else
	obs_source_draw(whitelist ? spv->white : spv->black, 0, 0, 0, 0, 0);
#endif
}

static uint32_t sync_pair_vid_size(void *data)
{
	return 32;
}

struct obs_source_info sync_video = {
	.id = "sync_video",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = sync_pair_vid_getname,
	.create = sync_pair_vid_create,
	.destroy = sync_pair_vid_destroy,
	.video_render = sync_pair_vid_render,
	.get_width = sync_pair_vid_size,
	.get_height = sync_pair_vid_size,
};
