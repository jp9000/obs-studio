#include <stdio.h>
#include <windows.h>
#include <d3d9.h>
#include "get-graphics-offsets.h"

typedef HRESULT(WINAPI *d3d9createex_t)(UINT, IDirect3D9Ex **);

struct d3d9_info {
	HMODULE module;
	HWND hwnd;
	IDirect3D9Ex *d3d9ex;
	IDirect3DDevice9Ex *device;
	IDirect3DSwapChain9 *swap;
};

static inline bool d3d9_init(d3d9_info &info)
{
	d3d9createex_t create;
	HRESULT hr;

	info.hwnd = CreateWindowExA(0, DUMMY_WNDCLASS, "d3d9 get-offset window",
				    WS_POPUP, 0, 0, 1, 1, nullptr, nullptr,
				    GetModuleHandleA(nullptr), nullptr);
	if (!info.hwnd) {
		return false;
	}

	info.module = LoadLibraryA("d3d9.dll");
	if (!info.module) {
		return false;
	}

	create = (d3d9createex_t)GetProcAddress(info.module,
						"Direct3DCreate9Ex");
	if (!create) {
		return false;
	}

	hr = create(D3D_SDK_VERSION, &info.d3d9ex);
	if (FAILED(hr)) {
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
	pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	hr = info.d3d9ex->CreateDeviceEx(
		D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, info.hwnd,
		D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_NOWINDOWCHANGES,
		&pp, nullptr, &info.device);
	if (FAILED(hr)) {
		return false;
	}

	hr = info.device->GetSwapChain(0, &info.swap);
	if (FAILED(hr)) {
		return false;
	}

	return true;
}

static inline void d3d9_free(d3d9_info &info)
{
	if (info.swap)
		info.swap->Release();
	if (info.device)
		info.device->Release();
	if (info.d3d9ex)
		info.d3d9ex->Release();
	if (info.hwnd)
		DestroyWindow(info.hwnd);
}

#ifdef _WIN64

#define MAX_CMP_SIZE 22

static const uint8_t mask[][MAX_CMP_SIZE] = {
	{0xF8, 0xFF, 0xC0, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0xFF, 0x00, 0xF8, 0xF8, 0x00, 0x00, 0x00, 0x00},
	{0xF8, 0xFF, 0xC0, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xC0, 0x00, 0x00,
	 0x00, 0x00, 0xFF, 0x00, 0xF8, 0xF8, 0x00, 0x00, 0x00, 0x00},
	{0xF8, 0xFF, 0xC0, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xC0, 0x00, 0x00,
	 0x00, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00},
};

static const uint8_t mask_cmp[][MAX_CMP_SIZE] = {
	/*
	* Windows 7
	* 48 8B 83 B8 3D 00 00                    mov     rax, [rbx+3DB8h]
	* 44 39 B8 68 50 00 00                    cmp     [rax+5068h], r15d
	* 75 12                                   jnz     short loc_7FF7AA90530
	* 41 B8 F9 19 00 00                       mov     r8d, 19F9h
	*/
	{0x48, 0x8B, 0x80, 0x00, 0x00, 0x00, 0x00, 0x44, 0x39, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x75, 0x00, 0x40, 0xB8, 0x00, 0x00, 0x00, 0x00},
	/*
	* Windows ???+
	* 49 8B 87 78 41 00 00                    mov     rax, [r15+4178h]
	* 39 98 E0 51 00 00                       cmp     [rax+51E0h], ebx
	* 75 12                                   jnz     short loc_1800AEC9C
	* 41 B9 C3 1A 00 00                       mov     r9d, 1AC3h
	*/
	{0x48, 0x8B, 0x80, 0x00, 0x00, 0x00, 0x00, 0x39, 0x80, 0x00, 0x00,
	 0x00, 0x00, 0x75, 0x00, 0x40, 0xB8, 0x00, 0x00, 0x00, 0x00},
	/*
	* Windows 10 April 2018
	* 49 8B 87 58 40 00 00                 mov     rax, [r15+4058h]
	* 39 98 C0 53 00 00                    cmp     [rax+53C0h], ebx
	* 75 12                                jnz     short loc_1800A7FEC
	* 48 8D 15 7F B6 09 00                 lea     rdx, addrErrorMsg
	*
	* Note: different instructions, last byte skipped due to MAX_CMP_SIZE
	*/
	{0x48, 0x8B, 0x80, 0x00, 0x00, 0x00, 0x00, 0x39, 0x80, 0x00, 0x00,
	 0x00, 0x00, 0x75, 0x00, 0x48, 0x8D, 0x00, 0x00, 0x00, 0x00}};

// Offset into the code for the numbers we're interested in
static const uint32_t code_offsets[][2] = {
	{3, 10},
	{3, 9},
	{3, 9},
};
#else

#define MAX_CMP_SIZE 20

static const uint8_t mask[][MAX_CMP_SIZE] = {
	{0xFF, 0xC0, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xC0, 0x00, 0x00, 0x00, 0x00,
	 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00},
	{0xFF, 0xC0, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xC0, 0x00, 0x00,
	 0x00, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00},
	{0xFF, 0xC0, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xC0, 0x00, 0x00,
	 0x00, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00},
};

static const uint8_t mask_cmp[][MAX_CMP_SIZE] = {
	/*
	* Windows 7+
	* 8B 83 E8 29 00 00      mov     eax, [ebx+29E8h]
	* 39 B0 80 4B 00 00      cmp     [eax+4B80h], esi
	* 75 14                  jnz     short loc_754CD9E1
	* 68 F9 19 00 00         push    19F9h
	*/
	{0x8B, 0x80, 0x00, 0x00, 0x00, 0x00, 0x39, 0x80, 0x00, 0x00, 0x00, 0x00,
	 0x75, 0x00, 0x68, 0x00, 0x00, 0x00, 0x00},

	/* Windows 10 Creator's Update+
	* 8B 86 F8 2B 00 00      mov     eax, [esi+2BF8h]
	* 83 B8 00 4D 00 00 00   cmp     dword ptr [eax+4D00h], 0
	* 75 0F                  jnz     short loc_100D793C
	* 68 C3 1A 00 00         push    1AC3h
	*/
	{0x8B, 0x80, 0x00, 0x00, 0x00, 0x00, 0x83, 0x80, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x75, 0x00, 0x68, 0x00, 0x00, 0x00, 0x00},

	/*
	* Windows 10 April 2018 Update
	* 8B 86 68 2B 00 00         mov     eax, [esi+2B68h]
	* 83 B8 F4 4D 00 00 00      cmp     dword ptr [eax+4DF4h], 0
	* 75 0F                     jnz     short loc_100D9A9C
	* BA 08 71 01 10            mov     edx, offset errMsg
	*/
	{0x8B, 0x80, 0x00, 0x00, 0x00, 0x00, 0x83, 0x80, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x75, 0x00, 0xBA, 0x00, 0x00, 0x00, 0x00}};

// Offset into the code for the numbers we're interested in
static const uint32_t code_offsets[][2] = {
	{2, 8},
	{2, 8},
	{2, 8},
};
#endif

#define MAX_FUNC_SCAN_BYTES 200

static inline bool pattern_matches(uint8_t *byte, uint32_t *offset1,
				   uint32_t *offset2)
{
	for (size_t j = 0; j < sizeof(mask) / sizeof(mask[0]); j++) {
		for (size_t i = 0; i < MAX_CMP_SIZE; i++) {
			if ((byte[i] & mask[j][i]) != mask_cmp[j][i])
				goto next_signature;
		}

		*offset1 = code_offsets[j][0];
		*offset2 = code_offsets[j][1];

		return true;
	next_signature:;
	}

	return false;
}

void get_d3d9_offsets(struct d3d9_offsets *offsets)
{
	d3d9_info info = {};
	bool success = d3d9_init(info);

	if (success) {
		uint8_t **vt = *(uint8_t ***)info.device;
		uint8_t *crr = vt[125];

		offsets->present = vtable_offset(info.module, info.device, 17);
		offsets->present_ex =
			vtable_offset(info.module, info.device, 121);
		offsets->present_swap =
			vtable_offset(info.module, info.swap, 3);

		uint32_t offset1, offset2;
		for (size_t i = 0; i < MAX_FUNC_SCAN_BYTES; i++) {
			if (pattern_matches(&crr[i], &offset1, &offset2)) {
#define get_offset(x) *(uint32_t *)&crr[i + x]
				uint32_t off1 = get_offset(offset1);
				uint32_t off2 = get_offset(offset2);

				/* check to make sure offsets are within
				 * expected values */
				if (off1 > 0xFFFF || off2 > 0xFFFF)
					break;

					/* check to make sure offsets actually point
				 * toward expected data */
#ifdef _MSC_VER
				__try {
					uint8_t *ptr = (uint8_t *)(info.device);

					uint8_t *d3d9_ptr =
						*(uint8_t **)(ptr + off1);
					if (d3d9_ptr != (uint8_t *)info.d3d9ex)
						break;

					BOOL &is_d3d9ex =
						*(BOOL *)(d3d9_ptr + off2);
					if (is_d3d9ex != TRUE)
						break;

				} __except (EXCEPTION_EXECUTE_HANDLER) {
					break;
				}
#endif

				offsets->d3d9_clsoff = off1;
				offsets->is_d3d9ex_clsoff = off2;
				break;
			}
		}
	}

	d3d9_free(info);
}
