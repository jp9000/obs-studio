/*
 * Copyright (c) 2014 Hugh Bailey <obs.jim@gmail.com>
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

#include "bmem.h"
#include "threading.h"
#include "util/platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __MINGW32__
#include <excpt.h>
#ifndef TRYLEVEL_NONE
#ifndef __MINGW64__
#define NO_SEH_MINGW
#endif
#ifndef __try
#define __try
#endif
#ifndef __except
#define __except (x) if (0)
#endif
#endif
#endif

int os_event_init(os_event_t **event, enum os_event_type type)
{
	HANDLE handle;

	handle = CreateEvent(NULL, (type == OS_EVENT_TYPE_MANUAL), FALSE, NULL);
	if (!handle)
		return -1;

	*event = (os_event_t *)handle;
	return 0;
}

void os_event_destroy(os_event_t *event)
{
	if (event)
		CloseHandle((HANDLE)event);
}

int os_event_wait(os_event_t *event)
{
	DWORD code;

	if (!event)
		return EINVAL;

	code = WaitForSingleObject((HANDLE)event, INFINITE);
	if (code != WAIT_OBJECT_0)
		return EINVAL;

	return 0;
}

int os_event_timedwait(os_event_t *event, unsigned long milliseconds)
{
	DWORD code;

	if (!event)
		return EINVAL;

	code = WaitForSingleObject((HANDLE)event, milliseconds);
	if (code == WAIT_TIMEOUT)
		return ETIMEDOUT;
	else if (code != WAIT_OBJECT_0)
		return EINVAL;

	return 0;
}

int os_event_try(os_event_t *event)
{
	DWORD code;

	if (!event)
		return EINVAL;

	code = WaitForSingleObject((HANDLE)event, 0);
	if (code == WAIT_TIMEOUT)
		return EAGAIN;
	else if (code != WAIT_OBJECT_0)
		return EINVAL;

	return 0;
}

int os_event_signal(os_event_t *event)
{
	if (!event)
		return EINVAL;

	if (!SetEvent((HANDLE)event))
		return EINVAL;

	return 0;
}

void os_event_reset(os_event_t *event)
{
	if (!event)
		return;

	ResetEvent((HANDLE)event);
}

int os_sem_init(os_sem_t **sem, int value)
{
	HANDLE handle = CreateSemaphore(NULL, (LONG)value, 0x7FFFFFFF, NULL);
	if (!handle)
		return -1;

	*sem = (os_sem_t *)handle;
	return 0;
}

void os_sem_destroy(os_sem_t *sem)
{
	if (sem)
		CloseHandle((HANDLE)sem);
}

int os_sem_post(os_sem_t *sem)
{
	if (!sem)
		return -1;
	return ReleaseSemaphore((HANDLE)sem, 1, NULL) ? 0 : -1;
}

int os_sem_wait(os_sem_t *sem)
{
	DWORD ret;

	if (!sem)
		return -1;
	ret = WaitForSingleObject((HANDLE)sem, INFINITE);
	return (ret == WAIT_OBJECT_0) ? 0 : -1;
}

#define VC_EXCEPTION 0x406D1388

#pragma pack(push, 8)
struct vs_threadname_info {
	DWORD type; /* 0x1000 */
	const char *name;
	DWORD thread_id;
	DWORD flags;
};
#pragma pack(pop)

#define THREADNAME_INFO_SIZE \
	(sizeof(struct vs_threadname_info) / sizeof(ULONG_PTR))

void os_set_thread_name(const char *name)
{
#ifdef __MINGW32__
	UNUSED_PARAMETER(name);
#else
	struct vs_threadname_info info;
	info.type = 0x1000;
	info.name = name;
	info.thread_id = GetCurrentThreadId();
	info.flags = 0;

#ifdef NO_SEH_MINGW
	__try1(EXCEPTION_EXECUTE_HANDLER)
	{
#else
	__try {
#endif
		RaiseException(VC_EXCEPTION, 0, THREADNAME_INFO_SIZE,
			       (ULONG_PTR *)&info);
#ifdef NO_SEH_MINGW
	}
	__except1{
#else
	} __except (EXCEPTION_EXECUTE_HANDLER) {
#endif
	}
#endif

	typedef HRESULT(WINAPI * set_thread_description_t)(HANDLE thread,
							   PCWSTR desc);

	HMODULE k32 = LoadLibraryW(L"Kernel32.dll");
	set_thread_description_t std = NULL;
	std = (set_thread_description_t)GetProcAddress(k32,
						       "SetThreadDescription");
	if (std) {
		wchar_t *wname;
		os_utf8_to_wcs_ptr(name, 0, &wname);

		std(GetCurrentThread(), wname);

		bfree(wname);
	}
	FreeLibrary(k32);
}
