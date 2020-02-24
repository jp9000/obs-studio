#include <stdlib.h>
#include <util/dstr.h>
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

/* clang-format on */

#define WC_CHECK_TIMER 1.0f

struct winrt_exports {
	bool *(*winrt_capture_supported)();
	struct winrt_capture *(*winrt_capture_init)(bool cursor, HWND window);
	void (*winrt_capture_free)(struct winrt_capture *capture);
	void (*winrt_capture_render)(struct winrt_capture *capture,
				     gs_effect_t *effect);
	int32_t (*winrt_capture_width)(const struct winrt_capture *capture);
	int32_t (*winrt_capture_height)(const struct winrt_capture *capture);
};

enum window_capture_method {
	METHOD_AUTO,
	METHOD_BITBLT,
	METHOD_WGC,
};

struct window_capture {
	obs_source_t *source;

	char *title;
	char *class;
	char *executable;
	enum window_capture_method method;
	enum window_priority priority;
	bool auto_choose_method;
	bool cursor;
	bool compatibility;
	bool use_wildcards; /* TODO */

	struct dc_capture capture;

	bool wgc_supported;
	void *winrt_module;
	struct winrt_exports exports;
	struct winrt_capture *capture_winrt;

	float resize_timer;
	float check_window_timer;
	float cursor_check_time;

	HWND window;
	RECT last_rect;
};

static void update_settings(struct window_capture *wc, obs_data_t *s)
{
	int method = (int)obs_data_get_int(s, "method");
	const char *window = obs_data_get_string(s, "window");
	int priority = (int)obs_data_get_int(s, "priority");

	bfree(wc->title);
	bfree(wc->class);
	bfree(wc->executable);

	build_window_strings(window, &wc->class, &wc->title, &wc->executable);

	if (wc->title != NULL) {
		blog(LOG_INFO,
		     "[window-capture: '%s'] update settings:\n"
		     "\texecutable: %s",
		     obs_source_get_name(wc->source), wc->executable);
		blog(LOG_DEBUG, "\tclass:      %s", wc->class);
	}

	if (!wc->wgc_supported) {
		method = METHOD_BITBLT;
	}

	wc->method = method;
	wc->priority = (enum window_priority)priority;
	wc->auto_choose_method = (method == METHOD_AUTO);
	wc->cursor = obs_data_get_bool(s, "cursor");
	wc->use_wildcards = obs_data_get_bool(s, "use_wildcards");
	wc->compatibility = obs_data_get_bool(s, "compatibility");
}

/* ------------------------------------------------------------------------- */

static const char *wc_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return TEXT_WINDOW_CAPTURE;
}

