/******************************************************************************
    Copyright (C) 2014 by Zachary Lund <admin@computerquip.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

/* Version 2 of the GLX backend...
 * Difference from version 1 is that we use XCB to help alleviate
 * pains in a threaded environment that is prone to error.
 * These errors must be readable and handled for the sake of,
 * not only the users' sanity, but my own.
 *
 * With that said, we have more error checking capabilities...
 * and not all of them are used to help simplify current code.
 *
 * TODO: Implement more complete error checking.
 * NOTE: GLX loading functions are placed illogically
 * 	for the sake of convenience.
 */

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>

#include <xcb/xcb.h>

#include <stdio.h>

#include "gl-nix.h"

#include <glad/glad_glx.h>

static const int ctx_attribs[] = {
#ifdef _DEBUG
	GLX_CONTEXT_FLAGS_ARB,
	GLX_CONTEXT_DEBUG_BIT_ARB,
#endif
	GLX_CONTEXT_PROFILE_MASK_ARB,
	GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
	GLX_CONTEXT_MAJOR_VERSION_ARB,
	3,
	GLX_CONTEXT_MINOR_VERSION_ARB,
	3,
	None,
};

static int ctx_pbuffer_attribs[] = {GLX_PBUFFER_WIDTH, 2, GLX_PBUFFER_HEIGHT, 2,
				    None};

static int ctx_visual_attribs[] = {GLX_STENCIL_SIZE,
				   0,
				   GLX_DEPTH_SIZE,
				   0,
				   GLX_BUFFER_SIZE,
				   32,
				   GLX_ALPHA_SIZE,
				   8,
				   GLX_DOUBLEBUFFER,
				   true,
				   GLX_X_RENDERABLE,
				   true,
				   None};

struct gl_windowinfo {
	/* We store this value since we can fetch a lot
	 * of information not only concerning the config
	 * but the visual, and various other settings
	 * for the context.
	 */
	GLXFBConfig config;

	/* Windows in X11 are defined with integers (XID).
	 * xcb_window_t is a define for this... they are
	 * compatible with Xlib as well.
	 */
	xcb_window_t window;

	/* We can't fetch screen without a request so we cache it. */
	int screen;
};

struct gl_platform {
	Display *display;
	GLXContext context;
	GLXPbuffer pbuffer;
};

/*
 * Since we cannot take advantage of the asynchronous nature of xcb,
 * all of the helper functions are synchronous but thread-safe.
 *
 * They check for errors and will return 0 on problems
 * with the exception of when 0 is a valid return value... in which case
 * read the specific function comments.
 */

/* Returns -1 on invalid screen. */
static int get_screen_num_from_xcb_screen(xcb_connection_t *xcb_conn,
					  xcb_screen_t *screen)
{
	xcb_screen_iterator_t iter =
		xcb_setup_roots_iterator(xcb_get_setup(xcb_conn));
	int screen_num = 0;

	for (; iter.rem; xcb_screen_next(&iter), ++screen_num)
		if (iter.data == screen)
			return screen_num;

	return -1;
}

static xcb_screen_t *get_screen_from_root(xcb_connection_t *xcb_conn,
					  xcb_window_t root)
{
	xcb_screen_iterator_t iter =
		xcb_setup_roots_iterator(xcb_get_setup(xcb_conn));

	while (iter.rem) {
		if (iter.data->root == root)
			return iter.data;

		xcb_screen_next(&iter);
	}

	return 0;
}

static inline int get_screen_num_from_root(xcb_connection_t *xcb_conn,
					   xcb_window_t root)
{
	xcb_screen_t *screen = get_screen_from_root(xcb_conn, root);

	if (!screen)
		return -1;

	return get_screen_num_from_xcb_screen(xcb_conn, screen);
}

static xcb_get_geometry_reply_t *get_window_geometry(xcb_connection_t *xcb_conn,
						     xcb_drawable_t drawable)
{
	xcb_get_geometry_cookie_t cookie;
	xcb_generic_error_t *error;
	xcb_get_geometry_reply_t *reply;

	cookie = xcb_get_geometry(xcb_conn, drawable);
	reply = xcb_get_geometry_reply(xcb_conn, cookie, &error);

	if (error) {
		blog(LOG_ERROR, "Failed to fetch parent window geometry!");
		free(error);
		free(reply);
		return 0;
	}

	free(error);
	return reply;
}

