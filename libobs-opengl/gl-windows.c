/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>

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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <util/darray.h>
#include "gl-subsystem.h"
#include <glad/glad_wgl.h>

/* Basically swapchain-specific information.  Fortunately for windows this is
 * super basic stuff */
struct gl_windowinfo {
	HWND hwnd;
	HDC hdc;
};

/* Like the other subsystems, the GL subsystem has one swap chain created by
 * default. */
struct gl_platform {
	HGLRC hrc;
	struct gl_windowinfo window;
};

/* For now, only support basic 32bit formats for graphics output. */
static inline int get_color_format_bits(enum gs_color_format format)
{
	switch ((uint32_t)format) {
	case GS_RGBA:
	case GS_BGRA:
		return 32;
	default:
		return 0;
	}
}

static inline int get_depth_format_bits(enum gs_zstencil_format zsformat)
{
	switch ((uint32_t)zsformat) {
	case GS_Z16:
		return 16;
	case GS_Z24_S8:
		return 24;
	default:
		return 0;
	}
}

static inline int get_stencil_format_bits(enum gs_zstencil_format zsformat)
{
	switch ((uint32_t)zsformat) {
	case GS_Z24_S8:
		return 8;
	default:
		return 0;
	}
}

/* would use designated initializers but Microsoft sort of sucks */
static inline void init_dummy_pixel_format(PIXELFORMATDESCRIPTOR *pfd)
{
	memset(pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
	pfd->nSize = sizeof(pfd);
	pfd->nVersion = 1;
	pfd->iPixelType = PFD_TYPE_RGBA;
	pfd->cColorBits = 32;
	pfd->cDepthBits = 24;
	pfd->cStencilBits = 8;
	pfd->iLayerType = PFD_MAIN_PLANE;
	pfd->dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL |
		       PFD_DOUBLEBUFFER;
}

static const char *dummy_window_class = "GLDummyWindow";
static bool registered_dummy_window_class = false;

struct dummy_context {
	HWND hwnd;
	HGLRC hrc;
	HDC hdc;
};

/* Need a dummy window for the dummy context */
static bool gl_register_dummy_window_class(void)
{
	WNDCLASSA wc;
	if (registered_dummy_window_class)
		return true;

	memset(&wc, 0, sizeof(wc));
	wc.style = CS_OWNDC;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpfnWndProc = DefWindowProc;
	wc.lpszClassName = dummy_window_class;

	if (!RegisterClassA(&wc)) {
		blog(LOG_ERROR, "Could not create dummy window class");
		return false;
	}

	registered_dummy_window_class = true;
	return true;
}

static inline HWND gl_create_dummy_window(void)
{
	HWND hwnd = CreateWindowExA(0, dummy_window_class, "Dummy GL Window",
				    WS_POPUP, 0, 0, 2, 2, NULL, NULL,
				    GetModuleHandle(NULL), NULL);
	if (!hwnd)
		blog(LOG_ERROR, "Could not create dummy context window");

	return hwnd;
}

static inline bool wgl_make_current(HDC hdc, HGLRC hglrc)
{
	bool success = wglMakeCurrent(hdc, hglrc);
	if (!success)
		blog(LOG_ERROR,
		     "wglMakeCurrent failed, GetLastError "
		     "returned %lu",
		     GetLastError());

	return success;
}

static inline HGLRC gl_init_basic_context(HDC hdc)
{
	HGLRC hglrc = wglCreateContext(hdc);
	if (!hglrc) {
		blog(LOG_ERROR, "wglCreateContext failed, %lu", GetLastError());
		return NULL;
	}

	if (!wgl_make_current(hdc, hglrc)) {
		wglDeleteContext(hglrc);
		return NULL;
	}

	return hglrc;
}

static inline HGLRC gl_init_context(HDC hdc)
{
	static const int attribs[] = {
#ifdef _DEBUG
		WGL_CONTEXT_FLAGS_ARB,
		WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
		WGL_CONTEXT_PROFILE_MASK_ARB,
		WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		WGL_CONTEXT_MAJOR_VERSION_ARB,
		3,
		WGL_CONTEXT_MINOR_VERSION_ARB,
		3,
		0,
		0};

	HGLRC hglrc = wglCreateContextAttribsARB(hdc, 0, attribs);
	if (!hglrc) {
		blog(LOG_ERROR,
		     "wglCreateContextAttribsARB failed, "
		     "%lu",
		     GetLastError());
		return NULL;
	}

	if (!wgl_make_current(hdc, hglrc)) {
		wglDeleteContext(hglrc);
		return NULL;
	}

	return hglrc;
}

static bool gl_dummy_context_init(struct dummy_context *dummy)
{
	PIXELFORMATDESCRIPTOR pfd;
	int format_index;

	if (!gl_register_dummy_window_class())
		return false;

	dummy->hwnd = gl_create_dummy_window();
	if (!dummy->hwnd)
		return false;

	dummy->hdc = GetDC(dummy->hwnd);

	init_dummy_pixel_format(&pfd);
	format_index = ChoosePixelFormat(dummy->hdc, &pfd);
	if (!format_index) {
		blog(LOG_ERROR, "Dummy ChoosePixelFormat failed, %lu",
		     GetLastError());
		return false;
	}

	if (!SetPixelFormat(dummy->hdc, format_index, &pfd)) {
		blog(LOG_ERROR, "Dummy SetPixelFormat failed, %lu",
		     GetLastError());
		return false;
	}

	dummy->hrc = gl_init_basic_context(dummy->hdc);
	if (!dummy->hrc) {
		blog(LOG_ERROR, "Failed to initialize dummy context");
		return false;
	}

	return true;
}

static inline void gl_dummy_context_free(struct dummy_context *dummy)
{
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(dummy->hrc);
	DestroyWindow(dummy->hwnd);
	memset(dummy, 0, sizeof(struct dummy_context));
}

static inline void required_extension_error(const char *extension)
{
	blog(LOG_ERROR, "OpenGL extension %s is required", extension);
}

static bool gl_init_extensions(HDC hdc)
{
	if (!gladLoadWGL(hdc)) {
		blog(LOG_ERROR, "Failed to load WGL entry functions.");
		return false;
	}

	if (!GLAD_WGL_ARB_pixel_format) {
		required_extension_error("ARB_pixel_format");
		return false;
	}

	if (!GLAD_WGL_ARB_create_context) {
		required_extension_error("ARB_create_context");
		return false;
	}

	if (!GLAD_WGL_ARB_create_context_profile) {
		required_extension_error("ARB_create_context_profile");
		return false;
	}

	return true;
}

static inline void add_attrib(struct darray *list, int attrib, int val)
{
	darray_push_back(sizeof(int), list, &attrib);
	darray_push_back(sizeof(int), list, &val);
}

/* Creates the real pixel format for the target window */
static int gl_choose_pixel_format(HDC hdc, const struct gs_init_data *info)
{
	struct darray attribs;
	int color_bits = get_color_format_bits(info->format);
	int depth_bits = get_depth_format_bits(info->zsformat);
	int stencil_bits = get_stencil_format_bits(info->zsformat);
	UINT num_formats;
	BOOL success;
	int format;

	if (!color_bits) {
		blog(LOG_ERROR, "gl_init_pixel_format: color format not "
				"supported");
		return false;
	}

	darray_init(&attribs);
	add_attrib(&attribs, WGL_DRAW_TO_WINDOW_ARB, GL_TRUE);
	add_attrib(&attribs, WGL_SUPPORT_OPENGL_ARB, GL_TRUE);
	add_attrib(&attribs, WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB);
	add_attrib(&attribs, WGL_DOUBLE_BUFFER_ARB, GL_TRUE);
	add_attrib(&attribs, WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB);
	add_attrib(&attribs, WGL_COLOR_BITS_ARB, color_bits);
	add_attrib(&attribs, WGL_DEPTH_BITS_ARB, depth_bits);
	add_attrib(&attribs, WGL_STENCIL_BITS_ARB, stencil_bits);
	add_attrib(&attribs, 0, 0);

	success = wglChoosePixelFormatARB(hdc, attribs.array, NULL, 1, &format,
					  &num_formats);
	if (!success || !num_formats) {
		blog(LOG_ERROR, "wglChoosePixelFormatARB failed, %lu",
		     GetLastError());
		format = 0;
	}

	darray_free(&attribs);

	return format;
}

static inline bool gl_getpixelformat(HDC hdc, const struct gs_init_data *info,
				     int *format, PIXELFORMATDESCRIPTOR *pfd)
{
	if (!format)
		return false;

	*format = gl_choose_pixel_format(hdc, info);

	if (!DescribePixelFormat(hdc, *format, sizeof(*pfd), pfd)) {
		blog(LOG_ERROR, "DescribePixelFormat failed, %lu",
		     GetLastError());
		return false;
	}

	return true;
}

static inline bool gl_setpixelformat(HDC hdc, int format,
				     PIXELFORMATDESCRIPTOR *pfd)
{
	if (!SetPixelFormat(hdc, format, pfd)) {
		blog(LOG_ERROR, "SetPixelFormat failed, %lu", GetLastError());
		return false;
	}

	return true;
}

static struct gl_windowinfo *gl_windowinfo_bare(const struct gs_init_data *info)
{
	struct gl_windowinfo *wi = bzalloc(sizeof(struct gl_windowinfo));
	wi->hwnd = info->window.hwnd;
	wi->hdc = GetDC(wi->hwnd);

	if (!wi->hdc) {
		blog(LOG_ERROR, "Unable to get device context from window");
		bfree(wi);
		return NULL;
	}

	return wi;
}

#define DUMMY_WNDCLASS "Dummy GL Window Class"

static bool register_dummy_class(void)
{
	static bool created = false;

	WNDCLASSA wc = {0};
	wc.style = CS_OWNDC;
	wc.hInstance = GetModuleHandleW(NULL);
	wc.lpfnWndProc = (WNDPROC)DefWindowProcA;
	wc.lpszClassName = DUMMY_WNDCLASS;

	if (created)
		return true;

	if (!RegisterClassA(&wc)) {
		blog(LOG_ERROR, "Failed to register dummy GL window class, %lu",
		     GetLastError());
		return false;
	}

	created = true;
	return true;
}

static bool create_dummy_window(struct gl_platform *plat)
{
	plat->window.hwnd = CreateWindowExA(0, DUMMY_WNDCLASS,
					    "OpenGL Dummy Window", WS_POPUP, 0,
					    0, 1, 1, NULL, NULL,
					    GetModuleHandleW(NULL), NULL);
	if (!plat->window.hwnd) {
		blog(LOG_ERROR, "Failed to create dummy GL window, %lu",
		     GetLastError());
		return false;
	}

	plat->window.hdc = GetDC(plat->window.hwnd);
	if (!plat->window.hdc) {
		blog(LOG_ERROR, "Failed to get dummy GL window DC (%lu)",
		     GetLastError());
		return false;
	}

	return true;
}

static bool init_default_swap(struct gl_platform *plat, gs_device_t *device,
			      int pixel_format, PIXELFORMATDESCRIPTOR *pfd)
{
	if (!gl_setpixelformat(plat->window.hdc, pixel_format, pfd))
		return false;

	return true;
}

void gl_update(gs_device_t *device)
{
	/* does nothing on windows */
	UNUSED_PARAMETER(device);
}

void gl_clear_context(gs_device_t *device)
{
	UNUSED_PARAMETER(device);
	wglMakeCurrent(NULL, NULL);
}

static void init_dummy_swap_info(struct gs_init_data *info)
{
	info->format = GS_RGBA;
	info->zsformat = GS_ZS_NONE;
}

struct gl_platform *gl_platform_create(gs_device_t *device, uint32_t adapter)
{
	struct gl_platform *plat = bzalloc(sizeof(struct gl_platform));
	struct dummy_context dummy;
	struct gs_init_data info = {0};
	int pixel_format;
	PIXELFORMATDESCRIPTOR pfd;

	memset(&dummy, 0, sizeof(struct dummy_context));
	init_dummy_swap_info(&info);

	if (!gl_dummy_context_init(&dummy))
		goto fail;
	if (!gl_init_extensions(dummy.hdc))
		goto fail;
	if (!register_dummy_class())
		return false;
	if (!create_dummy_window(plat))
		return false;

	/* you have to have a dummy context open before you can actually
	 * use wglChoosePixelFormatARB */
	if (!gl_getpixelformat(dummy.hdc, &info, &pixel_format, &pfd))
		goto fail;

	gl_dummy_context_free(&dummy);

	if (!init_default_swap(plat, device, pixel_format, &pfd))
		goto fail;

	plat->hrc = gl_init_context(plat->window.hdc);
	if (!plat->hrc)
		goto fail;

	if (!gladLoadGL()) {
		blog(LOG_ERROR, "Failed to initialize OpenGL entry functions.");
		goto fail;
	}

	UNUSED_PARAMETER(adapter);
	return plat;

fail:
	blog(LOG_ERROR, "gl_platform_create failed");
	gl_platform_destroy(plat);
	gl_dummy_context_free(&dummy);
	return NULL;
}

void gl_platform_destroy(struct gl_platform *plat)
{
	if (plat) {
		if (plat->hrc) {
			wglMakeCurrent(NULL, NULL);
			wglDeleteContext(plat->hrc);
		}

		if (plat->window.hdc)
			ReleaseDC(plat->window.hwnd, plat->window.hdc);
		if (plat->window.hwnd)
			DestroyWindow(plat->window.hwnd);

		bfree(plat);
	}
}

bool gl_platform_init_swapchain(struct gs_swap_chain *swap)
{
	UNUSED_PARAMETER(swap);
	return true;
}

void gl_platform_cleanup_swapchain(struct gs_swap_chain *swap)
{
	UNUSED_PARAMETER(swap);
}

struct gl_windowinfo *gl_windowinfo_create(const struct gs_init_data *info)
{
	struct gl_windowinfo *wi = gl_windowinfo_bare(info);
	PIXELFORMATDESCRIPTOR pfd;
	int pixel_format;

	if (!wi)
		return NULL;

	if (!gl_getpixelformat(wi->hdc, info, &pixel_format, &pfd))
		goto fail;
	if (!gl_setpixelformat(wi->hdc, pixel_format, &pfd))
		goto fail;

	return wi;

fail:
	blog(LOG_ERROR, "gl_windowinfo_create failed");
	gl_windowinfo_destroy(wi);
	return NULL;
}

void gl_windowinfo_destroy(struct gl_windowinfo *wi)
{
	if (wi) {
		if (wi->hdc)
			ReleaseDC(wi->hwnd, wi->hdc);
		bfree(wi);
	}
}

void device_enter_context(gs_device_t *device)
{
	HDC hdc = device->plat->window.hdc;
	if (device->cur_swap)
		hdc = device->cur_swap->wi->hdc;

	if (!wgl_make_current(hdc, device->plat->hrc))
		blog(LOG_ERROR, "device_enter_context (GL) failed");
}

void device_leave_context(gs_device_t *device)
{
	UNUSED_PARAMETER(device);
	wglMakeCurrent(NULL, NULL);
}

void *device_get_device_obj(gs_device_t *device)
{
	return device->plat->hrc;
}

void device_load_swapchain(gs_device_t *device, gs_swapchain_t *swap)
{
	HDC hdc = device->plat->window.hdc;
	if (device->cur_swap == swap)
		return;

	device->cur_swap = swap;

	if (swap)
		hdc = swap->wi->hdc;

	if (hdc) {
		if (!wgl_make_current(hdc, device->plat->hrc))
			blog(LOG_ERROR, "device_load_swapchain (GL) failed");
	}
}

void device_present(gs_device_t *device)
{
	if (!SwapBuffers(device->cur_swap->wi->hdc)) {
		blog(LOG_ERROR,
		     "SwapBuffers failed, GetLastError "
		     "returned %lu",
		     GetLastError());
		blog(LOG_ERROR, "device_present (GL) failed");
	}
}

extern void gl_getclientsize(const struct gs_swap_chain *swap, uint32_t *width,
			     uint32_t *height)
{
	RECT rc;
	if (swap) {
		GetClientRect(swap->wi->hwnd, &rc);
		*width = rc.right;
		*height = rc.bottom;
	} else {
		*width = 0;
		*height = 0;
	}
}

EXPORT bool device_gdi_texture_available(void)
{
	return false;
}

EXPORT bool device_shared_texture_available(void)
{
	return false;
}
