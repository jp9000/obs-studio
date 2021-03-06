/*
 * Copyright (c) 2013 Hugh Bailey <obs.jim@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define PSAPI_VERSION 1
#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <shlobj.h>
#include <intrin.h>
#include <psapi.h>

#include "base.h"
#include "platform.h"
#include "darray.h"
#include "dstr.h"
#include "windows/win-registry.h"
#include "windows/win-version.h"

#include "../../deps/w32-pthreads/pthread.h"

#define MAX_SZ_LEN 256

static bool have_clockfreq = false;
static LARGE_INTEGER clock_freq;
static uint32_t winver = 0;
static char win_release_id[MAX_SZ_LEN] = "unavailable";

static inline uint64_t get_clockfreq(void)
{
	if (!have_clockfreq) {
		QueryPerformanceFrequency(&clock_freq);
		have_clockfreq = true;
	}

	return clock_freq.QuadPart;
}

static inline uint32_t get_winver(void)
{
	if (!winver) {
		struct win_version_info ver;
		get_win_ver(&ver);
		winver = (ver.major << 8) | ver.minor;
	}

	return winver;
}

void *os_dlopen(const char *path)
{
	struct dstr dll_name;
	wchar_t *wpath;
	wchar_t *wpath_slash;
	HMODULE h_library = NULL;

	if (!path)
		return NULL;

	dstr_init_copy(&dll_name, path);
	dstr_replace(&dll_name, "\\", "/");
	if (!dstr_find(&dll_name, ".dll"))
		dstr_cat(&dll_name, ".dll");
	os_utf8_to_wcs_ptr(dll_name.array, 0, &wpath);

	dstr_free(&dll_name);

	/* to make module dependency issues easier to deal with, allow
	 * dynamically loaded libraries on windows to search for dependent
	 * libraries that are within the library's own directory */
	wpath_slash = wcsrchr(wpath, L'/');
	if (wpath_slash) {
		*wpath_slash = 0;
		SetDllDirectoryW(wpath);
		*wpath_slash = L'/';
	}

	h_library = LoadLibraryW(wpath);

	bfree(wpath);

	if (wpath_slash)
		SetDllDirectoryW(NULL);

	if (!h_library) {
		DWORD error = GetLastError();

		/* don't print error for libraries that aren't meant to be
		 * dynamically linked */
		if (error == ERROR_PROC_NOT_FOUND)
			return NULL;

		char *message = NULL;

		FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
				       FORMAT_MESSAGE_IGNORE_INSERTS |
				       FORMAT_MESSAGE_ALLOCATE_BUFFER,
			       NULL, error,
			       MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
			       (LPSTR)&message, 0, NULL);

		blog(LOG_INFO, "LoadLibrary failed for '%s': %s (%lu)", path,
		     message, error);

		if (message)
			LocalFree(message);
	}

	return h_library;
}

void *os_dlsym(void *module, const char *func)
{
	void *handle;

	handle = (void *)GetProcAddress(module, func);

	return handle;
}

void os_dlclose(void *module)
{
	FreeLibrary(module);
}

