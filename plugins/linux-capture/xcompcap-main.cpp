#include <glad/glad.h>
#include <glad/glad_glx.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <pthread.h>

#include <vector>

#include <obs-module.h>
#include <graphics/vec4.h>
#include <util/platform.h>

#include "xcompcap-main.hpp"
#include "xcompcap-helper.hpp"
#include "xcursor.h"

#define xdisp (XCompcap::disp())
#define WIN_STRING_DIV "\r\n"

bool XCompcapMain::init()
{
	if (!xdisp) {
		blog(LOG_ERROR, "failed opening display");
		return false;
	}

	XInitThreads();

	int eventBase, errorBase;
	if (!XCompositeQueryExtension(xdisp, &eventBase, &errorBase)) {
		blog(LOG_ERROR, "Xcomposite extension not supported");
		return false;
	}

	int major = 0, minor = 2;
	XCompositeQueryVersion(xdisp, &major, &minor);

	if (major == 0 && minor < 2) {
		blog(LOG_ERROR, "Xcomposite extension is too old: %d.%d < 0.2",
		     major, minor);
		return false;
	}

	return true;
}

void XCompcapMain::deinit()
{
	XCompcap::cleanupDisplay();
}

obs_properties_t *XCompcapMain::properties()
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *wins = obs_properties_add_list(
		props, "capture_window", obs_module_text("Window"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	for (Window win : XCompcap::getTopLevelWindows()) {
		std::string wname = XCompcap::getWindowName(win);
		std::string cls = XCompcap::getWindowClass(win);
		std::string winid = std::to_string((long long)win);
		std::string desc =
			(winid + WIN_STRING_DIV + wname + WIN_STRING_DIV + cls);

		obs_property_list_add_string(wins, wname.c_str(), desc.c_str());
	}

	obs_properties_add_int(props, "cut_top", obs_module_text("CropTop"), 0,
			       4096, 1);
	obs_properties_add_int(props, "cut_left", obs_module_text("CropLeft"),
			       0, 4096, 1);
	obs_properties_add_int(props, "cut_right", obs_module_text("CropRight"),
			       0, 4096, 1);
	obs_properties_add_int(props, "cut_bot", obs_module_text("CropBottom"),
			       0, 4096, 1);

	obs_properties_add_bool(props, "swap_redblue",
				obs_module_text("SwapRedBlue"));
	obs_properties_add_bool(props, "lock_x", obs_module_text("LockX"));

	obs_properties_add_bool(props, "show_cursor",
				obs_module_text("CaptureCursor"));

	obs_properties_add_bool(props, "include_border",
				obs_module_text("IncludeXBorder"));

	obs_properties_add_bool(props, "exclude_alpha",
				obs_module_text("ExcludeAlpha"));

	return props;
}

void XCompcapMain::defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "capture_window", "");
	obs_data_set_default_int(settings, "cut_top", 0);
	obs_data_set_default_int(settings, "cut_left", 0);
	obs_data_set_default_int(settings, "cut_right", 0);
	obs_data_set_default_int(settings, "cut_bot", 0);
	obs_data_set_default_bool(settings, "swap_redblue", false);
	obs_data_set_default_bool(settings, "lock_x", false);
	obs_data_set_default_bool(settings, "show_cursor", true);
	obs_data_set_default_bool(settings, "include_border", false);
	obs_data_set_default_bool(settings, "exclude_alpha", false);
}

#define FIND_WINDOW_INTERVAL 0.5

struct XCompcapMain_private {
	XCompcapMain_private()
		: win(0),
		  cut_top(0),
		  cur_cut_top(0),
		  cut_left(0),
		  cur_cut_left(0),
		  cut_right(0),
		  cur_cut_right(0),
		  cut_bot(0),
		  cur_cut_bot(0),
		  inverted(false),
		  width(0),
		  height(0),
		  pixmap(0),
		  glxpixmap(0),
		  tex(0),
		  gltex(0)
	{
		pthread_mutexattr_init(&lockattr);
		pthread_mutexattr_settype(&lockattr, PTHREAD_MUTEX_RECURSIVE);

		pthread_mutex_init(&lock, &lockattr);
	}