static bool gl_context_create(struct gl_platform *plat)
{
	Display *display = plat->display;
	int frame_buf_config_count = 0;
	GLXFBConfig *config = NULL;
	GLXContext context;
	bool success = false;

	if (!GLAD_GLX_ARB_create_context) {
		blog(LOG_ERROR, "ARB_GLX_create_context not supported!");
		return false;
	}

	config = glXChooseFBConfig(display, DefaultScreen(display),
				   ctx_visual_attribs, &frame_buf_config_count);
	if (!config) {
		blog(LOG_ERROR, "Failed to create OpenGL frame buffer config");
		return false;
	}

	context = glXCreateContextAttribsARB(display, config[0], NULL, true,
					     ctx_attribs);
	if (!context) {
		blog(LOG_ERROR, "Failed to create OpenGL context.");
		goto error;
	}

	plat->context = context;
	plat->display = display;

	plat->pbuffer =
		glXCreatePbuffer(display, config[0], ctx_pbuffer_attribs);
	if (!plat->pbuffer) {
		blog(LOG_ERROR, "Failed to create OpenGL pbuffer");
		goto error;
	}

	success = true;

error:
	XFree(config);
	XSync(display, false);
	return success;
}

static void gl_context_destroy(struct gl_platform *plat)
{
	Display *display = plat->display;

	glXMakeContextCurrent(display, None, None, NULL);
	glXDestroyContext(display, plat->context);
	bfree(plat);
}

static struct gl_windowinfo *
gl_x11_glx_windowinfo_create(const struct gs_init_data *info)
{
	UNUSED_PARAMETER(info);
	return bmalloc(sizeof(struct gl_windowinfo));
}

static void gl_x11_glx_windowinfo_destroy(struct gl_windowinfo *info)
{
	bfree(info);
}

static Display *open_windowless_display(void)
{
	Display *display = XOpenDisplay(NULL);
	xcb_connection_t *xcb_conn;
	xcb_screen_iterator_t screen_iterator;
	xcb_screen_t *screen;
	int screen_num;

	if (!display) {
		blog(LOG_ERROR, "Unable to open new X connection!");
		return NULL;
	}

	xcb_conn = XGetXCBConnection(display);
	if (!xcb_conn) {
		blog(LOG_ERROR, "Unable to get XCB connection to main display");
		goto error;
	}

	screen_iterator = xcb_setup_roots_iterator(xcb_get_setup(xcb_conn));
	screen = screen_iterator.data;
	if (!screen) {
		blog(LOG_ERROR, "Unable to get screen root");
		goto error;
	}

	screen_num = get_screen_num_from_root(xcb_conn, screen->root);
	if (screen_num == -1) {
		blog(LOG_ERROR, "Unable to get screen number from root");
		goto error;
	}

	if (!gladLoadGLX(display, screen_num)) {
		blog(LOG_ERROR, "Unable to load GLX entry functions.");
		goto error;
	}

	return display;

error:
	if (display)
		XCloseDisplay(display);
	return NULL;
}

static int x_error_handler(Display *display, XErrorEvent *error)
{
	char str1[512];
	char str2[512];
	char str3[512];
	XGetErrorText(display, error->error_code, str1, sizeof(str1));
	XGetErrorText(display, error->request_code, str2, sizeof(str2));
	XGetErrorText(display, error->minor_code, str3, sizeof(str3));

	blog(LOG_ERROR,
	     "X Error: %s, Major opcode: %s, "
	     "Minor opcode: %s, Serial: %lu",
	     str1, str2, str3, error->serial);
	return 0;
}

static struct gl_platform *gl_x11_glx_platform_create(gs_device_t *device,
						      uint32_t adapter)
{
	/* There's some trickery here... we're mixing libX11, xcb, and GLX
	   For an explanation see here: http://xcb.freedesktop.org/MixingCalls/
	   Essentially, GLX requires Xlib. Everything else we use xcb. */
	struct gl_platform *plat = bmalloc(sizeof(struct gl_platform));
	Display *display = open_windowless_display();

	if (!display) {
		goto fail_display_open;
	}

	XSetEventQueueOwner(display, XCBOwnsEventQueue);
	XSetErrorHandler(x_error_handler);

	/* We assume later that cur_swap is already set. */
	device->plat = plat;

	plat->display = display;

	if (!gl_context_create(plat)) {
		blog(LOG_ERROR, "Failed to create context!");
		goto fail_context_create;
	}

	if (!glXMakeContextCurrent(plat->display, plat->pbuffer, plat->pbuffer,
				   plat->context)) {
		blog(LOG_ERROR, "Failed to make context current.");
		goto fail_make_current;
	}

	if (!gladLoadGL()) {
		blog(LOG_ERROR, "Failed to load OpenGL entry functions.");
		goto fail_load_gl;
	}

	goto success;

fail_make_current:
	gl_context_destroy(plat);
fail_context_create:
fail_load_gl:
	XCloseDisplay(display);
fail_display_open:
	bfree(plat);
	plat = NULL;
success:
	UNUSED_PARAMETER(adapter);
	return plat;
}

