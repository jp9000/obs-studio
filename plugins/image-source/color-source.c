#include <obs-module.h>

struct color_source {
	struct vec4 color;
	struct vec4 color_srgb;

	uint32_t width;
	uint32_t height;

	obs_source_t *src;
};

static const char *color_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("ColorSource");
}

static void color_source_update(void *data, obs_data_t *settings)
{
	struct color_source *context = data;
	uint32_t color = (uint32_t)obs_data_get_int(settings, "color");
	uint32_t width = (uint32_t)obs_data_get_int(settings, "width");
	uint32_t height = (uint32_t)obs_data_get_int(settings, "height");

	vec4_from_rgba(&context->color, color);
	vec4_from_rgba_srgb(&context->color_srgb, color);
	context->width = width;
	context->height = height;
}

static void *color_source_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(source);

	struct color_source *context = bzalloc(sizeof(struct color_source));
	context->src = source;

	color_source_update(context, settings);

	return context;
}

static void color_source_destroy(void *data)
{
	bfree(data);
}

static obs_properties_t *color_source_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_color_alpha(props, "color",
				       obs_module_text("ColorSource.Color"));

	obs_properties_add_int(props, "width",
			       obs_module_text("ColorSource.Width"), 0, 4096,
			       1);

	obs_properties_add_int(props, "height",
			       obs_module_text("ColorSource.Height"), 0, 4096,
			       1);

	return props;
}

static void color_source_render_helper(struct color_source *context,
				       struct vec4 *colorVal)
{
	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	gs_effect_set_vec4(color, colorVal);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_draw_sprite(0, 0, context->width, context->height);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

static void color_source_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct color_source *context = data;

	/* need linear path for correct alpha blending */
	const bool linear_srgb = gs_get_linear_srgb() ||
				 (context->color.w < 1.0f);

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(linear_srgb);

	if (linear_srgb)
		color_source_render_helper(context, &context->color_srgb);
	else
		color_source_render_helper(context, &context->color);

	gs_enable_framebuffer_srgb(previous);
}

static uint32_t color_source_getwidth(void *data)
{
	struct color_source *context = data;
	return context->width;
}

static uint32_t color_source_getheight(void *data)
{
	struct color_source *context = data;
	return context->height;
}

static void color_source_defaults_v1(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "color", 0xFFFFFFFF);
	obs_data_set_default_int(settings, "width", 400);
	obs_data_set_default_int(settings, "height", 400);
}

static void color_source_defaults_v2(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "color", 0xFFFFFFFF);
	obs_data_set_default_int(settings, "width", 1920);
	obs_data_set_default_int(settings, "height", 1080);
}

static void color_source_defaults_v3(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "color", 0xFFD1D1D1);
	obs_data_set_default_int(settings, "width", 1920);
	obs_data_set_default_int(settings, "height", 1080);
}

struct obs_source_info color_source_info_v1 = {
	.id = "color_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW |
			OBS_SOURCE_CAP_OBSOLETE,
	.create = color_source_create,
	.destroy = color_source_destroy,
	.update = color_source_update,
	.get_name = color_source_get_name,
	.get_defaults = color_source_defaults_v1,
	.get_width = color_source_getwidth,
	.get_height = color_source_getheight,
	.video_render = color_source_render,
	.get_properties = color_source_properties,
	.icon_type = OBS_ICON_TYPE_COLOR,
};

struct obs_source_info color_source_info_v2 = {
	.id = "color_source",
	.version = 2,
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW |
			OBS_SOURCE_CAP_OBSOLETE,
	.create = color_source_create,
	.destroy = color_source_destroy,
	.update = color_source_update,
	.get_name = color_source_get_name,
	.get_defaults = color_source_defaults_v2,
	.get_width = color_source_getwidth,
	.get_height = color_source_getheight,
	.video_render = color_source_render,
	.get_properties = color_source_properties,
	.icon_type = OBS_ICON_TYPE_COLOR,
};

struct obs_source_info color_source_info_v3 = {
	.id = "color_source",
	.version = 3,
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.create = color_source_create,
	.destroy = color_source_destroy,
	.update = color_source_update,
	.get_name = color_source_get_name,
	.get_defaults = color_source_defaults_v3,
	.get_width = color_source_getwidth,
	.get_height = color_source_getheight,
	.video_render = color_source_render,
	.get_properties = color_source_properties,
	.icon_type = OBS_ICON_TYPE_COLOR,
};
