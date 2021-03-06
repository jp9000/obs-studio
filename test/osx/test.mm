#include <stdio.h>
#include <time.h>

#include <functional>
#include <memory>

#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>
#import <OpenGL/OpenGL.h>

#include <util/base.h>
#include <obs.h>

static const int cx = 800;
static const int cy = 600;

/* --------------------------------------------------- */

template<typename T, typename D_T, D_T D>
struct OBSUniqueHandle : std::unique_ptr<T, std::function<D_T>> {
	using base = std::unique_ptr<T, std::function<D_T>>;
	explicit OBSUniqueHandle(T *obj = nullptr) : base(obj, D) {}
	operator T *() { return base::get(); }
};

#define DECLARE_DELETER(x) decltype(x), x

using SourceContext =
	OBSUniqueHandle<obs_source, DECLARE_DELETER(obs_source_release)>;

using SceneContext =
	OBSUniqueHandle<obs_scene, DECLARE_DELETER(obs_scene_release)>;

using DisplayContext =
	OBSUniqueHandle<obs_display, DECLARE_DELETER(obs_display_destroy)>;

#undef DECLARE_DELETER

/* --------------------------------------------------- */

static void CreateOBS()
{
	if (!obs_startup("en", nullptr, nullptr))
		throw "Couldn't create OBS";

	struct obs_video_info ovi;
	ovi.adapter = 0;
	ovi.fps_num = 30000;
	ovi.fps_den = 1001;
	ovi.graphics_module = DL_OPENGL;
	ovi.output_format = VIDEO_FORMAT_RGBA;
	ovi.base_width = cx;
	ovi.base_height = cy;
	ovi.output_width = cx;
	ovi.output_height = cy;

	if (obs_reset_video(&ovi) != 0)
		throw "Couldn't initialize video";
}

static DisplayContext CreateDisplay(NSView *view)
{
	gs_init_data info = {};
	info.cx = cx;
	info.cy = cy;
	info.format = GS_RGBA;
	info.zsformat = GS_ZS_NONE;
	info.window.view = view;

	return DisplayContext{obs_display_create(&info, 0)};
}

static SceneContext SetupScene()
{
	/* ------------------------------------------------------ */
	/* load modules */
	obs_load_all_modules();

	/* ------------------------------------------------------ */
	/* create source */
	SourceContext source{
		obs_source_create("random", "a test source", nullptr, nullptr)};
	if (!source)
		throw "Couldn't create random test source";

	/* ------------------------------------------------------ */
	/* create scene and add source to scene */
	SceneContext scene{obs_scene_create("test scene")};
	if (!scene)
		throw "Couldn't create scene";

	obs_scene_add(scene, source);

	/* ------------------------------------------------------ */
	/* set the scene as the primary draw source and go */
	obs_set_output_source(0, obs_scene_get_source(scene));

	return scene;
}

@interface OBSTest : NSObject <NSApplicationDelegate, NSWindowDelegate> {
	NSWindow *win;
	NSView *view;
	DisplayContext display;
	SceneContext scene;
}
- (void)applicationDidFinishLaunching:(NSNotification *)notification;
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app;
- (void)windowWillClose:(NSNotification *)notification;
@end

@implementation OBSTest
- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
	UNUSED_PARAMETER(notification);

	try {
		NSRect content_rect = NSMakeRect(0, 0, cx, cy);
		win = [[NSWindow alloc]
			initWithContentRect:content_rect
				  styleMask:NSTitledWindowMask |
					    NSClosableWindowMask
				    backing:NSBackingStoreBuffered
				      defer:NO];
		if (!win)
			throw "Could not create window";

		view = [[NSView alloc] initWithFrame:content_rect];
		if (!view)
			throw "Could not create view";

		CreateOBS();

		win.title = @"foo";
		win.delegate = self;
		win.contentView = view;

		[win orderFrontRegardless];
		[win center];
		[win makeMainWindow];

		display = CreateDisplay(view);

		scene = SetupScene();

		obs_display_add_draw_callback(
			display.get(),
			[](void *, uint32_t, uint32_t) {
				obs_render_main_texture();
			},
			nullptr);

	} catch (char const *error) {
		NSLog(@"%s\n", error);

		[NSApp terminate:nil];
	}
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app
{
	UNUSED_PARAMETER(app);

	return YES;
}

- (void)windowWillClose:(NSNotification *)notification
{
	UNUSED_PARAMETER(notification);

	obs_set_output_source(0, nullptr);
	scene.reset();
	display.reset();

	obs_shutdown();
	NSLog(@"Number of memory leaks: %lu", bnum_allocs());
}
@end

/* --------------------------------------------------- */

int main()
{
	@autoreleasepool {
		NSApplication *app = [NSApplication sharedApplication];
		OBSTest *test = [[OBSTest alloc] init];
		app.delegate = test;

		[app run];
	}

	return 0;
}
