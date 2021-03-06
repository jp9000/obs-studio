#include <util/text-lookup.h>
#include <util/threading.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <obs-module.h>
#include <file-updater/file-updater.h>

#include "rtmp-format-ver.h"
#include "lookup-config.h"
#include "showroom.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("rtmp-services", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "OBS core RTMP services";
}

#define RTMP_SERVICES_LOG_STR "[rtmp-services plugin] "
#define RTMP_SERVICES_VER_STR "rtmp-services plugin (libobs " OBS_VERSION ")"

extern struct obs_service_info rtmp_common_service;
extern struct obs_service_info rtmp_custom_service;

static update_info_t *update_info = NULL;
static struct dstr module_name = {0};

const char *get_module_name(void)
{
	return module_name.array;
}

static bool confirm_service_file(void *param, struct file_download_data *file)
{
	if (astrcmpi(file->name, "services.json") == 0) {
		obs_data_t *data;
		int format_version;

		data = obs_data_create_from_json((char *)file->buffer.array);
		if (!data)
			return false;

		format_version = (int)obs_data_get_int(data, "format_version");
		obs_data_release(data);

		if (format_version != RTMP_SERVICES_FORMAT_VERSION)
			return false;
	}

	UNUSED_PARAMETER(param);
	return true;
}

extern void init_twitch_data(void);
extern void load_twitch_data(void);
extern void unload_twitch_data(void);
extern void twitch_ingests_refresh(int seconds);

static void refresh_callback(void *unused, calldata_t *cd)
{
	int seconds = (int)calldata_int(cd, "seconds");
	if (seconds <= 0)
		seconds = 3;
	if (seconds > 10)
		seconds = 10;

	twitch_ingests_refresh(seconds);

	UNUSED_PARAMETER(unused);
}

bool obs_module_load(void)
{
	init_twitch_data();

	dstr_copy(&module_name, "rtmp-services plugin (libobs ");
	dstr_cat(&module_name, obs_get_version_string());
	dstr_cat(&module_name, ")");

	proc_handler_t *ph = obs_get_proc_handler();
	proc_handler_add(ph, "void twitch_ingests_refresh(int seconds)",
			 refresh_callback, NULL);

#if !defined(_WIN32) || CHECK_FOR_SERVICE_UPDATES
	char *local_dir = obs_module_file("");
	char *cache_dir = obs_module_config_path("");

	if (cache_dir) {
		update_info = update_info_create(RTMP_SERVICES_LOG_STR,
						 module_name.array,
						 RTMP_SERVICES_URL, local_dir,
						 cache_dir,
						 confirm_service_file, NULL);
	}

	load_twitch_data();

	bfree(local_dir);
	bfree(cache_dir);
#endif

	obs_register_service(&rtmp_common_service);
	obs_register_service(&rtmp_custom_service);
	return true;
}

void obs_module_unload(void)
{
	update_info_destroy(update_info);
	unload_twitch_data();
	free_showroom_data();
	dstr_free(&module_name);
}
