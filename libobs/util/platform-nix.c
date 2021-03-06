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

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <stdlib.h>
#include <limits.h>
#include <dlfcn.h>
#include <unistd.h>
#include <glob.h>
#include <time.h>
#include <signal.h>

#include "obsconfig.h"

#if !defined(__APPLE__)
#include <sys/times.h>
#include <sys/wait.h>
#include <libgen.h>
#if defined(__FreeBSD__) || defined(__OpenBSD__)
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <unistd.h>
#if defined(__FreeBSD__)
#include <libprocstat.h>
#endif
#else
#include <sys/resource.h>
#endif
#include <spawn.h>
#endif

#include "darray.h"
#include "dstr.h"
#include "platform.h"
#include "threading.h"

void *os_dlopen(const char *path)
{
	struct dstr dylib_name;

	if (!path)
		return NULL;

	dstr_init_copy(&dylib_name, path);
#ifdef __APPLE__
	if (!dstr_find(&dylib_name, ".so") && !dstr_find(&dylib_name, ".dylib"))
#else
	if (!dstr_find(&dylib_name, ".so"))
#endif
		dstr_cat(&dylib_name, ".so");

#ifdef __APPLE__
	void *res = dlopen(dylib_name.array, RTLD_LAZY | RTLD_FIRST);
#else
	void *res = dlopen(dylib_name.array, RTLD_LAZY);
#endif
	if (!res)
		blog(LOG_ERROR, "os_dlopen(%s->%s): %s\n", path,
		     dylib_name.array, dlerror());

	dstr_free(&dylib_name);
	return res;
}

void *os_dlsym(void *module, const char *func)
{
	return dlsym(module, func);
}

void os_dlclose(void *module)
{
	if (module)
		dlclose(module);
}

bool os_is_obs_plugin(const char *path)
{
	UNUSED_PARAMETER(path);

	/* not necessary on this platform */
	return true;
}

#if !defined(__APPLE__)

struct os_cpu_usage_info {
	clock_t last_cpu_time, last_sys_time, last_user_time;
	int core_count;
};

os_cpu_usage_info_t *os_cpu_usage_info_start(void)
{
	struct os_cpu_usage_info *info = bmalloc(sizeof(*info));
	struct tms time_sample;

	info->last_cpu_time = times(&time_sample);
	info->last_sys_time = time_sample.tms_stime;
	info->last_user_time = time_sample.tms_utime;
	info->core_count = sysconf(_SC_NPROCESSORS_ONLN);
	return info;
}

double os_cpu_usage_info_query(os_cpu_usage_info_t *info)
{
	struct tms time_sample;
	clock_t cur_cpu_time;
	double percent;

	if (!info)
		return 0.0;

	cur_cpu_time = times(&time_sample);
	if (cur_cpu_time <= info->last_cpu_time ||
	    time_sample.tms_stime < info->last_sys_time ||
	    time_sample.tms_utime < info->last_user_time)
		return 0.0;

	percent = (double)(time_sample.tms_stime - info->last_sys_time +
			   (time_sample.tms_utime - info->last_user_time));
	percent /= (double)(cur_cpu_time - info->last_cpu_time);
	percent /= (double)info->core_count;

	info->last_cpu_time = cur_cpu_time;
	info->last_sys_time = time_sample.tms_stime;
	info->last_user_time = time_sample.tms_utime;

	return percent * 100.0;
}

void os_cpu_usage_info_destroy(os_cpu_usage_info_t *info)
{
	if (info)
		bfree(info);
}

#endif

bool os_sleepto_ns(uint64_t time_target)
{
	uint64_t current = os_gettime_ns();
	if (time_target < current)
		return false;

	time_target -= current;

	struct timespec req, remain;
	memset(&req, 0, sizeof(req));
	memset(&remain, 0, sizeof(remain));
	req.tv_sec = time_target / 1000000000;
	req.tv_nsec = time_target % 1000000000;

	while (nanosleep(&req, &remain)) {
		req = remain;
		memset(&remain, 0, sizeof(remain));
	}

	return true;
}

void os_sleep_ms(uint32_t duration)
{
	usleep(duration * 1000);
}

#if !defined(__APPLE__)

uint64_t os_gettime_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec);
}

/* should return $HOME/.[name], or when using XDG,
 * should return $HOME/.config/[name] as default */
