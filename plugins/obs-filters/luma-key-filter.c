#include <obs-module.h>

/* clang-format off */

#define SETTING_LUMA_MAX           "luma_max"
#define SETTING_LUMA_MIN           "luma_min"
#define SETTING_LUMA_MAX_SMOOTH    "luma_max_smooth"
#define SETTING_LUMA_MIN_SMOOTH    "luma_min_smooth"

#define TEXT_LUMA_MAX           obs_module_text("Luma.LumaMax")
#define TEXT_LUMA_MIN           obs_module_text("Luma.LumaMin")
#define TEXT_LUMA_MAX_SMOOTH    obs_module_text("Luma.LumaMaxSmooth")
#define TEXT_LUMA_MIN_SMOOTH    obs_module_text("Luma.LumaMinSmooth")

/* clang-format on */

struct luma_key_filter_data {
	obs_source_t *context;

	gs_effect_t *effect;

	gs_eparam_t *luma_max_param;
	gs_eparam_t *luma_min_param;
	gs_eparam_t *luma_max_smooth_param;
	gs_eparam_t *luma_min_smooth_param;

	float luma_max;
	float luma_min;
	float luma_max_smooth;
	float luma_min_smooth;
};

static const char *luma_key_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("LumaKeyFilter");
}

static void luma_key_update(void *data, obs_data_t *settings)
{
	struct luma_key_filter_data *filter = data;

	double lumaMax = obs_data_get_double(settings, SETTING_LUMA_MAX);
	double lumaMin = obs_data_get_double(settings, SETTING_LUMA_MIN);
	double lumaMaxSmooth =
		obs_data_get_double(settings, SETTING_LUMA_MAX_SMOOTH);
	double lumaMinSmooth =
		obs_data_get_double(settings, SETTING_LUMA_MIN_SMOOTH);

	filter->luma_max = (float)lumaMax;
	filter->luma_min = (float)lumaMin;
	filter->luma_max_smooth = (float)lumaMaxSmooth;
	filter->luma_min_smooth = (float)lumaMinSmooth;
}

static void luma_key_destroy(void *data)
{
	struct luma_key_filter_data *filter = data;

	if (filter->effect) {
		obs_enter_graphics();
		gs_effect_destroy(filter->effect);
		obs_leave_graphics();
	}

	bfree(data);
}

static void *luma_key_create_internal(obs_data_t *settings,
				      obs_source_t *context,
				      const char *effect_name)
{
	struct luma_key_filter_data *filter =
		bzalloc(sizeof(struct luma_key_filter_data));
	char *effect_path = obs_module_file(effect_name);

	filter->context = context;

	obs_enter_graphics();

	filter->effect = gs_effect_create_from_file(effect_path, NULL);
	if (filter->effect) {
		filter->luma_max_param =
			gs_effect_get_param_by_name(filter->effect, "lumaMax");
		filter->luma_min_param =
			gs_effect_get_param_by_name(filter->effect, "lumaMin");
		filter->luma_max_smooth_param = gs_effect_get_param_by_name(
			filter->effect, "lumaMaxSmooth");
		filter->luma_min_smooth_param = gs_effect_get_param_by_name(
			filter->effect, "lumaMinSmooth");
	}

	obs_leave_graphics();

	bfree(effect_path);

	if (!filter->effect) {
		luma_key_destroy(filter);
		return NULL;
	}

	luma_key_update(filter, settings);
	return filter;
}

static void *luma_key_create_v1(obs_data_t *settings, obs_source_t *context)
{
	return luma_key_create_internal(settings, context,
					"luma_key_filter.effect");
}

static void *luma_key_create_v2(obs_data_t *settings, obs_source_t *context)
{
	return luma_key_create_internal(settings, context,
					"luma_key_filter_v2.effect");
}

static void luma_key_render_internal(void *data, bool srgb)
{
	struct luma_key_filter_data *filter = data;

	if (!obs_source_process_filter_begin(filter->context, GS_RGBA,
					     OBS_ALLOW_DIRECT_RENDERING))
		return;

	gs_effect_set_float(filter->luma_max_param, filter->luma_max);
	gs_effect_set_float(filter->luma_min_param, filter->luma_min);
	gs_effect_set_float(filter->luma_max_smooth_param,
			    filter->luma_max_smooth);
	gs_effect_set_float(filter->luma_min_smooth_param,
			    filter->luma_min_smooth);

	const bool previous = gs_set_linear_srgb(srgb);
	obs_source_process_filter_end(filter->context, filter->effect, 0, 0);
	gs_set_linear_srgb(previous);
}

static void luma_key_render_v1(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	luma_key_render_internal(data, false);
}

static void luma_key_render_v2(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	luma_key_render_internal(data, true);
}

static obs_properties_t *luma_key_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_float_slider(props, SETTING_LUMA_MAX, TEXT_LUMA_MAX,
					0, 1, 0.01);
	obs_properties_add_float_slider(props, SETTING_LUMA_MAX_SMOOTH,
					TEXT_LUMA_MAX_SMOOTH, 0, 1, 0.01);
	obs_properties_add_float_slider(props, SETTING_LUMA_MIN, TEXT_LUMA_MIN,
					0, 1, 0.01);
	obs_properties_add_float_slider(props, SETTING_LUMA_MIN_SMOOTH,
					TEXT_LUMA_MIN_SMOOTH, 0, 1, 0.01);

	UNUSED_PARAMETER(data);
	return props;
}

static void luma_key_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, SETTING_LUMA_MAX, 1.0);
	obs_data_set_default_double(settings, SETTING_LUMA_MIN, 0.0);
	obs_data_set_default_double(settings, SETTING_LUMA_MAX_SMOOTH, 0.0);
	obs_data_set_default_double(settings, SETTING_LUMA_MIN_SMOOTH, 0.0);
}

struct obs_source_info luma_key_filter = {
	.id = "luma_key_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CAP_OBSOLETE,
	.get_name = luma_key_name,
	.create = luma_key_create_v1,
	.destroy = luma_key_destroy,
	.video_render = luma_key_render_v1,
	.update = luma_key_update,
	.get_properties = luma_key_properties,
	.get_defaults = luma_key_defaults,
};

struct obs_source_info luma_key_filter_v2 = {
	.id = "luma_key_filter",
	.version = 2,
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = luma_key_name,
	.create = luma_key_create_v2,
	.destroy = luma_key_destroy,
	.video_render = luma_key_render_v2,
	.update = luma_key_update,
	.get_properties = luma_key_properties,
	.get_defaults = luma_key_defaults,
};
