#include <obs-module.h>

#include "service-manager.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-services", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "OBS Core Stream Services";
}

bool obs_module_load()
{
	service_manager::initialize();
	return true;
}

void obs_module_unload()
{
	service_manager::finalize();
}
