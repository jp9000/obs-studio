#include <stdlib.h>
#include <util/dstr.h>
#include <util/threading.h>
#include "dc-capture.h"
#include "window-helpers.h"
#include "../../libobs/util/platform.h"
#include "../../libobs-winrt/winrt-capture.h"

/* clang-format off */

#define TEXT_WINDOW_CAPTURE obs_module_text("WindowCapture")
#define TEXT_WINDOW         obs_module_text("WindowCapture.Window")
#define TEXT_METHOD         obs_module_text("WindowCapture.Method")
#define TEXT_METHOD_AUTO    obs_module_text("WindowCapture.Method.Auto")
#define TEXT_METHOD_BITBLT  obs_module_text("WindowCapture.Method.BitBlt")
#define TEXT_METHOD_WGC     obs_module_text("WindowCapture.Method.WindowsGraphicsCapture")
#define TEXT_MATCH_PRIORITY obs_module_text("WindowCapture.Priority")
#define TEXT_MATCH_TITLE    obs_module_text("WindowCapture.Priority.Title")
#define TEXT_MATCH_CLASS    obs_module_text("WindowCapture.Priority.Class")
#define TEXT_MATCH_EXE      obs_module_text("WindowCapture.Priority.Exe")
#define TEXT_CAPTURE_CURSOR obs_module_text("CaptureCursor")
#define TEXT_COMPATIBILITY  obs_module_text("Compatibility")
#define TEXT_CLIENT_AREA    obs_module_text("ClientArea")

/* clang-format on */

#define WC_CHECK_TIMER 1.0f

typedef BOOL (*PFN_winrt_capture_supported)();
typedef BOOL (*PFN_winrt_capture_cursor_toggle_supported)();
typedef struct winrt_capture *(*PFN_winrt_capture_init_window)(
	BOOL cursor, HWND window, BOOL client_area);
typedef void (*PFN_winrt_capture_free)(struct winrt_capture *capture);

typedef BOOL (*PFN_winrt_capture_active)(const struct winrt_capture *capture);
typedef void (*PFN_winrt_capture_show_cursor)(struct winrt_capture *capture,
					      BOOL visible);
typedef void (*PFN_winrt_capture_render)(struct winrt_capture *capture,
					 gs_effect_t *effect);
typedef uint32_t (*PFN_winrt_capture_width)(const struct winrt_capture *capture);
typedef uint32_t (*PFN_winrt_capture_height)(
	const struct winrt_capture *capture);

struct winrt_exports {
	PFN_winrt_capture_supported winrt_capture_supported;
	PFN_winrt_capture_cursor_toggle_supported
		winrt_capture_cursor_toggle_supported;
	PFN_winrt_capture_init_window winrt_capture_init_window;
	PFN_winrt_capture_free winrt_capture_free;
	PFN_winrt_capture_active winrt_capture_active;
	PFN_winrt_capture_show_cursor winrt_capture_show_cursor;
	PFN_winrt_capture_render winrt_capture_render;
	PFN_winrt_capture_width winrt_capture_width;
	PFN_winrt_capture_height winrt_capture_height;
};

enum window_capture_method {
	METHOD_AUTO,
	METHOD_BITBLT,
	METHOD_WGC,
};

struct window_capture {
	obs_source_t *source;

	pthread_mutex_t update_mutex;

	char *title;
	char *class;
	char *executable;
	enum window_capture_method method;
	enum window_priority priority;
	bool cursor;
	bool compatibility;
	bool client_area;
	bool use_wildcards; /* TODO */

	struct dc_capture capture;

	bool wgc_supported;
	bool previously_failed;
	void *winrt_module;
	struct winrt_exports exports;
	struct winrt_capture *capture_winrt;

	float resize_timer;
	float check_window_timer;
	float cursor_check_time;

	HWND window;
	RECT last_rect;
};

static const char *wgc_partial_match_classes[] = {
	"Chrome",
	"Mozilla",
	NULL,
};

static const char *wgc_whole_match_classes[] = {
	"ApplicationFrameWindow",
	"Windows.UI.Core.CoreWindow",
	"XLMAIN",        /* excel*/
	"PPTFrameClass", /* powerpoint */
	"OpusApp",       /* word */
	NULL,
};

static enum window_capture_method
choose_method(enum window_capture_method method, bool wgc_supported,
	      const char *current_class)
{
	if (!wgc_supported)
		return METHOD_BITBLT;

	if (method != METHOD_AUTO)
		return method;

	if (!current_class)
		return METHOD_BITBLT;

	const char **class = wgc_partial_match_classes;
	while (*class) {
		if (astrstri(current_class, *class) != NULL) {
			return METHOD_WGC;
		}
		class ++;
	}

	class = wgc_whole_match_classes;
	while (*class) {
		if (astrcmpi(current_class, *class) == 0) {
			return METHOD_WGC;
		}
		class ++;
	}

	return METHOD_BITBLT;
}