#define WINRT_IMPORT(func)                                        \
	do {                                                      \
		exports->func = os_dlsym(module, #func);          \
		if (!exports->func) {                             \
			success = false;                          \
			blog(LOG_ERROR,                           \
			     "Could not load function '%s' from " \
			     "module '%s'",                       \
			     #func, module_name);                 \
		}                                                 \
	} while (false)

static bool load_winrt_imports(struct winrt_exports *exports, void *module,
			       const char *module_name)
{
	bool success = true;

	WINRT_IMPORT(winrt_capture_supported);
	WINRT_IMPORT(winrt_capture_init);
	WINRT_IMPORT(winrt_capture_free);
	WINRT_IMPORT(winrt_capture_render);
	WINRT_IMPORT(winrt_capture_width);
	WINRT_IMPORT(winrt_capture_height);

	return success;
}

static void *wc_create(obs_data_t *settings, obs_source_t *source)
{
	struct window_capture *wc = bzalloc(sizeof(struct window_capture));
	wc->source = source;

	obs_enter_graphics();
	const bool uses_d3d11 = gs_get_device_type() == GS_DEVICE_DIRECT3D_11;
	obs_leave_graphics();

	if (uses_d3d11) {
		static const char *const module = "libobs-winrt";
		bool use_winrt_capture = false;
		wc->winrt_module = os_dlopen(module);
		if (wc->winrt_module &&
		    load_winrt_imports(&wc->exports, wc->winrt_module,
				       module) &&
		    wc->exports.winrt_capture_supported()) {
			wc->wgc_supported = true;
		}
	}

	update_settings(wc, settings);
	return wc;
}

static void wc_destroy(void *data)
{
	struct window_capture *wc = data;

	if (wc) {
		obs_enter_graphics();
		dc_capture_free(&wc->capture);
		obs_leave_graphics();

		bfree(wc->title);
		bfree(wc->class);
		bfree(wc->executable);

		if (wc->winrt_module)
			os_dlclose(wc->winrt_module);

		bfree(wc);
	}
}

static void wc_update(void *data, obs_data_t *settings)
{
	struct window_capture *wc = data;
	update_settings(wc, settings);

	/* forces a reset */
	wc->window = NULL;
	wc->check_window_timer = WC_CHECK_TIMER;
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
}

static void encode_dstr(struct dstr *str)
{
	dstr_replace(str, "#", "#22");
	dstr_replace(str, ":", "#3A");
}

static char *decode_str(const char *src)
{
	struct dstr str = {0};
	dstr_copy(&str, src);
	dstr_replace(&str, "#3A", ":");
	dstr_replace(&str, "#22", "#");
	return str.array;
}

static bool window_changed_callback(obs_properties_t *props, obs_property_t *p,
				    obs_data_t *settings)
{
	const char *window = obs_data_get_string(settings, "capturedWindow");
	size_t size = obs_property_list_item_count(p);
	bool found = false;
	bool disabled_unknown_found = false;

	for (size_t i = 0; i < size; i++) {
		if (strcmp(obs_property_list_item_string(p, i), window) == 0) {
			found = true;
			continue;
		}
		if (obs_property_list_item_disabled(p, i))
			disabled_unknown_found = true;
	}

	if (!found && !disabled_unknown_found) {
		struct dstr name = {0};

		char *process = decode_str(strrchr(window, ':') + 1);
		size_t titleLen = strchr(window, ':') - window;
		char title[512];

		strncpy(title, window, titleLen);
		title[titleLen] = '\0';

		char *dec_title = decode_str(title);

		dstr_printf(&name, "[%s]: %s", process, dec_title);

		size_t idx =
			obs_property_list_add_string(p, name.array, window);
		obs_property_list_item_disable(p, idx, true);

		dstr_free(&name);

		bfree(process);
		bfree(dec_title);

		return true;
	}

	if (found && !disabled_unknown_found)
		return false;

	for (size_t i = 0; i < size;) {
		if (obs_property_list_item_disabled(p, i)) {
			obs_property_list_item_remove(p, i);
			continue;
		}
		i += 1;
	}

	return true;
}

static bool wc_capture_method_changed(obs_properties_t *props,
				      obs_property_t *p, obs_data_t *settings)
{
	const int method = (int)obs_data_get_int(settings, "method");
	const bool show_options = method != METHOD_WGC;

	p = obs_properties_get(props, "cursor");
	obs_property_set_visible(p, show_options);

	p = obs_properties_get(props, "compatibility");
	obs_property_set_visible(p, show_options);

	return true;
}

static obs_properties_t *wc_properties(void *data)
{
	struct window_capture *wc = data;

	obs_properties_t *ppts = obs_properties_create();
	obs_property_t *p;

	p = obs_properties_add_list(ppts, "method", TEXT_METHOD,
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, TEXT_METHOD_AUTO, METHOD_AUTO);
	obs_property_list_add_int(p, TEXT_METHOD_BITBLT, METHOD_BITBLT);
	obs_property_list_add_int(p, TEXT_METHOD_WGC, METHOD_WGC);
	obs_property_list_item_disable(p, 1, !wc->wgc_supported);
	obs_property_set_modified_callback(p, wc_capture_method_changed);

	p = obs_properties_add_list(ppts, "window", TEXT_WINDOW,
				    OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);
	fill_window_list(p, EXCLUDE_MINIMIZED, NULL);

	obs_property_set_modified_callback(p, window_changed_callback);

	p = obs_properties_add_list(ppts, "priority", TEXT_MATCH_PRIORITY,
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, TEXT_MATCH_TITLE, WINDOW_PRIORITY_TITLE);
	obs_property_list_add_int(p, TEXT_MATCH_CLASS, WINDOW_PRIORITY_CLASS);
	obs_property_list_add_int(p, TEXT_MATCH_EXE, WINDOW_PRIORITY_EXE);

	obs_properties_add_bool(ppts, "cursor", TEXT_CAPTURE_CURSOR);

	obs_properties_add_bool(ppts, "compatibility", TEXT_COMPATIBILITY);

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

static void auto_choose_method(struct window_capture *wc)
{
	wc->method = METHOD_BITBLT;

	if (!wc->class) {
		return;
	}

	const char **class = wgc_partial_match_classes;
	while (*class) {
		if (astrstri(wc->class, *class) != NULL) {
			wc->method = METHOD_WGC;
			return;
		}
		class ++;
	}

	class = wgc_whole_match_classes;
	while (*class) {
		if (astrcmpi(wc->class, *class) == 0) {
			wc->method = METHOD_WGC;
			return;
		}
		class ++;
	}
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

		if (wc->auto_choose_method) {
			auto_choose_method(wc);
		}

		wc->check_window_timer = 0.0f;

		wc->window = (wc->method == METHOD_WGC)
				     ? find_window_top_level(EXCLUDE_MINIMIZED,
							     wc->priority,
							     wc->class,
							     wc->title,
							     wc->executable)
				     : find_window(EXCLUDE_MINIMIZED,
						   wc->priority, wc->class,
						   wc->title, wc->executable);
		if (!wc->window) {
			obs_data_t *settings =
				obs_source_get_settings(wc->source);
			const char *window =
				obs_data_get_string(settings, "window");

			obs_data_set_string(settings, "capturedWindow", window);

			if (wc->capture.valid)
				dc_capture_free(&wc->capture);
			return;
		}

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

		wc->capture.cursor_hidden = foreground_pid && target_pid &&
					    foreground_pid != target_pid;

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
			wc->capture_winrt = wc->exports.winrt_capture_init(
				wc->cursor, wc->window);
		}
	}
	if (reset_capture) {
		struct dstr title = {0};
		struct dstr class = {0};
		struct dstr exec = {0};
		struct dstr win = {0};

		get_window_exe(&exec, wc->window);
		get_window_class(&class, wc->window);
		get_window_title(&title, wc->window);

		encode_dstr(&title);
		encode_dstr(&class);
		encode_dstr(&exec);

		dstr_printf(&win, "%s:%s:%s", title.array, class.array,
			    exec.array);

		obs_data_t *settings = obs_source_get_settings(wc->source);

		obs_data_set_string(settings, "capturedWindow", win.array);
		obs_source_update(wc->source, settings);

		obs_data_release(settings);
		dstr_free(&title);
		dstr_free(&class);
		dstr_free(&exec);
		dstr_free(&win);
	}

	obs_leave_graphics();
}

static void wc_render(void *data, gs_effect_t *effect)
{
	struct window_capture *wc = data;
	gs_effect_t *const opaque = obs_get_base_effect(OBS_EFFECT_OPAQUE);
	if (wc->method == METHOD_WGC)
		wc->exports.winrt_capture_render(wc->capture_winrt, opaque);
	else
		dc_capture_render(&wc->capture, opaque);

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
