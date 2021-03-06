#pragma once

#include <winternl.h>

#define THREAD_STATE_WAITING 5
#define THREAD_WAIT_REASON_SUSPENDED 5

typedef struct _OBS_SYSTEM_PROCESS_INFORMATION2 {
	ULONG NextEntryOffset;
	ULONG ThreadCount;
	BYTE Reserved1[48];
	PVOID Reserved2[3];
	HANDLE UniqueProcessId;
	PVOID Reserved3;
	ULONG HandleCount;
	BYTE Reserved4[4];
	PVOID Reserved5[11];
	SIZE_T PeakPagefileUsage;
	SIZE_T PrivatePageCount;
	LARGE_INTEGER Reserved6[6];
} OBS_SYSTEM_PROCESS_INFORMATION2;

typedef struct _OBS_SYSTEM_THREAD_INFORMATION {
	FILETIME KernelTime;
	FILETIME UserTime;
	FILETIME CreateTime;
	DWORD WaitTime;
	PVOID Address;
	HANDLE UniqueProcessId;
	HANDLE UniqueThreadId;
	DWORD Priority;
	DWORD BasePriority;
	DWORD ContextSwitches;
	DWORD ThreadState;
	DWORD WaitReason;
	DWORD Reserved1;
} OBS_SYSTEM_THREAD_INFORMATION;

#ifndef NT_SUCCESS
#define NT_SUCCESS(status) ((NTSTATUS)(status) >= 0)
#endif

#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)

#define init_named_attribs(o, name)                   \
	do {                                          \
		(o)->Length = sizeof(*(o));           \
		(o)->ObjectName = name;               \
		(o)->RootDirectory = NULL;            \
		(o)->Attributes = 0;                  \
		(o)->SecurityDescriptor = NULL;       \
		(o)->SecurityQualityOfService = NULL; \
	} while (false)

typedef void(WINAPI *RTLINITUNICODESTRINGFUNC)(PCUNICODE_STRING pstr,
					       const wchar_t *lpstrName);
typedef NTSTATUS(WINAPI *NTOPENFUNC)(PHANDLE phandle, ACCESS_MASK access,
				     POBJECT_ATTRIBUTES objattr);
typedef NTSTATUS(WINAPI *NTCREATEMUTANT)(PHANDLE phandle, ACCESS_MASK access,
					 POBJECT_ATTRIBUTES objattr,
					 BOOLEAN isowner);
typedef ULONG(WINAPI *RTLNTSTATUSTODOSERRORFUNC)(NTSTATUS status);
typedef NTSTATUS(WINAPI *NTQUERYSYSTEMINFORMATIONFUNC)(SYSTEM_INFORMATION_CLASS,
						       PVOID, ULONG, PULONG);

static FARPROC get_nt_func(const char *name)
{
	static bool initialized = false;
	static HANDLE ntdll = NULL;
	if (!initialized) {
		ntdll = GetModuleHandleW(L"ntdll");
		initialized = true;
	}

	return GetProcAddress(ntdll, name);
}

static void nt_set_last_error(NTSTATUS status)
{
	static bool initialized = false;
	static RTLNTSTATUSTODOSERRORFUNC func = NULL;

	if (!initialized) {
		func = (RTLNTSTATUSTODOSERRORFUNC)get_nt_func(
			"RtlNtStatusToDosError");
		initialized = true;
	}

	if (func)
		SetLastError(func(status));
}

static void rtl_init_str(UNICODE_STRING *unistr, const wchar_t *str)
{
	static bool initialized = false;
	static RTLINITUNICODESTRINGFUNC func = NULL;

	if (!initialized) {
		func = (RTLINITUNICODESTRINGFUNC)get_nt_func(
			"RtlInitUnicodeString");
		initialized = true;
	}

	if (func)
		func(unistr, str);
}

static NTSTATUS nt_query_information(SYSTEM_INFORMATION_CLASS info_class,
				     PVOID info, ULONG info_len, PULONG ret_len)
{
	static bool initialized = false;
	static NTQUERYSYSTEMINFORMATIONFUNC func = NULL;

	if (!initialized) {
		func = (NTQUERYSYSTEMINFORMATIONFUNC)get_nt_func(
			"NtQuerySystemInformation");
		initialized = true;
	}

	if (func)
		return func(info_class, info, info_len, ret_len);
	return (NTSTATUS)-1;
}

