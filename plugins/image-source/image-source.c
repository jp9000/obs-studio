#include <obs-module.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <sys/stat.h>

#define blog(log_level, format, ...)                    \
	blog(log_level, "[image_source: '%s'] " format, \
	     obs_source_get_name(context->source), ##__VA_ARGS__)

#define debug(format, ...) blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define info(format, ...) blog(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) blog(LOG_WARNING, format, ##__VA_ARGS__)

struct image_source {
	obs_source_t *source;

	char *file;
	bool persistent;
	time_t file_timestamp;
	float update_time_elapsed;
	uint64_t last_time;
	bool active;

	gs_image_file2_t if2;
};

static time_t get_modified_timestamp(const char *filename)
{
	struct stat stats;
	if (os_stat(filename, &stats) != 0)
		return -1;
	return stats.st_mtime;
}

static const char *image_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("ImageInput");
}

static void image_source_load(struct image_source *context)
{
	char *file = context->file;

	obs_enter_graphics();
	gs_image_file2_free(&context->if2);
	obs_leave_graphics();

	if (file && *file) {
		debug("loading texture '%s'", file);
		context->file_timestamp = get_modified_timestamp(file);
		gs_image_file2_init(&context->if2, file);
		context->update_time_elapsed = 0;

		obs_enter_graphics();
		gs_image_file2_init_texture(&context->if2);
		obs_leave_graphics();

		if (!context->if2.image.loaded)
			warn("failed to load texture '%s'", file);
	}
}

static void image_source_unload(struct image_source *context)
{
	obs_enter_graphics();
	gs_image_file2_free(&context->if2);
	obs_leave_graphics();
}

static void image_source_update(void *data, obs_data_t *settings)
{
	struct image_source *context = data;
	const char *file = obs_data_get_string(settings, "file");
	const bool unload = obs_data_get_bool(settings, "unload");

	if (context->file)
		bfree(context->file);
	context->file = bstrdup(file);
	context->persistent = !unload;

	/* Load the image if the source is persistent or showing */
	if (context->persistent || obs_source_showing(context->source))
		image_source_load(data);
	else
		image_source_unload(data);
}

static void image_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "unload", false);
}

static void image_source_show(void *data)
{
	struct image_source *context = data;

	if (!context->persistent)
		image_source_load(context);
}

static void image_source_hide(void *data)
{
	struct image_source *context = data;

	if (!context->persistent)
		image_source_unload(context);
}

static void *image_source_create(obs_data_t *settings, obs_source_t *source)
{
	struct image_source *context = bzalloc(sizeof(struct image_source));
	context->source = source;

	image_source_update(context, settings);
	return context;
}

static void image_source_destroy(void *data)
{
	struct image_source *context = data;

	image_source_unload(context);

	if (context->file)
		bfree(context->file);
	bfree(context);
}

static uint32_t image_source_getwidth(void *data)
{
	struct image_source *context = data;
	return context->if2.image.cx;
}

static uint32_t image_source_getheight(void *data)
{
	struct image_source *context = data;
	return context->if2.image.cy;
}

static void image_source_render(void *data, gs_effect_t *effect)
{
	struct image_source *context = data;

	if (!context->if2.image.texture)
		return;

	const bool linear_srgb = gs_get_linear_srgb();

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(linear_srgb);

	gs_eparam_t *const param = gs_effect_get_param_by_name(effect, "image");
	if (linear_srgb)
		gs_effect_set_texture_srgb(param, context->if2.image.texture);
	else
		gs_effect_set_texture(param, context->if2.image.texture);

	gs_draw_sprite(context->if2.image.texture, 0, context->if2.image.cx,
		       context->if2.image.cy);

	gs_enable_framebuffer_srgb(previous);
}

