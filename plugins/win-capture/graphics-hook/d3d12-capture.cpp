#include <windows.h>
#include "graphics-hook.h"

#if COMPILE_D3D12_HOOK

#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <inttypes.h>

#include <detours.h>

#include "dxgi-helpers.hpp"

#define MAX_BACKBUFFERS 8

typedef HRESULT(STDMETHODCALLTYPE *PFN_ExecuteCommandLists)(
	ID3D12CommandQueue *, UINT, ID3D12CommandList *const *);

static PFN_ExecuteCommandLists RealExecuteCommandLists = nullptr;

struct d3d12_data {
	uint32_t cx;
	uint32_t cy;
	DXGI_FORMAT format;
	bool using_shtex;
	bool multisampled;
	bool dxgi_1_4;

	ID3D11Device *device11;
	ID3D11DeviceContext *context11;
	ID3D11On12Device *device11on12;

	union {
		struct {
			struct shtex_data *shtex_info;
			ID3D11Resource *backbuffer11[MAX_BACKBUFFERS];
			UINT backbuffer_count;
			UINT cur_backbuffer;
			ID3D11Texture2D *copy_tex;
			HANDLE handle;
		};
	};
};

static struct d3d12_data data = {};

extern thread_local bool dxgi_presenting;
extern ID3D12CommandQueue *dxgi_possible_swap_queues[8];
extern size_t dxgi_possible_swap_queue_count;
extern bool dxgi_present_attempted;

void d3d12_free(void)
{
	if (data.copy_tex)
		data.copy_tex->Release();
	for (size_t i = 0; i < data.backbuffer_count; i++) {
		if (data.backbuffer11[i])
			data.backbuffer11[i]->Release();
	}
	if (data.device11)
		data.device11->Release();
	if (data.context11)
		data.context11->Release();
	if (data.device11on12)
		data.device11on12->Release();

	capture_free();

	memset(&data, 0, sizeof(data));

	hlog("----------------- d3d12 capture freed ----------------");
}

struct bb_info {
	ID3D12Resource *backbuffer[MAX_BACKBUFFERS];
	UINT count;
};

static bool create_d3d12_tex(bb_info &bb)
{
	D3D11_RESOURCE_FLAGS rf11 = {};
	HRESULT hr;

	if (!bb.count)
		return false;

	data.backbuffer_count = bb.count;

	for (UINT i = 0; i < bb.count; i++) {
		hr = data.device11on12->CreateWrappedResource(
			bb.backbuffer[i], &rf11,
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_PRESENT, __uuidof(ID3D11Resource),
			(void **)&data.backbuffer11[i]);
		if (FAILED(hr)) {
			hlog_hr("create_d3d12_tex: failed to create "
				"backbuffer11",
				hr);
			return false;
		}
	}

	D3D11_TEXTURE2D_DESC desc11 = {};
	desc11.Width = data.cx;
	desc11.Height = data.cy;
	desc11.MipLevels = 1;
	desc11.ArraySize = 1;
	desc11.Format = apply_dxgi_format_typeless(
		data.format, global_hook_info->allow_srgb_alias);
	desc11.SampleDesc.Count = 1;
	desc11.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc11.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

	hr = data.device11->CreateTexture2D(&desc11, nullptr, &data.copy_tex);
	if (FAILED(hr)) {
		hlog_hr("create_d3d12_tex: creation of d3d11 copy tex failed",
			hr);
		return false;
	}

	for (UINT i = 0; i < bb.count; i++) {
		data.device11on12->ReleaseWrappedResources(
			&data.backbuffer11[i], 1);
	}

	IDXGIResource *dxgi_res;
	hr = data.copy_tex->QueryInterface(__uuidof(IDXGIResource),
					   (void **)&dxgi_res);
	if (FAILED(hr)) {
		hlog_hr("create_d3d12_tex: failed to query "
			"IDXGIResource interface from texture",
			hr);
		return false;
	}

	hr = dxgi_res->GetSharedHandle(&data.handle);
	dxgi_res->Release();
	if (FAILED(hr)) {
		hlog_hr("create_d3d12_tex: failed to get shared handle", hr);
		return false;
	}

	return true;
}