bool os_is_obs_plugin(const char *path)
{
	struct dstr dll_name;
	wchar_t *wpath;

	HANDLE hFile = INVALID_HANDLE_VALUE;
	HANDLE hFileMapping = NULL;
	VOID *base = NULL;

	PIMAGE_DOS_HEADER dos_header;
	PIMAGE_NT_HEADERS nt_headers;
	PIMAGE_SECTION_HEADER section, last_section;

	bool ret = false;

	if (!path)
		return false;

	dstr_init_copy(&dll_name, path);
	dstr_replace(&dll_name, "\\", "/");
	if (!dstr_find(&dll_name, ".dll"))
		dstr_cat(&dll_name, ".dll");
	os_utf8_to_wcs_ptr(dll_name.array, 0, &wpath);

	dstr_free(&dll_name);

	hFile = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, NULL,
			    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	bfree(wpath);

	if (hFile == INVALID_HANDLE_VALUE)
		goto cleanup;

	hFileMapping =
		CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if (hFileMapping == NULL)
		goto cleanup;

	base = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
	if (!base)
		goto cleanup;

	/* all mapped file i/o must be prepared to handle exceptions */
	__try {

		dos_header = (PIMAGE_DOS_HEADER)base;

		if (dos_header->e_magic != IMAGE_DOS_SIGNATURE)
			goto cleanup;

		nt_headers = (PIMAGE_NT_HEADERS)((byte *)dos_header +
						 dos_header->e_lfanew);

		if (nt_headers->Signature != IMAGE_NT_SIGNATURE)
			goto cleanup;

		PIMAGE_DATA_DIRECTORY data_dir;
		data_dir =
			&nt_headers->OptionalHeader
				 .DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

		if (data_dir->Size == 0)
			goto cleanup;

		section = IMAGE_FIRST_SECTION(nt_headers);
		last_section = section;

		/* find the section that contains the export directory */
		int i;
		for (i = 0; i < nt_headers->FileHeader.NumberOfSections; i++) {
			if (section->VirtualAddress <=
			    data_dir->VirtualAddress) {
				last_section = section;
				section++;
				continue;
			} else {
				break;
			}
		}

		/* double check in case we exited early */
		if (last_section->VirtualAddress > data_dir->VirtualAddress ||
		    section->VirtualAddress <= data_dir->VirtualAddress)
			goto cleanup;

		section = last_section;

		/* get a pointer to the export directory */
		PIMAGE_EXPORT_DIRECTORY export;
		export = (PIMAGE_EXPORT_DIRECTORY)(
			(byte *)base + data_dir->VirtualAddress -
			section->VirtualAddress + section->PointerToRawData);

		if (export->NumberOfNames == 0)
			goto cleanup;

		/* get a pointer to the export directory names */
		DWORD *names_ptr;
		names_ptr = (DWORD *)((byte *)base + export->AddressOfNames -
				      section->VirtualAddress +
				      section->PointerToRawData);

		/* iterate through each name and see if its an obs plugin */
		CHAR *name;
		size_t j;
		for (j = 0; j < export->NumberOfNames; j++) {

			name = (CHAR *)base + names_ptr[j] -
			       section->VirtualAddress +
			       section->PointerToRawData;

			if (!strcmp(name, "obs_module_load")) {
				ret = true;
				goto cleanup;
			}
		}

	} __except (EXCEPTION_EXECUTE_HANDLER) {
		/* we failed somehow, for compatibility let's assume it
		 * was a valid plugin and let the loader deal with it */
		ret = true;
		goto cleanup;
	}

cleanup:
	if (base)
		UnmapViewOfFile(base);

	if (hFileMapping != NULL)
		CloseHandle(hFileMapping);

	if (hFile != INVALID_HANDLE_VALUE)
		CloseHandle(hFile);

	return ret;
}

union time_data {
	FILETIME ft;
	unsigned long long val;
};

struct os_cpu_usage_info {
	union time_data last_time, last_sys_time, last_user_time;
	DWORD core_count;
};

os_cpu_usage_info_t *os_cpu_usage_info_start(void)
{
	struct os_cpu_usage_info *info = bzalloc(sizeof(*info));
	SYSTEM_INFO si;
	FILETIME dummy;

	GetSystemInfo(&si);
	GetSystemTimeAsFileTime(&info->last_time.ft);
	GetProcessTimes(GetCurrentProcess(), &dummy, &dummy,
			&info->last_sys_time.ft, &info->last_user_time.ft);
	info->core_count = si.dwNumberOfProcessors;

	return info;
}

double os_cpu_usage_info_query(os_cpu_usage_info_t *info)
{
	union time_data cur_time, cur_sys_time, cur_user_time;
	FILETIME dummy;
	double percent;

	if (!info)
		return 0.0;

	GetSystemTimeAsFileTime(&cur_time.ft);
	GetProcessTimes(GetCurrentProcess(), &dummy, &dummy, &cur_sys_time.ft,
			&cur_user_time.ft);

	percent = (double)(cur_sys_time.val - info->last_sys_time.val +
			   (cur_user_time.val - info->last_user_time.val));
	percent /= (double)(cur_time.val - info->last_time.val);
	percent /= (double)info->core_count;

	info->last_time.val = cur_time.val;
	info->last_sys_time.val = cur_sys_time.val;
	info->last_user_time.val = cur_user_time.val;

	return percent * 100.0;
}

void os_cpu_usage_info_destroy(os_cpu_usage_info_t *info)
{
	if (info)
		bfree(info);
}

bool os_sleepto_ns(uint64_t time_target)
{
	uint64_t t = os_gettime_ns();
	uint32_t milliseconds;

	if (t >= time_target)
		return false;

	milliseconds = (uint32_t)((time_target - t) / 1000000);
	if (milliseconds > 1)
		Sleep(milliseconds - 1);

	for (;;) {
		t = os_gettime_ns();
		if (t >= time_target)
			return true;

#if 0
		Sleep(1);
#else
		Sleep(0);
#endif
	}
}

