#include <obs-module.h>

#ifdef ENABLE_WINMF
#include <util/profiler.h>
#include "mf-common.hpp"

extern "C" extern void RegisterMFAACEncoder();
extern void RegisterMFH264Encoders();
#endif


extern "C" bool obs_module_load(void)
{
#ifdef ENABLE_WINMF
	MFStartup(MF_VERSION, MFSTARTUP_FULL);

	RegisterMFAACEncoder();
	RegisterMFH264Encoders();
#endif

	return true;
}

extern "C" void obs_module_unload(void)
{
#ifdef ENABLE_WINMF
	MFShutdown();
#endif
}

OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Windows Media Foundations H.264/AAC encoder";
}

#ifdef ENABLE_WINMF
OBS_MODULE_USE_DEFAULT_LOCALE("win-mf", "en-US")
#endif
