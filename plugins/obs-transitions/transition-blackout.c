#include <obs-module.h>

#define S_BLK_SHAPE             "shape"
#define S_BLK_SHADOW            "shadow"

#define T_BLK_SHAPE             obs_module_text("Blackout.ShapeList")
#define T_BLK_SHADOW            obs_module_text("Blackout.ShadowPercentage")
#define T_BLK_CIRCLE            obs_module_text("Blackout.Circle")
#define T_BLK_RECTANGLE		obs_module_text("Blackout.Rectangle")

struct blackout_info {
	obs_source_t *source;

	gs_effect_t *effect;
	gs_eparam_t *a_param;
	gs_eparam_t *b_param;
	gs_eparam_t *time;
	gs_eparam_t *shape_param;
	gs_eparam_t *shadow_param;

	int shape;
	float shadow;
};

static const char *blackout_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("BlackoutTransition");
}

static void blackout_update(void *data, obs_data_t *settings)
{
	struct blackout_info *blk = data;

	blk->shape = obs_data_get_int(settings, S_BLK_SHAPE);
	blk->shadow = obs_data_get_int(settings, S_BLK_SHADOW) / 100.f;
}

static void *blackout_create(obs_data_t *settings, obs_source_t *source)
{
	struct blackout_info *blackout;
	char *file = obs_module_file("blackout_transition.effect");
	gs_effect_t *effect;

	obs_enter_graphics();
	effect = gs_effect_create_from_file(file, NULL);
	obs_leave_graphics();
	bfree(file);

	if (!effect) {
		blog(LOG_ERROR, "Could not find blackout_transition.effect");
		return NULL;
	}

	blackout = bmalloc(sizeof(*blackout));
	blackout->source = source;
	blackout->effect = effect;
	blackout->a_param = gs_effect_get_param_by_name(effect, "tex_a");
	blackout->b_param = gs_effect_get_param_by_name(effect, "tex_b");
	blackout->time = gs_effect_get_param_by_name(effect, "time");
	blackout->shape_param = gs_effect_get_param_by_name(effect, "shape");
	blackout->shadow_param = gs_effect_get_param_by_name(effect, "shadow");

	blackout_update(blackout, settings);

	return blackout;
}

static void blackout_destroy(void *data)
{
	struct blackout_info *blackout = data;
	bfree(blackout);
}

static void blackout_callback(void *data, gs_texture_t *a, gs_texture_t *b, float t,
			  uint32_t cx, uint32_t cy)
{
	struct blackout_info *blackout = data;

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);

	gs_effect_set_texture(blackout->a_param, a);
	gs_effect_set_texture(blackout->b_param, b);
	gs_effect_set_float(blackout->time, t);
	gs_effect_set_int(blackout->shape_param, blackout->shape);
	gs_effect_set_float(blackout->shadow_param, blackout->shadow);

	while (gs_effect_loop(blackout->effect, "blackout")) {
		gs_draw_sprite(NULL, 0, cx, cy);
	}

	gs_enable_framebuffer_srgb(previous);
}

static void blackout_video_render(void *data, gs_effect_t *effect)
{
	struct blackout_info *blackout = data;
	obs_transition_video_render(blackout->source, blackout_callback);
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

static bool blackout_audio_render(void *data, uint64_t *ts_out,
			      struct obs_source_audio_mix *audio,
			      uint32_t mixers, size_t channels,
			      size_t sample_rate)
{
	struct blackout_info *blackout = data;
	return obs_transition_audio_render(blackout->source, ts_out, audio, mixers,
					   channels, sample_rate, mix_a, mix_b);
}

static obs_properties_t* blackout_properties(void* data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_t* shapeList = obs_properties_add_list(
		props, S_BLK_SHAPE, T_BLK_SHAPE,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(shapeList, T_BLK_CIRCLE, 1);
	obs_property_list_add_int(shapeList, T_BLK_RECTANGLE, 2);

	obs_properties_add_int_slider(props, S_BLK_SHADOW, T_BLK_SHADOW,
		0, 75, 5);

	UNUSED_PARAMETER(data);
	return props;
}

static void blackout_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, S_BLK_SHAPE, 0);
	obs_data_set_default_int(settings, S_BLK_SHADOW, 30);
}

struct obs_source_info blackout_transition = {
	.id = "blackout_transition",
	.type = OBS_SOURCE_TYPE_TRANSITION,
	.get_name = blackout_get_name,
	.create = blackout_create,
	.destroy = blackout_destroy,
	.video_render = blackout_video_render,
	.audio_render = blackout_audio_render,
	.get_properties = blackout_properties,
	.update = blackout_update,
	.get_defaults = blackout_defaults
};
