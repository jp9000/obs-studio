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

#pragma once

#include <stdio.h>
#include <wchar.h>
#include <sys/types.h>
#include "c99defs.h"

/*
 * Platform-independent functions for Accessing files, encoding, DLLs,
 * sleep, timer, and timing.
 */

#ifdef __cplusplus
extern "C" {
#endif

EXPORT FILE *os_wfopen(const wchar_t *path, const char *mode);
EXPORT FILE *os_fopen(const char *path, const char *mode);
EXPORT int64_t os_fgetsize(FILE *file);

#ifdef _WIN32
EXPORT int os_stat(const char *file, struct stat *st);
#else
#define os_stat stat
#endif

EXPORT int os_fseeki64(FILE *file, int64_t offset, int origin);
EXPORT int64_t os_ftelli64(FILE *file);

EXPORT size_t os_fread_mbs(FILE *file, char **pstr);
EXPORT size_t os_fread_utf8(FILE *file, char **pstr);

/* functions purely for convenience */
EXPORT char *os_quick_read_utf8_file(const char *path);
EXPORT bool os_quick_write_utf8_file(const char *path, const char *str,
				     size_t len, bool marker);
EXPORT bool os_quick_write_utf8_file_safe(const char *path, const char *str,
					  size_t len, bool marker,
					  const char *temp_ext,
					  const char *backup_ext);
EXPORT char *os_quick_read_mbs_file(const char *path);
EXPORT bool os_quick_write_mbs_file(const char *path, const char *str,
				    size_t len);

EXPORT int64_t os_get_file_size(const char *path);
EXPORT int64_t os_get_free_space(const char *path);

EXPORT size_t os_mbs_to_wcs(const char *str, size_t str_len, wchar_t *dst,
			    size_t dst_size);
EXPORT size_t os_utf8_to_wcs(const char *str, size_t len, wchar_t *dst,
			     size_t dst_size);
EXPORT size_t os_wcs_to_mbs(const wchar_t *str, size_t len, char *dst,
			    size_t dst_size);
EXPORT size_t os_wcs_to_utf8(const wchar_t *str, size_t len, char *dst,
			     size_t dst_size);

EXPORT size_t os_mbs_to_wcs_ptr(const char *str, size_t len, wchar_t **pstr);
EXPORT size_t os_utf8_to_wcs_ptr(const char *str, size_t len, wchar_t **pstr);
EXPORT size_t os_wcs_to_mbs_ptr(const wchar_t *str, size_t len, char **pstr);
EXPORT size_t os_wcs_to_utf8_ptr(const wchar_t *str, size_t len, char **pstr);

EXPORT size_t os_utf8_to_mbs_ptr(const char *str, size_t len, char **pstr);
EXPORT size_t os_mbs_to_utf8_ptr(const char *str, size_t len, char **pstr);

EXPORT double os_strtod(const char *str);
EXPORT int os_dtostr(double value, char *dst, size_t size);

EXPORT void *os_dlopen(const char *path);
EXPORT void *os_dlsym(void *module, const char *func);
EXPORT void os_dlclose(void *module);
EXPORT bool os_is_obs_plugin(const char *path);

struct os_cpu_usage_info;
typedef struct os_cpu_usage_info os_cpu_usage_info_t;

EXPORT os_cpu_usage_info_t *os_cpu_usage_info_start(void);
EXPORT double os_cpu_usage_info_query(os_cpu_usage_info_t *info);
EXPORT void os_cpu_usage_info_destroy(os_cpu_usage_info_t *info);

typedef const void os_performance_token_t;
EXPORT os_performance_token_t *os_request_high_performance(const char *reason);
EXPORT void os_end_high_performance(os_performance_token_t *);

/**
 * Sleeps to a specific time (in nanoseconds).  Doesn't have to be super
 * accurate in terms of actual slept time because the target time is ensured.
 * Returns false if already at or past target time.
 */
EXPORT bool os_sleepto_ns(uint64_t time_target);
EXPORT void os_sleep_ms(uint32_t duration);

EXPORT uint64_t os_gettime_ns(void);

EXPORT int os_get_config_path(char *dst, size_t size, const char *name);
EXPORT char *os_get_config_path_ptr(const char *name);

EXPORT int os_get_program_data_path(char *dst, size_t size, const char *name);
EXPORT char *os_get_program_data_path_ptr(const char *name);

EXPORT char *os_get_executable_path_ptr(const char *name);

EXPORT bool os_file_exists(const char *path);

EXPORT size_t os_get_abs_path(const char *path, char *abspath, size_t size);
EXPORT char *os_get_abs_path_ptr(const char *path);

EXPORT const char *os_get_path_extension(const char *path);

struct os_dir;
typedef struct os_dir os_dir_t;

struct os_dirent {
	char d_name[256];
	bool directory;
};

EXPORT os_dir_t *os_opendir(const char *path);
EXPORT struct os_dirent *os_readdir(os_dir_t *dir);
EXPORT void os_closedir(os_dir_t *dir);

struct os_globent {
	char *path;
	bool directory;
};

struct os_glob_info {
	size_t gl_pathc;
	struct os_globent *gl_pathv;
};

typedef struct os_glob_info os_glob_t;

/* currently no flags available */

EXPORT int os_glob(const char *pattern, int flags, os_glob_t **pglob);
EXPORT void os_globfree(os_glob_t *pglob);

EXPORT int os_unlink(const char *path);
EXPORT int os_rmdir(const char *path);

EXPORT char *os_getcwd(char *path, size_t size);
EXPORT int os_chdir(const char *path);

EXPORT uint64_t os_get_free_disk_space(const char *dir);

#define MKDIR_EXISTS 1
#define MKDIR_SUCCESS 0
#define MKDIR_ERROR -1

EXPORT int os_mkdir(const char *path);
EXPORT int os_mkdirs(const char *path);
EXPORT int os_rename(const char *old_path, const char *new_path);
EXPORT int os_copyfile(const char *file_in, const char *file_out);
EXPORT int os_safe_replace(const char *target_path, const char *from_path,
			   const char *backup_path);

EXPORT char *os_generate_formatted_filename(const char *extension, bool space,
					    const char *format);

struct os_inhibit_info;
typedef struct os_inhibit_info os_inhibit_t;

EXPORT os_inhibit_t *os_inhibit_sleep_create(const char *reason);
EXPORT bool os_inhibit_sleep_set_active(os_inhibit_t *info, bool active);
EXPORT void os_inhibit_sleep_destroy(os_inhibit_t *info);

EXPORT void os_breakpoint(void);

EXPORT int os_get_physical_cores(void);
EXPORT int os_get_logical_cores(void);

EXPORT uint64_t os_get_sys_free_size(void);

struct os_proc_memory_usage {
	uint64_t resident_size;
	uint64_t virtual_size;
};
typedef struct os_proc_memory_usage os_proc_memory_usage_t;

EXPORT bool os_get_proc_memory_usage(os_proc_memory_usage_t *usage);
EXPORT uint64_t os_get_proc_resident_size(void);
EXPORT uint64_t os_get_proc_virtual_size(void);

#ifdef _MSC_VER
#define strtoll _strtoi64
#if _MSC_VER < 1900
#define snprintf _snprintf
#endif
#endif

/* clang-format off */
#ifdef __APPLE__
# define ARCH_BITS 64
#else
# ifdef _WIN32
#  ifdef _WIN64
#   define ARCH_BITS 64
#  else
#   define ARCH_BITS 32
#  endif
# else
#  ifdef __LP64__
#   define ARCH_BITS 64
#  else
#   define ARCH_BITS 32
#  endif
# endif
#endif
/* clang-format on */

#ifdef __cplusplus
}
#endif