static bool thread_is_suspended(DWORD process_id, DWORD thread_id)
{
	ULONG size = 4096;
	bool suspended = false;
	void *data = malloc(size);

	for (;;) {
		NTSTATUS stat = nt_query_information(SystemProcessInformation,
						     data, size, &size);
		if (NT_SUCCESS(stat))
			break;

		if (stat != STATUS_INFO_LENGTH_MISMATCH) {
			goto fail;
		}

		free(data);
		size += 1024;
		data = malloc(size);
	}

	OBS_SYSTEM_PROCESS_INFORMATION2 *spi = data;

	for (;;) {
		if (spi->UniqueProcessId == (HANDLE)(DWORD_PTR)process_id) {
			break;
		}

		ULONG offset = spi->NextEntryOffset;
		if (!offset)
			goto fail;

		spi = (OBS_SYSTEM_PROCESS_INFORMATION2 *)((BYTE *)spi + offset);
	}

	OBS_SYSTEM_THREAD_INFORMATION *sti;
	OBS_SYSTEM_THREAD_INFORMATION *info = NULL;
	sti = (OBS_SYSTEM_THREAD_INFORMATION *)((BYTE *)spi + sizeof(*spi));

	for (ULONG i = 0; i < spi->ThreadCount; i++) {
		if (sti[i].UniqueThreadId == (HANDLE)(DWORD_PTR)thread_id) {
			info = &sti[i];
			break;
		}
	}

	if (info) {
		suspended = info->ThreadState == THREAD_STATE_WAITING &&
			    info->WaitReason == THREAD_WAIT_REASON_SUSPENDED;
	}

fail:
	free(data);
	return suspended;
}

#define MAKE_NT_OPEN_FUNC(func_name, nt_name, access)             \
	static HANDLE func_name(const wchar_t *name)              \
	{                                                         \
		static bool initialized = false;                  \
		static NTOPENFUNC open = NULL;                    \
		HANDLE handle;                                    \
		NTSTATUS status;                                  \
		UNICODE_STRING unistr;                            \
		OBJECT_ATTRIBUTES attr;                           \
                                                                  \
		if (!initialized) {                               \
			open = (NTOPENFUNC)get_nt_func(#nt_name); \
			initialized = true;                       \
		}                                                 \
                                                                  \
		if (!open)                                        \
			return NULL;                              \
                                                                  \
		rtl_init_str(&unistr, name);                      \
		init_named_attribs(&attr, &unistr);               \
                                                                  \
		status = open(&handle, access, &attr);            \
		if (NT_SUCCESS(status))                           \
			return handle;                            \
		nt_set_last_error(status);                        \
		return NULL;                                      \
	}

MAKE_NT_OPEN_FUNC(nt_open_mutex, NtOpenMutant, SYNCHRONIZE)
MAKE_NT_OPEN_FUNC(nt_open_event, NtOpenEvent, EVENT_MODIFY_STATE | SYNCHRONIZE)
MAKE_NT_OPEN_FUNC(nt_open_map, NtOpenSection, FILE_MAP_READ | FILE_MAP_WRITE)

static HANDLE nt_create_mutex(const wchar_t *name)
{
	static bool initialized = false;
	static NTCREATEMUTANT create = NULL;
	HANDLE handle;
	NTSTATUS status;
	UNICODE_STRING unistr;
	OBJECT_ATTRIBUTES attr;

	if (!initialized) {
		create = (NTCREATEMUTANT)get_nt_func("NtCreateMutant");
		initialized = true;
	}

	if (!create)
		return NULL;

	rtl_init_str(&unistr, name);
	init_named_attribs(&attr, &unistr);

	status = create(&handle, SYNCHRONIZE, &attr, FALSE);
	if (NT_SUCCESS(status))
		return handle;
	nt_set_last_error(status);
	return NULL;
}