static void gl_x11_glx_platform_destroy(struct gl_platform *plat)
{
	if (!plat) /* In what case would platform be invalid here? */
		return;

	gl_context_destroy(plat);
}

static bool gl_x11_glx_platform_init_swapchain(struct gs_swap_chain *swap)
{
	Display *display = swap->device->plat->display;
	xcb_connection_t *xcb_conn = XGetXCBConnection(display);
	xcb_window_t wid = xcb_generate_id(xcb_conn);
	xcb_window_t parent = swap->info.window.id;
	xcb_get_geometry_reply_t *geometry =
		get_window_geometry(xcb_conn, parent);
	bool status = false;

	int screen_num;
	int visual;
	GLXFBConfig *fb_config;

	if (!geometry)
		goto fail_geometry_request;

	screen_num = get_screen_num_from_root(xcb_conn, geometry->root);
	if (screen_num == -1) {
		goto fail_screen;
	}

	/* ...fetch the best match... */
	{
		int num_configs;
		fb_config = glXChooseFBConfig(display, screen_num,
					      ctx_visual_attribs, &num_configs);

		if (!fb_config || !num_configs) {
			blog(LOG_ERROR, "Failed to find FBConfig!");
			goto fail_fb_config;
		}
	}

	/* ...then fetch matching visual info for xcb. */
	{
		int error = glXGetFBConfigAttrib(display, fb_config[0],
						 GLX_VISUAL_ID, &visual);

		if (error) {
			blog(LOG_ERROR, "Bad call to GetFBConfigAttrib!");
			goto fail_visual_id;
		}
	}

	xcb_colormap_t colormap = xcb_generate_id(xcb_conn);
	uint32_t mask = XCB_CW_BORDER_PIXEL | XCB_CW_COLORMAP;
	uint32_t mask_values[] = {0, colormap, 0};

	xcb_create_colormap(xcb_conn, XCB_COLORMAP_ALLOC_NONE, colormap, parent,
			    visual);

	xcb_create_window(xcb_conn, 24 /* Hardcoded? */, wid, parent, 0, 0,
			  geometry->width, geometry->height, 0, 0, visual, mask,
			  mask_values);

	swap->wi->config = fb_config[0];
	swap->wi->window = wid;

	xcb_map_window(xcb_conn, wid);

	XFree(fb_config);
	status = true;
	goto success;

fail_visual_id:
	XFree(fb_config);
fail_fb_config:
fail_screen:
fail_geometry_request:
success:
	free(geometry);
	return status;
}

static void gl_x11_glx_platform_cleanup_swapchain(struct gs_swap_chain *swap)
{
	UNUSED_PARAMETER(swap);
	/* Really nothing to clean up? */
}

static void gl_x11_glx_device_enter_context(gs_device_t *device)
{
	GLXContext context = device->plat->context;
	Display *display = device->plat->display;

	if (device->cur_swap) {
		XID window = device->cur_swap->wi->window;
		if (!glXMakeContextCurrent(display, window, window, context)) {
			blog(LOG_ERROR, "Failed to make context current.");
		}
	} else {
		GLXPbuffer pbuf = device->plat->pbuffer;
		if (!glXMakeContextCurrent(display, pbuf, pbuf, context)) {
			blog(LOG_ERROR, "Failed to make context current.");
		}
	}
}

static void gl_x11_glx_device_leave_context(gs_device_t *device)
{
	Display *display = device->plat->display;

	if (!glXMakeContextCurrent(display, None, None, NULL)) {
		blog(LOG_ERROR, "Failed to reset current context.");
	}
}

static void *gl_x11_glx_device_get_device_obj(gs_device_t *device)
{
	return device->plat->context;
}

static void gl_x11_glx_getclientsize(const struct gs_swap_chain *swap,
				     uint32_t *width, uint32_t *height)
{
	xcb_connection_t *xcb_conn =
		XGetXCBConnection(swap->device->plat->display);
	xcb_window_t window = swap->wi->window;

	xcb_get_geometry_reply_t *geometry =
		get_window_geometry(xcb_conn, window);
	if (geometry) {
		*width = geometry->width;
		*height = geometry->height;
	}

	free(geometry);
}

static void gl_x11_glx_clear_context(gs_device_t *device)
{
	Display *display = device->plat->display;

	if (!glXMakeContextCurrent(display, None, None, NULL)) {
		blog(LOG_ERROR, "Failed to reset current context.");
	}
}

