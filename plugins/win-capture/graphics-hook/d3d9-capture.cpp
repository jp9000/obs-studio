#include <d3d9.h>
#include <d3d11.h>
#include <dxgi.h>

#include "graphics-hook.h"
#include "../funchook.h"
#include "d3d9-patches.hpp"

typedef HRESULT(STDMETHODCALLTYPE *present_t)(IDirect3DDevice9 *, CONST RECT *,
					      CONST RECT *, HWND,
					      CONST RGNDATA *);
typedef HRESULT(STDMETHODCALLTYPE *present_ex_t)(IDirect3DDevice9 *,
						 CONST RECT *, CONST RECT *,
						 HWND, CONST RGNDATA *, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *present_swap_t)(IDirect3DSwapChain9 *,
						   CONST RECT *, CONST RECT *,
						   HWND, CONST RGNDATA *,
						   DWORD);
typedef HRESULT(STDMETHODCALLTYPE *reset_t)(IDirect3DDevice9 *,
					    D3DPRESENT_PARAMETERS *);
typedef HRESULT(STDMETHODCALLTYPE *reset_ex_t)(IDirect3DDevice9 *,
					       D3DPRESENT_PARAMETERS *,
					       D3DDISPLAYMODEEX *);

typedef HRESULT(WINAPI *createfactory1_t)(REFIID, void **);

static struct func_hook present;
static struct func_hook present_ex;
static struct func_hook present_swap;
static struct func_hook reset;
static struct func_hook reset_ex;

struct d3d9_data {
	HMODULE d3d9;
	IDirect3DDevice9 *device; /* do not release */
	uint32_t cx;
	uint32_t cy;
	D3DFORMAT d3d9_format;
	DXGI_FORMAT dxgi_format;
	bool using_shtex;

	/* shared texture */
	IDirect3DSurface9 *d3d9_copytex;
	ID3D11Device *d3d11_device;
	ID3D11DeviceContext *d3d11_context;
	ID3D11Resource *d3d11_tex;
	struct shtex_data *shtex_info;
	HANDLE handle;
	int patch;

	/* shared memory */
	IDirect3DSurface9 *copy_surfaces[NUM_BUFFERS];
	IDirect3DQuery9 *queries[NUM_BUFFERS];
	struct shmem_data *shmem_info;
	bool texture_mapped[NUM_BUFFERS];
	volatile bool issued_queries[NUM_BUFFERS];
	uint32_t pitch;
	int cur_tex;
	int copy_wait;
};

static struct d3d9_data data = {};

static void d3d9_free()
{
	capture_free();

	if (data.using_shtex) {
		if (data.d3d11_tex)
			data.d3d11_tex->Release();
		if (data.d3d11_context)
			data.d3d11_context->Release();
		if (data.d3d11_device)
			data.d3d11_device->Release();
		if (data.d3d9_copytex)
			data.d3d9_copytex->Release();
	} else {
		for (size_t i = 0; i < NUM_BUFFERS; i++) {
			if (data.copy_surfaces[i]) {
				if (data.texture_mapped[i])
					data.copy_surfaces[i]->UnlockRect();
				data.copy_surfaces[i]->Release();
			}
			if (data.queries[i])
				data.queries[i]->Release();
		}
	}

	memset(&data, 0, sizeof(data));

	hlog("----------------- d3d9 capture freed -----------------");
}

static DXGI_FORMAT d3d9_to_dxgi_format(D3DFORMAT format)
{
	switch ((unsigned long)format) {
	case D3DFMT_A2B10G10R10:
		return DXGI_FORMAT_R10G10B10A2_UNORM;
	case D3DFMT_A8R8G8B8:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case D3DFMT_X8R8G8B8:
		return DXGI_FORMAT_B8G8R8X8_UNORM;
	}

	return DXGI_FORMAT_UNKNOWN;
}

const static D3D_FEATURE_LEVEL feature_levels[] = {
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
};

