#include <windows.h>
#include <d3d10.h>
#include <VersionHelpers.h>
#include <dxgi1_2.h>
#include "get-graphics-offsets.h"

typedef HRESULT(WINAPI *d3d10create_t)(IDXGIAdapter *, D3D10_DRIVER_TYPE,
				       HMODULE, UINT, UINT,
				       DXGI_SWAP_CHAIN_DESC *,
				       IDXGISwapChain **, IUnknown **);
typedef HRESULT(WINAPI *create_fac_t)(IID *id, void **);

struct dxgi_info {
	HMODULE module;
	HWND hwnd;
	IDXGISwapChain *swap;
};

static const IID dxgiFactory2 = {0x50c83a1c,
				 0xe072,
				 0x4c48,
				 {0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6,
				  0xd0}};

static inline bool dxgi_init(dxgi_info &info)
{
	HMODULE d3d10_module;
	d3d10create_t create;
	create_fac_t create_factory;
	IDXGIFactory1 *factory;
	IDXGIAdapter1 *adapter;
	IUnknown *device;
	HRESULT hr;

	info.hwnd = CreateWindowExA(0, DUMMY_WNDCLASS,
				    "d3d10 get-offset window", WS_POPUP, 0, 0,
				    2, 2, nullptr, nullptr,
				    GetModuleHandleA(nullptr), nullptr);
	if (!info.hwnd) {
		return false;
	}

	info.module = LoadLibraryA("dxgi.dll");
	if (!info.module) {
		return false;
	}

	create_factory =
		(create_fac_t)GetProcAddress(info.module, "CreateDXGIFactory1");

	d3d10_module = LoadLibraryA("d3d10.dll");
	if (!d3d10_module) {
		return false;
	}

	create = (d3d10create_t)GetProcAddress(d3d10_module,
					       "D3D10CreateDeviceAndSwapChain");
	if (!create) {
		return false;
	}

	IID factory_iid = IsWindows8OrGreater() ? dxgiFactory2
						: __uuidof(IDXGIFactory1);

	hr = create_factory(&factory_iid, (void **)&factory);
	if (FAILED(hr)) {
		return false;
	}

	hr = factory->EnumAdapters1(0, &adapter);
	factory->Release();
	if (FAILED(hr)) {
		return false;
	}

	DXGI_SWAP_CHAIN_DESC desc = {};
	desc.BufferCount = 2;
	desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BufferDesc.Width = 2;
	desc.BufferDesc.Height = 2;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.OutputWindow = info.hwnd;
	desc.SampleDesc.Count = 1;
	desc.Windowed = true;

	hr = create(adapter, D3D10_DRIVER_TYPE_HARDWARE, nullptr, 0,
		    D3D10_SDK_VERSION, &desc, &info.swap, &device);
	adapter->Release();
	if (FAILED(hr)) {
		return false;
	}

	device->Release();
	return true;
}

static inline void dxgi_free(dxgi_info &info)
{
	if (info.swap)
		info.swap->Release();
	if (info.hwnd)
		DestroyWindow(info.hwnd);
}

void get_dxgi_offsets(struct dxgi_offsets *offsets,
		      struct dxgi_offsets2 *offsets2)
{
	dxgi_info info = {};
	bool success = dxgi_init(info);
	HRESULT hr;

	if (success) {
		offsets->present = vtable_offset(info.module, info.swap, 8);
		offsets->resize = vtable_offset(info.module, info.swap, 13);

		IDXGISwapChain1 *swap1;
		hr = info.swap->QueryInterface(__uuidof(IDXGISwapChain1),
					       (void **)&swap1);
		if (SUCCEEDED(hr)) {
			offsets->present1 =
				vtable_offset(info.module, swap1, 22);
			swap1->Release();
		}

		offsets2->release = vtable_offset(info.module, info.swap, 2);
	}

	dxgi_free(info);
}
