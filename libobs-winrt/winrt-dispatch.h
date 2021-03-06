#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <obs-module.h>

#ifdef __cplusplus
extern "C" {
#endif

EXPORT void winrt_initialize();
EXPORT void winrt_uninitialize();
EXPORT struct winrt_disaptcher *winrt_dispatcher_init();
EXPORT void winrt_dispatcher_free(struct winrt_disaptcher *dispatcher);

#ifdef __cplusplus
}
#endif