int os_get_config_path(char *dst, size_t size, const char *name)
{
#ifdef USE_XDG
	char *xdg_ptr = getenv("XDG_CONFIG_HOME");

	// If XDG_CONFIG_HOME is unset,
	// we use the default $HOME/.config/[name] instead
	if (xdg_ptr == NULL) {
		char *home_ptr = getenv("HOME");
		if (home_ptr == NULL)
			bcrash("Could not get $HOME\n");

		if (!name || !*name) {
			return snprintf(dst, size, "%s/.config", home_ptr);
		} else {
			return snprintf(dst, size, "%s/.config/%s", home_ptr,
					name);
		}
	} else {
		if (!name || !*name)
			return snprintf(dst, size, "%s", xdg_ptr);
		else
			return snprintf(dst, size, "%s/%s", xdg_ptr, name);
	}
#else
	char *path_ptr = getenv("HOME");
	if (path_ptr == NULL)
		bcrash("Could not get $HOME\n");

	if (!name || !*name)
		return snprintf(dst, size, "%s", path_ptr);
	else
		return snprintf(dst, size, "%s/.%s", path_ptr, name);
#endif
}

/* should return $HOME/.[name], or when using XDG,
 * should return $HOME/.config/[name] as default */
char *os_get_config_path_ptr(const char *name)
{
#ifdef USE_XDG
	struct dstr path;
	char *xdg_ptr = getenv("XDG_CONFIG_HOME");

	/* If XDG_CONFIG_HOME is unset,
	 * we use the default $HOME/.config/[name] instead */
	if (xdg_ptr == NULL) {
		char *home_ptr = getenv("HOME");
		if (home_ptr == NULL)
			bcrash("Could not get $HOME\n");

		dstr_init_copy(&path, home_ptr);
		dstr_cat(&path, "/.config/");
		dstr_cat(&path, name);
	} else {
		dstr_init_copy(&path, xdg_ptr);
		dstr_cat(&path, "/");
		dstr_cat(&path, name);
	}
	return path.array;
#else
	char *path_ptr = getenv("HOME");
	if (path_ptr == NULL)
		bcrash("Could not get $HOME\n");

	struct dstr path;
	dstr_init_copy(&path, path_ptr);
	dstr_cat(&path, "/.");
	dstr_cat(&path, name);
	return path.array;
#endif
}

int os_get_program_data_path(char *dst, size_t size, const char *name)
{
	return snprintf(dst, size, "/usr/local/share/%s", !!name ? name : "");
}

char *os_get_program_data_path_ptr(const char *name)
{
	size_t len =
		snprintf(NULL, 0, "/usr/local/share/%s", !!name ? name : "");
	char *str = bmalloc(len + 1);
	snprintf(str, len + 1, "/usr/local/share/%s", !!name ? name : "");
	str[len] = 0;
	return str;
}

#if defined(__OpenBSD__)
// a bit modified version of https://stackoverflow.com/a/31495527
ssize_t os_openbsd_get_executable_path(char *epath)
{
	int mib[4];
	char **argv;
	size_t len;
	const char *comm;
	int ok = 0;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC_ARGS;
	mib[2] = getpid();
	mib[3] = KERN_PROC_ARGV;

	if (sysctl(mib, 4, NULL, &len, NULL, 0) < 0)
		abort();

	if (!(argv = malloc(len)))
		abort();

	if (sysctl(mib, 4, argv, &len, NULL, 0) < 0)
		abort();

	comm = argv[0];

	if (*comm == '/' || *comm == '.') {
		if (realpath(comm, epath))
			ok = 1;
	} else {
		char *sp;
		char *xpath = strdup(getenv("PATH"));
		char *path = strtok_r(xpath, ":", &sp);
		struct stat st;

		if (!xpath)
			abort();

		while (path) {
			snprintf(epath, PATH_MAX, "%s/%s", path, comm);

			if (!stat(epath, &st) && (st.st_mode & S_IXUSR)) {
				ok = 1;
				break;
			}
			path = strtok_r(NULL, ":", &sp);
		}

		free(xpath);
	}

	free(argv);
	return ok ? (ssize_t)strlen(epath) : -1;
}
#endif