static inline bool shex_init_d3d11()
{
	PFN_D3D11_CREATE_DEVICE create_device;
	createfactory1_t create_factory;
	D3D_FEATURE_LEVEL level_used;
	IDXGIFactory *factory;
	IDXGIAdapter *adapter;
	HMODULE d3d11;
	HMODULE dxgi;
	HRESULT hr;

	d3d11 = load_system_library("d3d11.dll");
	if (!d3d11) {
		hlog("d3d9_init: Failed to load D3D11");
		return false;
	}

	dxgi = load_system_library("dxgi.dll");
	if (!dxgi) {
		hlog("d3d9_init: Failed to load DXGI");
		return false;
	}

	create_factory =
		(createfactory1_t)GetProcAddress(dxgi, "CreateDXGIFactory1");
	if (!create_factory) {
		hlog("d3d9_init: Failed to get CreateDXGIFactory1 address");
		return false;
	}

	create_device = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(
		d3d11, "D3D11CreateDevice");
	if (!create_device) {
		hlog("d3d9_init: Failed to get D3D11CreateDevice address");
		return false;
	}

	hr = create_factory(__uuidof(IDXGIFactory1), (void **)&factory);
	if (FAILED(hr)) {
		hlog_hr("d3d9_init: Failed to create factory object", hr);
		return false;
	}

	hr = factory->EnumAdapters(0, &adapter);
	factory->Release();

	if (FAILED(hr)) {
		hlog_hr("d3d9_init: Failed to get adapter", hr);
		return false;
	}

	hr = create_device(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
			   feature_levels,
			   sizeof(feature_levels) / sizeof(D3D_FEATURE_LEVEL),
			   D3D11_SDK_VERSION, &data.d3d11_device, &level_used,
			   &data.d3d11_context);
	adapter->Release();

	if (FAILED(hr)) {
		hlog_hr("d3d9_init: Failed to create D3D11 device", hr);
		return false;
	}

	return true;
}

static inline bool d3d9_shtex_init_shtex()
{
	IDXGIResource *res;
	HRESULT hr;

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = data.cx;
	desc.Height = data.cy;
	desc.Format = data.dxgi_format;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	hr = data.d3d11_device->CreateTexture2D(
		&desc, nullptr, (ID3D11Texture2D **)&data.d3d11_tex);
	if (FAILED(hr)) {
		hlog_hr("d3d9_shtex_init_shtex: Failed to create D3D11 texture",
			hr);
		return false;
	}

	hr = data.d3d11_tex->QueryInterface(__uuidof(IDXGIResource),
					    (void **)&res);
	if (FAILED(hr)) {
		hlog_hr("d3d9_shtex_init_shtex: Failed to query IDXGIResource",
			hr);
		return false;
	}

	hr = res->GetSharedHandle(&data.handle);
	res->Release();

	if (FAILED(hr)) {
		hlog_hr("d3d9_shtex_init_shtex: Failed to get shared handle",
			hr);
		return false;
	}

	return true;
}