static const char *get_method_name(int method)
{
	const char *method_name = "";
	switch (method) {
	case METHOD_AUTO:
		method_name = "Automatic";
		break;
	case METHOD_BITBLT:
		method_name = "BitBlt";
		break;
	case METHOD_WGC:
		method_name = "WGC";
		break;
	}

	return method_name;
}

static void log_settings(struct window_capture *wc, obs_data_t *s)
{
	int method = (int)obs_data_get_int(s, "method");

	if (wc->title != NULL) {
		blog(LOG_INFO,
		     "[window-capture: '%s'] update settings:\n"
		     "\texecutable: %s\n"
		     "\tmethod selected: %s\n"
		     "\tmethod chosen: %s\n",
		     obs_source_get_name(wc->source), wc->executable,
		     get_method_name(method), get_method_name(wc->method));
		blog(LOG_DEBUG, "\tclass:      %s", wc->class);
	}
}

static void update_settings(struct window_capture *wc, obs_data_t *s)
{
	pthread_mutex_lock(&wc->update_mutex);

	int method = (int)obs_data_get_int(s, "method");
	const char *window = obs_data_get_string(s, "window");
	int priority = (int)obs_data_get_int(s, "priority");

	bfree(wc->title);
	bfree(wc->class);
	bfree(wc->executable);

	build_window_strings(window, &wc->class, &wc->title, &wc->executable);

	wc->method = choose_method(method, wc->wgc_supported, wc->class);
	wc->priority = (enum window_priority)priority;
	wc->cursor = obs_data_get_bool(s, "cursor");
	wc->use_wildcards = obs_data_get_bool(s, "use_wildcards");
	wc->compatibility = obs_data_get_bool(s, "compatibility");
	wc->client_area = obs_data_get_bool(s, "client_area");

	pthread_mutex_unlock(&wc->update_mutex);
}

/* ------------------------------------------------------------------------- */

static const char *wc_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return TEXT_WINDOW_CAPTURE;
}