static bool d3d12_init_11on12(ID3D12Device *device)
{
	static HMODULE d3d11 = nullptr;
	static PFN_D3D11ON12_CREATE_DEVICE create_11_on_12 = nullptr;
	static bool initialized_11 = false;
	static bool initialized_func = false;

	if (!initialized_11 && !d3d11) {
		d3d11 = load_system_library("d3d11.dll");
		if (!d3d11) {
			hlog("d3d12_init_11on12: failed to load d3d11");
		}
		initialized_11 = true;
	}

	if (!d3d11) {
		return false;
	}

	if (!initialized_func && !create_11_on_12) {
		create_11_on_12 = (PFN_D3D11ON12_CREATE_DEVICE)GetProcAddress(
			d3d11, "D3D11On12CreateDevice");
		if (!create_11_on_12) {
			hlog("d3d12_init_11on12: Failed to get "
			     "D3D11On12CreateDevice address");
		}

		initialized_func = true;
	}

	if (!create_11_on_12) {
		return false;
	}

	bool created = false;

	for (size_t i = 0; i < dxgi_possible_swap_queue_count; ++i) {
		hlog("d3d12_init_11on12: creating 11 device: queue=0x%" PRIX64,
		     (uint64_t)(uintptr_t)dxgi_possible_swap_queues[i]);
		IUnknown *const queue = dxgi_possible_swap_queues[i];
		const HRESULT hr = create_11_on_12(device, 0, nullptr, 0,
						   &queue, 1, 0, &data.device11,
						   &data.context11, nullptr);
		created = SUCCEEDED(hr);
		if (created) {
			break;
		}

		hlog_hr("d3d12_init_11on12: failed to create 11 device", hr);
	}

	if (!created) {
		return false;
	}

	memset(dxgi_possible_swap_queues, 0, sizeof(dxgi_possible_swap_queues));
	dxgi_possible_swap_queue_count = 0;
	dxgi_present_attempted = false;

	const HRESULT hr =
		data.device11->QueryInterface(IID_PPV_ARGS(&data.device11on12));
	if (FAILED(hr)) {
		hlog_hr("d3d12_init_11on12: failed to query 11on12 device", hr);
		return false;
	}

	return true;
}

static bool d3d12_shtex_init(ID3D12Device *device, HWND window, bb_info &bb)
{
	if (!d3d12_init_11on12(device)) {
		return false;
	}
	if (!create_d3d12_tex(bb)) {
		return false;
	}
	if (!capture_init_shtex(&data.shtex_info, window, data.cx, data.cy,
				data.format, false, (uintptr_t)data.handle)) {
		return false;
	}

	hlog("d3d12 shared texture capture successful");
	return true;
}

static inline bool d3d12_init_format(IDXGISwapChain *swap, HWND &window,
				     bb_info &bb)
{
	DXGI_SWAP_CHAIN_DESC desc;
	IDXGISwapChain3 *swap3;
	HRESULT hr;

	hr = swap->GetDesc(&desc);
	if (FAILED(hr)) {
		hlog_hr("d3d12_init_format: swap->GetDesc failed", hr);
		return false;
	}

	data.format = strip_dxgi_format_srgb(desc.BufferDesc.Format);
	data.multisampled = desc.SampleDesc.Count > 1;
	window = desc.OutputWindow;
	data.cx = desc.BufferDesc.Width;
	data.cy = desc.BufferDesc.Height;

	hr = swap->QueryInterface(__uuidof(IDXGISwapChain3), (void **)&swap3);
	if (SUCCEEDED(hr)) {
		data.dxgi_1_4 = true;
		hlog("We're DXGI1.4 boys!");
		swap3->Release();
	}

	hlog("Buffer count: %d, swap effect: %d", (int)desc.BufferCount,
	     (int)desc.SwapEffect);

	bb.count = desc.SwapEffect == DXGI_SWAP_EFFECT_DISCARD
			   ? 1
			   : desc.BufferCount;

	if (bb.count == 1)
		data.dxgi_1_4 = false;

	if (bb.count > MAX_BACKBUFFERS) {
		hlog("Somehow it's using more than the max backbuffers.  "
		     "Not sure why anyone would do that.");
		bb.count = 1;
		data.dxgi_1_4 = false;
	}

	for (UINT i = 0; i < bb.count; i++) {
		hr = swap->GetBuffer(i, __uuidof(ID3D12Resource),
				     (void **)&bb.backbuffer[i]);
		if (SUCCEEDED(hr)) {
			bb.backbuffer[i]->Release();
		} else {
			return false;
		}
	}

	return true;
}

static void d3d12_init(IDXGISwapChain *swap)
{
	ID3D12Device *device = nullptr;
	const HRESULT hr = swap->GetDevice(IID_PPV_ARGS(&device));
	if (SUCCEEDED(hr)) {
		hlog("d3d12_init: device=0x%" PRIX64,
		     (uint64_t)(uintptr_t)device);

		HWND window;
		bb_info bb = {};
		if (d3d12_init_format(swap, window, bb)) {
			if (global_hook_info->force_shmem) {
				hlog("d3d12_init: shared memory capture currently "
				     "unsupported; ignoring");
			}

			if (!d3d12_shtex_init(device, window, bb))
				d3d12_free();
		}

		device->Release();
	} else {
		hlog_hr("d3d12_init: failed to get device from swap", hr);
	}
}

static inline void d3d12_copy_texture(ID3D11Resource *dst, ID3D11Resource *src)
{
	if (data.multisampled) {
		data.context11->ResolveSubresource(dst, 0, src, 0, data.format);
	} else {
		data.context11->CopyResource(dst, src);
	}
}

