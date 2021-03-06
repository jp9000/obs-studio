#include <obs-module.h>
#include <util/darray.h>
#include <util/threading.h>
#include <util/platform.h>

#import <CoreGraphics/CGWindow.h>
#import <Cocoa/Cocoa.h>

#include "window-utils.h"

struct window_capture {
	obs_source_t *source;

	struct cocoa_window window;

	//CGRect              bounds;
	//CGWindowListOption  window_option;
	CGWindowImageOption image_option;

	CGColorSpaceRef color_space;

	DARRAY(uint8_t) buffer;

	pthread_t capture_thread;
	os_event_t *capture_event;
	os_event_t *stop_event;
};

static CGImageRef get_image(struct window_capture *wc)
{
	NSArray *arr = (NSArray *)CGWindowListCreate(
		kCGWindowListOptionIncludingWindow, wc->window.window_id);
	[arr autorelease];

	if (!arr.count && !find_window(&wc->window, NULL, false))
		return NULL;

	return CGWindowListCreateImage(CGRectNull,
				       kCGWindowListOptionIncludingWindow,
				       wc->window.window_id, wc->image_option);
}

static inline void capture_frame(struct window_capture *wc)
{
	uint64_t ts = os_gettime_ns();
	CGImageRef img = get_image(wc);
	if (!img)
		return;

	size_t width = CGImageGetWidth(img);
	size_t height = CGImageGetHeight(img);

	CGRect rect = {{0, 0}, {width, height}};
	da_resize(wc->buffer, width * height * 4);
	uint8_t *data = wc->buffer.array;

	CGContextRef cg_context = CGBitmapContextCreate(
		data, width, height, 8, width * 4, wc->color_space,
		kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);
	CGContextSetBlendMode(cg_context, kCGBlendModeCopy);
	CGContextDrawImage(cg_context, rect, img);
	CGContextRelease(cg_context);
	CGImageRelease(img);

	struct obs_source_frame frame = {
		.format = VIDEO_FORMAT_BGRA,
		.width = width,
		.height = height,
		.data[0] = data,
		.linesize[0] = width * 4,
		.timestamp = ts,
	};

	obs_source_output_video(wc->source, &frame);
}

static void *capture_thread(void *data)
{
	struct window_capture *wc = data;

	for (;;) {
		os_event_wait(wc->capture_event);
		if (os_event_try(wc->stop_event) != EAGAIN)
			break;

		@autoreleasepool {
			capture_frame(wc);
		}
	}

	return NULL;
}

static inline void *window_capture_create_internal(obs_data_t *settings,
						   obs_source_t *source)
{
	struct window_capture *wc = bzalloc(sizeof(struct window_capture));

	wc->source = source;

	wc->color_space = CGColorSpaceCreateDeviceRGB();

	da_init(wc->buffer);

	init_window(&wc->window, settings);

	wc->image_option = obs_data_get_bool(settings, "show_shadow")
				   ? kCGWindowImageDefault
				   : kCGWindowImageBoundsIgnoreFraming;

	os_event_init(&wc->capture_event, OS_EVENT_TYPE_AUTO);
	os_event_init(&wc->stop_event, OS_EVENT_TYPE_MANUAL);

	pthread_create(&wc->capture_thread, NULL, capture_thread, wc);

	return wc;
}

static void *window_capture_create(obs_data_t *settings, obs_source_t *source)
{
	@autoreleasepool {
		return window_capture_create_internal(settings, source);
	}
}

static void window_capture_destroy(void *data)
{
	struct window_capture *cap = data;

	os_event_signal(cap->stop_event);
	os_event_signal(cap->capture_event);

	pthread_join(cap->capture_thread, NULL);

	CGColorSpaceRelease(cap->color_space);

	da_free(cap->buffer);

	os_event_destroy(cap->capture_event);
	os_event_destroy(cap->stop_event);

	destroy_window(&cap->window);

	bfree(cap);
}

static void window_capture_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "show_shadow", false);
	window_defaults(settings);
}

static obs_properties_t *window_capture_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();

	add_window_properties(props);

	obs_properties_add_bool(props, "show_shadow",
				obs_module_text("WindowCapture.ShowShadow"));

	return props;
}

static inline void window_capture_update_internal(struct window_capture *wc,
						  obs_data_t *settings)
{
	wc->image_option = obs_data_get_bool(settings, "show_shadow")
				   ? kCGWindowImageDefault
				   : kCGWindowImageBoundsIgnoreFraming;

	update_window(&wc->window, settings);

	if (wc->window.window_name.length) {
		blog(LOG_INFO,
		     "[window-capture: '%s'] update settings:\n"
		     "\twindow: %s\n"
		     "\towner:  %s",
		     obs_source_get_name(wc->source),
		     [wc->window.window_name UTF8String],
		     [wc->window.owner_name UTF8String]);
	}
}

static void window_capture_update(void *data, obs_data_t *settings)
{
	@autoreleasepool {
		return window_capture_update_internal(data, settings);
	}
}

static const char *window_capture_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("WindowCapture");
}

static inline void window_capture_tick_internal(struct window_capture *wc,
						float seconds)
{
	UNUSED_PARAMETER(seconds);
	os_event_signal(wc->capture_event);
}

static void window_capture_tick(void *data, float seconds)
{
	struct window_capture *wc = data;

	if (!obs_source_showing(wc->source))
		return;

	@autoreleasepool {
		return window_capture_tick_internal(data, seconds);
	}
}

struct obs_source_info window_capture_info = {
	.id = "window_capture",
	.type = OBS_SOURCE_TYPE_INPUT,
	.get_name = window_capture_getname,

	.create = window_capture_create,
	.destroy = window_capture_destroy,

	.output_flags = OBS_SOURCE_ASYNC_VIDEO,
	.video_tick = window_capture_tick,

	.get_defaults = window_capture_defaults,
	.get_properties = window_capture_properties,
	.update = window_capture_update,
	.icon_type = OBS_ICON_TYPE_WINDOW_CAPTURE,
};
