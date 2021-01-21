#include <obs-module.h>
#include <graphics/matrix4.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>

/* clang-format off */

#define SETTING_OPACITY                "opacity"
#define SETTING_CONTRAST               "contrast"
#define SETTING_BRIGHTNESS             "brightness"
#define SETTING_GAMMA                  "gamma"
#define SETTING_COLOR_TYPE             "key_color_type"
#define SETTING_KEY_COLOR              "key_color"
#define SETTING_SIMILARITY             "similarity"
#define SETTING_SMOOTHNESS             "smoothness"
#define SETTING_SPILL                  "spill"
#define SETTING_SPILL_TYPE             "spill_type"

#define TEXT_OPACITY                   obs_module_text("Opacity")
#define TEXT_CONTRAST                  obs_module_text("Contrast")
#define TEXT_BRIGHTNESS                obs_module_text("Brightness")
#define TEXT_GAMMA                     obs_module_text("Gamma")
#define TEXT_COLOR_TYPE                obs_module_text("KeyColorType")
#define TEXT_KEY_COLOR                 obs_module_text("KeyColor")
#define TEXT_SIMILARITY                obs_module_text("Similarity")
#define TEXT_SMOOTHNESS                obs_module_text("Smoothness")
#define TEXT_SPILL                     obs_module_text("ColorSpillReduction")
#define TEXT_SPILL_TYPE                obs_module_text("ColorSpillReductionType")

enum spill_type {
	SPILL_TYPE_SATURATION = 0,
	SPILL_TYPE_LIMIT_KEY_COLOR = 1
};

/* clang-format on */

struct chroma_key_filter_data {
	obs_source_t *context;

	gs_effect_t *effect;

	gs_eparam_t *color_param;
	gs_eparam_t *contrast_param;
	gs_eparam_t *brightness_param;
	gs_eparam_t *gamma_param;

	gs_eparam_t *pixel_size_param;
	gs_eparam_t *chroma_param;
	gs_eparam_t *similarity_param;
	gs_eparam_t *smoothness_param;
	gs_eparam_t *spill_param;
	gs_eparam_t *spill_type_param;
	gs_eparam_t *chroma_hue_param;

	struct vec4 color;
	float contrast;
	float brightness;
	float gamma;

	struct vec2 chroma;
	float similarity;
	float smoothness;
	float spill;
	enum spill_type spill_type;
	float chroma_hue;
};

static const char *chroma_key_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("ChromaKeyFilter");
}

static const float yuv_mat[16] = {0.182586f, -0.100644f, 0.439216f,  0.0f,
				  0.614231f, -0.338572f, -0.398942f, 0.0f,
				  0.062007f, 0.439216f,  -0.040274f, 0.0f,
				  0.062745f, 0.501961f,  0.501961f,  1.0f};

static const float sixty_deg_in_radians = 1.0471975512;

static inline void color_settings_update(struct chroma_key_filter_data *filter,
					 obs_data_t *settings)
{
	uint32_t opacity =
		(uint32_t)obs_data_get_int(settings, SETTING_OPACITY);
	uint32_t color = 0xFFFFFF | (((opacity * 255) / 100) << 24);
	double contrast = obs_data_get_double(settings, SETTING_CONTRAST);
	double brightness = obs_data_get_double(settings, SETTING_BRIGHTNESS);
	double gamma = obs_data_get_double(settings, SETTING_GAMMA);

	contrast = (contrast < 0.0) ? (1.0 / (-contrast + 1.0))
				    : (contrast + 1.0);

	brightness *= 0.5;

	gamma = (gamma < 0.0) ? (-gamma + 1.0) : (1.0 / (gamma + 1.0));

	filter->contrast = (float)contrast;
	filter->brightness = (float)brightness;
	filter->gamma = (float)gamma;

	vec4_from_rgba(&filter->color, color);
}

static inline float hue_from_rgb(struct vec4 *rgb)
{
	float xmax = rgb->ptr[0] > rgb->ptr[1] ? rgb->ptr[0] : rgb->ptr[1];
	xmax = rgb->ptr[2] > xmax ? rgb->ptr[2] : xmax;
	float xmin = rgb->ptr[0] < rgb->ptr[1] ? rgb->ptr[0] : rgb->ptr[1];
	xmin = rgb->ptr[2] < xmin ? rgb->ptr[2] : xmin;
	float chroma = xmax - xmin;
	if (chroma < 0.05) {
		return 0.0;
	} else if (xmax == rgb->ptr[0]) {
		return sixty_deg_in_radians * (rgb->ptr[1] - rgb->ptr[2]) /
		       chroma;
	} else if (xmax == rgb->ptr[1]) {
		return sixty_deg_in_radians *
		       (2.0 + (rgb->ptr[2] - rgb->ptr[0]) / chroma);
	} else { // xmax == rgb->ptr[2]
		return sixty_deg_in_radians *
		       (4.0 + (rgb->ptr[0] - rgb->ptr[1]) / chroma);
	}
}

