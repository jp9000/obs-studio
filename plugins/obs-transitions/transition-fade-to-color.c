#include <obs-module.h>

#define S_COLOR "color"
#define S_SWITCH_POINT "switch_point"

#define S_COLOR_TEXT obs_module_text("Color")
#define S_SWITCH_POINT_TEXT obs_module_text("SwitchPoint")

struct fade_to_color_info {
	obs_source_t *source;

	gs_effect_t *effect;
	gs_eparam_t *ep_tex;
	gs_eparam_t *ep_swp;
	gs_eparam_t *ep_color;

	struct vec4 color;
	float switch_point;
};

static inline float lerp(float a, float b, float x)
{
	return (1.0f - x) * a + x * b;
}

static inline float clamp(float x, float min, float max)
{
	if (x < min)
		return min;
	else if (x > max)
		return max;
	return x;
}

static inline float smoothstep(float min, float max, float x)
{
	x = clamp((x - min) / (max - min), 0.0f, 1.0f);
	return x * x * (3 - 2 * x);
}

static const char *fade_to_color_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("FadeToColorTransition");
}

static void fade_to_color_update(void *data, obs_data_t *settings)
{
	struct fade_to_color_info *fade_to_color = data;
	uint32_t color = (uint32_t)obs_data_get_int(settings, S_COLOR);
	uint32_t swp = (uint32_t)obs_data_get_int(settings, S_SWITCH_POINT);

	color |= 0xFF000000;

	vec4_from_rgba_srgb_premultiply(&fade_to_color->color, color);

	fade_to_color->switch_point = (float)swp / 100.0f;
}

static void *fade_to_color_create(obs_data_t *settings, obs_source_t *source)
{
	struct fade_to_color_info *fade_to_color;
	char *file = obs_module_file("fade_to_color_transition.effect");
	gs_effect_t *effect;

	obs_enter_graphics();
	effect = gs_effect_create_from_file(file, NULL);
	obs_leave_graphics();

	bfree(file);

	if (!effect) {
		blog(LOG_ERROR,
		     "Could not find fade_to_color_transition.effect");
		return NULL;
	}

	fade_to_color = bzalloc(sizeof(struct fade_to_color_info));

	fade_to_color->source = source;
	fade_to_color->effect = effect;

	fade_to_color->ep_tex = gs_effect_get_param_by_name(effect, "tex");
	fade_to_color->ep_swp = gs_effect_get_param_by_name(effect, "swp");
	fade_to_color->ep_color = gs_effect_get_param_by_name(effect, "color");

	obs_source_update(source, settings);

	return fade_to_color;
}

static void fade_to_color_destroy(void *data)
{
	struct fade_to_color_info *fade_to_color = data;
	bfree(fade_to_color);
}

static void fade_to_color_callback(void *data, gs_texture_t *a, gs_texture_t *b,
				   float t, uint32_t cx, uint32_t cy)
{
	struct fade_to_color_info *fade_to_color = data;

	float sa = smoothstep(0.0f, fade_to_color->switch_point, t);
	float sb = smoothstep(fade_to_color->switch_point, 1.0f, t);

	float swp = t < fade_to_color->switch_point ? sa : 1.0f - sb;

	gs_texture_t *const tex = (t < fade_to_color->switch_point) ? a : b;

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);

	gs_effect_set_texture_srgb(fade_to_color->ep_tex, tex);
	gs_effect_set_vec4(fade_to_color->ep_color, &fade_to_color->color);
	gs_effect_set_float(fade_to_color->ep_swp, swp);

	while (gs_effect_loop(fade_to_color->effect, "FadeToColor"))
		gs_draw_sprite(NULL, 0, cx, cy);

	gs_enable_framebuffer_srgb(previous);
}

static void fade_to_color_video_render(void *data, gs_effect_t *effect)
{
	struct fade_to_color_info *fade_to_color = data;
	obs_transition_video_render(fade_to_color->source,
				    fade_to_color_callback);
	UNUSED_PARAMETER(effect);
}

static float mix_a(void *data, float t)
{
	struct fade_to_color_info *fade_to_color = data;
	float sp = fade_to_color->switch_point;

	return lerp(1.0f - t, 0.0f, smoothstep(0.0f, sp, t));
}

static float mix_b(void *data, float t)
{
	struct fade_to_color_info *fade_to_color = data;
	float sp = fade_to_color->switch_point;

	return lerp(0.0f, t, smoothstep(sp, 1.0f, t));
}

static bool fade_to_color_audio_render(void *data, uint64_t *ts_out,
				       struct obs_source_audio_mix *audio,
				       uint32_t mixers, size_t channels,
				       size_t sample_rate)
{
	struct fade_to_color_info *fade_to_color = data;
	return obs_transition_audio_render(fade_to_color->source, ts_out, audio,
					   mixers, channels, sample_rate, mix_a,
					   mix_b);
}

static obs_properties_t *fade_to_color_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_color(props, S_COLOR, S_COLOR_TEXT);
	obs_property_t *p = obs_properties_add_int_slider(
		props, S_SWITCH_POINT, S_SWITCH_POINT_TEXT, 0, 100, 1);
	obs_property_int_set_suffix(p, "%");

	UNUSED_PARAMETER(data);
	return props;
}

static void fade_to_color_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, S_COLOR, 0xFF000000);
	obs_data_set_default_int(settings, S_SWITCH_POINT, 50);
}

struct obs_source_info fade_to_color_transition = {
	.id = "fade_to_color_transition",
	.type = OBS_SOURCE_TYPE_TRANSITION,
	.get_name = fade_to_color_get_name,
	.create = fade_to_color_create,
	.destroy = fade_to_color_destroy,
	.update = fade_to_color_update,
	.video_render = fade_to_color_video_render,
	.audio_render = fade_to_color_audio_render,
	.get_properties = fade_to_color_properties,
	.get_defaults = fade_to_color_defaults,
};