	~XCompcapMain_private()
	{
		pthread_mutex_destroy(&lock);
		pthread_mutexattr_destroy(&lockattr);
	}

	obs_source_t *source;

	std::string windowName;
	Window win = 0;
	int cut_top, cur_cut_top;
	int cut_left, cur_cut_left;
	int cut_right, cur_cut_right;
	int cut_bot, cur_cut_bot;
	bool inverted;
	bool swapRedBlue;
	bool lockX;
	bool include_border;
	bool exclude_alpha;
	bool draw_opaque;

	double window_check_time = 0.0;

	uint32_t width;
	uint32_t height;
	uint32_t border;

	Pixmap pixmap;
	GLXPixmap glxpixmap;
	gs_texture_t *tex;
	gs_texture_t *gltex;

	pthread_mutex_t lock;
	pthread_mutexattr_t lockattr;

	bool show_cursor = true;
	bool cursor_outside = false;
	xcursor_t *cursor = nullptr;
};

XCompcapMain::XCompcapMain(obs_data_t *settings, obs_source_t *source)
{
	p = new XCompcapMain_private;
	p->source = source;

	obs_enter_graphics();
	p->cursor = xcursor_init(xdisp);
	obs_leave_graphics();

	updateSettings(settings);
}

static void xcc_cleanup(XCompcapMain_private *p);

XCompcapMain::~XCompcapMain()
{
	ObsGsContextHolder obsctx;

	XCompcap::unregisterSource(this);

	if (p->tex) {
		gs_texture_destroy(p->tex);
		p->tex = 0;
	}

	xcc_cleanup(p);

	if (p->cursor) {
		xcursor_destroy(p->cursor);
		p->cursor = nullptr;
	}

	delete p;
}

static Window getWindowFromString(std::string wstr)
{
	XErrorLock xlock;

	if (wstr == "") {
		return XCompcap::getTopLevelWindows().front();
	}

	if (wstr.substr(0, 4) == "root") {
		int i = std::stoi("0" + wstr.substr(4));
		return RootWindow(xdisp, i);
	}

	size_t firstMark = wstr.find(WIN_STRING_DIV);
	size_t lastMark = wstr.rfind(WIN_STRING_DIV);
	size_t markSize = strlen(WIN_STRING_DIV);

	// wstr only consists of the window-id
	if (firstMark == std::string::npos)
		return (Window)std::stol(wstr);

	// wstr also contains window-name and window-class
	std::string wid = wstr.substr(0, firstMark);
	std::string wname = wstr.substr(firstMark + markSize,
					lastMark - firstMark - markSize);
	std::string wcls = wstr.substr(lastMark + markSize);

	Window winById = (Window)std::stol(wid);

	// first try to find a match by the window-id
	for (Window cwin : XCompcap::getTopLevelWindows()) {
		// match by window-id
		if (cwin == winById) {
			return cwin;
		}
	}

	// then try to find a match by name & class
	for (Window cwin : XCompcap::getTopLevelWindows()) {
		std::string cwinname = XCompcap::getWindowName(cwin);
		std::string ccls = XCompcap::getWindowClass(cwin);

		// match by name and class
		if (wname == cwinname && wcls == ccls) {
			return cwin;
		}
	}

	// no match
	blog(LOG_DEBUG, "Did not find Window By ID %s, Name '%s' or Class '%s'",
	     wid.c_str(), wname.c_str(), wcls.c_str());
	return 0;
}

static void xcc_cleanup(XCompcapMain_private *p)
{
	PLock lock(&p->lock);
	XErrorLock xlock;

	if (p->gltex) {
		GLuint gltex = *(GLuint *)gs_texture_get_obj(p->gltex);
		glBindTexture(GL_TEXTURE_2D, gltex);
		if (p->glxpixmap) {
			glXReleaseTexImageEXT(xdisp, p->glxpixmap,
					      GLX_FRONT_LEFT_EXT);
			if (xlock.gotError()) {
				blog(LOG_ERROR,
				     "cleanup glXReleaseTexImageEXT failed: %s",
				     xlock.getErrorText().c_str());
				xlock.resetError();
			}
			glXDestroyPixmap(xdisp, p->glxpixmap);
			if (xlock.gotError()) {
				blog(LOG_ERROR,
				     "cleanup glXDestroyPixmap failed: %s",
				     xlock.getErrorText().c_str());
				xlock.resetError();
			}
			p->glxpixmap = 0;
		}
		gs_texture_destroy(p->gltex);
		p->gltex = 0;
	}

	if (p->pixmap) {
		XFreePixmap(xdisp, p->pixmap);
		if (xlock.gotError()) {
			blog(LOG_ERROR, "cleanup glXDestroyPixmap failed: %s",
			     xlock.getErrorText().c_str());
			xlock.resetError();
		}
		p->pixmap = 0;
	}

	if (p->win) {
		p->win = 0;
	}

	if (p->tex) {
		gs_texture_destroy(p->tex);
		p->tex = 0;
	}
}

