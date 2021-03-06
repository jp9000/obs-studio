#include <stdlib.h>
#include <obs-module.h>
#include <util/threading.h>
#include <pthread.h>

#import <CoreGraphics/CGDisplayStream.h>
#import <Cocoa/Cocoa.h>

#include "window-utils.h"

enum crop_mode {
	CROP_NONE,
	CROP_MANUAL,
	CROP_TO_WINDOW,
	CROP_TO_WINDOW_AND_MANUAL,
	CROP_INVALID
};

static inline bool requires_window(enum crop_mode mode)
{
	return mode == CROP_TO_WINDOW || mode == CROP_TO_WINDOW_AND_MANUAL;
}

struct display_capture {
	obs_source_t *source;

	gs_samplerstate_t *sampler;
	gs_effect_t *effect;
	gs_texture_t *tex;
	gs_vertbuffer_t *vertbuf;

	NSScreen *screen;
	unsigned display;
	NSRect frame;
	bool hide_cursor;

	enum crop_mode crop;
	CGRect crop_rect;

	struct cocoa_window window;
	CGRect window_rect;
	bool on_screen;
	bool hide_when_minimized;

	os_event_t *disp_finished;
	CGDisplayStreamRef disp;
	IOSurfaceRef current, prev;

	pthread_mutex_t mutex;
};

static inline bool crop_mode_valid(enum crop_mode mode)
{
	return CROP_NONE <= mode && mode < CROP_INVALID;
}

static void destroy_display_stream(struct display_capture *dc)
{
	if (dc->disp) {
		CGDisplayStreamStop(dc->disp);
		os_event_wait(dc->disp_finished);
	}

	if (dc->tex) {
		gs_texture_destroy(dc->tex);
		dc->tex = NULL;
	}

	if (dc->current) {
		IOSurfaceDecrementUseCount(dc->current);
		CFRelease(dc->current);
		dc->current = NULL;
	}

	if (dc->prev) {
		IOSurfaceDecrementUseCount(dc->prev);
		CFRelease(dc->prev);
		dc->prev = NULL;
	}

	if (dc->disp) {
		CFRelease(dc->disp);
		dc->disp = NULL;
	}

	if (dc->screen) {
		[dc->screen release];
		dc->screen = nil;
	}

	os_event_destroy(dc->disp_finished);
}

static void display_capture_destroy(void *data)
{
	struct display_capture *dc = data;

	if (!dc)
		return;

	obs_enter_graphics();

	destroy_display_stream(dc);

	if (dc->sampler)
		gs_samplerstate_destroy(dc->sampler);
	if (dc->vertbuf)
		gs_vertexbuffer_destroy(dc->vertbuf);

	obs_leave_graphics();

	destroy_window(&dc->window);

	pthread_mutex_destroy(&dc->mutex);
	bfree(dc);
}

static inline void update_window_params(struct display_capture *dc)
{
	if (!requires_window(dc->crop))
		return;

	NSArray *arr = (NSArray *)CGWindowListCopyWindowInfo(
		kCGWindowListOptionIncludingWindow, dc->window.window_id);

	if (arr.count) {
		NSDictionary *dict = arr[0];
		NSDictionary *ref = dict[(NSString *)kCGWindowBounds];
		CGRectMakeWithDictionaryRepresentation((CFDictionaryRef)ref,
						       &dc->window_rect);
		dc->on_screen = dict[(NSString *)kCGWindowIsOnscreen] != nil;
		dc->window_rect =
			[dc->screen convertRectToBacking:dc->window_rect];

	} else {
		if (find_window(&dc->window, NULL, false))
			update_window_params(dc);
		else
			dc->on_screen = false;
	}

	[arr release];
}

