#include <obs-module.h>

#include "obs-outputs-config.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <mbedtls/threading.h>
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-outputs", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "OBS core RTMP/FLV/null/FTL outputs";
}

extern struct obs_output_info rtmp_output_info;
extern struct obs_output_info null_output_info;
extern struct obs_output_info flv_output_info;
#if COMPILE_FTL
extern struct obs_output_info ftl_output_info;
#endif

#if defined(_WIN32) && defined(MBEDTLS_THREADING_ALT)
void mbed_mutex_init(mbedtls_threading_mutex_t *m)
{
	CRITICAL_SECTION *c = bzalloc(sizeof(CRITICAL_SECTION));
	*m = c;
	InitializeCriticalSection(c);
}

void mbed_mutex_free(mbedtls_threading_mutex_t *m)
{
	CRITICAL_SECTION *c = *m;
	DeleteCriticalSection(c);
	bfree(*m);
	*m = NULL;
}

int mbed_mutex_lock(mbedtls_threading_mutex_t *m)
{
	CRITICAL_SECTION *c = *m;
	EnterCriticalSection(c);
	return 0;
}

int mbed_mutex_unlock(mbedtls_threading_mutex_t *m)
{
	CRITICAL_SECTION *c = *m;
	LeaveCriticalSection(c);
	return 0;
}
#endif

bool obs_module_load(void)
{
#ifdef _WIN32
	WSADATA wsad;
	WSAStartup(MAKEWORD(2, 2), &wsad);
#endif

#if defined(_WIN32) && defined(MBEDTLS_THREADING_ALT)
	mbedtls_threading_set_alt(mbed_mutex_init, mbed_mutex_free,
				  mbed_mutex_lock, mbed_mutex_unlock);
#endif

	obs_register_output(&rtmp_output_info);
	obs_register_output(&null_output_info);
	obs_register_output(&flv_output_info);
#if COMPILE_FTL
	obs_register_output(&ftl_output_info);
#endif
	return true;
}

void obs_module_unload(void)
{
#ifdef _WIN32
#ifdef MBEDTLS_THREADING_ALT
	mbedtls_threading_free_alt();
#endif
	WSACleanup();
#endif
}
