#ifdef _MSC_VER
#pragma warning(disable : 4214) /* nonstandard extension, non-int bitfield */
#pragma warning(disable : 4054) /* function pointer to data pointer */
#endif

#include <windows.h>

#define COBJMACROS
#include <dxgi.h>
#include <d3d11.h>

#include "gl-decs.h"
#include "graphics-hook.h"
#include "../funchook.h"

#define DUMMY_WINDOW_CLASS_NAME L"graphics_hook_gl_dummy_window"

/* clang-format off */

static const GUID GUID_IDXGIFactory1 =
{0x770aae78, 0xf26f, 0x4dba, {0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87}};
static const GUID GUID_IDXGIResource =
{0x035f3ab4, 0x482e, 0x4e50, {0xb4, 0x1f, 0x8a, 0x7f, 0x8b, 0xd8, 0x96, 0x0b}};

/* clang-format on */

static struct func_hook swap_buffers;
static struct func_hook wgl_swap_layer_buffers;
static struct func_hook wgl_swap_buffers;
static struct func_hook wgl_delete_context;

static bool darkest_dungeon_fix = false;
static bool functions_initialized = false;

struct gl_data {
	HDC hdc;
	uint32_t cx;
	uint32_t cy;
	DXGI_FORMAT format;
	GLuint fbo;
	bool using_shtex;
	bool shmem_fallback;

	union {
		/* shared texture */
		struct {
			struct shtex_data *shtex_info;
			ID3D11Device *d3d11_device;
			ID3D11DeviceContext *d3d11_context;
			ID3D11Texture2D *d3d11_tex;
			IDXGISwapChain *dxgi_swap;
			HANDLE gl_device;
			HANDLE gl_dxobj;
			HANDLE handle;
			HWND hwnd;
			GLuint texture;
		};
		/* shared memory */
		struct {
			struct shmem_data *shmem_info;
			int cur_tex;
			int copy_wait;
			GLuint pbos[NUM_BUFFERS];
			GLuint textures[NUM_BUFFERS];
			bool texture_ready[NUM_BUFFERS];
			bool texture_mapped[NUM_BUFFERS];
		};
	};
};

static HMODULE gl = NULL;
static bool nv_capture_available = false;
static struct gl_data data = {0};

static inline bool gl_error(const char *func, const char *str)
{
	GLenum error = glGetError();
	if (error != 0) {
		hlog("%s: %s: %lu", func, str, error);
		return true;
	}

	return false;
}

static void gl_free(void)
{
	capture_free();

	if (data.using_shtex) {
		if (data.gl_dxobj)
			jimglDXUnregisterObjectNV(data.gl_device,
						  data.gl_dxobj);
		if (data.gl_device)
			jimglDXCloseDeviceNV(data.gl_device);
		if (data.texture)
			glDeleteTextures(1, &data.texture);
		if (data.d3d11_tex)
			ID3D11Resource_Release(data.d3d11_tex);
		if (data.d3d11_context)
			ID3D11DeviceContext_Release(data.d3d11_context);
		if (data.d3d11_device)
			ID3D11Device_Release(data.d3d11_device);
		if (data.dxgi_swap)
			IDXGISwapChain_Release(data.dxgi_swap);
		if (data.hwnd)
			DestroyWindow(data.hwnd);
	} else {
		for (size_t i = 0; i < NUM_BUFFERS; i++) {
			if (data.pbos[i]) {
				if (data.texture_mapped[i]) {
					glBindBuffer(GL_PIXEL_PACK_BUFFER,
						     data.pbos[i]);
					glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
					glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
				}

				glDeleteBuffers(1, &data.pbos[i]);
			}

			if (data.textures[i])
				glDeleteTextures(1, &data.textures[i]);
		}
	}

	if (data.fbo)
		glDeleteFramebuffers(1, &data.fbo);

	gl_error("gl_free", "GL error occurred on free");

	memset(&data, 0, sizeof(data));

	hlog("------------------ gl capture freed ------------------");
}