void os_sleep_ms(uint32_t duration)
{
	/* windows 8+ appears to have decreased sleep precision */
	if (get_winver() >= 0x0602 && duration > 0)
		duration--;

	Sleep(duration);
}

uint64_t os_gettime_ns(void)
{
	LARGE_INTEGER current_time;
	double time_val;

	QueryPerformanceCounter(&current_time);
	time_val = (double)current_time.QuadPart;
	time_val *= 1000000000.0;
	time_val /= (double)get_clockfreq();

	return (uint64_t)time_val;
}

/* returns [folder]\[name] on windows */
static int os_get_path_internal(char *dst, size_t size, const char *name,
				int folder)
{
	wchar_t path_utf16[MAX_PATH];

	SHGetFolderPathW(NULL, folder, NULL, SHGFP_TYPE_CURRENT, path_utf16);

	if (os_wcs_to_utf8(path_utf16, 0, dst, size) != 0) {
		if (!name || !*name) {
			return (int)strlen(dst);
		}

		if (strcat_s(dst, size, "\\") == 0) {
			if (strcat_s(dst, size, name) == 0) {
				return (int)strlen(dst);
			}
		}
	}

	return -1;
}

static char *os_get_path_ptr_internal(const char *name, int folder)
{
	char *ptr;
	wchar_t path_utf16[MAX_PATH];
	struct dstr path;

	SHGetFolderPathW(NULL, folder, NULL, SHGFP_TYPE_CURRENT, path_utf16);

	os_wcs_to_utf8_ptr(path_utf16, 0, &ptr);
	dstr_init_move_array(&path, ptr);
	dstr_cat(&path, "\\");
	dstr_cat(&path, name);
	return path.array;
}

int os_get_config_path(char *dst, size_t size, const char *name)
{
	return os_get_path_internal(dst, size, name, CSIDL_APPDATA);
}

char *os_get_config_path_ptr(const char *name)
{
	return os_get_path_ptr_internal(name, CSIDL_APPDATA);
}

int os_get_program_data_path(char *dst, size_t size, const char *name)
{
	return os_get_path_internal(dst, size, name, CSIDL_COMMON_APPDATA);
}

char *os_get_program_data_path_ptr(const char *name)
{
	return os_get_path_ptr_internal(name, CSIDL_COMMON_APPDATA);
}

char *os_get_executable_path_ptr(const char *name)
{
	char *ptr;
	char *slash;
	wchar_t path_utf16[MAX_PATH];
	struct dstr path;

	GetModuleFileNameW(NULL, path_utf16, MAX_PATH);

	os_wcs_to_utf8_ptr(path_utf16, 0, &ptr);
	dstr_init_move_array(&path, ptr);
	dstr_replace(&path, "\\", "/");
	slash = strrchr(path.array, '/');
	if (slash) {
		size_t len = slash - path.array + 1;
		dstr_resize(&path, len);
	}

	if (name && *name) {
		dstr_cat(&path, name);
	}

	return path.array;
}

bool os_file_exists(const char *path)
{
	WIN32_FIND_DATAW wfd;
	HANDLE hFind;
	wchar_t *path_utf16;

	if (!os_utf8_to_wcs_ptr(path, 0, &path_utf16))
		return false;

	hFind = FindFirstFileW(path_utf16, &wfd);
	if (hFind != INVALID_HANDLE_VALUE)
		FindClose(hFind);

	bfree(path_utf16);
	return hFind != INVALID_HANDLE_VALUE;
}

size_t os_get_abs_path(const char *path, char *abspath, size_t size)
{
	wchar_t wpath[MAX_PATH];
	wchar_t wabspath[MAX_PATH];
	size_t out_len = 0;
	size_t len;

	if (!abspath)
		return 0;

	len = os_utf8_to_wcs(path, 0, wpath, MAX_PATH);
	if (!len)
		return 0;

	if (_wfullpath(wabspath, wpath, MAX_PATH) != NULL)
		out_len = os_wcs_to_utf8(wabspath, 0, abspath, size);
	return out_len;
}

char *os_get_abs_path_ptr(const char *path)
{
	char *ptr = bmalloc(MAX_PATH);

	if (!os_get_abs_path(path, ptr, MAX_PATH)) {
		bfree(ptr);
		ptr = NULL;
	}

	return ptr;
}