static inline bool d3d9_shtex_init_copytex()
{
	struct d3d9_offsets offsets = global_hook_info->offsets.d3d9;
	uint8_t *patch_addr = nullptr;
	BOOL *p_is_d3d9 = nullptr;
	uint8_t saved_data[MAX_PATCH_SIZE];
	size_t patch_size = 0;
	BOOL was_d3d9ex = false;
	IDirect3DTexture9 *tex;
	DWORD protect_val;
	HRESULT hr;

	if (offsets.d3d9_clsoff && offsets.is_d3d9ex_clsoff) {
		uint8_t *device_ptr = (uint8_t *)(data.device);
		uint8_t *d3d9_ptr =
			*(uint8_t **)(device_ptr + offsets.d3d9_clsoff);
		p_is_d3d9 = (BOOL *)(d3d9_ptr + offsets.is_d3d9ex_clsoff);
	} else {
		patch_addr = get_d3d9_patch_addr(data.d3d9, data.patch);
	}

	if (p_is_d3d9) {
		was_d3d9ex = *p_is_d3d9;
		*p_is_d3d9 = true;

	} else if (patch_addr) {
		patch_size = patch[data.patch].size;
		VirtualProtect(patch_addr, patch_size, PAGE_EXECUTE_READWRITE,
			       &protect_val);
		memcpy(saved_data, patch_addr, patch_size);
		memcpy(patch_addr, patch[data.patch].data, patch_size);
	}

	hr = data.device->CreateTexture(data.cx, data.cy, 1,
					D3DUSAGE_RENDERTARGET, data.d3d9_format,
					D3DPOOL_DEFAULT, &tex, &data.handle);

	if (p_is_d3d9) {
		*p_is_d3d9 = was_d3d9ex;

	} else if (patch_addr && patch_size) {
		memcpy(patch_addr, saved_data, patch_size);
		VirtualProtect(patch_addr, patch_size, protect_val,
			       &protect_val);
	}

	if (FAILED(hr)) {
		hlog_hr("d3d9_shtex_init_copytex: Failed to create shared texture",
			hr);
		return false;
	}

	hr = tex->GetSurfaceLevel(0, &data.d3d9_copytex);
	tex->Release();

	if (FAILED(hr)) {
		hlog_hr("d3d9_shtex_init_copytex: Failed to get surface level",
			hr);
		return false;
	}

	return true;
}

static bool d3d9_shtex_init(HWND window)
{
	data.using_shtex = true;

	if (!shex_init_d3d11()) {
		return false;
	}
	if (!d3d9_shtex_init_shtex()) {
		return false;
	}
	if (!d3d9_shtex_init_copytex()) {
		return false;
	}
	if (!capture_init_shtex(&data.shtex_info, window, data.cx, data.cy,
				data.dxgi_format, false,
				(uintptr_t)data.handle)) {
		return false;
	}

	hlog("d3d9 shared texture capture successful");
	return true;
}

static bool d3d9_shmem_init_buffers(size_t buffer)
{
	HRESULT hr;

	hr = data.device->CreateOffscreenPlainSurface(
		data.cx, data.cy, data.d3d9_format, D3DPOOL_SYSTEMMEM,
		&data.copy_surfaces[buffer], nullptr);
	if (FAILED(hr)) {
		hlog_hr("d3d9_shmem_init_buffers: Failed to create surface",
			hr);
		return false;
	}

	if (buffer == 0) {
		D3DLOCKED_RECT rect;
		hr = data.copy_surfaces[buffer]->LockRect(&rect, nullptr,
							  D3DLOCK_READONLY);
		if (FAILED(hr)) {
			hlog_hr("d3d9_shmem_init_buffers: Failed to lock "
				"buffer",
				hr);
			return false;
		}

		data.pitch = rect.Pitch;
		data.copy_surfaces[buffer]->UnlockRect();
	}

	hr = data.device->CreateQuery(D3DQUERYTYPE_EVENT,
				      &data.queries[buffer]);
	if (FAILED(hr)) {
		hlog_hr("d3d9_shmem_init_buffers: Failed to create query", hr);
		return false;
	}

	return true;
}

static bool d3d9_shmem_init(HWND window)
{
	data.using_shtex = false;

	for (size_t i = 0; i < NUM_BUFFERS; i++) {
		if (!d3d9_shmem_init_buffers(i)) {
			return false;
		}
	}
	if (!capture_init_shmem(&data.shmem_info, window, data.cx, data.cy,
				data.pitch, data.dxgi_format, false)) {
		return false;
	}

	hlog("d3d9 memory capture successful");
	return true;
}