static inline void *base_get_proc(const char *name)
{
	return (void *)GetProcAddress(gl, name);
}

static inline void *wgl_get_proc(const char *name)
{
	return (void *)jimglGetProcAddress(name);
}

static inline void *get_proc(const char *name)
{
	void *func = wgl_get_proc(name);
	if (!func)
		func = base_get_proc(name);

	return func;
}

static void init_nv_functions(void)
{
	jimglDXSetResourceShareHandleNV =
		get_proc("wglDXSetResourceShareHandleNV");
	jimglDXOpenDeviceNV = get_proc("wglDXOpenDeviceNV");
	jimglDXCloseDeviceNV = get_proc("wglDXCloseDeviceNV");
	jimglDXRegisterObjectNV = get_proc("wglDXRegisterObjectNV");
	jimglDXUnregisterObjectNV = get_proc("wglDXUnregisterObjectNV");
	jimglDXObjectAccessNV = get_proc("wglDXObjectAccessNV");
	jimglDXLockObjectsNV = get_proc("wglDXLockObjectsNV");
	jimglDXUnlockObjectsNV = get_proc("wglDXUnlockObjectsNV");

	nv_capture_available =
		!!jimglDXSetResourceShareHandleNV && !!jimglDXOpenDeviceNV &&
		!!jimglDXCloseDeviceNV && !!jimglDXRegisterObjectNV &&
		!!jimglDXUnregisterObjectNV && !!jimglDXObjectAccessNV &&
		!!jimglDXLockObjectsNV && !!jimglDXUnlockObjectsNV;

	if (nv_capture_available)
		hlog("Shared-texture OpenGL capture available");
}

