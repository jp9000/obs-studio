#include <obs-module.h>
#include <graphics/vec2.h>

struct crop_filter_data {
	obs_source_t *context;

	gs_effect_t *effect;
	gs_eparam_t *param_mul;
	gs_eparam_t *param_add;

	int left;
	int right;
	int top;
	int bottom;
	int abs_cx;
	int abs_cy;
	int width;
	int height;
	bool absolute;

	struct vec2 mul_val;
	struct vec2 add_val;
};

static const char *crop_filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("CropFilter");
}

static void *crop_filter_create(obs_data_t *settings, obs_source_t *context)
{
	struct crop_filter_data *filter = bzalloc(sizeof(*filter));
	char *effect_path = obs_module_file("crop_filter.effect");

	filter->context = context;

	obs_enter_graphics();
	filter->effect = gs_effect_create_from_file(effect_path, NULL);
	obs_leave_graphics();

	bfree(effect_path);

	if (!filter->effect) {
		bfree(filter);
		return NULL;
	}

	filter->param_mul =
		gs_effect_get_param_by_name(filter->effect, "mul_val");
	filter->param_add =
		gs_effect_get_param_by_name(filter->effect, "add_val");

	obs_source_update(context, settings);
	return filter;
}

static void crop_filter_destroy(void *data)
{
	struct crop_filter_data *filter = data;

	obs_enter_graphics();
	gs_effect_destroy(filter->effect);
	obs_leave_graphics();

	bfree(filter);
}

static void crop_filter_update(void *data, obs_data_t *settings)
{
	struct crop_filter_data *filter = data;

	filter->absolute = !obs_data_get_bool(settings, "relative");
	filter->left = (int)obs_data_get_int(settings, "left");
	filter->top = (int)obs_data_get_int(settings, "top");
	filter->right = (int)obs_data_get_int(settings, "right");
	filter->bottom = (int)obs_data_get_int(settings, "bottom");
	filter->abs_cx = (int)obs_data_get_int(settings, "cx");
	filter->abs_cy = (int)obs_data_get_int(settings, "cy");
}

static bool relative_clicked(obs_properties_t *props, obs_property_t *p,
			     obs_data_t *settings)
{
	bool relative = obs_data_get_bool(settings, "relative");

	obs_property_set_description(obs_properties_get(props, "left"),
				     relative ? obs_module_text("Crop.Left")
					      : "X");
	obs_property_set_description(obs_properties_get(props, "top"),
				     relative ? obs_module_text("Crop.Top")
					      : "Y");

	obs_property_set_visible(obs_properties_get(props, "right"), relative);
	obs_property_set_visible(obs_properties_get(props, "bottom"), relative);
	obs_property_set_visible(obs_properties_get(props, "cx"), !relative);
	obs_property_set_visible(obs_properties_get(props, "cy"), !relative);

	UNUSED_PARAMETER(p);
	return true;
}

static obs_properties_t *crop_filter_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *p = obs_properties_add_bool(
		props, "relative", obs_module_text("Crop.Relative"));

	obs_property_set_modified_callback(p, relative_clicked);

	obs_properties_add_int(props, "left", obs_module_text("Crop.Left"),
			       -8192, 8192, 1);
	obs_properties_add_int(props, "top", obs_module_text("Crop.Top"), -8192,
			       8192, 1);
	obs_properties_add_int(props, "right", obs_module_text("Crop.Right"),
			       -8192, 8192, 1);
	obs_properties_add_int(props, "bottom", obs_module_text("Crop.Bottom"),
			       -8192, 8192, 1);
	obs_properties_add_int(props, "cx", obs_module_text("Crop.Width"), 0,
			       8192, 1);
	obs_properties_add_int(props, "cy", obs_module_text("Crop.Height"), 0,
			       8192, 1);

	UNUSED_PARAMETER(data);
	return props;
}

static void crop_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "relative", true);
}

static void calc_crop_dimensions(struct crop_filter_data *filter,
				 struct vec2 *mul_val, struct vec2 *add_val)
{
	obs_source_t *target = obs_filter_get_target(filter->context);
	uint32_t width;
	uint32_t height;

	if (!target) {
		width = 0;
		height = 0;
		return;
	} else {
		width = obs_source_get_base_width(target);
		height = obs_source_get_base_height(target);
	}

	if (filter->absolute) {
		filter->width = filter->abs_cx;
		filter->height = filter->abs_cy;
	} else {
		filter->width = (int)width - filter->left - filter->right;
		filter->height = (int)height - filter->top - filter->bottom;
	}

	if (filter->width < 1)
		filter->width = 1;
	if (filter->height < 1)
		filter->height = 1;

	if (width) {
		mul_val->x = (float)filter->width / (float)width;
		add_val->x = (float)filter->left / (float)width;
	}

	if (height) {
		mul_val->y = (float)filter->height / (float)height;
		add_val->y = (float)filter->top / (float)height;
	}
}

static void crop_filter_tick(void *data, float seconds)
{
	struct crop_filter_data *filter = data;

	vec2_zero(&filter->mul_val);
	vec2_zero(&filter->add_val);
	calc_crop_dimensions(filter, &filter->mul_val, &filter->add_val);

	UNUSED_PARAMETER(seconds);
}

static void crop_filter_render(void *data, gs_effect_t *effect)
{
	struct crop_filter_data *filter = data;

	if (!obs_source_process_filter_begin(filter->context, GS_RGBA,
					     OBS_NO_DIRECT_RENDERING))
		return;

	gs_effect_set_vec2(filter->param_mul, &filter->mul_val);
	gs_effect_set_vec2(filter->param_add, &filter->add_val);

	obs_source_process_filter_end(filter->context, filter->effect,
				      filter->width, filter->height);

	UNUSED_PARAMETER(effect);
}

static uint32_t crop_filter_width(void *data)
{
	struct crop_filter_data *crop = data;
	return (uint32_t)crop->width;
}

static uint32_t crop_filter_height(void *data)
{
	struct crop_filter_data *crop = data;
	return (uint32_t)crop->height;
}

struct obs_source_info crop_filter = {
	.id = "crop_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = crop_filter_get_name,
	.create = crop_filter_create,
	.destroy = crop_filter_destroy,
	.update = crop_filter_update,
	.get_properties = crop_filter_properties,
	.get_defaults = crop_filter_defaults,
	.video_tick = crop_filter_tick,
	.video_render = crop_filter_render,
	.get_width = crop_filter_width,
	.get_height = crop_filter_height,
};