static bool d3d9_get_swap_desc(D3DPRESENT_PARAMETERS &pp)
{
	IDirect3DSwapChain9 *swap = nullptr;
	HRESULT hr;

	hr = data.device->GetSwapChain(0, &swap);
	if (FAILED(hr)) {
		hlog_hr("d3d9_get_swap_desc: Failed to get swap chain", hr);
		return false;
	}

	hr = swap->GetPresentParameters(&pp);
	swap->Release();

	if (FAILED(hr)) {
		hlog_hr("d3d9_get_swap_desc: Failed to get "
			"presentation parameters",
			hr);
		return false;
	}

	return true;
}

static bool d3d9_init_format_backbuffer(HWND &window)
{
	IDirect3DSurface9 *back_buffer = nullptr;
	D3DPRESENT_PARAMETERS pp;
	D3DSURFACE_DESC desc;
	HRESULT hr;

	if (!d3d9_get_swap_desc(pp)) {
		return false;
	}

	hr = data.device->GetRenderTarget(0, &back_buffer);
	if (FAILED(hr)) {
		return false;
	}

	hr = back_buffer->GetDesc(&desc);
	back_buffer->Release();

	if (FAILED(hr)) {
		hlog_hr("d3d9_init_format_backbuffer: Failed to get "
			"backbuffer descriptor",
			hr);
		return false;
	}

	data.d3d9_format = desc.Format;
	data.dxgi_format = d3d9_to_dxgi_format(desc.Format);
	window = pp.hDeviceWindow;

	data.cx = desc.Width;
	data.cy = desc.Height;

	return true;
}

static bool d3d9_init_format_swapchain(HWND &window)
{
	D3DPRESENT_PARAMETERS pp;

	if (!d3d9_get_swap_desc(pp)) {
		return false;
	}

	data.dxgi_format = d3d9_to_dxgi_format(pp.BackBufferFormat);
	data.d3d9_format = pp.BackBufferFormat;
	window = pp.hDeviceWindow;

	data.cx = pp.BackBufferWidth;
	data.cy = pp.BackBufferHeight;

	return true;
}

static void d3d9_init(IDirect3DDevice9 *device)
{
	IDirect3DDevice9Ex *d3d9ex = nullptr;
	bool has_d3d9ex_bool_offset =
		global_hook_info->offsets.d3d9.d3d9_clsoff &&
		global_hook_info->offsets.d3d9.is_d3d9ex_clsoff;
	bool success;
	HWND window = nullptr;
	HRESULT hr;

	data.d3d9 = get_system_module("d3d9.dll");
	data.device = device;

	hr = device->QueryInterface(__uuidof(IDirect3DDevice9Ex),
				    (void **)&d3d9ex);
	if (SUCCEEDED(hr)) {
		d3d9ex->Release();
		data.patch = -1;
	} else if (!has_d3d9ex_bool_offset) {
		data.patch = get_d3d9_patch(data.d3d9);
	} else {
		data.patch = -1;
	}

	if (!d3d9_init_format_backbuffer(window)) {
		if (!d3d9_init_format_swapchain(window)) {
			return;
		}
	}

	if (global_hook_info->force_shmem ||
	    (!d3d9ex && data.patch == -1 && !has_d3d9ex_bool_offset)) {
		success = d3d9_shmem_init(window);
	} else {
		success = d3d9_shtex_init(window);
	}

	if (!success)
		d3d9_free();
}

static inline HRESULT get_backbuffer(IDirect3DDevice9 *device,
				     IDirect3DSurface9 **surface)
{
	static bool use_backbuffer = false;
	static bool checked_exceptions = false;

	if (!checked_exceptions) {
		if (_strcmpi(get_process_name(), "hotd_ng.exe") == 0)
			use_backbuffer = true;
		checked_exceptions = true;
	}

	if (use_backbuffer) {
		return device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO,
					     surface);
	} else {
		return device->GetRenderTarget(0, surface);
	}
}