#define GET_PROC(cur_func, ptr, func)                                      \
	do {                                                               \
		ptr = get_proc(#func);                                     \
		if (!ptr) {                                                \
			hlog("%s: failed to get function '%s'", #cur_func, \
			     #func);                                       \
			success = false;                                   \
		}                                                          \
	} while (false)

static bool init_gl_functions(void)
{
	bool success = true;

	jimglGetProcAddress = base_get_proc("wglGetProcAddress");
	if (!jimglGetProcAddress) {
		hlog("init_gl_functions: failed to get wglGetProcAddress");
		return false;
	}

	GET_PROC(init_gl_functions, jimglMakeCurrent, wglMakeCurrent);
	GET_PROC(init_gl_functions, jimglGetCurrentDC, wglGetCurrentDC);
	GET_PROC(init_gl_functions, jimglGetCurrentContext,
		 wglGetCurrentContext);
	GET_PROC(init_gl_functions, glTexImage2D, glTexImage2D);
	GET_PROC(init_gl_functions, glReadBuffer, glReadBuffer);
	GET_PROC(init_gl_functions, glGetTexImage, glGetTexImage);
	GET_PROC(init_gl_functions, glDrawBuffer, glDrawBuffer);
	GET_PROC(init_gl_functions, glGetError, glGetError);
	GET_PROC(init_gl_functions, glBufferData, glBufferData);
	GET_PROC(init_gl_functions, glDeleteBuffers, glDeleteBuffers);
	GET_PROC(init_gl_functions, glDeleteTextures, glDeleteTextures);
	GET_PROC(init_gl_functions, glGenBuffers, glGenBuffers);
	GET_PROC(init_gl_functions, glGenTextures, glGenTextures);
	GET_PROC(init_gl_functions, glMapBuffer, glMapBuffer);
	GET_PROC(init_gl_functions, glUnmapBuffer, glUnmapBuffer);
	GET_PROC(init_gl_functions, glBindBuffer, glBindBuffer);
	GET_PROC(init_gl_functions, glGetIntegerv, glGetIntegerv);
	GET_PROC(init_gl_functions, glBindTexture, glBindTexture);
	GET_PROC(init_gl_functions, glGenFramebuffers, glGenFramebuffers);
	GET_PROC(init_gl_functions, glDeleteFramebuffers, glDeleteFramebuffers);
	GET_PROC(init_gl_functions, glBindFramebuffer, glBindFramebuffer);
	GET_PROC(init_gl_functions, glBlitFramebuffer, glBlitFramebuffer);
	GET_PROC(init_gl_functions, glFramebufferTexture2D,
		 glFramebufferTexture2D);

	init_nv_functions();
	return success;
}

static void get_window_size(HDC hdc, uint32_t *cx, uint32_t *cy)
{
	HWND hwnd = WindowFromDC(hdc);
	RECT rc = {0};

	if (darkest_dungeon_fix) {
		*cx = 1920;
		*cy = 1080;
	} else {
		GetClientRect(hwnd, &rc);
		*cx = rc.right;
		*cy = rc.bottom;
	}
}

static inline bool gl_shtex_init_window(void)
{
	data.hwnd = CreateWindowExW(
		0, DUMMY_WINDOW_CLASS_NAME, L"Dummy GL window, ignore",
		WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, 2, 2, NULL,
		NULL, GetModuleHandle(NULL), NULL);
	if (!data.hwnd) {
		hlog("gl_shtex_init_window: failed to create window: %d",
		     GetLastError());
		return false;
	}

	return true;
}

typedef HRESULT(WINAPI *create_dxgi_factory1_t)(REFIID, void **);

static const D3D_FEATURE_LEVEL feature_levels[] = {
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
};

static inline bool gl_shtex_init_d3d11(void)
{
	D3D_FEATURE_LEVEL level_used;
	IDXGIFactory1 *factory;
	IDXGIAdapter *adapter;
	HRESULT hr;

	HMODULE d3d11 = load_system_library("d3d11.dll");
	if (!d3d11) {
		hlog("gl_shtex_init_d3d11: failed to load D3D11.dll: %d",
		     GetLastError());
		return false;
	}

	HMODULE dxgi = load_system_library("dxgi.dll");
	if (!dxgi) {
		hlog("gl_shtex_init_d3d11: failed to load DXGI.dll: %d",
		     GetLastError());
		return false;
	}

	DXGI_SWAP_CHAIN_DESC desc = {0};
	desc.BufferCount = 2;
	desc.BufferDesc.Format = data.format;
	desc.BufferDesc.Width = 2;
	desc.BufferDesc.Height = 2;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.SampleDesc.Count = 1;
	desc.Windowed = true;
	desc.OutputWindow = data.hwnd;

	create_dxgi_factory1_t create_factory =
		(void *)GetProcAddress(dxgi, "CreateDXGIFactory1");
	if (!create_factory) {
		hlog("gl_shtex_init_d3d11: failed to load CreateDXGIFactory1 "
		     "procedure: %d",
		     GetLastError());
		return false;
	}

	PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN create =
		(void *)GetProcAddress(d3d11, "D3D11CreateDeviceAndSwapChain");
	if (!create) {
		hlog("gl_shtex_init_d3d11: failed to load "
		     "D3D11CreateDeviceAndSwapChain procedure: %d",
		     GetLastError());
		return false;
	}

	hr = create_factory(&GUID_IDXGIFactory1, (void **)&factory);
	if (FAILED(hr)) {
		hlog_hr("gl_shtex_init_d3d11: failed to create factory", hr);
		return false;
	}

	hr = IDXGIFactory1_EnumAdapters1(factory, 0,
					 (IDXGIAdapter1 **)&adapter);
	IDXGIFactory1_Release(factory);

	if (FAILED(hr)) {
		hlog_hr("gl_shtex_init_d3d11: failed to create adapter", hr);
		return false;
	}

	hr = create(adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, feature_levels,
		    sizeof(feature_levels) / sizeof(D3D_FEATURE_LEVEL),
		    D3D11_SDK_VERSION, &desc, &data.dxgi_swap,
		    &data.d3d11_device, &level_used, &data.d3d11_context);
	IDXGIAdapter_Release(adapter);

	if (FAILED(hr)) {
		hlog_hr("gl_shtex_init_d3d11: failed to create device", hr);
		return false;
	}

	return true;
}

static inline bool gl_shtex_init_d3d11_tex(void)
{
	IDXGIResource *dxgi_res;
	HRESULT hr;

	D3D11_TEXTURE2D_DESC desc = {0};
	desc.Width = data.cx;
	desc.Height = data.cy;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = data.format;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	hr = ID3D11Device_CreateTexture2D(data.d3d11_device, &desc, NULL,
					  &data.d3d11_tex);
	if (FAILED(hr)) {
		hlog_hr("gl_shtex_init_d3d11_tex: failed to create texture",
			hr);
		return false;
	}

	hr = ID3D11Device_QueryInterface(data.d3d11_tex, &GUID_IDXGIResource,
					 (void **)&dxgi_res);
	if (FAILED(hr)) {
		hlog_hr("gl_shtex_init_d3d11_tex: failed to get IDXGIResource",
			hr);
		return false;
	}

	hr = IDXGIResource_GetSharedHandle(dxgi_res, &data.handle);
	IDXGIResource_Release(dxgi_res);

	if (FAILED(hr)) {
		hlog_hr("gl_shtex_init_d3d11_tex: failed to get shared handle",
			hr);
		return false;
	}

	return true;
}

static inline bool gl_shtex_init_gl_tex(void)
{
	data.gl_device = jimglDXOpenDeviceNV(data.d3d11_device);
	if (!data.gl_device) {
		hlog("gl_shtex_init_gl_tex: failed to open device");
		return false;
	}

	glGenTextures(1, &data.texture);
	if (gl_error("gl_shtex_init_gl_tex", "failed to generate texture")) {
		return false;
	}

	data.gl_dxobj = jimglDXRegisterObjectNV(data.gl_device, data.d3d11_tex,
						data.texture, GL_TEXTURE_2D,
						WGL_ACCESS_WRITE_DISCARD_NV);
	if (!data.gl_dxobj) {
		hlog("gl_shtex_init_gl_tex: failed to register object");
		return false;
	}

	return true;
}

static inline bool gl_init_fbo(void)
{
	glGenFramebuffers(1, &data.fbo);
	return !gl_error("gl_init_fbo", "failed to initialize FBO");
}

static bool gl_shtex_init(HWND window)
{
	if (!gl_shtex_init_window()) {
		return false;
	}
	if (!gl_shtex_init_d3d11()) {
		return false;
	}
	if (!gl_shtex_init_d3d11_tex()) {
		return false;
	}
	if (!gl_shtex_init_gl_tex()) {
		return false;
	}
	if (!gl_init_fbo()) {
		return false;
	}
	if (!capture_init_shtex(&data.shtex_info, window, data.cx, data.cy,
				data.format, true, (uintptr_t)data.handle)) {
		return false;
	}

	hlog("gl shared texture capture successful");
	return true;
}

static inline bool gl_shmem_init_data(size_t idx, size_t size)
{
	glBindBuffer(GL_PIXEL_PACK_BUFFER, data.pbos[idx]);
	if (gl_error("gl_shmem_init_data", "failed to bind pbo")) {
		return false;
	}

	glBufferData(GL_PIXEL_PACK_BUFFER, size, 0, GL_STREAM_READ);
	if (gl_error("gl_shmem_init_data", "failed to set pbo data")) {
		return false;
	}

	glBindTexture(GL_TEXTURE_2D, data.textures[idx]);
	if (gl_error("gl_shmem_init_data", "failed to set bind texture")) {
		return false;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, data.cx, data.cy, 0, GL_BGRA,
		     GL_UNSIGNED_BYTE, NULL);
	if (gl_error("gl_shmem_init_data", "failed to set texture data")) {
		return false;
	}

	return true;
}

static inline bool gl_shmem_init_buffers(void)
{
	uint32_t size = data.cx * data.cy * 4;
	GLint last_pbo;
	GLint last_tex;

	glGenBuffers(NUM_BUFFERS, data.pbos);
	if (gl_error("gl_shmem_init_buffers", "failed to generate buffers")) {
		return false;
	}

	glGenTextures(NUM_BUFFERS, data.textures);
	if (gl_error("gl_shmem_init_buffers", "failed to generate textures")) {
		return false;
	}

	glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &last_pbo);
	if (gl_error("gl_shmem_init_buffers",
		     "failed to save pixel pack buffer")) {
		return false;
	}

	glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_tex);
	if (gl_error("gl_shmem_init_buffers", "failed to save texture")) {
		return false;
	}

	for (size_t i = 0; i < NUM_BUFFERS; i++) {
		if (!gl_shmem_init_data(i, size)) {
			return false;
		}
	}

	glBindBuffer(GL_PIXEL_PACK_BUFFER, last_pbo);
	glBindTexture(GL_TEXTURE_2D, last_tex);
	return true;
}