static void image_source_tick(void *data, float seconds)
{
	struct image_source *context = data;
	uint64_t frame_time = obs_get_video_frame_time();

	context->update_time_elapsed += seconds;

	if (obs_source_showing(context->source)) {
		if (context->update_time_elapsed >= 1.0f) {
			time_t t = get_modified_timestamp(context->file);
			context->update_time_elapsed = 0.0f;

			if (context->file_timestamp != t) {
				image_source_load(context);
			}
		}
	}

	if (obs_source_active(context->source)) {
		if (!context->active) {
			if (context->if2.image.is_animated_gif)
				context->last_time = frame_time;
			context->active = true;
		}

	} else {
		if (context->active) {
			if (context->if2.image.is_animated_gif) {
				context->if2.image.cur_frame = 0;
				context->if2.image.cur_loop = 0;
				context->if2.image.cur_time = 0;

				obs_enter_graphics();
				gs_image_file2_update_texture(&context->if2);
				obs_leave_graphics();
			}

			context->active = false;
		}

		return;
	}

	if (context->last_time && context->if2.image.is_animated_gif) {
		uint64_t elapsed = frame_time - context->last_time;
		bool updated = gs_image_file2_tick(&context->if2, elapsed);

		if (updated) {
			obs_enter_graphics();
			gs_image_file2_update_texture(&context->if2);
			obs_leave_graphics();
		}
	}

	context->last_time = frame_time;
}

static const char *image_filter =
	"All formats (*.bmp *.tga *.png *.jpeg *.jpg *.gif *.psd *.webp);;"
	"BMP Files (*.bmp);;"
	"Targa Files (*.tga);;"
	"PNG Files (*.png);;"
	"JPEG Files (*.jpeg *.jpg);;"
	"GIF Files (*.gif);;"
	"PSD Files (*.psd);;"
	"WebP Files (*.webp);;"
	"All Files (*.*)";

static obs_properties_t *image_source_properties(void *data)
{
	struct image_source *s = data;
	struct dstr path = {0};

	obs_properties_t *props = obs_properties_create();

	if (s && s->file && *s->file) {
		const char *slash;

		dstr_copy(&path, s->file);
		dstr_replace(&path, "\\", "/");
		slash = strrchr(path.array, '/');
		if (slash)
			dstr_resize(&path, slash - path.array + 1);
	}

	obs_properties_add_path(props, "file", obs_module_text("File"),
				OBS_PATH_FILE, image_filter, path.array);
	obs_properties_add_bool(props, "unload",
				obs_module_text("UnloadWhenNotShowing"));
	dstr_free(&path);

	return props;
}

uint64_t image_source_get_memory_usage(void *data)
{
	struct image_source *s = data;
	return s->if2.mem_usage;
}

static void missing_file_callback(void *src, const char *new_path, void *data)
{
	struct image_source *s = src;

	obs_source_t *source = s->source;
	obs_data_t *settings = obs_source_get_settings(source);
	obs_data_set_string(settings, "file", new_path);
	obs_source_update(source, settings);
	obs_data_release(settings);

	UNUSED_PARAMETER(data);
}

static obs_missing_files_t *image_source_missingfiles(void *data)
{
	struct image_source *s = data;
	obs_missing_files_t *files = obs_missing_files_create();

	if (strcmp(s->file, "") != 0) {
		if (!os_file_exists(s->file)) {
			obs_missing_file_t *file = obs_missing_file_create(
				s->file, missing_file_callback,
				OBS_MISSING_FILE_SOURCE, s->source, NULL);

			obs_missing_files_add_file(files, file);
		}
	}

	return files;
}

static struct obs_source_info image_source_info = {
	.id = "image_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = image_source_get_name,
	.create = image_source_create,
	.destroy = image_source_destroy,
	.update = image_source_update,
	.get_defaults = image_source_defaults,
	.show = image_source_show,
	.hide = image_source_hide,
	.get_width = image_source_getwidth,
	.get_height = image_source_getheight,
	.video_render = image_source_render,
	.video_tick = image_source_tick,
	.missing_files = image_source_missingfiles,
	.get_properties = image_source_properties,
	.icon_type = OBS_ICON_TYPE_IMAGE,
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("image-source", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Image/color/slideshow sources";
}

extern struct obs_source_info slideshow_info;
extern struct obs_source_info color_source_info_v1;
extern struct obs_source_info color_source_info_v2;
extern struct obs_source_info color_source_info_v3;

bool obs_module_load(void)
{
	obs_register_source(&image_source_info);
	obs_register_source(&color_source_info_v1);
	obs_register_source(&color_source_info_v2);
	obs_register_source(&color_source_info_v3);
	obs_register_source(&slideshow_info);
	return true;
}
