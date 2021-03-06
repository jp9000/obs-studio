#include <stdio.h>
#include <windows.h>
#include "../d3d8-api/d3d8.h"
#include "get-graphics-offsets.h"

typedef IDirect3D8 *(WINAPI *d3d8create_t)(UINT);

struct d3d8_info {
	HMODULE module;
	HWND hwnd;
	IDirect3D8 *d3d8;
	IDirect3DDevice8 *device;
};

static inline bool d3d8_init(d3d8_info &info)
{
	d3d8create_t create;
	HRESULT hr;

	info.hwnd = CreateWindowExA(0, DUMMY_WNDCLASS, "d3d8 get-addr window",
				    WS_POPUP, 0, 0, 1, 1, nullptr, nullptr,
				    GetModuleHandleA(nullptr), nullptr);
	if (!info.hwnd) {
		return false;
	}

	info.module = LoadLibraryA("d3d8.dll");
	if (!info.module) {
		return false;
	}

	create = (d3d8create_t)GetProcAddress(info.module, "Direct3DCreate8");
	if (!create) {
		return false;
	}

	info.d3d8 = create(D3D_SDK_VERSION);
	if (!info.d3d8) {
		return false;
	}

	D3DPRESENT_PARAMETERS pp = {};
	pp.Windowed = true;
	pp.SwapEffect = D3DSWAPEFFECT_FLIP;
	pp.BackBufferFormat = D3DFMT_A8R8G8B8;
	pp.BackBufferWidth = 2;
	pp.BackBufferHeight = 2;
	pp.BackBufferCount = 1;
	pp.hDeviceWindow = info.hwnd;

	hr = info.d3d8->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
				     info.hwnd,
				     D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp,
				     &info.device);
	if (FAILED(hr)) {
		return false;
	}

	return true;
}

static inline void d3d8_free(d3d8_info &info)
{
	if (info.device)
		info.device->Release();
	if (info.d3d8)
		info.d3d8->Release();
	if (info.hwnd)
		DestroyWindow(info.hwnd);
}

void get_d3d8_offsets(struct d3d8_offsets *offsets)
{
	d3d8_info info = {};
	bool success = d3d8_init(info);

	if (success) {
		offsets->present = vtable_offset(info.module, info.device, 15);
	}

	d3d8_free(info);
}