static bool gl_shmem_init(HWND window)
{
	if (!gl_shmem_init_buffers()) {
		return false;
	}
	if (!gl_init_fbo()) {
		return false;
	}
	if (!capture_init_shmem(&data.shmem_info, window, data.cx, data.cy,
				data.cx * 4, data.format, true)) {
		return false;
	}

	hlog("gl memory capture successful");
	return true;
}

#define INIT_SUCCESS 0
#define INIT_FAILED -1
#define INIT_SHTEX_FAILED -2

static int gl_init(HDC hdc)
{
	HWND window = WindowFromDC(hdc);
	int ret = INIT_FAILED;
	bool success = false;
	RECT rc = {0};

	if (darkest_dungeon_fix) {
		data.cx = 1920;
		data.cy = 1080;
	} else {
		GetClientRect(window, &rc);
		data.cx = rc.right;
		data.cy = rc.bottom;
	}

	data.hdc = hdc;
	data.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	data.using_shtex = nv_capture_available &&
			   !global_hook_info->force_shmem &&
			   !data.shmem_fallback;

	if (data.using_shtex) {
		success = gl_shtex_init(window);
		if (!success)
			ret = INIT_SHTEX_FAILED;
	} else {
		success = gl_shmem_init(window);
	}

	if (!success)
		gl_free();
	else
		ret = INIT_SUCCESS;

	return ret;
}

