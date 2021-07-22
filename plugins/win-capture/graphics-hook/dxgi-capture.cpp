#include <d3d10_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <inttypes.h>

#include "graphics-hook.h"

#include <detours.h>

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

release_t RealRelease = nullptr;
resize_buffers_t RealResizeBuffers = nullptr;
present_t RealPresent = nullptr;
present1_t RealPresent1 = nullptr;

thread_local bool dxgi_presenting = false;
struct ID3D12CommandQueue *dxgi_possible_swap_queues[8]{};
size_t dxgi_possible_swap_queue_count;
bool dxgi_present_attempted = false;

struct dxgi_swap_data {
	IDXGISwapChain *swap;
	void (*capture)(void *, void *, bool);
	void (*free)(void);
};

static struct dxgi_swap_data data = {};
static int swap_chain_mismatch_count = 0;
constexpr int swap_chain_mismtach_limit = 16;

static bool setup_dxgi(IDXGISwapChain *swap)
{
	IUnknown *device;
	HRESULT hr;

	hr = swap->GetDevice(__uuidof(ID3D11Device), (void **)&device);
	if (SUCCEEDED(hr)) {
		ID3D11Device *d3d11 = static_cast<ID3D11Device *>(device);
		D3D_FEATURE_LEVEL level = d3d11->GetFeatureLevel();
		device->Release();

		if (level >= D3D_FEATURE_LEVEL_11_0) {
			hlog("Found D3D11 11.0 device on swap chain");

			data.swap = swap;
			data.capture = d3d11_capture;
			data.free = d3d11_free;
			return true;
		}
	}

	hr = swap->GetDevice(__uuidof(ID3D10Device), (void **)&device);
	if (SUCCEEDED(hr)) {
		device->Release();

		hlog("Found D3D10 device on swap chain");

		data.swap = swap;
		data.capture = d3d10_capture;
		data.free = d3d10_free;
		return true;
	}

	hr = swap->GetDevice(__uuidof(ID3D11Device), (void **)&device);
	if (SUCCEEDED(hr)) {
		device->Release();

		hlog("Found D3D11 device on swap chain");

		data.swap = swap;
		data.capture = d3d11_capture;
		data.free = d3d11_free;
		return true;
	}

#if COMPILE_D3D12_HOOK
	hr = swap->GetDevice(__uuidof(ID3D12Device), (void **)&device);
	if (SUCCEEDED(hr)) {
		device->Release();

		hlog("Found D3D12 device on swap chain: swap=0x%" PRIX64
		     ", device=0x%" PRIX64,
		     (uint64_t)(uintptr_t)swap, (uint64_t)(uintptr_t)device);
		for (size_t i = 0; i < dxgi_possible_swap_queue_count; ++i) {
			hlog("    queue=0x%" PRIX64,
			     (uint64_t)(uintptr_t)dxgi_possible_swap_queues[i]);
		}

		if (dxgi_possible_swap_queue_count > 0) {
			data.swap = swap;
			data.capture = d3d12_capture;
			data.free = d3d12_free;
			return true;
		}
	}
#endif

	hlog_verbose("Failed to setup DXGI");
	return false;
}

