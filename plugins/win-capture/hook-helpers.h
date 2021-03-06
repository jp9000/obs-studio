#pragma once

#if !defined(__cplusplus) && !defined(inline)
#define inline __inline
#endif

#define GC_EVENT_FLAGS (EVENT_MODIFY_STATE | SYNCHRONIZE)
#define GC_MUTEX_FLAGS (SYNCHRONIZE)

static inline HANDLE create_event(const wchar_t *name)
{
	return CreateEventW(NULL, false, false, name);
}

static inline HANDLE open_event(const wchar_t *name)
{
	return OpenEventW(GC_EVENT_FLAGS, false, name);
}

static inline HANDLE create_mutex(const wchar_t *name)
{
	return CreateMutexW(NULL, false, name);
}

static inline HANDLE open_mutex(const wchar_t *name)
{
	return OpenMutexW(GC_MUTEX_FLAGS, false, name);
}

static inline HANDLE create_event_plus_id(const wchar_t *name, DWORD id)
{
	wchar_t new_name[64];
	_snwprintf(new_name, 64, L"%s%lu", name, id);
	return create_event(new_name);
}

static inline HANDLE create_mutex_plus_id(const wchar_t *name, DWORD id)
{
	wchar_t new_name[64];
	_snwprintf(new_name, 64, L"%s%lu", name, id);
	return create_mutex(new_name);
}

static inline bool object_signalled(HANDLE event)
{
	if (!event)
		return false;

	return WaitForSingleObject(event, 0) == WAIT_OBJECT_0;
}