static inline void chroma_settings_update(struct chroma_key_filter_data *filter,
					  obs_data_t *settings)
{
	int64_t similarity = obs_data_get_int(settings, SETTING_SIMILARITY);
	int64_t smoothness = obs_data_get_int(settings, SETTING_SMOOTHNESS);
	int64_t spill = obs_data_get_int(settings, SETTING_SPILL);
	int64_t spill_type = obs_data_get_int(settings, SETTING_SPILL_TYPE);
	uint32_t key_color =
		(uint32_t)obs_data_get_int(settings, SETTING_KEY_COLOR);
	const char *key_type =
		obs_data_get_string(settings, SETTING_COLOR_TYPE);
	struct vec4 key_rgb;
	struct vec4 key_color_v4;
	struct matrix4 yuv_mat_m4;

	if (strcmp(key_type, "green") == 0)
		key_color = 0x00FF00;
	else if (strcmp(key_type, "blue") == 0)
		key_color = 0xFF9900;
	else if (strcmp(key_type, "magenta") == 0)
		key_color = 0xFF00FF;

	vec4_from_rgba(&key_rgb, key_color | 0xFF000000);

	memcpy(&yuv_mat_m4, yuv_mat, sizeof(yuv_mat));
	vec4_transform(&key_color_v4, &key_rgb, &yuv_mat_m4);
	vec2_set(&filter->chroma, key_color_v4.y, key_color_v4.z);
	filter->chroma_hue = hue_from_rgb(&key_rgb);

	filter->similarity = (float)similarity / 1000.0f;
	filter->smoothness = (float)smoothness / 1000.0f;
	filter->spill = (float)spill / 1000.0f;
	filter->spill_type = spill_type;
}

static void chroma_key_update(void *data, obs_data_t *settings)
{
	struct chroma_key_filter_data *filter = data;

	color_settings_update(filter, settings);
	chroma_settings_update(filter, settings);
}

static void chroma_key_destroy(void *data)
{
	struct chroma_key_filter_data *filter = data;

	if (filter->effect) {
		obs_enter_graphics();
		gs_effect_destroy(filter->effect);
		obs_leave_graphics();
	}

	bfree(data);
}

static void *chroma_key_create(obs_data_t *settings, obs_source_t *context)
{
	struct chroma_key_filter_data *filter =
		bzalloc(sizeof(struct chroma_key_filter_data));
	char *effect_path = obs_module_file("chroma_key_filter.effect");

	filter->context = context;

	obs_enter_graphics();

	filter->effect = gs_effect_create_from_file(effect_path, NULL);
	if (filter->effect) {
		filter->color_param =
			gs_effect_get_param_by_name(filter->effect, "color");
		filter->contrast_param =
			gs_effect_get_param_by_name(filter->effect, "contrast");
		filter->brightness_param = gs_effect_get_param_by_name(
			filter->effect, "brightness");
		filter->gamma_param =
			gs_effect_get_param_by_name(filter->effect, "gamma");
		filter->chroma_param = gs_effect_get_param_by_name(
			filter->effect, "chroma_key");
		filter->pixel_size_param = gs_effect_get_param_by_name(
			filter->effect, "pixel_size");
		filter->similarity_param = gs_effect_get_param_by_name(
			filter->effect, "similarity");
		filter->smoothness_param = gs_effect_get_param_by_name(
			filter->effect, "smoothness");
		filter->spill_param =
			gs_effect_get_param_by_name(filter->effect, "spill");
		filter->spill_type_param = gs_effect_get_param_by_name(
			filter->effect, "spill_type");
		filter->chroma_hue_param = gs_effect_get_param_by_name(
			filter->effect, "chroma_hue");
	}

	obs_leave_graphics();

	bfree(effect_path);

	if (!filter->effect) {
		chroma_key_destroy(filter);
		return NULL;
	}

	chroma_key_update(filter, settings);
	return filter;
}

