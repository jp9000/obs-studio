#include <dxgi.h>

#include "../d3d8-api/d3d8.h"
#include "graphics-hook.h"
#include "../funchook.h"

typedef HRESULT(STDMETHODCALLTYPE *reset_t)(IDirect3DDevice8 *,
					    D3DPRESENT_PARAMETERS *);
typedef HRESULT(STDMETHODCALLTYPE *present_t)(IDirect3DDevice8 *, CONST RECT *,
					      CONST RECT *, HWND,
					      CONST RGNDATA *);

static struct func_hook present;
static struct func_hook reset;

struct d3d8_data {
	HMODULE d3d8;
	uint32_t cx;
	uint32_t cy;
	D3DFORMAT d3d8_format;
	DXGI_FORMAT dxgi_format;

	struct shmem_data *shmem_info;
	HWND window;
	uint32_t pitch;
	IDirect3DSurface8 *copy_surfaces[NUM_BUFFERS];
	bool texture_ready[NUM_BUFFERS];
	bool surface_locked[NUM_BUFFERS];
	int cur_surface;
	int copy_wait;
};

static d3d8_data data = {};

static DXGI_FORMAT d3d8_to_dxgi_format(D3DFORMAT format)
{
	switch ((unsigned long)format) {
	case D3DFMT_X1R5G5B5:
	case D3DFMT_A1R5G5B5:
		return DXGI_FORMAT_B5G5R5A1_UNORM;
	case D3DFMT_R5G6B5:
		return DXGI_FORMAT_B5G6R5_UNORM;
	case D3DFMT_A8R8G8B8:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case D3DFMT_X8R8G8B8:
		return DXGI_FORMAT_B8G8R8X8_UNORM;
	}

	return DXGI_FORMAT_UNKNOWN;
}

static IDirect3DSurface8 *d3d8_get_backbuffer(IDirect3DDevice8 *device)
{
	IDirect3DSurface8 *backbuffer;
	HRESULT hr;

	hr = device->GetRenderTarget(&backbuffer);
	if (FAILED(hr)) {
		hlog_hr("d3d8_get_backbuffer: Failed to get backbuffer", hr);
		backbuffer = nullptr;
	}

	return backbuffer;
}

static bool d3d8_get_window_handle(IDirect3DDevice8 *device)
{
	D3DDEVICE_CREATION_PARAMETERS parameters;
	HRESULT hr;
	hr = device->GetCreationParameters(&parameters);
	if (FAILED(hr)) {
		hlog_hr("d3d8_get_window_handle: Failed to get "
			"device creation parameters",
			hr);
		return false;
	}

	data.window = parameters.hFocusWindow;

	return true;
}

static bool d3d8_init_format_backbuffer(IDirect3DDevice8 *device)
{
	IDirect3DSurface8 *backbuffer;
	D3DSURFACE_DESC desc;
	HRESULT hr;

	if (!d3d8_get_window_handle(device))
		return false;

	backbuffer = d3d8_get_backbuffer(device);
	if (!backbuffer)
		return false;

	hr = backbuffer->GetDesc(&desc);
	backbuffer->Release();
	if (FAILED(hr)) {
		hlog_hr("d3d8_init_format_backbuffer: Failed to get "
			"backbuffer descriptor",
			hr);
		return false;
	}

	data.d3d8_format = desc.Format;
	data.dxgi_format = d3d8_to_dxgi_format(desc.Format);
	data.cx = desc.Width;
	data.cy = desc.Height;

	return true;
}

static bool d3d8_shmem_init_buffer(IDirect3DDevice8 *device, int idx)
{
	HRESULT hr;

	hr = device->CreateImageSurface(data.cx, data.cy, data.d3d8_format,
					&data.copy_surfaces[idx]);
	if (FAILED(hr)) {
		hlog_hr("d3d8_shmem_init_buffer: Failed to create surface", hr);
		return false;
	}

	if (idx == 0) {
		D3DLOCKED_RECT rect;
		hr = data.copy_surfaces[0]->LockRect(&rect, nullptr,
						     D3DLOCK_READONLY);
		if (FAILED(hr)) {
			hlog_hr("d3d8_shmem_init_buffer: Failed to lock buffer",
				hr);
			return false;
		}

		data.pitch = rect.Pitch;
		data.copy_surfaces[0]->UnlockRect();
	}

	return true;
}

static bool d3d8_shmem_init(IDirect3DDevice8 *device)
{
	for (int i = 0; i < NUM_BUFFERS; i++) {
		if (!d3d8_shmem_init_buffer(device, i)) {
			return false;
		}
	}
	if (!capture_init_shmem(&data.shmem_info, data.window, data.cx, data.cy,
				data.pitch, data.dxgi_format, false)) {
		return false;
	}

	hlog("d3d8 memory capture successful");
	return true;
}

static void d3d8_free()
{
	capture_free();

	for (size_t i = 0; i < NUM_BUFFERS; i++) {
		if (data.copy_surfaces[i]) {
			if (data.surface_locked[i])
				data.copy_surfaces[i]->UnlockRect();
			data.copy_surfaces[i]->Release();
		}
	}

	memset(&data, 0, sizeof(data));

	hlog("----------------- d3d8 capture freed -----------------");
}

static void d3d8_init(IDirect3DDevice8 *device)
{
	data.d3d8 = get_system_module("d3d8.dll");

	if (!d3d8_init_format_backbuffer(device))
		return;

	if (!d3d8_shmem_init(device))
		d3d8_free();
}