static inline void display_stream_update(struct display_capture *dc,
					 CGDisplayStreamFrameStatus status,
					 uint64_t display_time,
					 IOSurfaceRef frame_surface,
					 CGDisplayStreamUpdateRef update_ref)
{
	UNUSED_PARAMETER(display_time);
	UNUSED_PARAMETER(update_ref);

	if (status == kCGDisplayStreamFrameStatusStopped) {
		os_event_signal(dc->disp_finished);
		return;
	}

	IOSurfaceRef prev_current = NULL;

	if (frame_surface && !pthread_mutex_lock(&dc->mutex)) {
		prev_current = dc->current;
		dc->current = frame_surface;
		CFRetain(dc->current);
		IOSurfaceIncrementUseCount(dc->current);

		update_window_params(dc);

		pthread_mutex_unlock(&dc->mutex);
	}

	if (prev_current) {
		IOSurfaceDecrementUseCount(prev_current);
		CFRelease(prev_current);
	}

	size_t dropped_frames = CGDisplayStreamUpdateGetDropCount(update_ref);
	if (dropped_frames > 0)
		blog(LOG_INFO, "%s: Dropped %zu frames",
		     obs_source_get_name(dc->source), dropped_frames);
}

static bool init_display_stream(struct display_capture *dc)
{
	if (dc->display >= [NSScreen screens].count)
		return false;

	dc->screen = [[NSScreen screens][dc->display] retain];

	dc->frame = [dc->screen convertRectToBacking:dc->screen.frame];

	NSNumber *screen_num = dc->screen.deviceDescription[@"NSScreenNumber"];
	CGDirectDisplayID disp_id = (CGDirectDisplayID)screen_num.pointerValue;

	NSDictionary *rect_dict =
		CFBridgingRelease(CGRectCreateDictionaryRepresentation(
			CGRectMake(0, 0, dc->screen.frame.size.width,
				   dc->screen.frame.size.height)));

	CFBooleanRef show_cursor_cf = dc->hide_cursor ? kCFBooleanFalse
						      : kCFBooleanTrue;

	NSDictionary *dict = @{
		(__bridge NSString *)kCGDisplayStreamSourceRect: rect_dict,
		(__bridge NSString *)kCGDisplayStreamQueueDepth: @5,
		(__bridge NSString *)
		kCGDisplayStreamShowCursor: (id)show_cursor_cf,
	};

	os_event_init(&dc->disp_finished, OS_EVENT_TYPE_MANUAL);

	const CGSize *size = &dc->frame.size;
	dc->disp = CGDisplayStreamCreateWithDispatchQueue(
		disp_id, size->width, size->height, 'BGRA',
		(__bridge CFDictionaryRef)dict,
		dispatch_queue_create(NULL, NULL),
		^(CGDisplayStreamFrameStatus status, uint64_t displayTime,
		  IOSurfaceRef frameSurface,
		  CGDisplayStreamUpdateRef updateRef) {
			display_stream_update(dc, status, displayTime,
					      frameSurface, updateRef);
		});

	return !CGDisplayStreamStart(dc->disp);
}

bool init_vertbuf(struct display_capture *dc)
{
	struct gs_vb_data *vb_data = gs_vbdata_create();
	vb_data->num = 4;
	vb_data->points = bzalloc(sizeof(struct vec3) * 4);
	if (!vb_data->points)
		return false;

	vb_data->num_tex = 1;
	vb_data->tvarray = bzalloc(sizeof(struct gs_tvertarray));
	if (!vb_data->tvarray)
		return false;

	vb_data->tvarray[0].width = 2;
	vb_data->tvarray[0].array = bzalloc(sizeof(struct vec2) * 4);
	if (!vb_data->tvarray[0].array)
		return false;

	dc->vertbuf = gs_vertexbuffer_create(vb_data, GS_DYNAMIC);
	return dc->vertbuf != NULL;
}

void load_crop(struct display_capture *dc, obs_data_t *settings);

static void *display_capture_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(source);
	UNUSED_PARAMETER(settings);

	struct display_capture *dc = bzalloc(sizeof(struct display_capture));

	dc->source = source;
	dc->hide_cursor = !obs_data_get_bool(settings, "show_cursor");

	dc->effect = obs_get_base_effect(OBS_EFFECT_DEFAULT_RECT);
	if (!dc->effect)
		goto fail;

	obs_enter_graphics();

	struct gs_sampler_info info = {
		.filter = GS_FILTER_LINEAR,
		.address_u = GS_ADDRESS_CLAMP,
		.address_v = GS_ADDRESS_CLAMP,
		.address_w = GS_ADDRESS_CLAMP,
		.max_anisotropy = 1,
	};
	dc->sampler = gs_samplerstate_create(&info);
	if (!dc->sampler)
		goto fail;

	if (!init_vertbuf(dc))
		goto fail;

	obs_leave_graphics();

	init_window(&dc->window, settings);
	load_crop(dc, settings);

	dc->display = obs_data_get_int(settings, "display");
	pthread_mutex_init(&dc->mutex, NULL);

	if (!init_display_stream(dc))
		goto fail;

	return dc;

