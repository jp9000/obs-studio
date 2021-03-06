/*
 * Copyright (c) 2013-2018 Ruwen Hahn <palana@stunned.de>
 *                         Hugh "Jim" Bailey <obs.jim@gmail.com>
 *                         Marvin Scholz <epirat07@gmail.com>
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

#include "base.h"
#include "platform.h"
#include "dstr.h"

#include <dlfcn.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>

#include <CoreServices/CoreServices.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach-o/dyld.h>

#include <IOKit/pwr_mgt/IOPMLib.h>

#import <Cocoa/Cocoa.h>

#include "apple/cfstring-utils.h"

uint64_t os_gettime_ns(void)
{
	return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}

/* gets the location [domain mask]/Library/Application Support/[name] */
static int os_get_path_internal(char *dst, size_t size, const char *name,
				NSSearchPathDomainMask domainMask)
{
	NSArray *paths = NSSearchPathForDirectoriesInDomains(
		NSApplicationSupportDirectory, domainMask, YES);

	if ([paths count] == 0)
		bcrash("Could not get home directory (platform-cocoa)");

	NSString *application_support = paths[0];
	const char *base_path = [application_support UTF8String];

	if (!name || !*name)
		return snprintf(dst, size, "%s", base_path);
	else
		return snprintf(dst, size, "%s/%s", base_path, name);
}

static char *os_get_path_ptr_internal(const char *name,
				      NSSearchPathDomainMask domainMask)
{
	NSArray *paths = NSSearchPathForDirectoriesInDomains(
		NSApplicationSupportDirectory, domainMask, YES);

	if ([paths count] == 0)
		bcrash("Could not get home directory (platform-cocoa)");

	NSString *application_support = paths[0];

	NSUInteger len = [application_support
		lengthOfBytesUsingEncoding:NSUTF8StringEncoding];

	char *path_ptr = bmalloc(len + 1);

	path_ptr[len] = 0;

	memcpy(path_ptr, [application_support UTF8String], len);

	struct dstr path;
	dstr_init_move_array(&path, path_ptr);
	dstr_cat(&path, "/");
	dstr_cat(&path, name);
	return path.array;
}

int os_get_config_path(char *dst, size_t size, const char *name)
{
	return os_get_path_internal(dst, size, name, NSUserDomainMask);
}

char *os_get_config_path_ptr(const char *name)
{
	return os_get_path_ptr_internal(name, NSUserDomainMask);
}

int os_get_program_data_path(char *dst, size_t size, const char *name)
{
	return os_get_path_internal(dst, size, name, NSLocalDomainMask);
}

char *os_get_program_data_path_ptr(const char *name)
{
	return os_get_path_ptr_internal(name, NSLocalDomainMask);
}

