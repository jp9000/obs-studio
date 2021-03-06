#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <shellapi.h>
#include <stdbool.h>
#include "../obfuscate.h"
#include "../inject-library.h"

#if defined(_MSC_VER) && !defined(inline)
#define inline __inline
#endif

static void load_debug_privilege(void)
{
	const DWORD flags = TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY;
	TOKEN_PRIVILEGES tp;
	HANDLE token;
	LUID val;

	if (!OpenProcessToken(GetCurrentProcess(), flags, &token)) {
		return;
	}

	if (!!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &val)) {
		tp.PrivilegeCount = 1;
		tp.Privileges[0].Luid = val;
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		AdjustTokenPrivileges(token, false, &tp, sizeof(tp), NULL,
				      NULL);
	}

	CloseHandle(token);
}

static inline HANDLE open_process(DWORD desired_access, bool inherit_handle,
				  DWORD process_id)
{
	HANDLE(WINAPI * open_process_proc)(DWORD, BOOL, DWORD);
	open_process_proc = get_obfuscated_func(GetModuleHandleW(L"KERNEL32"),
						"HxjcQrmkb|~",
						0xc82efdf78201df87);

	return open_process_proc(desired_access, inherit_handle, process_id);
}

static inline int inject_library(HANDLE process, const wchar_t *dll)
{
	return inject_library_obf(process, dll, "E}mo|d[cefubWk~bgk",
				  0x7c3371986918e8f6, "Rqbr`T{cnor{Bnlgwz",
				  0x81bf81adc9456b35, "]`~wrl`KeghiCt",
				  0xadc6a7b9acd73c9b, "Zh}{}agHzfd@{",
				  0x57135138eb08ff1c, "DnafGhj}l~sX",
				  0x350bfacdf81b2018);
}

static inline int inject_library_safe(DWORD thread_id, const wchar_t *dll)
{
	return inject_library_safe_obf(thread_id, dll, "[bs^fbkmwuKfmfOvI",
				       0xEAD293602FCF9778ULL);
}

static inline int inject_library_full(DWORD process_id, const wchar_t *dll)
{
	HANDLE process = open_process(PROCESS_ALL_ACCESS, false, process_id);
	int ret;

	if (process) {
		ret = inject_library(process, dll);
		CloseHandle(process);
	} else {
		ret = INJECT_ERROR_OPEN_PROCESS_FAIL;
	}

	return ret;
}

static int inject_helper(wchar_t *argv[], const wchar_t *dll)
{
	DWORD id;
	DWORD use_safe_inject;

	use_safe_inject = wcstol(argv[2], NULL, 10);

	id = wcstol(argv[3], NULL, 10);
	if (id == 0) {
		return INJECT_ERROR_INVALID_PARAMS;
	}

	return use_safe_inject ? inject_library_safe(id, dll)
			       : inject_library_full(id, dll);
}

#define UNUSED_PARAMETER(x) ((void)(x))

int main(int argc, char *argv_ansi[])
{
	wchar_t dll_path[MAX_PATH];
	LPWSTR pCommandLineW;
	LPWSTR *argv;
	int ret = INJECT_ERROR_INVALID_PARAMS;

	SetErrorMode(SEM_FAILCRITICALERRORS);
	load_debug_privilege();

	pCommandLineW = GetCommandLineW();
	argv = CommandLineToArgvW(pCommandLineW, &argc);
	if (argv && argc == 4) {
		DWORD size = GetModuleFileNameW(NULL, dll_path, MAX_PATH);
		if (size) {
			ret = inject_helper(argv, argv[1]);
		}
	}
	LocalFree(argv);

	UNUSED_PARAMETER(argv_ansi);
	return ret;
}
