#include <obs-module.h>

struct fade_info {
	obs_source_t *source;

	gs_effect_t *effect;
	gs_eparam_t *a_param;
	gs_eparam_t *b_param;
	gs_eparam_t *fade_param;
};

static const char *fade_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("FadeTransition");
}

static void *fade_create(obs_data_t *settings, obs_source_t *source)
{
	struct fade_info *fade;
	char *file = obs_module_file("fade_transition.effect");
	gs_effect_t *effect;

	obs_enter_graphics();
	effect = gs_effect_create_from_file(file, NULL);
	obs_leave_graphics();
	bfree(file);

	if (!effect) {
		blog(LOG_ERROR, "Could not find fade_transition.effect");
		return NULL;
	}

	fade = bmalloc(sizeof(*fade));
	fade->source = source;
	fade->effect = effect;
	fade->a_param = gs_effect_get_param_by_name(effect, "tex_a");
	fade->b_param = gs_effect_get_param_by_name(effect, "tex_b");
	fade->fade_param = gs_effect_get_param_by_name(effect, "fade_val");

	UNUSED_PARAMETER(settings);
	return fade;
}

static void fade_destroy(void *data)
{
	struct fade_info *fade = data;
	bfree(fade);
}

static void fade_callback(void *data, gs_texture_t *a, gs_texture_t *b, float t,
			  uint32_t cx, uint32_t cy)
{
	struct fade_info *fade = data;

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);

	gs_effect_set_texture_srgb(fade->a_param, a);
	gs_effect_set_texture_srgb(fade->b_param, b);
	gs_effect_set_float(fade->fade_param, t);

	while (gs_effect_loop(fade->effect, "Fade"))
		gs_draw_sprite(NULL, 0, cx, cy);

	gs_enable_framebuffer_srgb(previous);
}

static void fade_video_render(void *data, gs_effect_t *effect)
{
	struct fade_info *fade = data;
	obs_transition_video_render(fade->source, fade_callback);
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

static bool fade_audio_render(void *data, uint64_t *ts_out,
			      struct obs_source_audio_mix *audio,
			      uint32_t mixers, size_t channels,
			      size_t sample_rate)
{
	struct fade_info *fade = data;
	return obs_transition_audio_render(fade->source, ts_out, audio, mixers,
					   channels, sample_rate, mix_a, mix_b);
}

struct obs_source_info fade_transition = {
	.id = "fade_transition",
	.type = OBS_SOURCE_TYPE_TRANSITION,
	.get_name = fade_get_name,
	.create = fade_create,
	.destroy = fade_destroy,
	.video_render = fade_video_render,
	.audio_render = fade_audio_render,
};