char *os_get_executable_path_ptr(const char *name)
{
	char exe[PATH_MAX];
	char abs_path[PATH_MAX];
	uint32_t size = sizeof(exe);
	struct dstr path;
	char *slash;

	if (_NSGetExecutablePath(exe, &size) != 0) {
		return NULL;
	}

	if (!realpath(exe, abs_path)) {
		return NULL;
	}

	dstr_init_copy(&path, abs_path);
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

struct os_cpu_usage_info {
	int64_t last_cpu_time;
	int64_t last_sys_time;
	int core_count;
};

static inline void add_time_value(time_value_t *dst, time_value_t *a,
				  time_value_t *b)
{
	dst->microseconds = a->microseconds + b->microseconds;
	dst->seconds = a->seconds + b->seconds;

	if (dst->microseconds >= 1000000) {
		dst->seconds += dst->microseconds / 1000000;
		dst->microseconds %= 1000000;
	}
}

static bool get_time_info(int64_t *cpu_time, int64_t *sys_time)
{
	mach_port_t task = mach_task_self();
	struct task_thread_times_info thread_data;
	struct task_basic_info_64 task_data;
	mach_msg_type_number_t count;
	kern_return_t kern_ret;
	time_value_t cur_time;

	*cpu_time = 0;
	*sys_time = 0;

	count = TASK_THREAD_TIMES_INFO_COUNT;
	kern_ret = task_info(task, TASK_THREAD_TIMES_INFO,
			     (task_info_t)&thread_data, &count);
	if (kern_ret != KERN_SUCCESS)
		return false;

	count = TASK_BASIC_INFO_64_COUNT;
	kern_ret = task_info(task, TASK_BASIC_INFO_64, (task_info_t)&task_data,
			     &count);
	if (kern_ret != KERN_SUCCESS)
		return false;

	add_time_value(&cur_time, &thread_data.user_time,
		       &thread_data.system_time);
	add_time_value(&cur_time, &cur_time, &task_data.user_time);
	add_time_value(&cur_time, &cur_time, &task_data.system_time);

	*cpu_time = os_gettime_ns() / 1000;
	*sys_time = cur_time.seconds * 1000000 + cur_time.microseconds;
	return true;
}

os_cpu_usage_info_t *os_cpu_usage_info_start(void)
{
	struct os_cpu_usage_info *info = bmalloc(sizeof(*info));

	if (!get_time_info(&info->last_cpu_time, &info->last_sys_time)) {
		bfree(info);
		return NULL;
	}

	info->core_count = sysconf(_SC_NPROCESSORS_ONLN);
	return info;
}

double os_cpu_usage_info_query(os_cpu_usage_info_t *info)
{
	int64_t sys_time, cpu_time;
	int64_t sys_time_delta, cpu_time_delta;

	if (!info || !get_time_info(&cpu_time, &sys_time))
		return 0.0;

	sys_time_delta = sys_time - info->last_sys_time;
	cpu_time_delta = cpu_time - info->last_cpu_time;

	if (cpu_time_delta == 0)
		return 0.0;

	info->last_sys_time = sys_time;
	info->last_cpu_time = cpu_time;

	return (double)sys_time_delta * 100.0 / (double)cpu_time_delta /
	       (double)info->core_count;
}

void os_cpu_usage_info_destroy(os_cpu_usage_info_t *info)
{
	if (info)
		bfree(info);
}

os_performance_token_t *os_request_high_performance(const char *reason)
{
	@autoreleasepool {
		NSProcessInfo *pi = [NSProcessInfo processInfo];
		SEL sel = @selector(beginActivityWithOptions:reason:);
		if (![pi respondsToSelector:sel])
			return nil;

		//taken from http://stackoverflow.com/a/20100906
		id activity = [pi beginActivityWithOptions:0x00FFFFFF
						    reason:@(reason)];

		return CFBridgingRetain(activity);
	}
}

void os_end_high_performance(os_performance_token_t *token)
{
	@autoreleasepool {
		NSProcessInfo *pi = [NSProcessInfo processInfo];
		SEL sel = @selector(beginActivityWithOptions:reason:);
		if (![pi respondsToSelector:sel])
			return;

		[pi endActivity:CFBridgingRelease(token)];
	}
}

struct os_inhibit_info {
	CFStringRef reason;
	IOPMAssertionID sleep_id;
	IOPMAssertionID user_id;
	bool active;
};

os_inhibit_t *os_inhibit_sleep_create(const char *reason)
{
	struct os_inhibit_info *info = bzalloc(sizeof(*info));
	if (!reason)
		info->reason = CFStringCreateWithCString(
			kCFAllocatorDefault, reason, kCFStringEncodingUTF8);
	else
		info->reason =
			CFStringCreateCopy(kCFAllocatorDefault, CFSTR(""));

	return info;
}

bool os_inhibit_sleep_set_active(os_inhibit_t *info, bool active)
{
	IOReturn success;

	if (!info)
		return false;
	if (info->active == active)
		return false;

	if (active) {
		IOPMAssertionDeclareUserActivity(
			info->reason, kIOPMUserActiveLocal, &info->user_id);
		success = IOPMAssertionCreateWithName(
			kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn,
			info->reason, &info->sleep_id);

		if (success != kIOReturnSuccess) {
			blog(LOG_WARNING, "Failed to disable sleep");
			return false;
		}
	} else {
		IOPMAssertionRelease(info->sleep_id);
	}

	info->active = active;
	return true;
}

void os_inhibit_sleep_destroy(os_inhibit_t *info)
{
	if (info) {
		os_inhibit_sleep_set_active(info, false);
		CFRelease(info->reason);
		bfree(info);
	}
}

static int physical_cores = 0;
static int logical_cores = 0;
static bool core_count_initialized = false;

static void os_get_cores_internal(void)
{
	if (core_count_initialized)
		return;

	core_count_initialized = true;

	size_t size;
	int ret;

	size = sizeof(physical_cores);
	ret = sysctlbyname("machdep.cpu.core_count", &physical_cores, &size,
			   NULL, 0);
	if (ret != 0)
		return;

	ret = sysctlbyname("machdep.cpu.thread_count", &logical_cores, &size,
			   NULL, 0);
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

static inline bool os_get_sys_memory_usage_internal(vm_statistics_t vmstat)
{
	mach_msg_type_number_t out_count = HOST_VM_INFO_COUNT;
	if (host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)vmstat,
			    &out_count) != KERN_SUCCESS)
		return false;
	return true;
}