static void gl_copy_backbuffer(GLuint dst)
{
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, data.fbo);
	if (gl_error("gl_copy_backbuffer", "failed to bind FBO")) {
		return;
	}

	glBindTexture(GL_TEXTURE_2D, dst);
	if (gl_error("gl_copy_backbuffer", "failed to bind texture")) {
		return;
	}

	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			       GL_TEXTURE_2D, dst, 0);
	if (gl_error("gl_copy_backbuffer", "failed to set frame buffer")) {
		return;
	}

	glReadBuffer(GL_BACK);

	/* darkest dungeon fix */
	darkest_dungeon_fix = glGetError() == GL_INVALID_OPERATION &&
			      _strcmpi(process_name, "Darkest.exe") == 0;

	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	if (gl_error("gl_copy_backbuffer", "failed to set draw buffer")) {
		return;
	}

	glBlitFramebuffer(0, 0, data.cx, data.cy, 0, 0, data.cx, data.cy,
			  GL_COLOR_BUFFER_BIT, GL_LINEAR);
	gl_error("gl_copy_backbuffer", "failed to blit");
}

static void gl_shtex_capture(void)
{
	GLint last_fbo;
	GLint last_tex;

	jimglDXLockObjectsNV(data.gl_device, 1, &data.gl_dxobj);

	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &last_fbo);
	if (gl_error("gl_shtex_capture", "failed to get last fbo")) {
		return;
	}

	glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_tex);
	if (gl_error("gl_shtex_capture", "failed to get last texture")) {
		return;
	}

	gl_copy_backbuffer(data.texture);

	glBindTexture(GL_TEXTURE_2D, last_tex);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, last_fbo);

	jimglDXUnlockObjectsNV(data.gl_device, 1, &data.gl_dxobj);

	IDXGISwapChain_Present(data.dxgi_swap, 0, 0);
}

