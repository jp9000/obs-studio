#pragma once

#include "graphics-hook-config.h"

#ifdef _MSC_VER
/* conversion from data/function pointer */
#pragma warning(disable : 4152)
#endif

#include "../graphics-hook-info.h"
#include <ipc-util/pipe.h>
#include <psapi.h>

#ifdef __cplusplus
extern "C" {
#else
#if defined(_MSC_VER) && !defined(inline)
#define inline __inline
#endif
#endif

#define NUM_BUFFERS 3

extern void hlog(const char *format, ...);
extern void hlog_hr(const char *text, HRESULT hr);
static inline const char *get_process_name(void);
static inline HMODULE get_system_module(const char *module);
static inline HMODULE load_system_library(const char *module);
extern uint64_t os_gettime_ns(void);

#define flog(format, ...) hlog("%s: " format, __FUNCTION__, ##__VA_ARGS__)
#define flog_hr(text, hr) hlog_hr(__FUNCTION__ ": " text, hr)

static inline bool capture_active(void);
static inline bool capture_ready(void);
static inline bool capture_should_stop(void);
static inline bool capture_should_init(void);

extern void shmem_copy_data(size_t idx, void *volatile data);
extern bool shmem_texture_data_lock(int idx);
extern void shmem_texture_data_unlock(int idx);

extern bool hook_ddraw(void);
extern bool hook_d3d8(void);
extern bool hook_d3d9(void);
extern bool hook_d3d12(void);
extern bool hook_dxgi(void);
extern bool hook_gl(void);
#if COMPILE_VULKAN_HOOK
extern bool hook_vulkan(void);
#endif

extern void d3d10_capture(void *swap, void *backbuffer, bool capture_overlay);
extern void d3d10_free(void);
extern void d3d11_capture(void *swap, void *backbuffer, bool capture_overlay);
extern void d3d11_free(void);

#if COMPILE_D3D12_HOOK
extern void d3d12_capture(void *swap, void *backbuffer, bool capture_overlay);
extern void d3d12_free(void);
#endif

extern bool rehook_gl(void);

extern bool capture_init_shtex(struct shtex_data **data, HWND window,
			       uint32_t cx, uint32_t cy, uint32_t format,
			       bool flip, uintptr_t handle);
extern bool capture_init_shmem(struct shmem_data **data, HWND window,
			       uint32_t cx, uint32_t cy, uint32_t pitch,
			       uint32_t format, bool flip);
extern void capture_free(void);

extern struct hook_info *global_hook_info;

struct vertex {
	struct {
		float x, y, z, w;
	} pos;
	struct {
		float u, v;
	} tex;
};

static inline bool duplicate_handle(HANDLE *dst, HANDLE src)
{
	return !!DuplicateHandle(GetCurrentProcess(), src, GetCurrentProcess(),
				 dst, 0, false, DUPLICATE_SAME_ACCESS);
}

static inline void *get_offset_addr(HMODULE module, uint32_t offset)
{
	return (void *)((uintptr_t)module + (uintptr_t)offset);
}

/* ------------------------------------------------------------------------- */

extern ipc_pipe_client_t pipe;
extern HANDLE signal_restart;
extern HANDLE signal_stop;
extern HANDLE signal_ready;
extern HANDLE signal_exit;
extern HANDLE tex_mutexes[2];
extern char system_path[MAX_PATH];
extern char process_name[MAX_PATH];
extern wchar_t keepalive_name[64];
extern HWND dummy_window;
extern volatile bool active;

static inline const char *get_process_name(void)
{
	return process_name;
}

static inline HMODULE get_system_module(const char *module)
{
	char base_path[MAX_PATH];

	strcpy(base_path, system_path);
	strcat(base_path, "\\");
	strcat(base_path, module);
	return GetModuleHandleA(base_path);
}

static inline uint32_t module_size(HMODULE module)
{
	MODULEINFO info;
	bool success = !!GetModuleInformation(GetCurrentProcess(), module,
					      &info, sizeof(info));
	return success ? info.SizeOfImage : 0;
}

static inline HMODULE load_system_library(const char *name)
{
	char base_path[MAX_PATH];
	HMODULE module;

	strcpy(base_path, system_path);
	strcat(base_path, "\\");
	strcat(base_path, name);

	module = GetModuleHandleA(base_path);
	if (module)
		return module;

	return LoadLibraryA(base_path);
}

static inline bool capture_alive(void)
{
	HANDLE handle = OpenMutexW(SYNCHRONIZE, false, keepalive_name);
	const bool success = handle != NULL;
	if (success)
		CloseHandle(handle);
	return success;
}

static inline bool capture_active(void)
{
	return active;
}

static inline bool frame_ready(uint64_t interval)
{
	static uint64_t last_time = 0;
	uint64_t elapsed;
	uint64_t t;

	if (!interval) {
		return true;
	}

	t = os_gettime_ns();
	elapsed = t - last_time;

	if (elapsed < interval) {
		return false;
	}

	last_time = (elapsed > interval * 2) ? t : last_time + interval;
	return true;
}

static inline bool capture_ready(void)
{
	return capture_active() &&
	       frame_ready(global_hook_info->frame_interval);
}

static inline bool capture_stopped(void)
{
	return WaitForSingleObject(signal_stop, 0) == WAIT_OBJECT_0;
}

static inline bool capture_restarted(void)
{
	return WaitForSingleObject(signal_restart, 0) == WAIT_OBJECT_0;
}

static inline bool capture_should_stop(void)
{
	bool stop_requested = false;

	if (capture_active()) {
		static uint64_t last_keepalive_check = 0;
		uint64_t cur_time = os_gettime_ns();
		bool alive = true;

		if (cur_time - last_keepalive_check > 5000000000) {
			alive = capture_alive();
			last_keepalive_check = cur_time;
		}

		stop_requested = capture_stopped() || !alive;
	}

	return stop_requested;
}

extern bool init_pipe(void);

static inline bool capture_should_init(void)
{
	if (!capture_active() && capture_restarted()) {
		if (capture_alive()) {
			if (!ipc_pipe_client_valid(&pipe)) {
				init_pipe();
			}
			return true;
		}
	}

	return false;
}

#ifdef __cplusplus
}
#endif