uint64_t os_get_sys_free_size(void)
{
	vm_statistics_data_t vmstat = {};
	if (!os_get_sys_memory_usage_internal(&vmstat))
		return 0;

	return vmstat.free_count * vm_page_size;
}

#ifndef MACH_TASK_BASIC_INFO
typedef task_basic_info_data_t mach_task_basic_info_data_t;
#endif

static inline bool
os_get_proc_memory_usage_internal(mach_task_basic_info_data_t *taskinfo)
{
#ifdef MACH_TASK_BASIC_INFO
	const task_flavor_t flavor = MACH_TASK_BASIC_INFO;
	mach_msg_type_number_t out_count = MACH_TASK_BASIC_INFO_COUNT;
#else
	const task_flavor_t flavor = TASK_BASIC_INFO;
	mach_msg_type_number_t out_count = TASK_BASIC_INFO_COUNT;
#endif
	if (task_info(mach_task_self(), flavor, (task_info_t)taskinfo,
		      &out_count) != KERN_SUCCESS)
		return false;
	return true;
}

bool os_get_proc_memory_usage(os_proc_memory_usage_t *usage)
{
	mach_task_basic_info_data_t taskinfo = {};
	if (!os_get_proc_memory_usage_internal(&taskinfo))
		return false;

	usage->resident_size = taskinfo.resident_size;
	usage->virtual_size = taskinfo.virtual_size;
	return true;
}

uint64_t os_get_proc_resident_size(void)
{
	mach_task_basic_info_data_t taskinfo = {};
	if (!os_get_proc_memory_usage_internal(&taskinfo))
		return 0;
	return taskinfo.resident_size;
}

uint64_t os_get_proc_virtual_size(void)
{
	mach_task_basic_info_data_t taskinfo = {};
	if (!os_get_proc_memory_usage_internal(&taskinfo))
		return 0;
	return taskinfo.virtual_size;
}

/* Obtains a copy of the contents of a CFString in specified encoding.
 * Returns char* (must be bfree'd by caller) or NULL on failure.
 */
char *cfstr_copy_cstr(CFStringRef cfstring, CFStringEncoding cfstring_encoding)
{
	if (!cfstring)
		return NULL;

	// Try the quick way to obtain the buffer
	const char *tmp_buffer =
		CFStringGetCStringPtr(cfstring, cfstring_encoding);

	if (tmp_buffer != NULL)
		return bstrdup(tmp_buffer);

	// The quick way did not work, try the more expensive one
	CFIndex length = CFStringGetLength(cfstring);
	CFIndex max_size =
		CFStringGetMaximumSizeForEncoding(length, cfstring_encoding);

	// If result would exceed LONG_MAX, kCFNotFound is returned
	if (max_size == kCFNotFound)
		return NULL;

	// Account for the null terminator
	max_size++;

	char *buffer = bmalloc(max_size);

	if (buffer == NULL) {
		return NULL;
	}

	// Copy CFString in requested encoding to buffer
	Boolean success = CFStringGetCString(cfstring, buffer, max_size,
					     cfstring_encoding);

	if (!success) {
		bfree(buffer);
		buffer = NULL;
	}
	return buffer;
}

/* Copies the contents of a CFString in specified encoding to a given dstr.
 * Returns true on success or false on failure.
 * In case of failure, the dstr capacity but not size is changed.
 */
bool cfstr_copy_dstr(CFStringRef cfstring, CFStringEncoding cfstring_encoding,
		     struct dstr *str)
{
	if (!cfstring)
		return false;

	// Try the quick way to obtain the buffer
	const char *tmp_buffer =
		CFStringGetCStringPtr(cfstring, cfstring_encoding);

	if (tmp_buffer != NULL) {
		dstr_copy(str, tmp_buffer);
		return true;
	}

	// The quick way did not work, try the more expensive one
	CFIndex length = CFStringGetLength(cfstring);
	CFIndex max_size =
		CFStringGetMaximumSizeForEncoding(length, cfstring_encoding);

	// If result would exceed LONG_MAX, kCFNotFound is returned
	if (max_size == kCFNotFound)
		return NULL;

	// Account for the null terminator
	max_size++;

	dstr_ensure_capacity(str, max_size);

	// Copy CFString in requested encoding to dstr buffer
	Boolean success = CFStringGetCString(cfstring, str->array, max_size,
					     cfstring_encoding);

	if (success)
		dstr_resize(str, max_size);

	return (bool)success;
}