fail:
	obs_leave_graphics();
	display_capture_destroy(dc);
	return NULL;
}

static void build_sprite(struct gs_vb_data *data, float fcx, float fcy,
			 float start_u, float end_u, float start_v, float end_v)
{
	struct vec2 *tvarray = data->tvarray[0].array;

	vec3_set(data->points + 1, fcx, 0.0f, 0.0f);
	vec3_set(data->points + 2, 0.0f, fcy, 0.0f);
	vec3_set(data->points + 3, fcx, fcy, 0.0f);
	vec2_set(tvarray, start_u, start_v);
	vec2_set(tvarray + 1, end_u, start_v);
	vec2_set(tvarray + 2, start_u, end_v);
	vec2_set(tvarray + 3, end_u, end_v);
}

static inline void build_sprite_rect(struct gs_vb_data *data, float origin_x,
				     float origin_y, float end_x, float end_y)
{
	build_sprite(data, fabs(end_x - origin_x), fabs(end_y - origin_y),
		     origin_x, end_x, origin_y, end_y);
}

static void display_capture_video_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);

	struct display_capture *dc = data;

	if (!dc->current)
		return;
	if (!obs_source_showing(dc->source))
		return;

	IOSurfaceRef prev_prev = dc->prev;
	if (pthread_mutex_lock(&dc->mutex))
		return;
	dc->prev = dc->current;
	dc->current = NULL;
	pthread_mutex_unlock(&dc->mutex);

	if (prev_prev == dc->prev)
		return;

	if (requires_window(dc->crop) && !dc->on_screen)
		goto cleanup;

	CGPoint origin = {0.f};
	CGPoint end = {0.f};

	switch (dc->crop) {
		float x, y;
	case CROP_INVALID:
		break;

	case CROP_MANUAL:
		origin.x += dc->crop_rect.origin.x;
		origin.y += dc->crop_rect.origin.y;
		end.y -= dc->crop_rect.size.height;
		end.x -= dc->crop_rect.size.width;
	case CROP_NONE:
		end.y += dc->frame.size.height;
		end.x += dc->frame.size.width;
		break;

	case CROP_TO_WINDOW_AND_MANUAL:
		origin.x += dc->crop_rect.origin.x;
		origin.y += dc->crop_rect.origin.y;
		end.y -= dc->crop_rect.size.height;
		end.x -= dc->crop_rect.size.width;
	case CROP_TO_WINDOW:
		origin.x += x = dc->window_rect.origin.x - dc->frame.origin.x;
		origin.y += y = dc->window_rect.origin.y - dc->frame.origin.y;
		end.y += dc->window_rect.size.height + y;
		end.x += dc->window_rect.size.width + x;
		break;
	}

	obs_enter_graphics();
	build_sprite_rect(gs_vertexbuffer_get_data(dc->vertbuf), origin.x,
			  origin.y, end.x, end.y);

	if (dc->tex)
		gs_texture_rebind_iosurface(dc->tex, dc->prev);
	else
		dc->tex = gs_texture_create_from_iosurface(dc->prev);
	obs_leave_graphics();

cleanup:
	if (prev_prev) {
		IOSurfaceDecrementUseCount(prev_prev);
		CFRelease(prev_prev);
	}
}

