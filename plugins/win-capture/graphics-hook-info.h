#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "hook-helpers.h"

#define EVENT_CAPTURE_RESTART L"CaptureHook_Restart"
#define EVENT_CAPTURE_STOP L"CaptureHook_Stop"

#define EVENT_HOOK_READY L"CaptureHook_HookReady"
#define EVENT_HOOK_EXIT L"CaptureHook_Exit"

#define EVENT_HOOK_INIT L"CaptureHook_Initialize"

#define WINDOW_HOOK_KEEPALIVE L"CaptureHook_KeepAlive"

#define MUTEX_TEXTURE1 L"CaptureHook_TextureMutex1"
#define MUTEX_TEXTURE2 L"CaptureHook_TextureMutex2"

#define SHMEM_HOOK_INFO L"CaptureHook_HookInfo"
#define SHMEM_TEXTURE L"CaptureHook_Texture"

#define PIPE_NAME "CaptureHook_Pipe"

#pragma pack(push, 8)

struct d3d8_offsets {
	uint32_t present;
};

struct d3d9_offsets {
	uint32_t present;
	uint32_t present_ex;
	uint32_t present_swap;
	uint32_t d3d9_clsoff;
	uint32_t is_d3d9ex_clsoff;
};

struct d3d12_offsets {
	uint32_t execute_command_lists;
};

struct dxgi_offsets {
	uint32_t present;
	uint32_t resize;

	uint32_t present1;
};

struct dxgi_offsets2 {
	uint32_t release;
};

struct ddraw_offsets {
	uint32_t surface_create;
	uint32_t surface_restore;
	uint32_t surface_release;
	uint32_t surface_unlock;
	uint32_t surface_blt;
	uint32_t surface_flip;
	uint32_t surface_set_palette;
	uint32_t palette_set_entries;
};

struct shmem_data {
	volatile int last_tex;
	uint32_t tex1_offset;
	uint32_t tex2_offset;
};

struct shtex_data {
	uint32_t tex_handle;
};

enum capture_type {
	CAPTURE_TYPE_MEMORY,
	CAPTURE_TYPE_TEXTURE,
};

struct graphics_offsets {
	struct d3d8_offsets d3d8;
	struct d3d9_offsets d3d9;
	struct dxgi_offsets dxgi;
	struct ddraw_offsets ddraw;
	struct dxgi_offsets2 dxgi2;
	struct d3d12_offsets d3d12;
};

struct hook_info {
	/* hook version */
	uint32_t hook_ver_major;
	uint32_t hook_ver_minor;

	/* capture info */
	enum capture_type type;
	uint32_t window;
	uint32_t format;
	uint32_t cx;
	uint32_t cy;
	uint32_t UNUSED_base_cx;
	uint32_t UNUSED_base_cy;
	uint32_t pitch;
	uint32_t map_id;
	uint32_t map_size;
	bool flip;

	/* additional options */
	bool d3d12_use_swap_queue;
	uint64_t frame_interval;
	bool UNUSED_use_scale;
	bool force_shmem;
	bool capture_overlay;
	bool allow_srgb_alias;

	/* hook addresses */
	struct graphics_offsets offsets;

	uint32_t reserved[126];
};
static_assert(sizeof(struct hook_info) == 648, "ABI compatibility");

#pragma pack(pop)

#define GC_MAPPING_FLAGS (FILE_MAP_READ | FILE_MAP_WRITE)

static inline HANDLE create_hook_info(DWORD id)
{
	HANDLE handle = NULL;

	wchar_t new_name[64];
	const int len = swprintf(new_name, _countof(new_name),
				 SHMEM_HOOK_INFO L"%lu", id);
	if (len > 0) {
		handle = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL,
					    PAGE_READWRITE, 0,
					    sizeof(struct hook_info), new_name);
	}

	return handle;
}