static inline void d3d9_shtex_capture(IDirect3DSurface9 *backbuffer)
{
	HRESULT hr;

	hr = data.device->StretchRect(backbuffer, nullptr, data.d3d9_copytex,
				      nullptr, D3DTEXF_NONE);
	if (FAILED(hr))
		hlog_hr("d3d9_shtex_capture: StretchRect failed", hr);
}

static void d3d9_shmem_capture_copy(int i)
{
	IDirect3DSurface9 *target = data.copy_surfaces[i];
	D3DLOCKED_RECT rect;
	HRESULT hr;

	if (!data.issued_queries[i]) {
		return;
	}
	if (data.queries[i]->GetData(0, 0, 0) != S_OK) {
		return;
	}

	data.issued_queries[i] = false;

	hr = target->LockRect(&rect, nullptr, D3DLOCK_READONLY);
	if (SUCCEEDED(hr)) {
		data.texture_mapped[i] = true;
		shmem_copy_data(i, rect.pBits);
	}
}

static inline void d3d9_shmem_capture(IDirect3DSurface9 *backbuffer)
{
	int next_tex;
	HRESULT hr;

	next_tex = (data.cur_tex + 1) % NUM_BUFFERS;
	d3d9_shmem_capture_copy(next_tex);

	if (data.copy_wait < NUM_BUFFERS - 1) {
		data.copy_wait++;
	} else {
		IDirect3DSurface9 *src = backbuffer;
		IDirect3DSurface9 *dst = data.copy_surfaces[data.cur_tex];

		if (shmem_texture_data_lock(data.cur_tex)) {
			dst->UnlockRect();
			data.texture_mapped[data.cur_tex] = false;
			shmem_texture_data_unlock(data.cur_tex);
		}

		hr = data.device->GetRenderTargetData(src, dst);
		if (FAILED(hr)) {
			hlog_hr("d3d9_shmem_capture: GetRenderTargetData "
				"failed",
				hr);
		}

		data.queries[data.cur_tex]->Issue(D3DISSUE_END);
		data.issued_queries[data.cur_tex] = true;
	}

	data.cur_tex = next_tex;
}

static void d3d9_capture(IDirect3DDevice9 *device,
			 IDirect3DSurface9 *backbuffer)
{
	if (capture_should_stop()) {
		d3d9_free();
	}
	if (capture_should_init()) {
		d3d9_init(device);
	}
	if (capture_ready()) {
		if (data.device != device) {
			d3d9_free();
			return;
		}

		if (data.using_shtex)
			d3d9_shtex_capture(backbuffer);
		else
			d3d9_shmem_capture(backbuffer);
	}
}

/* this is used just in case Present calls PresentEx or vise versa. */
static int present_recurse = 0;

static inline void present_begin(IDirect3DDevice9 *device,
				 IDirect3DSurface9 *&backbuffer)
{
	HRESULT hr;

	if (!present_recurse) {
		hr = get_backbuffer(device, &backbuffer);
		if (FAILED(hr)) {
			hlog_hr("d3d9_shmem_capture: Failed to get "
				"backbuffer",
				hr);
		}

		if (!global_hook_info->capture_overlay) {
			d3d9_capture(device, backbuffer);
		}
	}

	present_recurse++;
}

static inline void present_end(IDirect3DDevice9 *device,
			       IDirect3DSurface9 *backbuffer)
{
	present_recurse--;

	if (!present_recurse) {
		if (global_hook_info->capture_overlay) {
			if (!present_recurse)
				d3d9_capture(device, backbuffer);
		}

		if (backbuffer)
			backbuffer->Release();
	}
}

static bool hooked_reset = false;
static void setup_reset_hooks(IDirect3DDevice9 *device);

static HRESULT STDMETHODCALLTYPE hook_present(IDirect3DDevice9 *device,
					      CONST RECT *src_rect,
					      CONST RECT *dst_rect,
					      HWND override_window,
					      CONST RGNDATA *dirty_region)
{
	IDirect3DSurface9 *backbuffer = nullptr;
	HRESULT hr;

	if (!hooked_reset)
		setup_reset_hooks(device);

	present_begin(device, backbuffer);

	unhook(&present);
	present_t call = (present_t)present.call_addr;
	hr = call(device, src_rect, dst_rect, override_window, dirty_region);
	rehook(&present);

	present_end(device, backbuffer);

	return hr;
}