#define WINRT_IMPORT(func)                                           \
	do {                                                         \
		exports->func = (PFN_##func)os_dlsym(module, #func); \
		if (!exports->func) {                                \
			success = false;                             \
			blog(LOG_ERROR,                              \
			     "Could not load function '%s' from "    \
			     "module '%s'",                          \
			     #func, module_name);                    \
		}                                                    \
	} while (false)

static bool load_winrt_imports(struct winrt_exports *exports, void *module,
			       const char *module_name)
{
	bool success = true;

	WINRT_IMPORT(winrt_capture_supported);
	WINRT_IMPORT(winrt_capture_cursor_toggle_supported);
	WINRT_IMPORT(winrt_capture_init_window);
	WINRT_IMPORT(winrt_capture_free);
	WINRT_IMPORT(winrt_capture_active);
	WINRT_IMPORT(winrt_capture_show_cursor);
	WINRT_IMPORT(winrt_capture_render);
	WINRT_IMPORT(winrt_capture_width);
	WINRT_IMPORT(winrt_capture_height);

	return success;
}

static void *wc_create(obs_data_t *settings, obs_source_t *source)
{
	struct window_capture *wc = bzalloc(sizeof(struct window_capture));
	wc->source = source;

	pthread_mutex_init(&wc->update_mutex, NULL);

	obs_enter_graphics();
	const bool uses_d3d11 = gs_get_device_type() == GS_DEVICE_DIRECT3D_11;
	obs_leave_graphics();

	if (uses_d3d11) {
		static const char *const module = "libobs-winrt";
		wc->winrt_module = os_dlopen(module);
		if (wc->winrt_module &&
		    load_winrt_imports(&wc->exports, wc->winrt_module,
				       module) &&
		    wc->exports.winrt_capture_supported()) {
			wc->wgc_supported = true;
		}
	}

	update_settings(wc, settings);
	log_settings(wc, settings);
	return wc;
}

static void wc_actual_destroy(void *data)
{
	struct window_capture *wc = data;

	if (wc->capture_winrt) {
		wc->exports.winrt_capture_free(wc->capture_winrt);
	}

	obs_enter_graphics();
	dc_capture_free(&wc->capture);
	obs_leave_graphics();

	bfree(wc->title);
	bfree(wc->class);
	bfree(wc->executable);

	if (wc->winrt_module)
		os_dlclose(wc->winrt_module);

	pthread_mutex_destroy(&wc->update_mutex);

	bfree(wc);
}

static void wc_destroy(void *data)
{
	obs_queue_task(OBS_TASK_GRAPHICS, wc_actual_destroy, data, false);
}

static void wc_update(void *data, obs_data_t *settings)
{
	struct window_capture *wc = data;
	update_settings(wc, settings);
	log_settings(wc, settings);

	/* forces a reset */
	wc->window = NULL;
	wc->check_window_timer = WC_CHECK_TIMER;

	wc->previously_failed = false;
}

static uint32_t wc_width(void *data)
{
	struct window_capture *wc = data;
	return (wc->method == METHOD_WGC)
		       ? wc->exports.winrt_capture_width(wc->capture_winrt)
		       : wc->capture.width;
}

static uint32_t wc_height(void *data)
{
	struct window_capture *wc = data;
	return (wc->method == METHOD_WGC)
		       ? wc->exports.winrt_capture_height(wc->capture_winrt)
		       : wc->capture.height;
}

static void wc_defaults(obs_data_t *defaults)
{
	obs_data_set_default_int(defaults, "method", METHOD_AUTO);
	obs_data_set_default_bool(defaults, "cursor", true);
	obs_data_set_default_bool(defaults, "compatibility", false);
	obs_data_set_default_bool(defaults, "client_area", true);
}

static void update_settings_visibility(obs_properties_t *props,
				       struct window_capture *wc)
{
	pthread_mutex_lock(&wc->update_mutex);

	const enum window_capture_method method = wc->method;
	const bool bitblt_options = method == METHOD_BITBLT;
	const bool wgc_options = method == METHOD_WGC;

	const bool wgc_cursor_toggle =
		wgc_options &&
		wc->exports.winrt_capture_cursor_toggle_supported();

	obs_property_t *p = obs_properties_get(props, "cursor");
	obs_property_set_visible(p, bitblt_options || wgc_cursor_toggle);

	p = obs_properties_get(props, "compatibility");
	obs_property_set_visible(p, bitblt_options);

	p = obs_properties_get(props, "client_area");
	obs_property_set_visible(p, wgc_options);

	pthread_mutex_unlock(&wc->update_mutex);
}

static bool wc_capture_method_changed(obs_properties_t *props,
				      obs_property_t *p, obs_data_t *settings)
{
	UNUSED_PARAMETER(p);

	struct window_capture *wc = obs_properties_get_param(props);
	update_settings(wc, settings);

	update_settings_visibility(props, wc);

	return true;
}

extern bool check_window_property_setting(obs_properties_t *ppts,
					  obs_property_t *p,
					  obs_data_t *settings, const char *val,
					  size_t idx);

static bool wc_window_changed(obs_properties_t *props, obs_property_t *p,
			      obs_data_t *settings)
{
	struct window_capture *wc = obs_properties_get_param(props);
	update_settings(wc, settings);

	update_settings_visibility(props, wc);

	check_window_property_setting(props, p, settings, "window", 0);
	return true;
}

static obs_properties_t *wc_properties(void *data)
{
	struct window_capture *wc = data;

	obs_properties_t *ppts = obs_properties_create();
	obs_properties_set_param(ppts, wc, NULL);

	obs_property_t *p;

	p = obs_properties_add_list(ppts, "window", TEXT_WINDOW,
				    OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);
	fill_window_list(p, EXCLUDE_MINIMIZED, NULL);
	obs_property_set_modified_callback(p, wc_window_changed);

	p = obs_properties_add_list(ppts, "method", TEXT_METHOD,
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, TEXT_METHOD_AUTO, METHOD_AUTO);
	obs_property_list_add_int(p, TEXT_METHOD_BITBLT, METHOD_BITBLT);
	obs_property_list_add_int(p, TEXT_METHOD_WGC, METHOD_WGC);
	obs_property_list_item_disable(p, 2, !wc->wgc_supported);
	obs_property_set_modified_callback(p, wc_capture_method_changed);

	p = obs_properties_add_list(ppts, "priority", TEXT_MATCH_PRIORITY,
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, TEXT_MATCH_TITLE, WINDOW_PRIORITY_TITLE);
	obs_property_list_add_int(p, TEXT_MATCH_CLASS, WINDOW_PRIORITY_CLASS);
	obs_property_list_add_int(p, TEXT_MATCH_EXE, WINDOW_PRIORITY_EXE);

	obs_properties_add_bool(ppts, "cursor", TEXT_CAPTURE_CURSOR);

	obs_properties_add_bool(ppts, "compatibility", TEXT_COMPATIBILITY);

	obs_properties_add_bool(ppts, "client_area", TEXT_CLIENT_AREA);

	return ppts;
}

static void wc_hide(void *data)
{
	struct window_capture *wc = data;

	if (wc->capture_winrt) {
		wc->exports.winrt_capture_free(wc->capture_winrt);
		wc->capture_winrt = NULL;
	}

	memset(&wc->last_rect, 0, sizeof(wc->last_rect));
}

#define RESIZE_CHECK_TIME 0.2f
#define CURSOR_CHECK_TIME 0.2f

static void wc_tick(void *data, float seconds)
{
	struct window_capture *wc = data;
	RECT rect;
	bool reset_capture = false;

	if (!obs_source_showing(wc->source))
		return;

	if (!wc->window || !IsWindow(wc->window)) {
		if (!wc->title && !wc->class)
			return;

		wc->check_window_timer += seconds;

		if (wc->check_window_timer < WC_CHECK_TIMER) {
			if (wc->capture.valid)
				dc_capture_free(&wc->capture);
			return;
		}

		if (wc->capture_winrt) {
			wc->exports.winrt_capture_free(wc->capture_winrt);
			wc->capture_winrt = NULL;
		}

		wc->check_window_timer = 0.0f;

		wc->window = (wc->method == METHOD_WGC)
				     ? find_window_top_level(INCLUDE_MINIMIZED,
							     wc->priority,
							     wc->class,
							     wc->title,
							     wc->executable)
				     : find_window(INCLUDE_MINIMIZED,
						   wc->priority, wc->class,
						   wc->title, wc->executable);
		if (!wc->window) {
			if (wc->capture.valid)
				dc_capture_free(&wc->capture);
			return;
		}

		wc->previously_failed = false;
		reset_capture = true;

	} else if (IsIconic(wc->window)) {
		return;
	}

	wc->cursor_check_time += seconds;
	if (wc->cursor_check_time > CURSOR_CHECK_TIME) {
		DWORD foreground_pid, target_pid;

		// Can't just compare the window handle in case of app with child windows
		if (!GetWindowThreadProcessId(GetForegroundWindow(),
					      &foreground_pid))
			foreground_pid = 0;

		if (!GetWindowThreadProcessId(wc->window, &target_pid))
			target_pid = 0;

		const bool cursor_hidden = foreground_pid && target_pid &&
					   foreground_pid != target_pid;
		wc->capture.cursor_hidden = cursor_hidden;
		if (wc->capture_winrt)
			wc->exports.winrt_capture_show_cursor(wc->capture_winrt,
							      !cursor_hidden);

		wc->cursor_check_time = 0.0f;
	}

	obs_enter_graphics();

	if (wc->method == METHOD_BITBLT) {
		GetClientRect(wc->window, &rect);

		if (!reset_capture) {
			wc->resize_timer += seconds;

			if (wc->resize_timer >= RESIZE_CHECK_TIME) {
				if ((rect.bottom - rect.top) !=
					    (wc->last_rect.bottom -
					     wc->last_rect.top) ||
				    (rect.right - rect.left) !=
					    (wc->last_rect.right -
					     wc->last_rect.left))
					reset_capture = true;

				wc->resize_timer = 0.0f;
			}
		}

		if (reset_capture) {
			wc->resize_timer = 0.0f;
			wc->last_rect = rect;
			dc_capture_free(&wc->capture);
			dc_capture_init(&wc->capture, 0, 0,
					rect.right - rect.left,
					rect.bottom - rect.top, wc->cursor,
					wc->compatibility);
		}

		dc_capture_capture(&wc->capture, wc->window);
	} else if (wc->method == METHOD_WGC) {
		if (wc->window && (wc->capture_winrt == NULL)) {
			if (!wc->previously_failed) {
				wc->capture_winrt =
					wc->exports.winrt_capture_init_window(
						wc->cursor, wc->window,
						wc->client_area);

				if (!wc->capture_winrt) {
					wc->previously_failed = true;
				}
			}
		}
	}

	obs_leave_graphics();
}

static void wc_render(void *data, gs_effect_t *effect)
{
	struct window_capture *wc = data;
	gs_effect_t *const opaque = obs_get_base_effect(OBS_EFFECT_OPAQUE);
	if (wc->method == METHOD_WGC) {
		if (wc->capture_winrt) {
			if (wc->exports.winrt_capture_active(
				    wc->capture_winrt)) {
				wc->exports.winrt_capture_render(
					wc->capture_winrt, opaque);
			} else {
				wc->exports.winrt_capture_free(
					wc->capture_winrt);
				wc->capture_winrt = NULL;
			}
		}
	} else {
		dc_capture_render(&wc->capture, opaque);
	}

	UNUSED_PARAMETER(effect);
}

struct obs_source_info window_capture_info = {
	.id = "window_capture",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = wc_getname,
	.create = wc_create,
	.destroy = wc_destroy,
	.update = wc_update,
	.video_render = wc_render,
	.hide = wc_hide,
	.video_tick = wc_tick,
	.get_width = wc_width,
	.get_height = wc_height,
	.get_defaults = wc_defaults,
	.get_properties = wc_properties,
	.icon_type = OBS_ICON_TYPE_WINDOW_CAPTURE,
};
