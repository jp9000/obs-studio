#include <windows.h>
#include <stdbool.h>
#include "obfuscate.h"
#include "inject-library.h"

typedef HANDLE(WINAPI *create_remote_thread_t)(HANDLE, LPSECURITY_ATTRIBUTES,
					       SIZE_T, LPTHREAD_START_ROUTINE,
					       LPVOID, DWORD, LPDWORD);
typedef BOOL(WINAPI *write_process_memory_t)(HANDLE, LPVOID, LPCVOID, SIZE_T,
					     SIZE_T *);
typedef LPVOID(WINAPI *virtual_alloc_ex_t)(HANDLE, LPVOID, SIZE_T, DWORD,
					   DWORD);
typedef BOOL(WINAPI *virtual_free_ex_t)(HANDLE, LPVOID, SIZE_T, DWORD);

int inject_library_obf(HANDLE process, const wchar_t *dll,
		       const char *create_remote_thread_obf, uint64_t obf1,
		       const char *write_process_memory_obf, uint64_t obf2,
		       const char *virtual_alloc_ex_obf, uint64_t obf3,
		       const char *virtual_free_ex_obf, uint64_t obf4,
		       const char *load_library_w_obf, uint64_t obf5)
{
	int ret = INJECT_ERROR_UNLIKELY_FAIL;
	DWORD last_error = 0;
	bool success = false;
	size_t written_size;
	DWORD thread_id;
	HANDLE thread = NULL;
	size_t size;
	void *mem;

	/* -------------------------------- */

	HMODULE kernel32 = GetModuleHandleW(L"KERNEL32");
	create_remote_thread_t create_remote_thread;
	write_process_memory_t write_process_memory;
	virtual_alloc_ex_t virtual_alloc_ex;
	virtual_free_ex_t virtual_free_ex;
	FARPROC load_library_w;

	create_remote_thread = (create_remote_thread_t)get_obfuscated_func(
		kernel32, create_remote_thread_obf, obf1);
	write_process_memory = (write_process_memory_t)get_obfuscated_func(
		kernel32, write_process_memory_obf, obf2);
	virtual_alloc_ex = (virtual_alloc_ex_t)get_obfuscated_func(
		kernel32, virtual_alloc_ex_obf, obf3);
	virtual_free_ex = (virtual_free_ex_t)get_obfuscated_func(
		kernel32, virtual_free_ex_obf, obf4);
	load_library_w = (FARPROC)get_obfuscated_func(kernel32,
						      load_library_w_obf, obf5);

	/* -------------------------------- */

	size = (wcslen(dll) + 1) * sizeof(wchar_t);
	mem = virtual_alloc_ex(process, NULL, size, MEM_RESERVE | MEM_COMMIT,
			       PAGE_READWRITE);
	if (!mem) {
		goto fail;
	}

	success = write_process_memory(process, mem, dll, size, &written_size);
	if (!success) {
		goto fail;
	}

	thread = create_remote_thread(process, NULL, 0,
				      (LPTHREAD_START_ROUTINE)load_library_w,
				      mem, 0, &thread_id);
	if (!thread) {
		goto fail;
	}

	if (WaitForSingleObject(thread, 4000) == WAIT_OBJECT_0) {
		DWORD code;
		GetExitCodeThread(thread, &code);
		ret = (code != 0) ? 0 : INJECT_ERROR_INJECT_FAILED;

		SetLastError(0);
	}

fail:
	if (ret == INJECT_ERROR_UNLIKELY_FAIL) {
		last_error = GetLastError();
	}
	if (thread) {
		CloseHandle(thread);
	}
	if (mem) {
		virtual_free_ex(process, mem, 0, MEM_RELEASE);
	}
	if (last_error != 0) {
		SetLastError(last_error);
	}

	return ret;
}

/* ------------------------------------------------------------------------- */

typedef HHOOK(WINAPI *set_windows_hook_ex_t)(int, HOOKPROC, HINSTANCE, DWORD);

#define RETRY_INTERVAL_MS 500
#define TOTAL_RETRY_TIME_MS 4000
#define RETRY_COUNT (TOTAL_RETRY_TIME_MS / RETRY_INTERVAL_MS)

int inject_library_safe_obf(DWORD thread_id, const wchar_t *dll,
			    const char *set_windows_hook_ex_obf, uint64_t obf1)
{
	HMODULE user32 = GetModuleHandleW(L"USER32");
	set_windows_hook_ex_t set_windows_hook_ex;
	HMODULE lib = LoadLibraryW(dll);
	HOOKPROC proc;
	HHOOK hook;
	size_t i;

	if (!lib || !user32) {
		return INJECT_ERROR_UNLIKELY_FAIL;
	}

#ifdef _WIN64
	proc = (HOOKPROC)GetProcAddress(lib, "dummy_debug_proc");
#else
	proc = (HOOKPROC)GetProcAddress(lib, "_dummy_debug_proc@12");
#endif

	if (!proc) {
		return INJECT_ERROR_UNLIKELY_FAIL;
	}

	set_windows_hook_ex = (set_windows_hook_ex_t)get_obfuscated_func(
		user32, set_windows_hook_ex_obf, obf1);

	hook = set_windows_hook_ex(WH_GETMESSAGE, proc, lib, thread_id);
	if (!hook) {
		return GetLastError();
	}

	/* SetWindowsHookEx does not inject the library in to the target
	 * process unless the event associated with it has occurred, so
	 * repeatedly send the hook message to start the hook at small
	 * intervals to signal to SetWindowsHookEx to process the message and
	 * therefore inject the library in to the target process.  Repeating
	 * this is mostly just a precaution. */

	for (i = 0; i < RETRY_COUNT; i++) {
		Sleep(RETRY_INTERVAL_MS);
		PostThreadMessage(thread_id, WM_USER + 432, 0, (LPARAM)hook);
	}
	return 0;
}
