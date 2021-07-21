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

	CGWindowImageOption image_option;

	DARRAY(uint8_t) buffer;

	pthread_t capture_thread;
	os_event_t *capture_event;
	os_event_t *stop_event;
};

static CGImageRef get_image(struct window_capture *wc)
{
	CFArrayRef arr = CGWindowListCreate(kCGWindowListOptionIncludingWindow,
					    wc->window.window_id);

	if (!CFArrayGetCount(arr)) {
		CFRelease(arr);

		if (!find_window(&wc->window, NULL))
			return NULL;

		arr = CGWindowListCreate(kCGWindowListOptionIncludingWindow,
					 wc->window.window_id);

		if (!CFArrayGetCount(arr))
			return NULL;
	}

	CGImageRef image = CGWindowListCreateImageFromArray(CGRectNull, arr,
							    wc->image_option);

	CFRelease(arr);

	return image;
}

static inline void capture_frame(struct window_capture *wc)
{
	uint64_t ts = os_gettime_ns();
	CGImageRef image = get_image(wc);

	if (!image)
		return;

	size_t width = CGImageGetWidth(image);
	size_t height = CGImageGetHeight(image);

	if (CGImageGetBitsPerPixel(image) != 32 ||
	    CGImageGetBitsPerComponent(image) != 8) {
		CGImageRelease(image);
		return;
	}

	CGDataProviderRef provider = CGImageGetDataProvider(image);
	CFDataRef data = CGDataProviderCopyData(provider);

	da_resize(wc->buffer, CFDataGetLength(data));
	uint8_t *buffer = wc->buffer.array;

	CFDataGetBytes(data, CFRangeMake(0, CFDataGetLength(data)), buffer);

	struct obs_source_frame frame = {
		.format = VIDEO_FORMAT_BGRA,
		.width = width,
		.height = height,
		.data[0] = buffer,
		.linesize[0] = CGImageGetBytesPerRow(image),
		.timestamp = ts,
	};

	obs_source_output_video(wc->source, &frame);

	CFRelease(data);
	CGImageRelease(image);
}

static void *capture_thread(void *data)
{
	struct window_capture *wc = data;

	os_set_thread_name(obs_source_get_name(wc->source));

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

	da_init(wc->buffer);

	init_window(&wc->window, settings);

	wc->image_option = obs_data_get_bool(settings, "show_shadow")
				   ? kCGWindowImageDefault
				   : kCGWindowImageBoundsIgnoreFraming;

	os_event_init(&wc->capture_event, OS_EVENT_TYPE_AUTO);
	os_event_init(&wc->stop_event, OS_EVENT_TYPE_MANUAL);

	assert([NSThread isMultiThreaded] == 1);

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
	struct window_capture *wc = data;

	os_event_signal(wc->stop_event);
	os_event_signal(wc->capture_event);

	pthread_join(wc->capture_thread, NULL);

	da_free(wc->buffer);

	os_event_destroy(wc->capture_event);
	os_event_destroy(wc->stop_event);

	destroy_window(&wc->window);

	bfree(wc);
}

static void window_capture_defaults(obs_data_t *settings)
{
	window_defaults(settings);

	obs_data_set_default_bool(settings, "show_shadow", false);
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