char *os_get_executable_path_ptr(const char *name)
{
	char exe[PATH_MAX];
#if defined(__FreeBSD__) || defined(__DragonFly__)
	int sysctlname[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
	size_t pathlen = PATH_MAX;
	ssize_t count;
	if (sysctl(sysctlname, nitems(sysctlname), exe, &pathlen, NULL, 0) ==
	    -1) {
		blog(LOG_ERROR, "sysctl(KERN_PROC_PATHNAME) failed, errno %d",
		     errno);
		return NULL;
	}
	count = pathlen;
#elif defined(__OpenBSD__)
	ssize_t count = os_openbsd_get_executable_path(exe);
#else
	ssize_t count = readlink("/proc/self/exe", exe, PATH_MAX - 1);
	if (count >= 0) {
		exe[count] = '\0';
	}
#endif
	const char *path_out = NULL;
	struct dstr path;

	if (count == -1) {
		return NULL;
	}

	path_out = dirname(exe);
	if (!path_out) {
		return NULL;
	}

	dstr_init_copy(&path, path_out);
	dstr_cat(&path, "/");

	if (name && *name) {
		dstr_cat(&path, name);
	}

	return path.array;
}

#endif

bool os_file_exists(const char *path)
{
	return access(path, F_OK) == 0;
}

size_t os_get_abs_path(const char *path, char *abspath, size_t size)
{
	size_t min_size = size < PATH_MAX ? size : PATH_MAX;
	char newpath[PATH_MAX];
	int ret;

	if (!abspath)
		return 0;

	if (!realpath(path, newpath))
		return 0;

	ret = snprintf(abspath, min_size, "%s", newpath);
	return ret >= 0 ? ret : 0;
}

char *os_get_abs_path_ptr(const char *path)
{
	char *ptr = bmalloc(512);

	if (!os_get_abs_path(path, ptr, 512)) {
		bfree(ptr);
		ptr = NULL;
	}

	return ptr;
}

struct os_dir {
	const char *path;
	DIR *dir;
	struct dirent *cur_dirent;
	struct os_dirent out;
};

os_dir_t *os_opendir(const char *path)
{
	struct os_dir *dir;
	DIR *dir_val;

	dir_val = opendir(path);
	if (!dir_val)
		return NULL;

	dir = bzalloc(sizeof(struct os_dir));
	dir->dir = dir_val;
	dir->path = path;
	return dir;
}

static inline bool is_dir(const char *path)
{
	struct stat stat_info;
	if (stat(path, &stat_info) == 0)
		return !!S_ISDIR(stat_info.st_mode);

	blog(LOG_DEBUG, "is_dir: stat for %s failed, errno: %d", path, errno);
	return false;
}

struct os_dirent *os_readdir(os_dir_t *dir)
{
	struct dstr file_path = {0};

	if (!dir)
		return NULL;

	dir->cur_dirent = readdir(dir->dir);
	if (!dir->cur_dirent)
		return NULL;

	const size_t length = strlen(dir->cur_dirent->d_name);
	if (sizeof(dir->out.d_name) <= length)
		return NULL;
	memcpy(dir->out.d_name, dir->cur_dirent->d_name, length + 1);

	dstr_copy(&file_path, dir->path);
	dstr_cat(&file_path, "/");
	dstr_cat(&file_path, dir->out.d_name);

	dir->out.directory = is_dir(file_path.array);

	dstr_free(&file_path);

	return &dir->out;
}

void os_closedir(os_dir_t *dir)
{
	if (dir) {
		closedir(dir->dir);
		bfree(dir);
	}
}

int64_t os_get_free_space(const char *path)
{
	struct statvfs info;
	int64_t ret = (int64_t)statvfs(path, &info);

	if (ret == 0)
		ret = (int64_t)info.f_bsize * (int64_t)info.f_bfree;

	return ret;
}

struct posix_glob_info {
	struct os_glob_info base;
	glob_t gl;
};

int os_glob(const char *pattern, int flags, os_glob_t **pglob)
{
	struct posix_glob_info pgi;
	int ret = glob(pattern, 0, NULL, &pgi.gl);

	if (ret == 0) {
		DARRAY(struct os_globent) list;
		da_init(list);

		for (size_t i = 0; i < pgi.gl.gl_pathc; i++) {
			struct os_globent ent = {0};

			ent.path = pgi.gl.gl_pathv[i];
			ent.directory = is_dir(ent.path);
			da_push_back(list, &ent);
		}

		pgi.base.gl_pathc = list.num;
		pgi.base.gl_pathv = list.array;

		*pglob = bmemdup(&pgi, sizeof(pgi));
	} else {
		*pglob = NULL;
	}

	UNUSED_PARAMETER(flags);
	return ret;
}

void os_globfree(os_glob_t *pglob)
{
	if (pglob) {
		struct posix_glob_info *pgi = (struct posix_glob_info *)pglob;
		globfree(&pgi->gl);

		bfree(pgi->base.gl_pathv);
		bfree(pgi);
	}
}

int os_unlink(const char *path)
{
	return unlink(path);
}

int os_rmdir(const char *path)
{
	return rmdir(path);
}

int os_mkdir(const char *path)
{
	if (mkdir(path, 0755) == 0)
		return MKDIR_SUCCESS;

	return (errno == EEXIST) ? MKDIR_EXISTS : MKDIR_ERROR;
}

int os_rename(const char *old_path, const char *new_path)
{
	return rename(old_path, new_path);
}

int os_safe_replace(const char *target, const char *from, const char *backup)
{
	if (backup && os_file_exists(target) && rename(target, backup) != 0)
		return -1;
	return rename(from, target);
}

#if !defined(__APPLE__)
os_performance_token_t *os_request_high_performance(const char *reason)
{
	UNUSED_PARAMETER(reason);
	return NULL;
}

void os_end_high_performance(os_performance_token_t *token)
{
	UNUSED_PARAMETER(token);
}
#endif

int os_copyfile(const char *file_path_in, const char *file_path_out)
{
	FILE *file_out = NULL;
	FILE *file_in = NULL;
	uint8_t data[4096];
	int ret = -1;
	size_t size;

	if (os_file_exists(file_path_out))
		return -1;

	file_in = fopen(file_path_in, "rb");
	if (!file_in)
		return -1;

	file_out = fopen(file_path_out, "ab+");
	if (!file_out)
		goto error;

	do {
		size = fread(data, 1, sizeof(data), file_in);
		if (size)
			size = fwrite(data, 1, size, file_out);
	} while (size == sizeof(data));

	ret = feof(file_in) ? 0 : -1;

error:
	if (file_out)
		fclose(file_out);
	fclose(file_in);
	return ret;
}

char *os_getcwd(char *path, size_t size)
{
	return getcwd(path, size);
}

int os_chdir(const char *path)
{
	return chdir(path);
}

#if !defined(__APPLE__)

#if HAVE_DBUS
struct dbus_sleep_info;

extern struct dbus_sleep_info *dbus_sleep_info_create(void);
extern void dbus_inhibit_sleep(struct dbus_sleep_info *dbus, const char *sleep,
			       bool active);
extern void dbus_sleep_info_destroy(struct dbus_sleep_info *dbus);
#endif

struct os_inhibit_info {
#if HAVE_DBUS
	struct dbus_sleep_info *dbus;
#endif
	pthread_t screensaver_thread;
	os_event_t *stop_event;
	char *reason;
	posix_spawnattr_t attr;
	bool active;
};

os_inhibit_t *os_inhibit_sleep_create(const char *reason)
{
	struct os_inhibit_info *info = bzalloc(sizeof(*info));
	sigset_t set;

#if HAVE_DBUS
	info->dbus = dbus_sleep_info_create();
#endif

	os_event_init(&info->stop_event, OS_EVENT_TYPE_AUTO);
	posix_spawnattr_init(&info->attr);

	sigemptyset(&set);
	posix_spawnattr_setsigmask(&info->attr, &set);
	sigaddset(&set, SIGPIPE);
	posix_spawnattr_setsigdefault(&info->attr, &set);
	posix_spawnattr_setflags(&info->attr, POSIX_SPAWN_SETSIGDEF |
						      POSIX_SPAWN_SETSIGMASK);

	info->reason = bstrdup(reason);
	return info;
}

extern char **environ;

static void reset_screensaver(os_inhibit_t *info)
{
	char *argv[3] = {(char *)"xdg-screensaver", (char *)"reset", NULL};
	pid_t pid;

	int err = posix_spawnp(&pid, "xdg-screensaver", NULL, &info->attr, argv,
			       environ);
	if (err == 0) {
		int status;
		while (waitpid(pid, &status, 0) == -1)
			;
	} else {
		blog(LOG_WARNING, "Failed to create xdg-screensaver: %d", err);
	}
}

static void *screensaver_thread(void *param)
{
	os_inhibit_t *info = param;

	while (os_event_timedwait(info->stop_event, 30000) == ETIMEDOUT)
		reset_screensaver(info);

	return NULL;
}

bool os_inhibit_sleep_set_active(os_inhibit_t *info, bool active)
{
	int ret;

	if (!info)
		return false;
	if (info->active == active)
		return false;

#if HAVE_DBUS
	if (info->dbus)
		dbus_inhibit_sleep(info->dbus, info->reason, active);
#endif

	if (!info->stop_event)
		return true;

	if (active) {
		ret = pthread_create(&info->screensaver_thread, NULL,
				     &screensaver_thread, info);
		if (ret < 0) {
			blog(LOG_ERROR, "Failed to create screensaver "
					"inhibitor thread");
			return false;
		}
	} else {
		os_event_signal(info->stop_event);
		pthread_join(info->screensaver_thread, NULL);
	}

	info->active = active;
	return true;
}

void os_inhibit_sleep_destroy(os_inhibit_t *info)
{
	if (info) {
		os_inhibit_sleep_set_active(info, false);
#if HAVE_DBUS
		dbus_sleep_info_destroy(info->dbus);
#endif
		os_event_destroy(info->stop_event);
		posix_spawnattr_destroy(&info->attr);
		bfree(info->reason);
		bfree(info);
	}
}

#endif

void os_breakpoint()
{
	raise(SIGTRAP);
}

#ifndef __APPLE__
static int physical_cores = 0;
static int logical_cores = 0;
static bool core_count_initialized = false;

static void os_get_cores_internal(void)
{
	if (core_count_initialized)
		return;

	core_count_initialized = true;

	logical_cores = sysconf(_SC_NPROCESSORS_ONLN);

#if defined(__linux__)
	int physical_id = -1;
	int last_physical_id = -1;
	int core_count = 0;
	char *line = NULL;
	size_t linecap = 0;

	FILE *fp;
	struct dstr proc_phys_id;
	struct dstr proc_phys_ids;

	fp = fopen("/proc/cpuinfo", "r");
	if (!fp)
		return;

	dstr_init(&proc_phys_id);
	dstr_init(&proc_phys_ids);

	while (getline(&line, &linecap, fp) != -1) {
		if (!strncmp(line, "physical id", 11)) {
			char *start = strchr(line, ':');
			if (!start || *(++start) == '\0')
				continue;

			physical_id = atoi(start);
			dstr_free(&proc_phys_id);
			dstr_init(&proc_phys_id);
			dstr_catf(&proc_phys_id, "%d", physical_id);
		}

		if (!strncmp(line, "cpu cores", 9)) {
			char *start = strchr(line, ':');
			if (!start || *(++start) == '\0')
				continue;

			if (dstr_is_empty(&proc_phys_ids) ||
			    (!dstr_is_empty(&proc_phys_ids) &&
			     !dstr_find(&proc_phys_ids, proc_phys_id.array))) {
				dstr_cat_dstr(&proc_phys_ids, &proc_phys_id);
				dstr_cat(&proc_phys_ids, " ");
				core_count += atoi(start);
			}
		}

		if (*line == '\n' && physical_id != last_physical_id) {
			last_physical_id = physical_id;
		}
	}

	if (core_count == 0)
		physical_cores = logical_cores;
	else
		physical_cores = core_count;

	fclose(fp);
	dstr_free(&proc_phys_ids);
	dstr_free(&proc_phys_id);
	free(line);
#elif defined(__FreeBSD__)
	char *text = os_quick_read_utf8_file("/var/run/dmesg.boot");
	char *core_count = text;
	int packages = 0;
	int cores = 0;

	struct dstr proc_packages;
	struct dstr proc_cores;
	dstr_init(&proc_packages);
	dstr_init(&proc_cores);

	if (!text || !*text) {
		physical_cores = logical_cores;
		return;
	}

	core_count = strstr(core_count, "\nFreeBSD/SMP: ");
	if (!core_count)
		goto FreeBSD_cores_cleanup;

	core_count++;
	core_count = strstr(core_count, "\nFreeBSD/SMP: ");
	if (!core_count)
		goto FreeBSD_cores_cleanup;

	core_count = strstr(core_count, ": ");
	core_count += 2;
	size_t len = strcspn(core_count, " ");
	dstr_ncopy(&proc_packages, core_count, len);

	core_count = strstr(core_count, "package(s) x ");
	if (!core_count)
		goto FreeBSD_cores_cleanup;

	core_count += 13;
	len = strcspn(core_count, " ");
	dstr_ncopy(&proc_cores, core_count, len);

FreeBSD_cores_cleanup:
	if (!dstr_is_empty(&proc_packages))
		packages = atoi(proc_packages.array);
	if (!dstr_is_empty(&proc_cores))
		cores = atoi(proc_cores.array);

	if (packages == 0)
		physical_cores = logical_cores;
	else if (cores == 0)
		physical_cores = packages;
	else
		physical_cores = packages * cores;

	dstr_free(&proc_cores);
	dstr_free(&proc_packages);
	bfree(text);
#else
	physical_cores = logical_cores;
#endif
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

#ifdef __FreeBSD__
uint64_t os_get_sys_free_size(void)
{
	uint64_t mem_free = 0;
	size_t length = sizeof(mem_free);
	if (sysctlbyname("vm.stats.vm.v_free_count", &mem_free, &length, NULL,
			 0) < 0)
		return 0;
	return mem_free;
}

static inline bool os_get_proc_memory_usage_internal(struct kinfo_proc *kinfo)
{
	int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
	size_t length = sizeof(*kinfo);
	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), kinfo, &length, NULL, 0) <
	    0)
		return false;
	return true;
}