static HRESULT STDMETHODCALLTYPE hook_present_ex(
	IDirect3DDevice9 *device, CONST RECT *src_rect, CONST RECT *dst_rect,
	HWND override_window, CONST RGNDATA *dirty_region, DWORD flags)
{
	IDirect3DSurface9 *backbuffer = nullptr;
	HRESULT hr;

	if (!hooked_reset)
		setup_reset_hooks(device);

	present_begin(device, backbuffer);

	unhook(&present_ex);
	present_ex_t call = (present_ex_t)present_ex.call_addr;
	hr = call(device, src_rect, dst_rect, override_window, dirty_region,
		  flags);
	rehook(&present_ex);

	present_end(device, backbuffer);

	return hr;
}

static HRESULT STDMETHODCALLTYPE hook_present_swap(
	IDirect3DSwapChain9 *swap, CONST RECT *src_rect, CONST RECT *dst_rect,
	HWND override_window, CONST RGNDATA *dirty_region, DWORD flags)
{
	IDirect3DSurface9 *backbuffer = nullptr;
	IDirect3DDevice9 *device = nullptr;
	HRESULT hr;

	if (!present_recurse) {
		hr = swap->GetDevice(&device);
		if (SUCCEEDED(hr)) {
			device->Release();
		}
	}

	if (device) {
		if (!hooked_reset)
			setup_reset_hooks(device);

		present_begin(device, backbuffer);
	}

	unhook(&present_swap);
	present_swap_t call = (present_swap_t)present_swap.call_addr;
	hr = call(swap, src_rect, dst_rect, override_window, dirty_region,
		  flags);
	rehook(&present_swap);

	if (device)
		present_end(device, backbuffer);

	return hr;
}

static HRESULT STDMETHODCALLTYPE hook_reset(IDirect3DDevice9 *device,
					    D3DPRESENT_PARAMETERS *params)
{
	HRESULT hr;

	if (capture_active())
		d3d9_free();

	unhook(&reset);
	reset_t call = (reset_t)reset.call_addr;
	hr = call(device, params);
	rehook(&reset);

	return hr;
}

static HRESULT STDMETHODCALLTYPE hook_reset_ex(IDirect3DDevice9 *device,
					       D3DPRESENT_PARAMETERS *params,
					       D3DDISPLAYMODEEX *dmex)
{
	HRESULT hr;

	if (capture_active())
		d3d9_free();

	unhook(&reset_ex);
	reset_ex_t call = (reset_ex_t)reset_ex.call_addr;
	hr = call(device, params, dmex);
	rehook(&reset_ex);

	return hr;
}

static void setup_reset_hooks(IDirect3DDevice9 *device)
{
	IDirect3DDevice9Ex *d3d9ex = nullptr;
	uintptr_t *vtable = *(uintptr_t **)device;
	HRESULT hr;

	hook_init(&reset, (void *)vtable[16], (void *)hook_reset,
		  "IDirect3DDevice9::Reset");
	rehook(&reset);

	hr = device->QueryInterface(__uuidof(IDirect3DDevice9Ex),
				    (void **)&d3d9ex);
	if (SUCCEEDED(hr)) {
		hook_init(&reset_ex, (void *)vtable[132], (void *)hook_reset_ex,
			  "IDirect3DDevice9Ex::ResetEx");
		rehook(&reset_ex);

		d3d9ex->Release();
	}

	hooked_reset = true;
}

typedef HRESULT(WINAPI *d3d9create_ex_t)(UINT, IDirect3D9Ex **);