static void chroma_key_render(void *data, gs_effect_t *effect)
{
	struct chroma_key_filter_data *filter = data;
	obs_source_t *target = obs_filter_get_target(filter->context);
	uint32_t width = obs_source_get_base_width(target);
	uint32_t height = obs_source_get_base_height(target);
	struct vec2 pixel_size;

	if (!obs_source_process_filter_begin(filter->context, GS_RGBA,
					     OBS_ALLOW_DIRECT_RENDERING))
		return;

	vec2_set(&pixel_size, 1.0f / (float)width, 1.0f / (float)height);

	gs_effect_set_vec4(filter->color_param, &filter->color);
	gs_effect_set_float(filter->contrast_param, filter->contrast);
	gs_effect_set_float(filter->brightness_param, filter->brightness);
	gs_effect_set_float(filter->gamma_param, filter->gamma);
	gs_effect_set_vec2(filter->chroma_param, &filter->chroma);
	gs_effect_set_vec2(filter->pixel_size_param, &pixel_size);
	gs_effect_set_float(filter->similarity_param, filter->similarity);
	gs_effect_set_float(filter->smoothness_param, filter->smoothness);
	gs_effect_set_float(filter->spill_param, filter->spill);
	gs_effect_set_int(filter->spill_type_param, filter->spill_type);
	gs_effect_set_float(filter->chroma_hue_param, filter->chroma_hue);

	obs_source_process_filter_end(filter->context, filter->effect, 0, 0);

	UNUSED_PARAMETER(effect);
}

static bool key_type_changed(obs_properties_t *props, obs_property_t *p,
			     obs_data_t *settings)
{
	const char *type = obs_data_get_string(settings, SETTING_COLOR_TYPE);
	bool custom = strcmp(type, "custom") == 0;

	obs_property_set_visible(obs_properties_get(props, SETTING_KEY_COLOR),
				 custom);

	UNUSED_PARAMETER(p);
	return true;
}

static obs_properties_t *chroma_key_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *p = obs_properties_add_list(props, SETTING_COLOR_TYPE,
						    TEXT_COLOR_TYPE,
						    OBS_COMBO_TYPE_LIST,
						    OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, obs_module_text("Green"), "green");
	obs_property_list_add_string(p, obs_module_text("Blue"), "blue");
	obs_property_list_add_string(p, obs_module_text("Magenta"), "magenta");
	obs_property_list_add_string(p, obs_module_text("Custom"), "custom");

	obs_property_set_modified_callback(p, key_type_changed);

	obs_properties_add_color(props, SETTING_KEY_COLOR, TEXT_KEY_COLOR);
	obs_properties_add_int_slider(props, SETTING_SIMILARITY,
				      TEXT_SIMILARITY, 1, 1000, 1);
	obs_properties_add_int_slider(props, SETTING_SMOOTHNESS,
				      TEXT_SMOOTHNESS, 1, 1000, 1);

	obs_property_t *p2 = obs_properties_add_list(props, SETTING_SPILL_TYPE,
						     TEXT_SPILL_TYPE,
						     OBS_COMBO_TYPE_LIST,
						     OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p2, obs_module_text("SPILL_TYPE_SATURATION"),
				  SPILL_TYPE_SATURATION);
	obs_property_list_add_int(p2,
				  obs_module_text("SPILL_TYPE_LIMIT_KEY_COLOR"),
				  SPILL_TYPE_LIMIT_KEY_COLOR);

	obs_properties_add_int_slider(props, SETTING_SPILL, TEXT_SPILL, 1, 1000,
				      1);

	obs_properties_add_int_slider(props, SETTING_OPACITY, TEXT_OPACITY, 0,
				      100, 1);
	obs_properties_add_float_slider(props, SETTING_CONTRAST, TEXT_CONTRAST,
					-1.0, 1.0, 0.01);
	obs_properties_add_float_slider(props, SETTING_BRIGHTNESS,
					TEXT_BRIGHTNESS, -1.0, 1.0, 0.01);
	obs_properties_add_float_slider(props, SETTING_GAMMA, TEXT_GAMMA, -1.0,
					1.0, 0.01);

	UNUSED_PARAMETER(data);
	return props;
}

static void chroma_key_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, SETTING_OPACITY, 100);
	obs_data_set_default_double(settings, SETTING_CONTRAST, 0.0);
	obs_data_set_default_double(settings, SETTING_BRIGHTNESS, 0.0);
	obs_data_set_default_double(settings, SETTING_GAMMA, 0.0);
	obs_data_set_default_int(settings, SETTING_KEY_COLOR, 0x00FF00);
	obs_data_set_default_string(settings, SETTING_COLOR_TYPE, "green");
	obs_data_set_default_int(settings, SETTING_SIMILARITY, 400);
	obs_data_set_default_int(settings, SETTING_SMOOTHNESS, 80);
	obs_data_set_default_int(settings, SETTING_SPILL, 100);
	obs_data_set_default_int(settings, SETTING_SPILL_TYPE,
				 SPILL_TYPE_SATURATION);
}

struct obs_source_info chroma_key_filter = {
	.id = "chroma_key_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = chroma_key_name,
	.create = chroma_key_create,
	.destroy = chroma_key_destroy,
	.video_render = chroma_key_render,
	.update = chroma_key_update,
	.get_properties = chroma_key_properties,
	.get_defaults = chroma_key_defaults,
};