bool os_get_proc_memory_usage(os_proc_memory_usage_t *usage)
{
	struct kinfo_proc kinfo;
	if (!os_get_proc_memory_usage_internal(&kinfo))
		return false;

	usage->resident_size =
		(uint64_t)kinfo.ki_rssize * sysconf(_SC_PAGESIZE);
	usage->virtual_size = (uint64_t)kinfo.ki_size;
	return true;
}

uint64_t os_get_proc_resident_size(void)
{
	struct kinfo_proc kinfo;
	if (!os_get_proc_memory_usage_internal(&kinfo))
		return 0;
	return (uint64_t)kinfo.ki_rssize * sysconf(_SC_PAGESIZE);
}

uint64_t os_get_proc_virtual_size(void)
{
	struct kinfo_proc kinfo;
	if (!os_get_proc_memory_usage_internal(&kinfo))
		return 0;
	return (uint64_t)kinfo.ki_size;
}
#else
uint64_t os_get_sys_free_size(void)
{
	return 0;
}

typedef struct {
	unsigned long virtual_size;
	unsigned long resident_size;
	unsigned long share_pages;
	unsigned long text;
	unsigned long library;
	unsigned long data;
	unsigned long dirty_pages;
} statm_t;

static inline bool os_get_proc_memory_usage_internal(statm_t *statm)
{
	const char *statm_path = "/proc/self/statm";

	FILE *f = fopen(statm_path, "r");
	if (!f)
		return false;

	if (fscanf(f, "%lu %lu %lu %lu %lu %lu %lu", &statm->virtual_size,
		   &statm->resident_size, &statm->share_pages, &statm->text,
		   &statm->library, &statm->data, &statm->dirty_pages) != 7) {
		fclose(f);
		return false;
	}

	fclose(f);
	return true;
}

bool os_get_proc_memory_usage(os_proc_memory_usage_t *usage)
{
	statm_t statm = {};
	if (!os_get_proc_memory_usage_internal(&statm))
		return false;

	usage->resident_size = statm.resident_size;
	usage->virtual_size = statm.virtual_size;
	return true;
}

uint64_t os_get_proc_resident_size(void)
{
	statm_t statm = {};
	if (!os_get_proc_memory_usage_internal(&statm))
		return 0;
	return (uint64_t)statm.resident_size;
}

uint64_t os_get_proc_virtual_size(void)
{
	statm_t statm = {};
	if (!os_get_proc_memory_usage_internal(&statm))
		return 0;
	return (uint64_t)statm.virtual_size;
}
#endif
#endif

uint64_t os_get_free_disk_space(const char *dir)
{
	struct statvfs info;
	if (statvfs(dir, &info) != 0)
		return 0;

	return (uint64_t)info.f_frsize * (uint64_t)info.f_bavail;
}