static ULONG STDMETHODCALLTYPE hook_release(IUnknown *unknown)
{
	const ULONG refs = RealRelease(unknown);

	hlog_verbose("Release callback: Refs=%lu", refs);
	if (unknown == data.swap && refs == 0) {
		hlog_verbose("No more refs, so reset capture");

		data.swap = nullptr;
		data.capture = nullptr;
		memset(dxgi_possible_swap_queues, 0,
		       sizeof(dxgi_possible_swap_queues));
		dxgi_possible_swap_queue_count = 0;
		dxgi_present_attempted = false;

		data.free();
		data.free = nullptr;
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
	hlog_verbose("ResizeBuffers callback");

	data.swap = nullptr;
	data.capture = nullptr;
	memset(dxgi_possible_swap_queues, 0, sizeof(dxgi_possible_swap_queues));
	dxgi_possible_swap_queue_count = 0;
	dxgi_present_attempted = false;

	if (data.free)
		data.free();
	data.free = nullptr;

	const HRESULT hr = RealResizeBuffers(swap, buffer_count, width, height,
					     format, flags);

	resize_buffers_called = true;

	return hr;
}

static inline IUnknown *get_dxgi_backbuffer(IDXGISwapChain *swap)
{
	IUnknown *res = nullptr;

	const HRESULT hr = swap->GetBuffer(0, IID_PPV_ARGS(&res));
	if (FAILED(hr))
		hlog_hr("get_dxgi_backbuffer: GetBuffer failed", hr);

	return res;
}

static void update_mismatch_count(bool match)
{
	if (match) {
		swap_chain_mismatch_count = 0;
	} else {
		++swap_chain_mismatch_count;

		if (swap_chain_mismatch_count == swap_chain_mismtach_limit) {
			data.swap = nullptr;
			data.capture = nullptr;
			memset(dxgi_possible_swap_queues, 0,
			       sizeof(dxgi_possible_swap_queues));
			dxgi_possible_swap_queue_count = 0;
			dxgi_present_attempted = false;

			data.free();
			data.free = nullptr;

			swap_chain_mismatch_count = 0;
		}
	}
}

static HRESULT STDMETHODCALLTYPE hook_present(IDXGISwapChain *swap,
					      UINT sync_interval, UINT flags)
{
	const bool capture_overlay = global_hook_info->capture_overlay;
	const bool test_draw = (flags & DXGI_PRESENT_TEST) != 0;

	if (data.swap) {
		update_mismatch_count(swap == data.swap);
	}

	if (!data.swap && !capture_active()) {
		setup_dxgi(swap);
	}

	hlog_verbose(
		"Present callback: sync_interval=%u, flags=%u, current_swap=0x%" PRIX64
		", expected_swap=0x%" PRIX64,
		sync_interval, flags, swap, data.swap);
	const bool capture = !test_draw && swap == data.swap && data.capture;
	if (capture && !capture_overlay) {
		IUnknown *backbuffer = get_dxgi_backbuffer(swap);

		if (backbuffer) {
			data.capture(swap, backbuffer, capture_overlay);
			backbuffer->Release();
		}
	}

	dxgi_presenting = true;
	const HRESULT hr = RealPresent(swap, sync_interval, flags);
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
			IUnknown *backbuffer = get_dxgi_backbuffer(swap);

			if (backbuffer) {
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
	const bool capture_overlay = global_hook_info->capture_overlay;
	const bool test_draw = (flags & DXGI_PRESENT_TEST) != 0;

	if (data.swap) {
		update_mismatch_count(swap == data.swap);
	}

	if (!data.swap && !capture_active()) {
		setup_dxgi(swap);
	}

	hlog_verbose(
		"Present1 callback: sync_interval=%u, flags=%u, current_swap=0x%" PRIX64
		", expected_swap=0x%" PRIX64,
		sync_interval, flags, swap, data.swap);
	const bool capture = !test_draw && swap == data.swap && !!data.capture;
	if (capture && !capture_overlay) {
		IUnknown *backbuffer = get_dxgi_backbuffer(swap);

		if (backbuffer) {
			DXGI_SWAP_CHAIN_DESC1 desc;
			swap->GetDesc1(&desc);
			data.capture(swap, backbuffer, capture_overlay);
			backbuffer->Release();
		}
	}

	dxgi_presenting = true;
	const HRESULT hr = RealPresent1(swap, sync_interval, flags, params);
	dxgi_presenting = false;
	dxgi_present_attempted = true;

	if (capture && capture_overlay) {
		if (resize_buffers_called) {
			resize_buffers_called = false;
		} else {
			IUnknown *backbuffer = get_dxgi_backbuffer(swap);

			if (backbuffer) {
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
	if (!dxgi_module) {
		hlog_verbose("Failed to find dxgi.dll. Skipping hook attempt.");
		return false;
	}

	/* ---------------------- */

	void *present_addr = get_offset_addr(
		dxgi_module, global_hook_info->offsets.dxgi.present);
	void *resize_addr = get_offset_addr(
		dxgi_module, global_hook_info->offsets.dxgi.resize);
	void *present1_addr = nullptr;
	if (global_hook_info->offsets.dxgi.present1)
		present1_addr = get_offset_addr(
			dxgi_module, global_hook_info->offsets.dxgi.present1);
	void *release_addr = nullptr;
	if (global_hook_info->offsets.dxgi2.release)
		release_addr = get_offset_addr(
			dxgi_module, global_hook_info->offsets.dxgi2.release);

	DetourTransactionBegin();

	RealPresent = (present_t)present_addr;
	DetourAttach(&(PVOID &)RealPresent, hook_present);

	RealResizeBuffers = (resize_buffers_t)resize_addr;
	DetourAttach(&(PVOID &)RealResizeBuffers, hook_resize_buffers);

	if (present1_addr) {
		RealPresent1 = (present1_t)present1_addr;
		DetourAttach(&(PVOID &)RealPresent1, hook_present1);
	}

	if (release_addr) {
		RealRelease = (release_t)release_addr;
		DetourAttach(&(PVOID &)RealRelease, hook_release);
	}

	const LONG error = DetourTransactionCommit();
	const bool success = error == NO_ERROR;
	if (success) {
		hlog("Hooked IDXGISwapChain::Present");
		hlog("Hooked IDXGISwapChain::ResizeBuffers");
		if (RealPresent1)
			hlog("Hooked IDXGISwapChain1::Present1");
		if (RealRelease)
			hlog("Hooked IDXGISwapChain::Release");
		hlog("Hooked DXGI");
	} else {
		RealPresent = nullptr;
		RealResizeBuffers = nullptr;
		RealPresent1 = nullptr;
		RealRelease = nullptr;
		hlog("Failed to attach Detours hook: %ld", error);
	}

	return success;
}
