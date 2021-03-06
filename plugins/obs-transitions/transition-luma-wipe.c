#include <obs-module.h>
#include <graphics/image-file.h>
#include <util/dstr.h>

/* clang-format off */

#define S_LUMA_IMG              "luma_image"
#define S_LUMA_INV              "luma_invert"
#define S_LUMA_SOFT             "luma_softness"

#define T_LUMA_IMG              obs_module_text("LumaWipe.Image")
#define T_LUMA_INV              obs_module_text("LumaWipe.Invert")
#define T_LUMA_SOFT             obs_module_text("LumaWipe.Softness")

/* clang-format on */

struct luma_wipe_info {
	obs_source_t *source;

	gs_effect_t *effect;
	gs_eparam_t *ep_a_tex;
	gs_eparam_t *ep_b_tex;
	gs_eparam_t *ep_l_tex;
	gs_eparam_t *ep_progress;
	gs_eparam_t *ep_invert;
	gs_eparam_t *ep_softness;

	gs_image_file_t luma_image;
	bool invert_luma;
	float softness;
	obs_data_t *wipes_list;
};

static const char *luma_wipe_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("LumaWipeTransition");
}

static void luma_wipe_update(void *data, obs_data_t *settings)
{
	struct luma_wipe_info *lwipe = data;

	const char *name = obs_data_get_string(settings, S_LUMA_IMG);
	lwipe->invert_luma = obs_data_get_bool(settings, S_LUMA_INV);
	lwipe->softness = (float)obs_data_get_double(settings, S_LUMA_SOFT);

	struct dstr path = {0};

	dstr_copy(&path, "luma_wipes/");
	dstr_cat(&path, name);

	char *file = obs_module_file(path.array);

	obs_enter_graphics();
	gs_image_file_free(&lwipe->luma_image);
	obs_leave_graphics();

	gs_image_file_init(&lwipe->luma_image, file);

	obs_enter_graphics();
	gs_image_file_init_texture(&lwipe->luma_image);
	obs_leave_graphics();

	bfree(file);
	dstr_free(&path);

	UNUSED_PARAMETER(settings);
}

static void luma_wipe_get_list(void *data)
{
	struct luma_wipe_info *lwipe = data;

	char *path = obs_module_file("luma_wipes/wipes.json");

	lwipe->wipes_list = obs_data_create_from_json_file(path);

	bfree(path);
}

static void *luma_wipe_create(obs_data_t *settings, obs_source_t *source)
{
	struct luma_wipe_info *lwipe;
	gs_effect_t *effect;
	char *file = obs_module_file("luma_wipe_transition.effect");

	obs_enter_graphics();
	effect = gs_effect_create_from_file(file, NULL);
	obs_leave_graphics();

	if (!effect) {
		blog(LOG_ERROR, "Could not open luma_wipe_transition.effect");
		return NULL;
	}

	bfree(file);

	lwipe = bzalloc(sizeof(*lwipe));

	lwipe->effect = effect;
	lwipe->ep_a_tex = gs_effect_get_param_by_name(effect, "a_tex");
	lwipe->ep_b_tex = gs_effect_get_param_by_name(effect, "b_tex");
	lwipe->ep_l_tex = gs_effect_get_param_by_name(effect, "l_tex");
	lwipe->ep_progress = gs_effect_get_param_by_name(effect, "progress");
	lwipe->ep_invert = gs_effect_get_param_by_name(effect, "invert");
	lwipe->ep_softness = gs_effect_get_param_by_name(effect, "softness");
	lwipe->source = source;

	luma_wipe_get_list(lwipe);

	luma_wipe_update(lwipe, settings);

	return lwipe;
}

static void luma_wipe_destroy(void *data)
{
	struct luma_wipe_info *lwipe = data;

	obs_enter_graphics();
	gs_image_file_free(&lwipe->luma_image);
	obs_leave_graphics();

	obs_data_release(lwipe->wipes_list);

	bfree(lwipe);
}

static obs_properties_t *luma_wipe_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	struct luma_wipe_info *lwipe = data;

	obs_property_t *p;

	p = obs_properties_add_list(props, S_LUMA_IMG, T_LUMA_IMG,
				    OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);

	obs_data_item_t *item = obs_data_first(lwipe->wipes_list);

	for (; item != NULL; obs_data_item_next(&item)) {
		const char *name = obs_data_item_get_name(item);
		const char *path = obs_data_item_get_string(item);
		obs_property_list_add_string(p, obs_module_text(name), path);
	}

	obs_properties_add_float(props, S_LUMA_SOFT, T_LUMA_SOFT, 0.0, 1.0,
				 0.05);
	obs_properties_add_bool(props, S_LUMA_INV, T_LUMA_INV);

	return props;
}

static void luma_wipe_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, S_LUMA_IMG, "linear-h.png");
	obs_data_set_default_double(settings, S_LUMA_SOFT, 0.03);
	obs_data_set_default_bool(settings, S_LUMA_INV, false);
}

static void luma_wipe_callback(void *data, gs_texture_t *a, gs_texture_t *b,
			       float t, uint32_t cx, uint32_t cy)
{
	struct luma_wipe_info *lwipe = data;

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);

	gs_effect_set_texture_srgb(lwipe->ep_a_tex, a);
	gs_effect_set_texture_srgb(lwipe->ep_b_tex, b);
	gs_effect_set_texture(lwipe->ep_l_tex, lwipe->luma_image.texture);
	gs_effect_set_float(lwipe->ep_progress, t);

	gs_effect_set_bool(lwipe->ep_invert, lwipe->invert_luma);
	gs_effect_set_float(lwipe->ep_softness, lwipe->softness);

	while (gs_effect_loop(lwipe->effect, "LumaWipe"))
		gs_draw_sprite(NULL, 0, cx, cy);

	gs_enable_framebuffer_srgb(previous);
}

void luma_wipe_video_render(void *data, gs_effect_t *effect)
{
	struct luma_wipe_info *lwipe = data;
	obs_transition_video_render(lwipe->source, luma_wipe_callback);
	UNUSED_PARAMETER(effect);
}

static float mix_a(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return 1.0f - t;
}

static float mix_b(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return t;
}

bool luma_wipe_audio_render(void *data, uint64_t *ts_out,
			    struct obs_source_audio_mix *audio, uint32_t mixers,
			    size_t channels, size_t sample_rate)
{
	struct luma_wipe_info *lwipe = data;
	return obs_transition_audio_render(lwipe->source, ts_out, audio, mixers,
					   channels, sample_rate, mix_a, mix_b);
}

struct obs_source_info luma_wipe_transition = {
	.id = "wipe_transition",
	.type = OBS_SOURCE_TYPE_TRANSITION,
	.get_name = luma_wipe_get_name,
	.create = luma_wipe_create,
	.destroy = luma_wipe_destroy,
	.update = luma_wipe_update,
	.video_render = luma_wipe_video_render,
	.audio_render = luma_wipe_audio_render,
	.get_properties = luma_wipe_properties,
	.get_defaults = luma_wipe_defaults,
};