static void gl_shmem_capture_copy(int i)
{
	if (data.texture_ready[i]) {
		GLvoid *buffer;

		data.texture_ready[i] = false;

		glBindBuffer(GL_PIXEL_PACK_BUFFER, data.pbos[i]);
		if (gl_error("gl_shmem_capture_queue_copy",
			     "failed to bind pbo")) {
			return;
		}

		buffer = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
		if (buffer) {
			data.texture_mapped[i] = true;
			shmem_copy_data(i, buffer);
		}
	}
}

static inline void gl_shmem_capture_stage(GLuint dst_pbo, GLuint src_tex)
{
	glBindTexture(GL_TEXTURE_2D, src_tex);
	if (gl_error("gl_shmem_capture_stage", "failed to bind src_tex")) {
		return;
	}

	glBindBuffer(GL_PIXEL_PACK_BUFFER, dst_pbo);
	if (gl_error("gl_shmem_capture_stage", "failed to bind dst_pbo")) {
		return;
	}

	glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
	if (gl_error("gl_shmem_capture_stage", "failed to read src_tex")) {
		return;
	}
}

static void gl_shmem_capture(void)
{
	int next_tex;
	GLint last_fbo;
	GLint last_tex;
	GLint last_pbo;

	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &last_fbo);
	if (gl_error("gl_shmem_capture", "failed to get last fbo")) {
		return;
	}

	glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_tex);
	if (gl_error("gl_shmem_capture", "failed to get last texture")) {
		return;
	}

	glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &last_pbo);
	if (gl_error("gl_shmem_capture", "failed to get last pbo")) {
		return;
	}

	next_tex = (data.cur_tex + 1) % NUM_BUFFERS;
	gl_shmem_capture_copy(next_tex);

	if (data.copy_wait < NUM_BUFFERS - 1) {
		data.copy_wait++;
	} else {
		if (shmem_texture_data_lock(data.cur_tex)) {
			glBindBuffer(GL_PIXEL_PACK_BUFFER,
				     data.pbos[data.cur_tex]);
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			data.texture_mapped[data.cur_tex] = false;
			shmem_texture_data_unlock(data.cur_tex);
		}

		gl_copy_backbuffer(data.textures[data.cur_tex]);
		gl_shmem_capture_stage(data.pbos[data.cur_tex],
				       data.textures[data.cur_tex]);
		data.texture_ready[data.cur_tex] = true;
	}

	glBindTexture(GL_TEXTURE_2D, last_tex);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, last_pbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, last_fbo);

	data.cur_tex = next_tex;
}

static void gl_capture(HDC hdc)
{
	static bool critical_failure = false;

	if (critical_failure) {
		return;
	}

	if (!functions_initialized) {
		functions_initialized = init_gl_functions();
		if (!functions_initialized) {
			critical_failure = true;
			return;
		}
	}

	/* reset error flag */
	glGetError();

	if (capture_should_stop()) {
		gl_free();
	}
	if (capture_should_init()) {
		if (gl_init(hdc) == INIT_SHTEX_FAILED) {
			data.shmem_fallback = true;
			gl_init(hdc);
		}
	}
	if (capture_ready() && hdc == data.hdc) {
		uint32_t new_cx;
		uint32_t new_cy;

		/* reset capture if resized */
		get_window_size(hdc, &new_cx, &new_cy);
		if (new_cx != data.cx || new_cy != data.cy) {
			if (new_cx != 0 && new_cy != 0)
				gl_free();
			return;
		}

		if (data.using_shtex)
			gl_shtex_capture();
		else
			gl_shmem_capture();
	}
}

static BOOL WINAPI hook_swap_buffers(HDC hdc)
{
	BOOL ret;

	if (!global_hook_info->capture_overlay)
		gl_capture(hdc);

	unhook(&swap_buffers);
	BOOL(WINAPI * call)(HDC) = swap_buffers.call_addr;
	ret = call(hdc);
	rehook(&swap_buffers);

	if (global_hook_info->capture_overlay)
		gl_capture(hdc);

	return ret;
}

