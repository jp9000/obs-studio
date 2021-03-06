#include <windows.h>
#include <stdint.h>

#define INJECT_ERROR_INJECT_FAILED -1
#define INJECT_ERROR_INVALID_PARAMS -2
#define INJECT_ERROR_OPEN_PROCESS_FAIL -3
#define INJECT_ERROR_UNLIKELY_FAIL -4

extern int inject_library_obf(HANDLE process, const wchar_t *dll,
			      const char *create_remote_thread_obf,
			      uint64_t obf1,
			      const char *write_process_memory_obf,
			      uint64_t obf2, const char *virtual_alloc_ex_obf,
			      uint64_t obf3, const char *virtual_free_ex_obf,
			      uint64_t obf4, const char *load_library_w_obf,
			      uint64_t obf5);

extern int inject_library_safe_obf(DWORD thread_id, const wchar_t *dll,
				   const char *set_windows_hook_ex_obf,
				   uint64_t obf1);