static void gl_x11_glx_update(gs_device_t *device)
{
	Display *display = device->plat->display;
	xcb_window_t window = device->cur_swap->wi->window;

	uint32_t values[] = {device->cur_swap->info.cx,
			     device->cur_swap->info.cy};

	xcb_configure_window(XGetXCBConnection(display), window,
			     XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
			     values);
}

static void gl_x11_glx_device_load_swapchain(gs_device_t *device,
					     gs_swapchain_t *swap)
{
	if (device->cur_swap == swap)
		return;

	Display *dpy = device->plat->display;
	GLXContext ctx = device->plat->context;

	device->cur_swap = swap;

	if (swap) {
		XID window = swap->wi->window;
		if (!glXMakeContextCurrent(dpy, window, window, ctx)) {
			blog(LOG_ERROR, "Failed to make context current.");
		}
	} else {
		GLXPbuffer pbuf = device->plat->pbuffer;
		if (!glXMakeContextCurrent(dpy, pbuf, pbuf, ctx)) {
			blog(LOG_ERROR, "Failed to make context current.");
		}
	}
}

enum swap_type {
	SWAP_TYPE_NORMAL,
	SWAP_TYPE_EXT,
	SWAP_TYPE_MESA,
	SWAP_TYPE_SGI,
};

static void gl_x11_glx_device_present(gs_device_t *device)
{
	static bool initialized = false;
	static enum swap_type swap_type = SWAP_TYPE_NORMAL;

	Display *display = device->plat->display;
	XID window = device->cur_swap->wi->window;

	if (!initialized) {
		if (GLAD_GLX_EXT_swap_control)
			swap_type = SWAP_TYPE_EXT;
		else if (GLAD_GLX_MESA_swap_control)
			swap_type = SWAP_TYPE_MESA;
		else if (GLAD_GLX_SGI_swap_control)
			swap_type = SWAP_TYPE_SGI;

		initialized = true;
	}

	xcb_connection_t *xcb_conn = XGetXCBConnection(display);
	xcb_generic_event_t *xcb_event;
	while ((xcb_event = xcb_poll_for_event(xcb_conn))) {
		/* TODO: Handle XCB events. */
		free(xcb_event);
	}

	switch (swap_type) {
	case SWAP_TYPE_EXT:
		glXSwapIntervalEXT(display, window, 0);
		break;
	case SWAP_TYPE_MESA:
		glXSwapIntervalMESA(0);
		break;
	case SWAP_TYPE_SGI:
		glXSwapIntervalSGI(0);
		break;
	case SWAP_TYPE_NORMAL:;
	}

	glXSwapBuffers(display, window);
}

static struct gs_texture *gl_x11_glx_device_texture_create_from_dmabuf(
	gs_device_t *device, unsigned int width, unsigned int height,
	enum gs_color_format color_format, uint32_t n_planes, const int *fds,
	const uint32_t *strides, const uint32_t *offsets,
	const uint64_t *modifiers)
{
	UNUSED_PARAMETER(device);
	UNUSED_PARAMETER(width);
	UNUSED_PARAMETER(height);
	UNUSED_PARAMETER(color_format);
	UNUSED_PARAMETER(n_planes);
	UNUSED_PARAMETER(fds);
	UNUSED_PARAMETER(strides);
	UNUSED_PARAMETER(offsets);
	UNUSED_PARAMETER(modifiers);

	return NULL;
}

static const struct gl_winsys_vtable glx_winsys_vtable = {
	.windowinfo_create = gl_x11_glx_windowinfo_create,
	.windowinfo_destroy = gl_x11_glx_windowinfo_destroy,
	.platform_create = gl_x11_glx_platform_create,
	.platform_destroy = gl_x11_glx_platform_destroy,
	.platform_init_swapchain = gl_x11_glx_platform_init_swapchain,
	.platform_cleanup_swapchain = gl_x11_glx_platform_cleanup_swapchain,
	.device_enter_context = gl_x11_glx_device_enter_context,
	.device_leave_context = gl_x11_glx_device_leave_context,
	.device_get_device_obj = gl_x11_glx_device_get_device_obj,
	.getclientsize = gl_x11_glx_getclientsize,
	.clear_context = gl_x11_glx_clear_context,
	.update = gl_x11_glx_update,
	.device_load_swapchain = gl_x11_glx_device_load_swapchain,
	.device_present = gl_x11_glx_device_present,
	.device_texture_create_from_dmabuf =
		gl_x11_glx_device_texture_create_from_dmabuf,
};

const struct gl_winsys_vtable *gl_x11_glx_get_winsys_vtable(void)
{
	return &glx_winsys_vtable;
}