static BOOL WINAPI hook_wgl_swap_buffers(HDC hdc)
{
	BOOL ret;

	if (!global_hook_info->capture_overlay)
		gl_capture(hdc);

	unhook(&wgl_swap_buffers);
	BOOL(WINAPI * call)(HDC) = wgl_swap_buffers.call_addr;
	ret = call(hdc);
	rehook(&wgl_swap_buffers);

	if (global_hook_info->capture_overlay)
		gl_capture(hdc);

	return ret;
}

static BOOL WINAPI hook_wgl_swap_layer_buffers(HDC hdc, UINT planes)
{
	BOOL ret;

	if (!global_hook_info->capture_overlay)
		gl_capture(hdc);

	unhook(&wgl_swap_layer_buffers);
	BOOL(WINAPI * call)(HDC, UINT) = wgl_swap_layer_buffers.call_addr;
	ret = call(hdc, planes);
	rehook(&wgl_swap_layer_buffers);

	if (global_hook_info->capture_overlay)
		gl_capture(hdc);

	return ret;
}

static BOOL WINAPI hook_wgl_delete_context(HGLRC hrc)
{
	BOOL ret;

	if (capture_active() && functions_initialized) {
		HDC last_hdc = jimglGetCurrentDC();
		HGLRC last_hrc = jimglGetCurrentContext();

		jimglMakeCurrent(data.hdc, hrc);
		gl_free();
		jimglMakeCurrent(last_hdc, last_hrc);
	}

	unhook(&wgl_delete_context);
	BOOL(WINAPI * call)(HGLRC) = wgl_delete_context.call_addr;
	ret = call(hrc);
	rehook(&wgl_delete_context);

	return ret;
}

static bool gl_register_window(void)
{
	WNDCLASSW wc = {0};
	wc.style = CS_OWNDC;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpfnWndProc = DefWindowProc;
	wc.lpszClassName = DUMMY_WINDOW_CLASS_NAME;

	if (!RegisterClassW(&wc)) {
		hlog("gl_register_window: failed to register window class: %d",
		     GetLastError());
		return false;
	}

	return true;
}

bool hook_gl(void)
{
	void *wgl_dc_proc;
	void *wgl_slb_proc;
	void *wgl_sb_proc;

	gl = get_system_module("opengl32.dll");
	if (!gl) {
		return false;
	}

	/* "life is feudal: your own" somehow uses both opengl and directx at
	 * the same time, so blacklist it from capturing opengl */
	const char *process_name = get_process_name();
	if (_strcmpi(process_name, "yo_cm_client.exe") == 0 ||
	    _strcmpi(process_name, "cm_client.exe") == 0) {
		hlog("Ignoring opengl for game: %s", process_name);
		return true;
	}

	if (!gl_register_window()) {
		return true;
	}

	wgl_dc_proc = base_get_proc("wglDeleteContext");
	wgl_slb_proc = base_get_proc("wglSwapLayerBuffers");
	wgl_sb_proc = base_get_proc("wglSwapBuffers");

	hook_init(&swap_buffers, SwapBuffers, hook_swap_buffers, "SwapBuffers");
	if (wgl_dc_proc) {
		hook_init(&wgl_delete_context, wgl_dc_proc,
			  hook_wgl_delete_context, "wglDeleteContext");
		rehook(&wgl_delete_context);
	}
	if (wgl_slb_proc) {
		hook_init(&wgl_swap_layer_buffers, wgl_slb_proc,
			  hook_wgl_swap_layer_buffers, "wglSwapLayerBuffers");
		rehook(&wgl_swap_layer_buffers);
	}
	if (wgl_sb_proc) {
		hook_init(&wgl_swap_buffers, wgl_sb_proc, hook_wgl_swap_buffers,
			  "wglSwapBuffers");
		rehook(&wgl_swap_buffers);
	}

	rehook(&swap_buffers);

	return true;
}
