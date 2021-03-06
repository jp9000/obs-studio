#include <obs-module.h>
#include <graphics/vec2.h>
#include "easings.h"

struct swipe_info {
	obs_source_t *source;

	gs_effect_t *effect;
	gs_eparam_t *a_param;
	gs_eparam_t *b_param;
	gs_eparam_t *swipe_param;

	struct vec2 dir;
	bool swipe_in;
};

#define S_DIRECTION "direction"
#define S_SWIPE_IN "swipe_in"

static const char *swipe_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("SwipeTransition");
}

static void *swipe_create(obs_data_t *settings, obs_source_t *source)
{
	struct swipe_info *swipe;
	char *file = obs_module_file("swipe_transition.effect");
	gs_effect_t *effect;

	obs_enter_graphics();
	effect = gs_effect_create_from_file(file, NULL);
	obs_leave_graphics();
	bfree(file);

	if (!effect) {
		blog(LOG_ERROR, "Could not find swipe_transition.effect");
		return NULL;
	}

	swipe = bmalloc(sizeof(*swipe));
	swipe->source = source;
	swipe->effect = effect;
	swipe->a_param = gs_effect_get_param_by_name(effect, "tex_a");
	swipe->b_param = gs_effect_get_param_by_name(effect, "tex_b");
	swipe->swipe_param = gs_effect_get_param_by_name(effect, "swipe_val");

	obs_source_update(source, settings);

	UNUSED_PARAMETER(settings);
	return swipe;
}

static void swipe_destroy(void *data)
{
	struct swipe_info *swipe = data;
	bfree(swipe);
}

static void swipe_update(void *data, obs_data_t *settings)
{
	struct swipe_info *swipe = data;
	const char *dir = obs_data_get_string(settings, S_DIRECTION);

	swipe->swipe_in = obs_data_get_bool(settings, S_SWIPE_IN);

	if (strcmp(dir, "right") == 0)
		swipe->dir = (struct vec2){-1.0f, 0.0f};
	else if (strcmp(dir, "up") == 0)
		swipe->dir = (struct vec2){0.0f, 1.0f};
	else if (strcmp(dir, "down") == 0)
		swipe->dir = (struct vec2){0.0f, -1.0f};
	else /* left */
		swipe->dir = (struct vec2){1.0f, 0.0f};
}

static void swipe_callback(void *data, gs_texture_t *a, gs_texture_t *b,
			   float t, uint32_t cx, uint32_t cy)
{
	struct swipe_info *swipe = data;
	struct vec2 swipe_val = swipe->dir;

	if (swipe->swipe_in)
		vec2_neg(&swipe_val, &swipe_val);

	t = cubic_ease_in_out(t);

	vec2_mulf(&swipe_val, &swipe_val, swipe->swipe_in ? 1.0f - t : t);

	const bool linear_srgb = gs_get_linear_srgb();

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(linear_srgb);

	gs_texture_t *t0 = swipe->swipe_in ? b : a;
	gs_texture_t *t1 = swipe->swipe_in ? a : b;
	if (linear_srgb) {
		gs_effect_set_texture_srgb(swipe->a_param, t0);
		gs_effect_set_texture_srgb(swipe->b_param, t1);
	} else {
		gs_effect_set_texture(swipe->a_param, t0);
		gs_effect_set_texture(swipe->b_param, t1);
	}
	gs_effect_set_vec2(swipe->swipe_param, &swipe_val);

	while (gs_effect_loop(swipe->effect, "Swipe"))
		gs_draw_sprite(NULL, 0, cx, cy);

	gs_enable_framebuffer_srgb(previous);
}

static void swipe_video_render(void *data, gs_effect_t *effect)
{
	struct swipe_info *swipe = data;
	obs_transition_video_render(swipe->source, swipe_callback);
	UNUSED_PARAMETER(effect);
}

static float mix_a(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return 1.0f - cubic_ease_in_out(t);
}

static float mix_b(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return cubic_ease_in_out(t);
}

static bool swipe_audio_render(void *data, uint64_t *ts_out,
			       struct obs_source_audio_mix *audio,
			       uint32_t mixers, size_t channels,
			       size_t sample_rate)
{
	struct swipe_info *swipe = data;
	return obs_transition_audio_render(swipe->source, ts_out, audio, mixers,
					   channels, sample_rate, mix_a, mix_b);
}

static obs_properties_t *swipe_properties(void *data)
{
	obs_properties_t *ppts = obs_properties_create();
	obs_property_t *p;

	p = obs_properties_add_list(ppts, S_DIRECTION,
				    obs_module_text("Direction"),
				    OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, obs_module_text("Direction.Left"),
				     "left");
	obs_property_list_add_string(p, obs_module_text("Direction.Right"),
				     "right");
	obs_property_list_add_string(p, obs_module_text("Direction.Up"), "up");
	obs_property_list_add_string(p, obs_module_text("Direction.Down"),
				     "down");

	obs_properties_add_bool(ppts, S_SWIPE_IN, obs_module_text("SwipeIn"));

	UNUSED_PARAMETER(data);
	return ppts;
}

struct obs_source_info swipe_transition = {
	.id = "swipe_transition",
	.type = OBS_SOURCE_TYPE_TRANSITION,
	.get_name = swipe_get_name,
	.create = swipe_create,
	.destroy = swipe_destroy,
	.update = swipe_update,
	.video_render = swipe_video_render,
	.audio_render = swipe_audio_render,
	.get_properties = swipe_properties,
};
