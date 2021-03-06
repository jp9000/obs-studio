#include <d3d10_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>

#include "graphics-hook.h"
#include "../funchook.h"

#if COMPILE_D3D12_HOOK
#include <d3d12.h>
#endif

typedef ULONG(STDMETHODCALLTYPE *release_t)(IUnknown *);
typedef HRESULT(STDMETHODCALLTYPE *resize_buffers_t)(IDXGISwapChain *, UINT,
						     UINT, UINT, DXGI_FORMAT,
						     UINT);
typedef HRESULT(STDMETHODCALLTYPE *present_t)(IDXGISwapChain *, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE *present1_t)(IDXGISwapChain1 *, UINT, UINT,
					       const DXGI_PRESENT_PARAMETERS *);

static struct func_hook release;
static struct func_hook resize_buffers;
static struct func_hook present;
static struct func_hook present1;

thread_local bool dxgi_presenting = false;
struct ID3D12CommandQueue *dxgi_possible_swap_queue = nullptr;
bool dxgi_present_attempted = false;

struct dxgi_swap_data {
	IDXGISwapChain *swap;
	void (*capture)(void *, void *, bool);
	void (*free)(void);
};

static struct dxgi_swap_data data = {};

static bool setup_dxgi(IDXGISwapChain *swap)
{
	IUnknown *device;
	HRESULT hr;

	hr = swap->GetDevice(__uuidof(ID3D11Device), (void **)&device);
	if (SUCCEEDED(hr)) {
		ID3D11Device *d3d11 = reinterpret_cast<ID3D11Device *>(device);
		D3D_FEATURE_LEVEL level = d3d11->GetFeatureLevel();
		device->Release();

		if (level >= D3D_FEATURE_LEVEL_11_0) {
			data.swap = swap;
			data.capture = d3d11_capture;
			data.free = d3d11_free;
			return true;
		}
	}

	hr = swap->GetDevice(__uuidof(ID3D10Device), (void **)&device);
	if (SUCCEEDED(hr)) {
		data.swap = swap;
		data.capture = d3d10_capture;
		data.free = d3d10_free;
		device->Release();
		return true;
	}

	hr = swap->GetDevice(__uuidof(ID3D11Device), (void **)&device);
	if (SUCCEEDED(hr)) {
		data.swap = swap;
		data.capture = d3d11_capture;
		data.free = d3d11_free;
		device->Release();
		return true;
	}

#if COMPILE_D3D12_HOOK
	hr = swap->GetDevice(__uuidof(ID3D12Device), (void **)&device);
	if (SUCCEEDED(hr)) {
		if (!global_hook_info->d3d12_use_swap_queue ||
		    dxgi_possible_swap_queue) {
			data.swap = swap;
			data.capture = d3d12_capture;
			data.free = d3d12_free;
			device->Release();
			return true;
		}
	}
#endif

	return false;
}

static ULONG STDMETHODCALLTYPE hook_release(IUnknown *unknown)
{
	unhook(&release);
	release_t call = (release_t)release.call_addr;
	ULONG refs = call(unknown);
	rehook(&release);

	if (unknown == data.swap && refs == 0) {
		data.free();
		data.swap = nullptr;
		data.free = nullptr;
		data.capture = nullptr;
		dxgi_possible_swap_queue = nullptr;
		dxgi_present_attempted = false;
	}

	return refs;
}

static bool resize_buffers_called = false;

static HRESULT STDMETHODCALLTYPE hook_resize_buffers(IDXGISwapChain *swap,
						     UINT buffer_count,
						     UINT width, UINT height,
						     DXGI_FORMAT format,
						     UINT flags)
{
	HRESULT hr;

	if (!!data.free)
		data.free();

	data.swap = nullptr;
	data.free = nullptr;
	data.capture = nullptr;
	dxgi_possible_swap_queue = nullptr;
	dxgi_present_attempted = false;

	unhook(&resize_buffers);
	resize_buffers_t call = (resize_buffers_t)resize_buffers.call_addr;
	hr = call(swap, buffer_count, width, height, format, flags);
	rehook(&resize_buffers);

	resize_buffers_called = true;

	return hr;
}

static inline IUnknown *get_dxgi_backbuffer(IDXGISwapChain *swap)
{
	IDXGIResource *res = nullptr;
	HRESULT hr;

	hr = swap->GetBuffer(0, __uuidof(IUnknown), (void **)&res);
	if (FAILED(hr))
		hlog_hr("get_dxgi_backbuffer: GetBuffer failed", hr);

	return res;
}