static void d3d8_shmem_capture_copy(int idx)
{
	D3DLOCKED_RECT rect;
	HRESULT hr;

	if (data.texture_ready[idx]) {
		data.texture_ready[idx] = false;

		IDirect3DSurface8 *target = data.copy_surfaces[idx];
		hr = target->LockRect(&rect, nullptr, D3DLOCK_READONLY);
		if (SUCCEEDED(hr)) {
			data.surface_locked[idx] = true;
			shmem_copy_data(idx, rect.pBits);
		}
	}
}

static void d3d8_shmem_capture(IDirect3DDevice8 *device,
			       IDirect3DSurface8 *backbuffer)
{
	int next_surface;
	HRESULT hr;

	next_surface = (data.cur_surface + 1) % NUM_BUFFERS;
	d3d8_shmem_capture_copy(next_surface);

	if (data.copy_wait < NUM_BUFFERS - 1) {
		data.copy_wait++;
	} else {
		IDirect3DSurface8 *src = backbuffer;
		IDirect3DSurface8 *dst = data.copy_surfaces[data.cur_surface];

		if (shmem_texture_data_lock(data.cur_surface)) {
			dst->UnlockRect();
			data.surface_locked[data.cur_surface] = false;
			shmem_texture_data_unlock(data.cur_surface);
		}

		hr = device->CopyRects(src, nullptr, 0, dst, nullptr);
		if (FAILED(hr)) {
			hlog_hr("d3d8_shmem_capture: CopyRects "
				"failed",
				hr);
		}

		data.texture_ready[data.cur_surface] = true;
	}

	data.cur_surface = next_surface;
}

static void d3d8_capture(IDirect3DDevice8 *device,
			 IDirect3DSurface8 *backbuffer)
{
	if (capture_should_stop()) {
		d3d8_free();
	}
	if (capture_should_init()) {
		d3d8_init(device);
	}
	if (capture_ready()) {
		d3d8_shmem_capture(device, backbuffer);
	}
}

static HRESULT STDMETHODCALLTYPE hook_reset(IDirect3DDevice8 *device,
					    D3DPRESENT_PARAMETERS *parameters)
{
	HRESULT hr;

	if (capture_active())
		d3d8_free();

	unhook(&reset);
	reset_t call = (reset_t)reset.call_addr;
	hr = call(device, parameters);
	rehook(&reset);

	return hr;
}

static bool hooked_reset = false;

static void setup_reset_hooks(IDirect3DDevice8 *device)
{
	uintptr_t *vtable = *(uintptr_t **)device;

	hook_init(&reset, (void *)vtable[14], (void *)hook_reset,
		  "IDirect3DDevice8::Reset");
	rehook(&reset);

	hooked_reset = true;
}

static HRESULT STDMETHODCALLTYPE hook_present(IDirect3DDevice8 *device,
					      CONST RECT *src_rect,
					      CONST RECT *dst_rect,
					      HWND override_window,
					      CONST RGNDATA *dirty_region)
{
	IDirect3DSurface8 *backbuffer;
	HRESULT hr;

	if (!hooked_reset)
		setup_reset_hooks(device);

	backbuffer = d3d8_get_backbuffer(device);
	if (backbuffer) {
		d3d8_capture(device, backbuffer);
		backbuffer->Release();
	}

	unhook(&present);
	present_t call = (present_t)present.call_addr;
	hr = call(device, src_rect, dst_rect, override_window, dirty_region);
	rehook(&present);

	return hr;
}

typedef IDirect3D8 *(WINAPI *d3d8create_t)(UINT);

static bool manually_get_d3d8_present_addr(HMODULE d3d8_module,
					   void **present_addr)
{
	d3d8create_t create;
	D3DPRESENT_PARAMETERS pp;
	HRESULT hr;

	IDirect3DDevice8 *device;
	IDirect3D8 *d3d8;

	hlog("D3D8 value invalid, manually obtaining");

	create = (d3d8create_t)GetProcAddress(d3d8_module, "Direct3DCreate8");
	if (!create) {
		hlog("Failed to load Direct3DCreate8");
		return false;
	}

	d3d8 = create(D3D_SDK_VERSION);
	if (!d3d8) {
		hlog("Failed to create D3D8 context");
		return false;
	}

	memset(&pp, 0, sizeof(pp));
	pp.Windowed = true;
	pp.SwapEffect = D3DSWAPEFFECT_FLIP;
	pp.BackBufferFormat = D3DFMT_A8R8G8B8;
	pp.BackBufferWidth = 2;
	pp.BackBufferHeight = 2;
	pp.BackBufferCount = 1;
	pp.hDeviceWindow = dummy_window;

	hr = d3d8->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
				dummy_window,
				D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp,
				&device);
	d3d8->Release();

	if (SUCCEEDED(hr)) {
		uintptr_t *vtable = *(uintptr_t **)device;
		*present_addr = (void *)vtable[15];

		device->Release();
	} else {
		hlog("Failed to create D3D8 device");
		return false;
	}

	return true;
}

bool hook_d3d8(void)
{
	HMODULE d3d8_module = get_system_module("d3d8.dll");
	uint32_t d3d8_size;
	void *present_addr = nullptr;

	if (!d3d8_module) {
		return false;
	}

	d3d8_size = module_size(d3d8_module);

	if (global_hook_info->offsets.d3d8.present < d3d8_size) {
		present_addr = get_offset_addr(
			d3d8_module, global_hook_info->offsets.d3d8.present);
	} else {
		if (!dummy_window) {
			return false;
		}

		if (!manually_get_d3d8_present_addr(d3d8_module,
						    &present_addr)) {
			hlog("Failed to get D3D8 value");
			return true;
		}
	}

	if (!present_addr) {
		hlog("Invalid D3D8 value");
		return true;
	}

	hook_init(&present, present_addr, (void *)hook_present,
		  "IDirect3DDevice8::Present");

	rehook(&present);

	hlog("Hooked D3D8");
	return true;
}
