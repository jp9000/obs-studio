#include <stdio.h>
#include <time.h>
#include <windows.h>

#include <util/base.h>
#include <graphics/vec2.h>
#include <media-io/audio-resampler.h>
#include <obs.h>

#include <intrin.h>

static const int cx = 800;
static const int cy = 600;

/* --------------------------------------------------- */

class SourceContext {
	obs_source_t *source;

public:
	inline SourceContext(obs_source_t *source) : source(source) {}
	inline ~SourceContext() { obs_source_release(source); }
	inline operator obs_source_t *() { return source; }
};

/* --------------------------------------------------- */

class SceneContext {
	obs_scene_t *scene;

public:
	inline SceneContext(obs_scene_t *scene) : scene(scene) {}
	inline ~SceneContext() { obs_scene_release(scene); }
	inline operator obs_scene_t *() { return scene; }
};

/* --------------------------------------------------- */

class DisplayContext {
	obs_display_t *display;

public:
	inline DisplayContext(obs_display_t *display) : display(display) {}
	inline ~DisplayContext() { obs_display_destroy(display); }
	inline operator obs_display_t *() { return display; }
};

/* --------------------------------------------------- */

static LRESULT CALLBACK sceneProc(HWND hwnd, UINT message, WPARAM wParam,
				  LPARAM lParam)
{
	switch (message) {

	case WM_CLOSE:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	return 0;
}

static void do_log(int log_level, const char *msg, va_list args, void *param)
{
	char bla[4096];
	vsnprintf(bla, 4095, msg, args);

	OutputDebugStringA(bla);
	OutputDebugStringA("\n");

	if (log_level < LOG_WARNING)
		__debugbreak();

	UNUSED_PARAMETER(param);
}

static void CreateOBS(HWND hwnd)
{
	RECT rc;
	GetClientRect(hwnd, &rc);

	if (!obs_startup("en-US", nullptr, nullptr))
		throw "Couldn't create OBS";

	struct obs_video_info ovi;
	ovi.adapter = 0;
	ovi.base_width = rc.right;
	ovi.base_height = rc.bottom;
	ovi.fps_num = 30000;
	ovi.fps_den = 1001;
	ovi.graphics_module = DL_OPENGL;
	ovi.output_format = VIDEO_FORMAT_RGBA;
	ovi.output_width = rc.right;
	ovi.output_height = rc.bottom;

	if (obs_reset_video(&ovi) != 0)
		throw "Couldn't initialize video";
}

static DisplayContext CreateDisplay(HWND hwnd)
{
	RECT rc;
	GetClientRect(hwnd, &rc);

	gs_init_data info = {};
	info.cx = rc.right;
	info.cy = rc.bottom;
	info.format = GS_RGBA;
	info.zsformat = GS_ZS_NONE;
	info.window.hwnd = hwnd;

	return obs_display_create(&info, 0);
}

static void AddTestItems(obs_scene_t *scene, obs_source_t *source)
{
	obs_sceneitem_t *item = NULL;
	struct vec2 scale;

	vec2_set(&scale, 20.0f, 20.0f);

	item = obs_scene_add(scene, source);
	obs_sceneitem_set_scale(item, &scale);
}

static HWND CreateTestWindow(HINSTANCE instance)
{
	WNDCLASS wc;

	memset(&wc, 0, sizeof(wc));
	wc.lpszClassName = TEXT("bla");
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.hInstance = instance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpfnWndProc = (WNDPROC)sceneProc;

	if (!RegisterClass(&wc))
		return 0;

	return CreateWindow(TEXT("bla"), TEXT("bla"),
			    WS_OVERLAPPEDWINDOW | WS_VISIBLE, 1920 / 2 - cx / 2,
			    1080 / 2 - cy / 2, cx, cy, NULL, NULL, instance,
			    NULL);
}

/* --------------------------------------------------- */

static void RenderWindow(void *data, uint32_t cx, uint32_t cy)
{
	obs_render_main_texture();

	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(cx);
	UNUSED_PARAMETER(cy);
}

/* --------------------------------------------------- */

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdLine,
		   int numCmd)
{
	HWND hwnd = NULL;
	base_set_log_handler(do_log, nullptr);

	try {
		hwnd = CreateTestWindow(instance);
		if (!hwnd)
			throw "Couldn't create main window";

		/* ------------------------------------------------------ */
		/* create OBS */
		CreateOBS(hwnd);

		/* ------------------------------------------------------ */
		/* load modules */
		obs_load_all_modules();

		/* ------------------------------------------------------ */
		/* create source */
		SourceContext source = obs_source_create(
			"random", "some randon source", NULL, nullptr);
		if (!source)
			throw "Couldn't create random test source";

		/* ------------------------------------------------------ */
		/* create filter */
		SourceContext filter = obs_source_create(
			"test_filter", "a nice green filter", NULL, nullptr);
		if (!filter)
			throw "Couldn't create test filter";
		obs_source_filter_add(source, filter);

		/* ------------------------------------------------------ */
		/* create scene and add source to scene (twice) */
		SceneContext scene = obs_scene_create("test scene");
		if (!scene)
			throw "Couldn't create scene";

		AddTestItems(scene, source);

		/* ------------------------------------------------------ */
		/* set the scene as the primary draw source and go */
		obs_set_output_source(0, obs_scene_get_source(scene));

		/* ------------------------------------------------------ */
		/* create display for output and set the output render callback */
		DisplayContext display = CreateDisplay(hwnd);
		obs_display_add_draw_callback(display, RenderWindow, nullptr);

		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

	} catch (char *error) {
		MessageBoxA(NULL, error, NULL, 0);
	}

	obs_shutdown();

	blog(LOG_INFO, "Number of memory leaks: %ld", bnum_allocs());
	DestroyWindow(hwnd);

	UNUSED_PARAMETER(prevInstance);
	UNUSED_PARAMETER(cmdLine);
	UNUSED_PARAMETER(numCmd);
	return 0;
}