static bool manually_get_d3d9_addrs(HMODULE d3d9_module, void **present_addr,
				    void **present_ex_addr,
				    void **present_swap_addr)
{
	d3d9create_ex_t create_ex;
	D3DPRESENT_PARAMETERS pp;
	HRESULT hr;

	IDirect3DDevice9Ex *device;
	IDirect3D9Ex *d3d9ex;

	hlog("D3D9 values invalid, manually obtaining");

	create_ex = (d3d9create_ex_t)GetProcAddress(d3d9_module,
						    "Direct3DCreate9Ex");
	if (!create_ex) {
		hlog("Failed to load Direct3DCreate9Ex");
		return false;
	}
	if (FAILED(create_ex(D3D_SDK_VERSION, &d3d9ex))) {
		hlog("Failed to create D3D9 context");
		return false;
	}

	memset(&pp, 0, sizeof(pp));
	pp.Windowed = 1;
	pp.SwapEffect = D3DSWAPEFFECT_FLIP;
	pp.BackBufferFormat = D3DFMT_A8R8G8B8;
	pp.BackBufferCount = 1;
	pp.hDeviceWindow = (HWND)dummy_window;
	pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	hr = d3d9ex->CreateDeviceEx(
		D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, dummy_window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_NOWINDOWCHANGES,
		&pp, NULL, &device);
	d3d9ex->Release();

	if (SUCCEEDED(hr)) {
		uintptr_t *vtable = *(uintptr_t **)device;
		IDirect3DSwapChain9 *swap;

		*present_addr = (void *)vtable[17];
		*present_ex_addr = (void *)vtable[121];

		hr = device->GetSwapChain(0, &swap);
		if (SUCCEEDED(hr)) {
			vtable = *(uintptr_t **)swap;
			*present_swap_addr = (void *)vtable[3];

			swap->Release();
		}

		device->Release();
	} else {
		hlog("Failed to create D3D9 device");
		return false;
	}

	return true;
}

bool hook_d3d9(void)
{
	HMODULE d3d9_module = get_system_module("d3d9.dll");
	uint32_t d3d9_size;
	void *present_addr = nullptr;
	void *present_ex_addr = nullptr;
	void *present_swap_addr = nullptr;

	if (!d3d9_module) {
		return false;
	}

	d3d9_size = module_size(d3d9_module);

	if (global_hook_info->offsets.d3d9.present < d3d9_size &&
	    global_hook_info->offsets.d3d9.present_ex < d3d9_size &&
	    global_hook_info->offsets.d3d9.present_swap < d3d9_size) {

		present_addr = get_offset_addr(
			d3d9_module, global_hook_info->offsets.d3d9.present);
		present_ex_addr = get_offset_addr(
			d3d9_module, global_hook_info->offsets.d3d9.present_ex);
		present_swap_addr = get_offset_addr(
			d3d9_module,
			global_hook_info->offsets.d3d9.present_swap);
	} else {
		if (!dummy_window) {
			return false;
		}

		if (!manually_get_d3d9_addrs(d3d9_module, &present_addr,
					     &present_ex_addr,
					     &present_swap_addr)) {
			hlog("Failed to get D3D9 values");
			return true;
		}
	}

	if (!present_addr && !present_ex_addr && !present_swap_addr) {
		hlog("Invalid D3D9 values");
		return true;
	}

	if (present_swap_addr) {
		hook_init(&present_swap, present_swap_addr,
			  (void *)hook_present_swap,
			  "IDirect3DSwapChain9::Present");
		rehook(&present_swap);
	}
	if (present_ex_addr) {
		hook_init(&present_ex, present_ex_addr, (void *)hook_present_ex,
			  "IDirect3DDevice9Ex::PresentEx");
		rehook(&present_ex);
	}
	if (present_addr) {
		hook_init(&present, present_addr, (void *)hook_present,
			  "IDirect3DDevice9::Present");
		rehook(&present);
	}

	hlog("Hooked D3D9");
	return true;
}
