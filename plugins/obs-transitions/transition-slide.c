#include <obs-module.h>
#include <graphics/vec2.h>
#include "easings.h"

#define S_DIRECTION "direction"

struct slide_info {
	obs_source_t *source;

	gs_effect_t *effect;
	gs_eparam_t *a_param;
	gs_eparam_t *b_param;
	gs_eparam_t *tex_a_dir_param;
	gs_eparam_t *tex_b_dir_param;

	struct vec2 dir;
	bool slide_in;
};

static const char *slide_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("SlideTransition");
}

static void slide_update(void *data, obs_data_t *settings)
{
	struct slide_info *slide = data;
	const char *dir = obs_data_get_string(settings, S_DIRECTION);

	if (strcmp(dir, "right") == 0)
		slide->dir = (struct vec2){-1.0f, 0.0f};
	else if (strcmp(dir, "up") == 0)
		slide->dir = (struct vec2){0.0f, 1.0f};
	else if (strcmp(dir, "down") == 0)
		slide->dir = (struct vec2){0.0f, -1.0f};
	else /* left */
		slide->dir = (struct vec2){1.0f, 0.0f};
}

void *slide_create(obs_data_t *settings, obs_source_t *source)
{
	struct slide_info *slide;
	gs_effect_t *effect;

	char *file = obs_module_file("slide_transition.effect");

	obs_enter_graphics();
	effect = gs_effect_create_from_file(file, NULL);
	obs_leave_graphics();

	bfree(file);

	if (!effect) {
		blog(LOG_ERROR, "Could not find slide_transition.effect");
		return NULL;
	}

	slide = bzalloc(sizeof(*slide));

	slide->source = source;
	slide->effect = effect;

	slide->a_param = gs_effect_get_param_by_name(effect, "tex_a");
	slide->b_param = gs_effect_get_param_by_name(effect, "tex_b");

	slide->tex_a_dir_param =
		gs_effect_get_param_by_name(effect, "tex_a_dir");
	slide->tex_b_dir_param =
		gs_effect_get_param_by_name(effect, "tex_b_dir");

	obs_source_update(source, settings);

	return slide;
}

void slide_destroy(void *data)
{
	struct slide_info *slide = data;
	bfree(slide);
}

static void slide_callback(void *data, gs_texture_t *a, gs_texture_t *b,
			   float t, uint32_t cx, uint32_t cy)
{
	struct slide_info *slide = data;

	struct vec2 tex_a_dir = slide->dir;
	struct vec2 tex_b_dir = slide->dir;

	t = cubic_ease_in_out(t);

	vec2_mulf(&tex_a_dir, &tex_a_dir, t);
	vec2_mulf(&tex_b_dir, &tex_b_dir, 1.0f - t);

	const bool linear_srgb = gs_get_linear_srgb();

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(linear_srgb);

	if (linear_srgb) {
		gs_effect_set_texture_srgb(slide->a_param, a);
		gs_effect_set_texture_srgb(slide->b_param, b);
	} else {
		gs_effect_set_texture(slide->a_param, a);
		gs_effect_set_texture(slide->b_param, b);
	}

	gs_effect_set_vec2(slide->tex_a_dir_param, &tex_a_dir);
	gs_effect_set_vec2(slide->tex_b_dir_param, &tex_b_dir);

	while (gs_effect_loop(slide->effect, "Slide"))
		gs_draw_sprite(NULL, 0, cx, cy);

	gs_enable_framebuffer_srgb(previous);
}

void slide_video_render(void *data, gs_effect_t *effect)
{
	struct slide_info *slide = data;
	obs_transition_video_render(slide->source, slide_callback);
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

bool slide_audio_render(void *data, uint64_t *ts_out,
			struct obs_source_audio_mix *audio, uint32_t mixers,
			size_t channels, size_t sample_rate)
{
	struct slide_info *slide = data;
	return obs_transition_audio_render(slide->source, ts_out, audio, mixers,
					   channels, sample_rate, mix_a, mix_b);
}

static obs_properties_t *slide_properties(void *data)
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

	UNUSED_PARAMETER(data);
	return ppts;
}

struct obs_source_info slide_transition = {
	.id = "slide_transition",
	.type = OBS_SOURCE_TYPE_TRANSITION,
	.get_name = slide_get_name,
	.create = slide_create,
	.destroy = slide_destroy,
	.update = slide_update,
	.video_render = slide_video_render,
	.audio_render = slide_audio_render,
	.get_properties = slide_properties,
};