struct os_dir {
	HANDLE handle;
	WIN32_FIND_DATA wfd;
	bool first;
	struct os_dirent out;
};

os_dir_t *os_opendir(const char *path)
{
	struct dstr path_str = {0};
	struct os_dir *dir = NULL;
	WIN32_FIND_DATA wfd;
	HANDLE handle;
	wchar_t *w_path;

	dstr_copy(&path_str, path);
	dstr_cat(&path_str, "/*.*");

	if (os_utf8_to_wcs_ptr(path_str.array, path_str.len, &w_path) > 0) {
		handle = FindFirstFileW(w_path, &wfd);
		if (handle != INVALID_HANDLE_VALUE) {
			dir = bzalloc(sizeof(struct os_dir));
			dir->handle = handle;
			dir->first = true;
			dir->wfd = wfd;
		}

		bfree(w_path);
	}

	dstr_free(&path_str);

	return dir;
}

static inline bool is_dir(WIN32_FIND_DATA *wfd)
{
	return !!(wfd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

struct os_dirent *os_readdir(os_dir_t *dir)
{
	if (!dir)
		return NULL;

	if (dir->first) {
		dir->first = false;
	} else {
		if (!FindNextFileW(dir->handle, &dir->wfd))
			return NULL;
	}

	os_wcs_to_utf8(dir->wfd.cFileName, 0, dir->out.d_name,
		       sizeof(dir->out.d_name));

	dir->out.directory = is_dir(&dir->wfd);

	return &dir->out;
}

void os_closedir(os_dir_t *dir)
{
	if (dir) {
		FindClose(dir->handle);
		bfree(dir);
	}
}

int64_t os_get_free_space(const char *path)
{
	ULARGE_INTEGER remainingSpace;
	char abs_path[512];
	wchar_t w_abs_path[512];

	if (os_get_abs_path(path, abs_path, 512) > 0) {
		if (os_utf8_to_wcs(abs_path, 0, w_abs_path, 512) > 0) {
			BOOL success = GetDiskFreeSpaceExW(
				w_abs_path, (PULARGE_INTEGER)&remainingSpace,
				NULL, NULL);
			if (success)
				return (int64_t)remainingSpace.QuadPart;
		}
	}

	return -1;
}

static void make_globent(struct os_globent *ent, WIN32_FIND_DATA *wfd,
			 const char *pattern)
{
	struct dstr name = {0};
	struct dstr path = {0};
	char *slash;

	dstr_from_wcs(&name, wfd->cFileName);
	dstr_copy(&path, pattern);
	slash = strrchr(path.array, '/');
	if (slash)
		dstr_resize(&path, slash + 1 - path.array);
	else
		dstr_free(&path);

	dstr_cat_dstr(&path, &name);
	ent->path = path.array;
	ent->directory = is_dir(wfd);

	dstr_free(&name);
}

int os_glob(const char *pattern, int flags, os_glob_t **pglob)
{
	DARRAY(struct os_globent) files;
	HANDLE handle;
	WIN32_FIND_DATA wfd;
	int ret = -1;
	wchar_t *w_path;

	da_init(files);

	if (os_utf8_to_wcs_ptr(pattern, 0, &w_path) > 0) {
		handle = FindFirstFileW(w_path, &wfd);
		if (handle != INVALID_HANDLE_VALUE) {
			do {
				struct os_globent ent = {0};
				make_globent(&ent, &wfd, pattern);
				if (ent.path)
					da_push_back(files, &ent);
			} while (FindNextFile(handle, &wfd));
			FindClose(handle);

			*pglob = bmalloc(sizeof(**pglob));
			(*pglob)->gl_pathc = files.num;
			(*pglob)->gl_pathv = files.array;

			ret = 0;
		}

		bfree(w_path);
	}

	if (ret != 0)
		*pglob = NULL;

	UNUSED_PARAMETER(flags);
	return ret;
}

void os_globfree(os_glob_t *pglob)
{
	if (pglob) {
		for (size_t i = 0; i < pglob->gl_pathc; i++)
			bfree(pglob->gl_pathv[i].path);
		bfree(pglob->gl_pathv);
		bfree(pglob);
	}
}

int os_unlink(const char *path)
{
	wchar_t *w_path;
	bool success;

	os_utf8_to_wcs_ptr(path, 0, &w_path);
	if (!w_path)
		return -1;

	success = !!DeleteFileW(w_path);
	bfree(w_path);

	return success ? 0 : -1;
}

int os_rmdir(const char *path)
{
	wchar_t *w_path;
	bool success;

	os_utf8_to_wcs_ptr(path, 0, &w_path);
	if (!w_path)
		return -1;

	success = !!RemoveDirectoryW(w_path);
	bfree(w_path);

	return success ? 0 : -1;
}

int os_mkdir(const char *path)
{
	wchar_t *path_utf16;
	BOOL success;

	if (!os_utf8_to_wcs_ptr(path, 0, &path_utf16))
		return MKDIR_ERROR;

	success = CreateDirectory(path_utf16, NULL);
	bfree(path_utf16);

	if (!success)
		return (GetLastError() == ERROR_ALREADY_EXISTS) ? MKDIR_EXISTS
								: MKDIR_ERROR;

	return MKDIR_SUCCESS;
}

int os_rename(const char *old_path, const char *new_path)
{
	wchar_t *old_path_utf16 = NULL;
	wchar_t *new_path_utf16 = NULL;
	int code = -1;

	if (!os_utf8_to_wcs_ptr(old_path, 0, &old_path_utf16)) {
		return -1;
	}
	if (!os_utf8_to_wcs_ptr(new_path, 0, &new_path_utf16)) {
		goto error;
	}

	code = MoveFileExW(old_path_utf16, new_path_utf16,
			   MOVEFILE_REPLACE_EXISTING)
		       ? 0
		       : -1;

error:
	bfree(old_path_utf16);
	bfree(new_path_utf16);
	return code;
}

int os_safe_replace(const char *target, const char *from, const char *backup)
{
	wchar_t *wtarget = NULL;
	wchar_t *wfrom = NULL;
	wchar_t *wbackup = NULL;
	int code = -1;

	if (!target || !from)
		return -1;
	if (!os_utf8_to_wcs_ptr(target, 0, &wtarget))
		return -1;
	if (!os_utf8_to_wcs_ptr(from, 0, &wfrom))
		goto fail;
	if (backup && !os_utf8_to_wcs_ptr(backup, 0, &wbackup))
		goto fail;

	if (ReplaceFileW(wtarget, wfrom, wbackup, 0, NULL, NULL)) {
		code = 0;
	} else if (GetLastError() == ERROR_FILE_NOT_FOUND) {
		code = MoveFileExW(wfrom, wtarget, MOVEFILE_REPLACE_EXISTING)
			       ? 0
			       : -1;
	}

fail:
	bfree(wtarget);
	bfree(wfrom);
	bfree(wbackup);
	return code;
}

BOOL WINAPI DllMain(HINSTANCE hinst_dll, DWORD reason, LPVOID reserved)
{
	switch (reason) {

	case DLL_PROCESS_ATTACH:
		timeBeginPeriod(1);
#ifdef PTW32_STATIC_LIB
		pthread_win32_process_attach_np();
#endif
		break;

	case DLL_PROCESS_DETACH:
		timeEndPeriod(1);
#ifdef PTW32_STATIC_LIB
		pthread_win32_process_detach_np();
#endif
		break;

	case DLL_THREAD_ATTACH:
#ifdef PTW32_STATIC_LIB
		pthread_win32_thread_attach_np();
#endif
		break;

	case DLL_THREAD_DETACH:
#ifdef PTW32_STATIC_LIB
		pthread_win32_thread_detach_np();
#endif
		break;
	}

	UNUSED_PARAMETER(hinst_dll);
	UNUSED_PARAMETER(reserved);
	return true;
}

os_performance_token_t *os_request_high_performance(const char *reason)
{
	UNUSED_PARAMETER(reason);
	return NULL;
}

void os_end_high_performance(os_performance_token_t *token)
{
	UNUSED_PARAMETER(token);
}

int os_copyfile(const char *file_in, const char *file_out)
{
	wchar_t *file_in_utf16 = NULL;
	wchar_t *file_out_utf16 = NULL;
	int code = -1;

	if (!os_utf8_to_wcs_ptr(file_in, 0, &file_in_utf16)) {
		return -1;
	}
	if (!os_utf8_to_wcs_ptr(file_out, 0, &file_out_utf16)) {
		goto error;
	}

	code = CopyFileW(file_in_utf16, file_out_utf16, true) ? 0 : -1;

error:
	bfree(file_in_utf16);
	bfree(file_out_utf16);
	return code;
}

char *os_getcwd(char *path, size_t size)
{
	wchar_t *path_w;
	DWORD len;

	len = GetCurrentDirectoryW(0, NULL);
	if (!len)
		return NULL;

	path_w = bmalloc((len + 1) * sizeof(wchar_t));
	GetCurrentDirectoryW(len + 1, path_w);
	os_wcs_to_utf8(path_w, (size_t)len, path, size);
	bfree(path_w);

	return path;
}

int os_chdir(const char *path)
{
	wchar_t *path_w = NULL;
	size_t size;
	int ret;

	size = os_utf8_to_wcs_ptr(path, 0, &path_w);
	if (!path_w)
		return -1;

	ret = SetCurrentDirectoryW(path_w) ? 0 : -1;
	bfree(path_w);

	return ret;
}

typedef DWORD(WINAPI *get_file_version_info_size_w_t)(LPCWSTR module,
						      LPDWORD unused);
typedef BOOL(WINAPI *get_file_version_info_w_t)(LPCWSTR module, DWORD unused,
						DWORD len, LPVOID data);
typedef BOOL(WINAPI *ver_query_value_w_t)(LPVOID data, LPCWSTR subblock,
					  LPVOID *buf, PUINT sizeout);

static get_file_version_info_size_w_t get_file_version_info_size = NULL;
static get_file_version_info_w_t get_file_version_info = NULL;
static ver_query_value_w_t ver_query_value = NULL;
static bool ver_initialized = false;
static bool ver_initialize_success = false;

static bool initialize_version_functions(void)
{
	HMODULE ver = GetModuleHandleW(L"version");

	ver_initialized = true;

	if (!ver) {
		ver = LoadLibraryW(L"version");
		if (!ver) {
			blog(LOG_ERROR, "Failed to load windows "
					"version library");
			return false;
		}
	}

	get_file_version_info_size =
		(get_file_version_info_size_w_t)GetProcAddress(
			ver, "GetFileVersionInfoSizeW");
	get_file_version_info = (get_file_version_info_w_t)GetProcAddress(
		ver, "GetFileVersionInfoW");
	ver_query_value =
		(ver_query_value_w_t)GetProcAddress(ver, "VerQueryValueW");

	if (!get_file_version_info_size || !get_file_version_info ||
	    !ver_query_value) {
		blog(LOG_ERROR, "Failed to load windows version "
				"functions");
		return false;
	}

	ver_initialize_success = true;
	return true;
}

bool get_dll_ver(const wchar_t *lib, struct win_version_info *ver_info)
{
	VS_FIXEDFILEINFO *info = NULL;
	UINT len = 0;
	BOOL success;
	LPVOID data;
	DWORD size;
	char utf8_lib[512];

	if (!ver_initialized && !initialize_version_functions())
		return false;
	if (!ver_initialize_success)
		return false;

	os_wcs_to_utf8(lib, 0, utf8_lib, sizeof(utf8_lib));

	size = get_file_version_info_size(lib, NULL);
	if (!size) {
		blog(LOG_ERROR, "Failed to get %s version info size", utf8_lib);
		return false;
	}

	data = bmalloc(size);
	if (!get_file_version_info(lib, 0, size, data)) {
		blog(LOG_ERROR, "Failed to get %s version info", utf8_lib);
		bfree(data);
		return false;
	}

	success = ver_query_value(data, L"\\", (LPVOID *)&info, &len);
	if (!success || !info || !len) {
		blog(LOG_ERROR, "Failed to get %s version info value",
		     utf8_lib);
		bfree(data);
		return false;
	}

	ver_info->major = (int)HIWORD(info->dwFileVersionMS);
	ver_info->minor = (int)LOWORD(info->dwFileVersionMS);
	ver_info->build = (int)HIWORD(info->dwFileVersionLS);
	ver_info->revis = (int)LOWORD(info->dwFileVersionLS);

	bfree(data);
	return true;
}

bool is_64_bit_windows(void)
{
#if defined(_WIN64)
	return true;
#elif defined(_WIN32)
	BOOL b64 = false;
	return IsWow64Process(GetCurrentProcess(), &b64) && b64;
#endif
}

void get_reg_dword(HKEY hkey, LPCWSTR sub_key, LPCWSTR value_name,
		   struct reg_dword *info)
{
	struct reg_dword reg = {0};
	HKEY key;
	LSTATUS status;

	status = RegOpenKeyEx(hkey, sub_key, 0, KEY_READ, &key);

	if (status != ERROR_SUCCESS) {
		info->status = status;
		info->size = 0;
		info->return_value = 0;
		return;
	}

	reg.size = sizeof(reg.return_value);

	reg.status = RegQueryValueExW(key, value_name, NULL, NULL,
				      (LPBYTE)&reg.return_value, &reg.size);

	RegCloseKey(key);

	*info = reg;
}

#define WINVER_REG_KEY L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"

static inline void rtl_get_ver(struct win_version_info *ver)
{
	RTL_OSVERSIONINFOEXW osver = {0};
	HMODULE ntdll = GetModuleHandleW(L"ntdll");
	NTSTATUS s;

	NTSTATUS(WINAPI * get_ver)
	(RTL_OSVERSIONINFOEXW *) =
		(void *)GetProcAddress(ntdll, "RtlGetVersion");
	if (!get_ver) {
		return;
	}

	osver.dwOSVersionInfoSize = sizeof(osver);
	s = get_ver(&osver);
	if (s < 0) {
		return;
	}

	ver->major = osver.dwMajorVersion;
	ver->minor = osver.dwMinorVersion;
	ver->build = osver.dwBuildNumber;
	ver->revis = 0;
}

static inline bool get_reg_sz(HKEY key, const wchar_t *val, wchar_t *buf,
			      const size_t size)
{
	DWORD dwsize = (DWORD)size;
	LSTATUS status;

	status = RegQueryValueExW(key, val, NULL, NULL, (LPBYTE)buf, &dwsize);
	buf[(size / sizeof(wchar_t)) - 1] = 0;
	return status == ERROR_SUCCESS;
}

static inline void get_reg_ver(struct win_version_info *ver)
{
	HKEY key;
	DWORD size, dw_val;
	LSTATUS status;
	wchar_t str[MAX_SZ_LEN];

	status = RegOpenKeyW(HKEY_LOCAL_MACHINE, WINVER_REG_KEY, &key);
	if (status != ERROR_SUCCESS)
		return;

	size = sizeof(dw_val);

	status = RegQueryValueExW(key, L"CurrentMajorVersionNumber", NULL, NULL,
				  (LPBYTE)&dw_val, &size);
	if (status == ERROR_SUCCESS)
		ver->major = (int)dw_val;

	status = RegQueryValueExW(key, L"CurrentMinorVersionNumber", NULL, NULL,
				  (LPBYTE)&dw_val, &size);
	if (status == ERROR_SUCCESS)
		ver->minor = (int)dw_val;

	status = RegQueryValueExW(key, L"UBR", NULL, NULL, (LPBYTE)&dw_val,
				  &size);
	if (status == ERROR_SUCCESS)
		ver->revis = (int)dw_val;

	if (get_reg_sz(key, L"CurrentBuildNumber", str, sizeof(str))) {
		ver->build = wcstol(str, NULL, 10);
	}

	if (get_reg_sz(key, L"ReleaseId", str, sizeof(str))) {
		os_wcs_to_utf8(str, 0, win_release_id, MAX_SZ_LEN);
	}

	RegCloseKey(key);
}

static inline bool version_higher(struct win_version_info *cur,
				  struct win_version_info *new)
{
	if (new->major > cur->major) {
		return true;
	}

	if (new->major == cur->major) {
		if (new->minor > cur->minor) {
			return true;
		}
		if (new->minor == cur->minor) {
			if (new->build > cur->build) {
				return true;
			}
			if (new->build == cur->build) {
				return new->revis > cur->revis;
			}
		}
	}

	return false;
}

static inline void use_higher_ver(struct win_version_info *cur,
				  struct win_version_info *new)
{
	if (version_higher(cur, new))
		*cur = *new;
}

void get_win_ver(struct win_version_info *info)
{
	static struct win_version_info ver = {0};
	static bool got_version = false;

	if (!info)
		return;

	if (!got_version) {
		struct win_version_info reg_ver = {0};
		struct win_version_info rtl_ver = {0};
		struct win_version_info nto_ver = {0};

		get_reg_ver(&reg_ver);
		rtl_get_ver(&rtl_ver);
		get_dll_ver(L"ntoskrnl.exe", &nto_ver);

		ver = reg_ver;
		use_higher_ver(&ver, &rtl_ver);
		use_higher_ver(&ver, &nto_ver);

		got_version = true;
	}

	*info = ver;
}

const char *get_win_release_id(void)
{
	return win_release_id;
}

uint32_t get_win_ver_int(void)
{
	return get_winver();
}

struct os_inhibit_info {
	bool active;
};

os_inhibit_t *os_inhibit_sleep_create(const char *reason)
{
	UNUSED_PARAMETER(reason);
	return bzalloc(sizeof(struct os_inhibit_info));
}

bool os_inhibit_sleep_set_active(os_inhibit_t *info, bool active)
{
	if (!info)
		return false;
	if (info->active == active)
		return false;

	if (active) {
		SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED |
					ES_AWAYMODE_REQUIRED |
					ES_DISPLAY_REQUIRED);
	} else {
		SetThreadExecutionState(ES_CONTINUOUS);
	}

	info->active = active;
	return true;
}

void os_inhibit_sleep_destroy(os_inhibit_t *info)
{
	if (info) {
		os_inhibit_sleep_set_active(info, false);
		bfree(info);
	}
}

void os_breakpoint(void)
{
	__debugbreak();
}

DWORD num_logical_cores(ULONG_PTR mask)
{
	DWORD left_shift = sizeof(ULONG_PTR) * 8 - 1;
	DWORD bit_set_count = 0;
	ULONG_PTR bit_test = (ULONG_PTR)1 << left_shift;

	for (DWORD i = 0; i <= left_shift; ++i) {
		bit_set_count += ((mask & bit_test) ? 1 : 0);
		bit_test /= 2;
	}

	return bit_set_count;
}

static int physical_cores = 0;
static int logical_cores = 0;
static bool core_count_initialized = false;

static void os_get_cores_internal(void)
{
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION info = NULL, temp = NULL;
	DWORD len = 0;

	if (core_count_initialized)
		return;

	core_count_initialized = true;

	GetLogicalProcessorInformation(info, &len);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		return;

	info = malloc(len);

	if (GetLogicalProcessorInformation(info, &len)) {
		DWORD num = len / sizeof(*info);
		temp = info;

		for (DWORD i = 0; i < num; i++) {
			if (temp->Relationship == RelationProcessorCore) {
				ULONG_PTR mask = temp->ProcessorMask;

				physical_cores++;
				logical_cores += num_logical_cores(mask);
			}

			temp++;
		}
	}

	free(info);
}

int os_get_physical_cores(void)
{
	if (!core_count_initialized)
		os_get_cores_internal();
	return physical_cores;
}

int os_get_logical_cores(void)
{
	if (!core_count_initialized)
		os_get_cores_internal();
	return logical_cores;
}

static inline bool os_get_sys_memory_usage_internal(MEMORYSTATUSEX *msex)
{
	if (!GlobalMemoryStatusEx(msex))
		return false;
	return true;
}

uint64_t os_get_sys_free_size(void)
{
	MEMORYSTATUSEX msex = {sizeof(MEMORYSTATUSEX)};
	if (!os_get_sys_memory_usage_internal(&msex))
		return 0;
	return msex.ullAvailPhys;
}

static inline bool
os_get_proc_memory_usage_internal(PROCESS_MEMORY_COUNTERS *pmc)
{
	if (!GetProcessMemoryInfo(GetCurrentProcess(), pmc, sizeof(*pmc)))
		return false;
	return true;
}

bool os_get_proc_memory_usage(os_proc_memory_usage_t *usage)
{
	PROCESS_MEMORY_COUNTERS pmc = {sizeof(PROCESS_MEMORY_COUNTERS)};
	if (!os_get_proc_memory_usage_internal(&pmc))
		return false;

	usage->resident_size = pmc.WorkingSetSize;
	usage->virtual_size = pmc.PagefileUsage;
	return true;
}

uint64_t os_get_proc_resident_size(void)
{
	PROCESS_MEMORY_COUNTERS pmc = {sizeof(PROCESS_MEMORY_COUNTERS)};
	if (!os_get_proc_memory_usage_internal(&pmc))
		return 0;
	return pmc.WorkingSetSize;
}

uint64_t os_get_proc_virtual_size(void)
{
	PROCESS_MEMORY_COUNTERS pmc = {sizeof(PROCESS_MEMORY_COUNTERS)};
	if (!os_get_proc_memory_usage_internal(&pmc))
		return 0;
	return pmc.PagefileUsage;
}

uint64_t os_get_free_disk_space(const char *dir)
{
	wchar_t *wdir = NULL;
	if (!os_utf8_to_wcs_ptr(dir, 0, &wdir))
		return 0;

	ULARGE_INTEGER free;
	bool success = !!GetDiskFreeSpaceExW(wdir, &free, NULL, NULL);
	bfree(wdir);

	return success ? free.QuadPart : 0;
}