static inline void d3d12_shtex_capture(IDXGISwapChain *swap,
				       bool capture_overlay)
{
	bool dxgi_1_4 = data.dxgi_1_4;
	UINT cur_idx;

	if (dxgi_1_4) {
		IDXGISwapChain3 *swap3 =
			reinterpret_cast<IDXGISwapChain3 *>(swap);
		cur_idx = swap3->GetCurrentBackBufferIndex();
	} else {
		cur_idx = data.cur_backbuffer;
	}

	ID3D11Resource *backbuffer = data.backbuffer11[cur_idx];

	data.device11on12->AcquireWrappedResources(&backbuffer, 1);
	d3d12_copy_texture(data.copy_tex, backbuffer);
	data.device11on12->ReleaseWrappedResources(&backbuffer, 1);
	data.context11->Flush();

	if (!dxgi_1_4) {
		if (++data.cur_backbuffer >= data.backbuffer_count)
			data.cur_backbuffer = 0;
	}
}

void d3d12_capture(void *swap_ptr, void *, bool capture_overlay)
{
	IDXGISwapChain *swap = (IDXGISwapChain *)swap_ptr;

	if (capture_should_stop()) {
		d3d12_free();
	}
	if (capture_should_init()) {
		d3d12_init(swap);
	}
	if (capture_ready()) {
		d3d12_shtex_capture(swap, capture_overlay);
	}
}

static bool try_append_queue_if_unique(ID3D12CommandQueue *queue)
{
	for (size_t i = 0; i < dxgi_possible_swap_queue_count; ++i) {
		if (dxgi_possible_swap_queues[i] == queue)
			return false;
	}

	dxgi_possible_swap_queues[dxgi_possible_swap_queue_count] = queue;
	++dxgi_possible_swap_queue_count;
	return true;
}

static HRESULT STDMETHODCALLTYPE
hook_execute_command_lists(ID3D12CommandQueue *queue, UINT NumCommandLists,
			   ID3D12CommandList *const *ppCommandLists)
{
	hlog_verbose("ExecuteCommandLists callback: queue=0x%" PRIX64,
		     (uint64_t)(uintptr_t)queue);

	if (dxgi_possible_swap_queue_count <
	    _countof(dxgi_possible_swap_queues)) {
		if (dxgi_presenting &&
		    (queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT)) {
			if (try_append_queue_if_unique(queue)) {
				hlog("Remembering D3D12 queue from present: queue=0x%" PRIX64,
				     (uint64_t)(uintptr_t)queue);
			}
		} else if (dxgi_present_attempted &&
			   (queue->GetDesc().Type ==
			    D3D12_COMMAND_LIST_TYPE_DIRECT)) {
			if (try_append_queue_if_unique(queue)) {
				hlog("Remembering D3D12 queue from first direct submit after present: queue=0x%" PRIX64,
				     (uint64_t)(uintptr_t)queue);
			}
		} else {
			hlog_verbose("Ignoring D3D12 queue=0x%" PRIX64,
				     (uint64_t)(uintptr_t)queue);
		}
	}

	return RealExecuteCommandLists(queue, NumCommandLists, ppCommandLists);
}

static bool
manually_get_d3d12_addrs(HMODULE d3d12_module,
			 PFN_ExecuteCommandLists *execute_command_lists_addr)
{
	PFN_D3D12_CREATE_DEVICE create =
		(PFN_D3D12_CREATE_DEVICE)GetProcAddress(d3d12_module,
							"D3D12CreateDevice");
	if (!create) {
		hlog("Failed to load D3D12CreateDevice");
		return false;
	}

	bool success = false;
	ID3D12Device *device;
	if (SUCCEEDED(create(NULL, D3D_FEATURE_LEVEL_11_0,
			     IID_PPV_ARGS(&device)))) {
		D3D12_COMMAND_QUEUE_DESC desc{};
		ID3D12CommandQueue *queue;
		HRESULT hr =
			device->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue));
		success = SUCCEEDED(hr);
		if (success) {
			void **queue_vtable = *(void ***)queue;
			*execute_command_lists_addr =
				(PFN_ExecuteCommandLists)queue_vtable[10];

			queue->Release();
		} else {
			hlog("Failed to create D3D12 command queue");
		}

		device->Release();
	} else {
		hlog("Failed to create D3D12 device");
	}

	return success;
}

bool hook_d3d12(void)
{
	HMODULE d3d12_module = get_system_module("d3d12.dll");
	if (!d3d12_module) {
		hlog_verbose(
			"Failed to find d3d12.dll. Skipping hook attempt.");
		return false;
	}

	PFN_ExecuteCommandLists execute_command_lists_addr = nullptr;
	if (!manually_get_d3d12_addrs(d3d12_module,
				      &execute_command_lists_addr)) {
		hlog("Failed to get D3D12 values");
		return true;
	}

	if (!execute_command_lists_addr) {
		hlog("Invalid D3D12 values");
		return true;
	}

	DetourTransactionBegin();

	RealExecuteCommandLists = execute_command_lists_addr;
	DetourAttach(&(PVOID &)RealExecuteCommandLists,
		     hook_execute_command_lists);

	const LONG error = DetourTransactionCommit();
	const bool success = error == NO_ERROR;
	if (success) {
		hlog("Hooked ID3D12CommandQueue::ExecuteCommandLists");
		hlog("Hooked D3D12");
	} else {
		RealExecuteCommandLists = nullptr;
		hlog("Failed to attach Detours hook: %ld", error);
	}

	return success;
}

#endif