static HRESULT STDMETHODCALLTYPE hook_present(IDXGISwapChain *swap,
					      UINT sync_interval, UINT flags)
{
	IUnknown *backbuffer = nullptr;
	bool capture_overlay = global_hook_info->capture_overlay;
	bool test_draw = (flags & DXGI_PRESENT_TEST) != 0;
	bool capture;
	HRESULT hr;

	if (!data.swap && !capture_active()) {
		setup_dxgi(swap);
	}

	capture = !test_draw && swap == data.swap && !!data.capture;
	if (capture && !capture_overlay) {
		backbuffer = get_dxgi_backbuffer(swap);

		if (!!backbuffer) {
			data.capture(swap, backbuffer, capture_overlay);
			backbuffer->Release();
		}
	}

	dxgi_presenting = true;
	unhook(&present);
	present_t call = (present_t)present.call_addr;
	hr = call(swap, sync_interval, flags);
	rehook(&present);
	dxgi_presenting = false;
	dxgi_present_attempted = true;

	if (capture && capture_overlay) {
		/*
		 * It seems that the first call to Present after ResizeBuffers
		 * will cause the backbuffer to be invalidated, so do not
		 * perform the post-overlay capture if ResizeBuffers has
		 * recently been called.  (The backbuffer returned by
		 * get_dxgi_backbuffer *will* be invalid otherwise)
		 */
		if (resize_buffers_called) {
			resize_buffers_called = false;
		} else {
			backbuffer = get_dxgi_backbuffer(swap);

			if (!!backbuffer) {
				data.capture(swap, backbuffer, capture_overlay);
				backbuffer->Release();
			}
		}
	}

	return hr;
}

static HRESULT STDMETHODCALLTYPE
hook_present1(IDXGISwapChain1 *swap, UINT sync_interval, UINT flags,
	      const DXGI_PRESENT_PARAMETERS *params)
{
	IUnknown *backbuffer = nullptr;
	bool capture_overlay = global_hook_info->capture_overlay;
	bool test_draw = (flags & DXGI_PRESENT_TEST) != 0;
	bool capture;
	HRESULT hr;

	if (!data.swap && !capture_active()) {
		setup_dxgi(swap);
	}

	capture = !test_draw && swap == data.swap && !!data.capture;
	if (capture && !capture_overlay) {
		backbuffer = get_dxgi_backbuffer(swap);

		if (!!backbuffer) {
			DXGI_SWAP_CHAIN_DESC1 desc;
			swap->GetDesc1(&desc);
			data.capture(swap, backbuffer, capture_overlay);
			backbuffer->Release();
		}
	}

	dxgi_presenting = true;
	unhook(&present1);
	present1_t call = (present1_t)present1.call_addr;
	hr = call(swap, sync_interval, flags, params);
	rehook(&present1);
	dxgi_presenting = false;
	dxgi_present_attempted = true;

	if (capture && capture_overlay) {
		if (resize_buffers_called) {
			resize_buffers_called = false;
		} else {
			backbuffer = get_dxgi_backbuffer(swap);

			if (!!backbuffer) {
				data.capture(swap, backbuffer, capture_overlay);
				backbuffer->Release();
			}
		}
	}

	return hr;
}

bool hook_dxgi(void)
{
	HMODULE dxgi_module = get_system_module("dxgi.dll");
	void *present_addr;
	void *resize_addr;
	void *present1_addr = nullptr;
	void *release_addr = nullptr;

	if (!dxgi_module) {
		return false;
	}

	/* ---------------------- */

	present_addr = get_offset_addr(dxgi_module,
				       global_hook_info->offsets.dxgi.present);
	resize_addr = get_offset_addr(dxgi_module,
				      global_hook_info->offsets.dxgi.resize);
	if (global_hook_info->offsets.dxgi.present1)
		present1_addr = get_offset_addr(
			dxgi_module, global_hook_info->offsets.dxgi.present1);
	if (global_hook_info->offsets.dxgi2.release)
		release_addr = get_offset_addr(
			dxgi_module, global_hook_info->offsets.dxgi2.release);

	hook_init(&present, present_addr, (void *)hook_present,
		  "IDXGISwapChain::Present");
	hook_init(&resize_buffers, resize_addr, (void *)hook_resize_buffers,
		  "IDXGISwapChain::ResizeBuffers");
	if (present1_addr)
		hook_init(&present1, present1_addr, (void *)hook_present1,
			  "IDXGISwapChain1::Present1");
	if (release_addr)
		hook_init(&release, release_addr, (void *)hook_release,
			  "IDXGISwapChain::Release");

	rehook(&resize_buffers);
	rehook(&present);
	if (present1_addr)
		rehook(&present1);
	if (release_addr)
		rehook(&release);

	hlog("Hooked DXGI");
	return true;
}