static void display_capture_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct display_capture *dc = data;

	if (!dc->tex || (requires_window(dc->crop) && !dc->on_screen))
		return;

	const bool linear_srgb = gs_get_linear_srgb();

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(linear_srgb);

	gs_vertexbuffer_flush(dc->vertbuf);
	gs_load_vertexbuffer(dc->vertbuf);
	gs_load_indexbuffer(NULL);
	gs_load_samplerstate(dc->sampler, 0);
	gs_technique_t *tech = gs_effect_get_technique(dc->effect, "Draw");
	gs_eparam_t *param = gs_effect_get_param_by_name(dc->effect, "image");
	if (linear_srgb)
		gs_effect_set_texture_srgb(param, dc->tex);
	else
		gs_effect_set_texture(param, dc->tex);
	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_draw(GS_TRISTRIP, 0, 4);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_enable_framebuffer_srgb(previous);
}

static const char *display_capture_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("DisplayCapture");
}

#define CROPPED_LENGTH(rect, origin_, length)                       \
	fabs((rect##.size.##length - dc->crop_rect.size.##length) - \
	     (rect##.origin.##origin_ + dc->crop_rect.origin.##origin_))

static uint32_t display_capture_getwidth(void *data)
{
	struct display_capture *dc = data;

	float crop = dc->crop_rect.origin.x + dc->crop_rect.size.width;
	switch (dc->crop) {
	case CROP_NONE:
		return dc->frame.size.width;

	case CROP_MANUAL:
		return fabs(dc->frame.size.width - crop);

	case CROP_TO_WINDOW:
		return dc->window_rect.size.width;

	case CROP_TO_WINDOW_AND_MANUAL:
		return fabs(dc->window_rect.size.width - crop);

	case CROP_INVALID:
		break;
	}
	return 0;
}

static uint32_t display_capture_getheight(void *data)
{
	struct display_capture *dc = data;

	float crop = dc->crop_rect.origin.y + dc->crop_rect.size.height;
	switch (dc->crop) {
	case CROP_NONE:
		return dc->frame.size.height;

	case CROP_MANUAL:
		return fabs(dc->frame.size.height - crop);

	case CROP_TO_WINDOW:
		return dc->window_rect.size.height;

	case CROP_TO_WINDOW_AND_MANUAL:
		return fabs(dc->window_rect.size.height - crop);

	case CROP_INVALID:
		break;
	}
	return 0;
}

static void display_capture_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "display", 0);
	obs_data_set_default_bool(settings, "show_cursor", true);
	obs_data_set_default_int(settings, "crop_mode", CROP_NONE);

	window_defaults(settings);
}

void load_crop_mode(enum crop_mode *mode, obs_data_t *settings)
{
	*mode = obs_data_get_int(settings, "crop_mode");
	if (!crop_mode_valid(*mode))
		*mode = CROP_NONE;
}

void load_crop(struct display_capture *dc, obs_data_t *settings)
{
	load_crop_mode(&dc->crop, settings);

#define CROP_VAR_NAME(var, mode) (mode "." #var)
#define LOAD_CROP_VAR(var, mode) \
	dc->crop_rect.var =      \
		obs_data_get_double(settings, CROP_VAR_NAME(var, mode));
	switch (dc->crop) {
	case CROP_MANUAL:
		LOAD_CROP_VAR(origin.x, "manual");
		LOAD_CROP_VAR(origin.y, "manual");
		LOAD_CROP_VAR(size.width, "manual");
		LOAD_CROP_VAR(size.height, "manual");
		break;

	case CROP_TO_WINDOW_AND_MANUAL:
		LOAD_CROP_VAR(origin.x, "window");
		LOAD_CROP_VAR(origin.y, "window");
		LOAD_CROP_VAR(size.width, "window");
		LOAD_CROP_VAR(size.height, "window");
		break;

	case CROP_NONE:
	case CROP_TO_WINDOW:
	case CROP_INVALID:
		break;
	}
#undef LOAD_CROP_VAR
}

static void display_capture_update(void *data, obs_data_t *settings)
{
	struct display_capture *dc = data;

	load_crop(dc, settings);

	if (requires_window(dc->crop))
		update_window(&dc->window, settings);

	unsigned display = obs_data_get_int(settings, "display");
	bool show_cursor = obs_data_get_bool(settings, "show_cursor");
	if (dc->display == display && dc->hide_cursor != show_cursor)
		return;

	obs_enter_graphics();

	destroy_display_stream(dc);
	dc->display = display;
	dc->hide_cursor = !show_cursor;
	init_display_stream(dc);

	obs_leave_graphics();
}

static bool switch_crop_mode(obs_properties_t *props, obs_property_t *p,
			     obs_data_t *settings)
{
	UNUSED_PARAMETER(p);

	enum crop_mode crop;
	load_crop_mode(&crop, settings);

	const char *name;
	bool visible;
#define LOAD_CROP_VAR(var, mode)         \
	name = CROP_VAR_NAME(var, mode); \
	obs_property_set_visible(obs_properties_get(props, name), visible);

	visible = crop == CROP_MANUAL;
	LOAD_CROP_VAR(origin.x, "manual");
	LOAD_CROP_VAR(origin.y, "manual");
	LOAD_CROP_VAR(size.width, "manual");
	LOAD_CROP_VAR(size.height, "manual");

	visible = crop == CROP_TO_WINDOW_AND_MANUAL;
	LOAD_CROP_VAR(origin.x, "window");
	LOAD_CROP_VAR(origin.y, "window");
	LOAD_CROP_VAR(size.width, "window");
	LOAD_CROP_VAR(size.height, "window");
#undef LOAD_CROP_VAR

	show_window_properties(props, visible || crop == CROP_TO_WINDOW);
	return true;
}

static const char *crop_names[] = {"CropMode.None", "CropMode.Manual",
				   "CropMode.ToWindow",
				   "CropMode.ToWindowAndManual"};

#ifndef COUNTOF
#define COUNTOF(x) (sizeof(x) / sizeof(x[0]))
#endif
static obs_properties_t *display_capture_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();

	obs_property_t *list = obs_properties_add_list(
		props, "display", obs_module_text("DisplayCapture.Display"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	for (unsigned i = 0; i < [NSScreen screens].count; i++) {
		char buf[10];
		sprintf(buf, "%u", i);
		obs_property_list_add_int(list, buf, i);
	}

	obs_properties_add_bool(props, "show_cursor",
				obs_module_text("DisplayCapture.ShowCursor"));

	obs_property_t *crop = obs_properties_add_list(
		props, "crop_mode", obs_module_text("CropMode"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_set_modified_callback(crop, switch_crop_mode);

	for (unsigned i = 0; i < COUNTOF(crop_names); i++) {
		const char *name = obs_module_text(crop_names[i]);
		obs_property_list_add_int(crop, name, i);
	}

	add_window_properties(props);
	show_window_properties(props, false);

	obs_property_t *p;
	const char *name;
	float min;
#define LOAD_CROP_VAR(var, mode)                                               \
	name = CROP_VAR_NAME(var, mode);                                       \
	p = obs_properties_add_float(                                          \
		props, name, obs_module_text("Crop." #var), min, 4096.f, .5f); \
	obs_property_set_visible(p, false);

	min = 0.f;
	LOAD_CROP_VAR(origin.x, "manual");
	LOAD_CROP_VAR(origin.y, "manual");
	LOAD_CROP_VAR(size.width, "manual");
	LOAD_CROP_VAR(size.height, "manual");

	min = -4096.f;
	LOAD_CROP_VAR(origin.x, "window");
	LOAD_CROP_VAR(origin.y, "window");
	LOAD_CROP_VAR(size.width, "window");
	LOAD_CROP_VAR(size.height, "window");
#undef LOAD_CROP_VAR

	return props;
}

struct obs_source_info display_capture_info = {
	.id = "display_capture",
	.type = OBS_SOURCE_TYPE_INPUT,
	.get_name = display_capture_getname,

	.create = display_capture_create,
	.destroy = display_capture_destroy,

	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW |
			OBS_SOURCE_DO_NOT_DUPLICATE,
	.video_tick = display_capture_video_tick,
	.video_render = display_capture_video_render,

	.get_width = display_capture_getwidth,
	.get_height = display_capture_getheight,

	.get_defaults = display_capture_defaults,
	.get_properties = display_capture_properties,
	.update = display_capture_update,
	.icon_type = OBS_ICON_TYPE_DESKTOP_CAPTURE,
};