static gs_color_format gs_format_from_tex()
{
	GLint iformat = 0;
	// consider GL_ARB_internalformat_query
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT,
				 &iformat);

	// These formats are known to be wrong on Intel platforms. We intentionally
	// use swapped internal formats here to preserve historic behavior which
	// swapped colors accidentally and because D3D11 would not support a
	// GS_RGBX format
	switch (iformat) {
	case GL_RGB:
		return GS_BGRX_UNORM;
	case GL_RGBA:
		return GS_RGBA_UNORM;
	default:
		return GS_RGBA_UNORM;
	}
}

// from libobs-opengl/gl-subsystem.h because we need to handle GLX modifying textures outside libobs.
struct fb_info;

struct gs_texture {
	gs_device_t *device;
	enum gs_texture_type type;
	enum gs_color_format format;
	GLenum gl_format;
	GLenum gl_target;
	GLenum gl_internal_format;
	GLenum gl_type;
	GLuint texture;
	uint32_t levels;
	bool is_dynamic;
	bool is_render_target;
	bool is_dummy;
	bool gen_mipmaps;

	gs_samplerstate_t *cur_sampler;
	struct fbo_info *fbo;
};
// End shitty hack.

void XCompcapMain::updateSettings(obs_data_t *settings)
{
	ObsGsContextHolder obsctx;
	XErrorLock xlock;

	PLock lock(&p->lock);

	blog(LOG_DEBUG, "Settings updating");

	Window prevWin = p->win;

	xcc_cleanup(p);

	if (settings) {
		/* Settings initialized or changed */
		const char *windowName =
			obs_data_get_string(settings, "capture_window");

		p->windowName = windowName;
		p->win = getWindowFromString(windowName);
		XCompcap::registerSource(this, p->win);

		p->cut_top = obs_data_get_int(settings, "cut_top");
		p->cut_left = obs_data_get_int(settings, "cut_left");
		p->cut_right = obs_data_get_int(settings, "cut_right");
		p->cut_bot = obs_data_get_int(settings, "cut_bot");
		p->lockX = obs_data_get_bool(settings, "lock_x");
		p->swapRedBlue = obs_data_get_bool(settings, "swap_redblue");
		p->show_cursor = obs_data_get_bool(settings, "show_cursor");
		p->include_border =
			obs_data_get_bool(settings, "include_border");
		p->exclude_alpha = obs_data_get_bool(settings, "exclude_alpha");
		p->draw_opaque = false;
	} else {
		/* New Window found (stored in p->win), just re-initialize GL-Mapping  */
		p->win = prevWin;
	}

	if (xlock.gotError()) {
		blog(LOG_ERROR, "registeringSource failed: %s",
		     xlock.getErrorText().c_str());
		return;
	}
	XSync(xdisp, 0);

	XWindowAttributes attr;
	if (!p->win || !XGetWindowAttributes(xdisp, p->win, &attr)) {
		p->win = 0;
		p->width = 0;
		p->height = 0;
		return;
	}

	if (p->win && p->cursor && p->show_cursor) {
		Window child;
		int x, y;

		XTranslateCoordinates(xdisp, p->win, attr.root, 0, 0, &x, &y,
				      &child);
		xcursor_offset(p->cursor, x, y);
	}

	const int config_attrs[] = {GLX_BIND_TO_TEXTURE_RGBA_EXT,
				    GL_TRUE,
				    GLX_DRAWABLE_TYPE,
				    GLX_PIXMAP_BIT,
				    GLX_BIND_TO_TEXTURE_TARGETS_EXT,
				    GLX_TEXTURE_2D_BIT_EXT,
				    GLX_DOUBLEBUFFER,
				    GL_FALSE,
				    None};
	int nelem = 0;
	GLXFBConfig *configs = glXChooseFBConfig(
		xdisp, XCompcap::getRootWindowScreen(attr.root), config_attrs,
		&nelem);

	bool found = false;
	GLXFBConfig config;
	for (int i = 0; i < nelem; i++) {
		config = configs[i];
		XVisualInfo *visual = glXGetVisualFromFBConfig(xdisp, config);
		if (!visual)
			continue;

		if (attr.depth != visual->depth) {
			XFree(visual);
			continue;
		}
		XFree(visual);
		found = true;
		break;
	}
	if (!found) {
		blog(LOG_ERROR, "no matching fb config found");
		p->win = 0;
		p->height = 0;
		p->width = 0;
		XFree(configs);
		return;
	}

	if (p->exclude_alpha || attr.depth != 32) {
		p->draw_opaque = true;
	}

	int inverted;
	glXGetFBConfigAttrib(xdisp, config, GLX_Y_INVERTED_EXT, &inverted);
	p->inverted = inverted != 0;

	p->border = attr.border_width;

	if (p->include_border) {
		p->width = attr.width + p->border * 2;
		p->height = attr.height + p->border * 2;
	} else {
		p->width = attr.width;
		p->height = attr.height;
	}

	if (p->cut_top + p->cut_bot < (int)p->height) {
		p->cur_cut_top = p->cut_top;
		p->cur_cut_bot = p->cut_bot;
	} else {
		p->cur_cut_top = 0;
		p->cur_cut_bot = 0;
	}

	if (p->cut_left + p->cut_right < (int)p->width) {
		p->cur_cut_left = p->cut_left;
		p->cur_cut_right = p->cut_right;
	} else {
		p->cur_cut_left = 0;
		p->cur_cut_right = 0;
	}

	// Precautionary since we dont error check every GLX call above.
	xlock.resetError();

	p->pixmap = XCompositeNameWindowPixmap(xdisp, p->win);
	if (xlock.gotError()) {
		blog(LOG_ERROR, "XCompositeNameWindowPixmap failed: %s",
		     xlock.getErrorText().c_str());
		p->pixmap = 0;
		XFree(configs);
		return;
	}

	// Should be consistent format with config we are using. Since we searched on RGBA lets use RGBA here.
	const int pixmap_attrs[] = {GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
				    GLX_TEXTURE_FORMAT_EXT,
				    GLX_TEXTURE_FORMAT_RGBA_EXT, None};

	p->glxpixmap = glXCreatePixmap(xdisp, config, p->pixmap, pixmap_attrs);
	if (xlock.gotError()) {
		blog(LOG_ERROR, "glXCreatePixmap failed: %s",
		     xlock.getErrorText().c_str());
		XFreePixmap(xdisp, p->pixmap);
		XFree(configs);
		p->pixmap = 0;
		p->glxpixmap = 0;
		return;
	}
	XFree(configs);

	// Build an OBS texture to bind the pixmap to.
	p->gltex = gs_texture_create(p->width, p->height, GS_RGBA_UNORM, 1, 0,
				     GS_GL_DUMMYTEX);
	GLuint gltex = *(GLuint *)gs_texture_get_obj(p->gltex);
	glBindTexture(GL_TEXTURE_2D, gltex);
	glXBindTexImageEXT(xdisp, p->glxpixmap, GLX_FRONT_LEFT_EXT, NULL);
	if (xlock.gotError()) {
		blog(LOG_ERROR, "glXBindTexImageEXT failed: %s",
		     xlock.getErrorText().c_str());
		XFreePixmap(xdisp, p->pixmap);
		XFree(configs);
		p->pixmap = 0;
		p->glxpixmap = 0;
		return;
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// glxBindTexImageEXT might modify the textures format.
	gs_color_format format = gs_format_from_tex();
	glBindTexture(GL_TEXTURE_2D, 0);
	// sync OBS texture format based on any glxBindTexImageEXT changes
	p->gltex->format = format;

	// Create a pure OBS texture to use for rendering. Using the same
	// format so we can copy instead of drawing from the source gltex.
	if (p->tex)
		gs_texture_destroy(p->tex);
	p->tex = gs_texture_create(width(), height(), format, 1, 0,
				   GS_GL_DUMMYTEX);
	if (p->swapRedBlue) {
		GLuint tex = *(GLuint *)gs_texture_get_obj(p->tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	if (!p->windowName.empty()) {
		blog(LOG_INFO,
		     "[window-capture: '%s'] update settings:\n"
		     "\ttitle: %s\n"
		     "\tclass: %s\n"
		     "\tBit depth: %i\n"
		     "\tFound proper GLXFBConfig (in %i): %s\n",
		     obs_source_get_name(p->source),
		     XCompcap::getWindowName(p->win).c_str(),
		     XCompcap::getWindowClass(p->win).c_str(), attr.depth,
		     nelem, found ? "yes" : "no");
	}
}

void XCompcapMain::tick(float seconds)
{
	if (!obs_source_showing(p->source))
		return;

	// Must be taken before xlock to prevent deadlock on shutdown
	ObsGsContextHolder obsctx;

	PLock lock(&p->lock, true);

	if (!lock.isLocked())
		return;

	XCompcap::processEvents();

	if (p->win && XCompcap::sourceWasReconfigured(this)) {
		p->window_check_time = FIND_WINDOW_INTERVAL;
		p->win = 0;
	}

	XDisplayLock xlock;
	XWindowAttributes attr;

	if (!p->win || !XGetWindowAttributes(xdisp, p->win, &attr)) {
		p->window_check_time += (double)seconds;

		if (p->window_check_time < FIND_WINDOW_INTERVAL)
			return;

		Window newWin = getWindowFromString(p->windowName);

		p->window_check_time = 0.0;

		if (newWin && XGetWindowAttributes(xdisp, newWin, &attr)) {
			p->win = newWin;
			XCompcap::registerSource(this, p->win);
			updateSettings(0);
		} else {
			return;
		}
	}

	if (!p->tex || !p->gltex)
		return;

	if (p->lockX) {
		// XDisplayLock is still live so we should already be locked.
		XLockDisplay(xdisp);
		XSync(xdisp, 0);
	}

	if (p->include_border) {
		gs_copy_texture_region(p->tex, 0, 0, p->gltex, p->cur_cut_left,
				       p->cur_cut_top, width(), height());
	} else {
		gs_copy_texture_region(p->tex, 0, 0, p->gltex,
				       p->cur_cut_left + p->border,
				       p->cur_cut_top + p->border, width(),
				       height());
	}

	if (p->cursor && p->show_cursor) {
		xcursor_tick(p->cursor);

		p->cursor_outside =
			p->cursor->x < p->cur_cut_left ||
			p->cursor->y < p->cur_cut_top ||
			p->cursor->x > int(p->width - p->cur_cut_right) ||
			p->cursor->y > int(p->height - p->cur_cut_bot);
	}

	if (p->lockX)
		XUnlockDisplay(xdisp);
}

void XCompcapMain::render(gs_effect_t *effect)
{
	if (!p->win)
		return;

	PLock lock(&p->lock, true);

	if (p->draw_opaque)
		effect = obs_get_base_effect(OBS_EFFECT_OPAQUE);
	else
		effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);

	if (!lock.isLocked() || !p->tex)
		return;

	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, p->tex);

	while (gs_effect_loop(effect, "Draw")) {
		gs_draw_sprite(p->tex, 0, 0, 0);
	}

	if (p->cursor && p->gltex && p->show_cursor && !p->cursor_outside) {
		effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);

		while (gs_effect_loop(effect, "Draw")) {
			xcursor_render(p->cursor, -p->cur_cut_left,
				       -p->cur_cut_top);
		}
	}
}

uint32_t XCompcapMain::width()
{
	if (!p->win)
		return 0;

	return p->width - p->cur_cut_left - p->cur_cut_right;
}

uint32_t XCompcapMain::height()
{
	if (!p->win)
		return 0;

	return p->height - p->cur_cut_bot - p->cur_cut_top;
}
