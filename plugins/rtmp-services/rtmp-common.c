#include <util/platform.h>
#include <util/dstr.h>
#include <obs-module.h>
#include <jansson.h>
#include <obs-config.h>

#include "rtmp-format-ver.h"
#include "twitch.h"
#include "younow.h"
#include "nimotv.h"
#include "showroom.h"

struct rtmp_common {
	char *service;
	char *server;
	char *key;

	char *output;
	struct obs_service_resolution *supported_resolutions;
	size_t supported_resolutions_count;
	int max_fps;

	bool supports_additional_audio_track;
};

static const char *rtmp_common_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("StreamingServices");
}

static json_t *open_services_file(void);
static inline json_t *find_service(json_t *root, const char *name,
				   const char **p_new_name);
static inline bool get_bool_val(json_t *service, const char *key);
static inline const char *get_string_val(json_t *service, const char *key);
static inline int get_int_val(json_t *service, const char *key);

extern void twitch_ingests_refresh(int seconds);

static void ensure_valid_url(struct rtmp_common *service, json_t *json,
			     obs_data_t *settings)
{
	json_t *servers = json_object_get(json, "servers");
	const char *top_url = NULL;
	json_t *server;
	size_t index;

	if (!service->server || !servers || !json_is_array(servers))
		return;
	if (astrstri(service->service, "Facebook") == NULL)
		return;

	json_array_foreach (servers, index, server) {
		const char *url = get_string_val(server, "url");
		if (!url)
			continue;

		if (!top_url)
			top_url = url;

		if (astrcmpi(service->server, url) == 0)
			return;
	}

	/* server was not found in server list, use first server instead */
	if (top_url) {
		bfree(service->server);
		service->server = bstrdup(top_url);
		obs_data_set_string(settings, "server", top_url);
	}
}

static void update_recommendations(struct rtmp_common *service, json_t *rec)
{
	const char *out = get_string_val(rec, "output");
	if (out)
		service->output = bstrdup(out);

	json_t *sr = json_object_get(rec, "supported resolutions");
	if (sr && json_is_array(sr)) {
		DARRAY(struct obs_service_resolution) res_list;
		json_t *res_obj;
		size_t index;

		da_init(res_list);

		json_array_foreach (sr, index, res_obj) {
			if (!json_is_string(res_obj))
				continue;

			const char *res_str = json_string_value(res_obj);
			struct obs_service_resolution res;
			if (sscanf(res_str, "%dx%d", &res.cx, &res.cy) != 2)
				continue;
			if (res.cx <= 0 || res.cy <= 0)
				continue;

			da_push_back(res_list, &res);
		}

		if (res_list.num) {
			service->supported_resolutions = res_list.array;
			service->supported_resolutions_count = res_list.num;
		}
	}

	service->max_fps = get_int_val(rec, "max fps");
}

static void rtmp_common_update(void *data, obs_data_t *settings)
{
	struct rtmp_common *service = data;

	bfree(service->service);
	bfree(service->server);
	bfree(service->output);
	bfree(service->key);
	bfree(service->supported_resolutions);

	service->service = bstrdup(obs_data_get_string(settings, "service"));
	service->server = bstrdup(obs_data_get_string(settings, "server"));
	service->key = bstrdup(obs_data_get_string(settings, "key"));
	service->supports_additional_audio_track = false;
	service->output = NULL;
	service->supported_resolutions = NULL;
	service->supported_resolutions_count = 0;
	service->max_fps = 0;

	json_t *root = open_services_file();
	if (root) {
		const char *new_name;
		json_t *serv = find_service(root, service->service, &new_name);

		if (new_name) {
			bfree(service->service);
			service->service = bstrdup(new_name);
		}

		if (serv) {
			json_t *rec = json_object_get(serv, "recommended");
			if (json_is_object(rec)) {
				update_recommendations(service, rec);
			}

			service->supports_additional_audio_track = get_bool_val(
				serv, "supports_additional_audio_track");
			ensure_valid_url(service, serv, settings);
		}
	}
	json_decref(root);

	if (!service->output)
		service->output = bstrdup("rtmp_output");
}

static void rtmp_common_destroy(void *data)
{
	struct rtmp_common *service = data;

	bfree(service->supported_resolutions);
	bfree(service->service);
	bfree(service->server);
	bfree(service->output);
	bfree(service->key);
	bfree(service);
}

static void *rtmp_common_create(obs_data_t *settings, obs_service_t *service)
{
	struct rtmp_common *data = bzalloc(sizeof(struct rtmp_common));
	rtmp_common_update(data, settings);

	UNUSED_PARAMETER(service);
	return data;
}

static inline const char *get_string_val(json_t *service, const char *key)
{
	json_t *str_val = json_object_get(service, key);
	if (!str_val || !json_is_string(str_val))
		return NULL;

	return json_string_value(str_val);
}

static inline int get_int_val(json_t *service, const char *key)
{
	json_t *integer_val = json_object_get(service, key);
	if (!integer_val || !json_is_integer(integer_val))
		return 0;

	return (int)json_integer_value(integer_val);
}

static inline bool get_bool_val(json_t *service, const char *key)
{
	json_t *bool_val = json_object_get(service, key);
	if (!bool_val || !json_is_boolean(bool_val))
		return false;

	return json_is_true(bool_val);
}

static void add_service(obs_property_t *list, json_t *service, bool show_all,
			const char *cur_service)
{
	json_t *servers;
	const char *name;
	bool common;

	if (!json_is_object(service)) {
		blog(LOG_WARNING, "rtmp-common.c: [add_service] service "
				  "is not an object");
		return;
	}

	name = get_string_val(service, "name");
	if (!name) {
		blog(LOG_WARNING, "rtmp-common.c: [add_service] service "
				  "has no name");
		return;
	}

	common = get_bool_val(service, "common");
	if (!show_all && !common && strcmp(cur_service, name) != 0) {
		return;
	}

	servers = json_object_get(service, "servers");
	if (!servers || !json_is_array(servers)) {
		blog(LOG_WARNING,
		     "rtmp-common.c: [add_service] service "
		     "'%s' has no servers",
		     name);
		return;
	}

	obs_property_list_add_string(list, name, name);
}

static void add_services(obs_property_t *list, json_t *root, bool show_all,
			 const char *cur_service)
{
	json_t *service;
	size_t index;

	if (!json_is_array(root)) {
		blog(LOG_WARNING, "rtmp-common.c: [add_services] JSON file "
				  "root is not an array");
		return;
	}

	json_array_foreach (root, index, service) {
		add_service(list, service, show_all, cur_service);
	}

	service = find_service(root, cur_service, NULL);
	if (!service && cur_service && *cur_service) {
		obs_property_list_insert_string(list, 0, cur_service,
						cur_service);
		obs_property_list_item_disable(list, 0, true);
	}
}

static json_t *open_json_file(const char *file)
{
	char *file_data = os_quick_read_utf8_file(file);
	json_error_t error;
	json_t *root;
	json_t *list;
	int format_ver;

	if (!file_data)
		return NULL;

	root = json_loads(file_data, JSON_REJECT_DUPLICATES, &error);
	bfree(file_data);

	if (!root) {
		blog(LOG_WARNING,
		     "rtmp-common.c: [open_json_file] "
		     "Error reading JSON file (%d): %s",
		     error.line, error.text);
		return NULL;
	}

	format_ver = get_int_val(root, "format_version");

	if (format_ver != RTMP_SERVICES_FORMAT_VERSION) {
		blog(LOG_DEBUG,
		     "rtmp-common.c: [open_json_file] "
		     "Wrong format version (%d), expected %d",
		     format_ver, RTMP_SERVICES_FORMAT_VERSION);
		json_decref(root);
		return NULL;
	}

	list = json_object_get(root, "services");
	if (list)
		json_incref(list);
	json_decref(root);

	if (!list) {
		blog(LOG_WARNING, "rtmp-common.c: [open_json_file] "
				  "No services list");
		return NULL;
	}

	return list;
}

static json_t *open_services_file(void)
{
	char *file;
	json_t *root = NULL;

	file = obs_module_config_path("services.json");
	if (file) {
		root = open_json_file(file);
		bfree(file);
	}

	if (!root) {
		file = obs_module_file("services.json");
		if (file) {
			root = open_json_file(file);
			bfree(file);
		}
	}

	return root;
}

static void build_service_list(obs_property_t *list, json_t *root,
			       bool show_all, const char *cur_service)
{
	obs_property_list_clear(list);
	add_services(list, root, show_all, cur_service);
}

static void properties_data_destroy(void *data)
{
	json_t *root = data;
	if (root)
		json_decref(root);
}

static bool fill_twitch_servers_locked(obs_property_t *servers_prop)
{
	size_t count = twitch_ingest_count();

	obs_property_list_add_string(servers_prop,
				     obs_module_text("Server.Auto"), "auto");

	if (count <= 1)
		return false;

	for (size_t i = 0; i < count; i++) {
		struct twitch_ingest ing = twitch_ingest(i);
		obs_property_list_add_string(servers_prop, ing.name, ing.url);
	}

	return true;
}

static inline bool fill_twitch_servers(obs_property_t *servers_prop)
{
	bool success;

	twitch_ingests_lock();
	success = fill_twitch_servers_locked(servers_prop);
	twitch_ingests_unlock();

	return success;
}

static void fill_servers(obs_property_t *servers_prop, json_t *service,
			 const char *name)
{
	json_t *servers, *server;
	size_t index;

	obs_property_list_clear(servers_prop);

	servers = json_object_get(service, "servers");

	if (!json_is_array(servers)) {
		blog(LOG_WARNING,
		     "rtmp-common.c: [fill_servers] "
		     "Servers for service '%s' not a valid object",
		     name);
		return;
	}

	if (strcmp(name, "Twitch") == 0) {
		if (fill_twitch_servers(servers_prop))
			return;
	}

	if (strcmp(name, "Nimo TV") == 0) {
		obs_property_list_add_string(
			servers_prop, obs_module_text("Server.Auto"), "auto");
	}

	json_array_foreach (servers, index, server) {
		const char *server_name = get_string_val(server, "name");
		const char *url = get_string_val(server, "url");

		if (!server_name || !url)
			continue;

		obs_property_list_add_string(servers_prop, server_name, url);
	}
}

static void fill_more_info_link(json_t *service, obs_data_t *settings)
{
	const char *more_info_link;

	more_info_link = get_string_val(service, "more_info_link");
	if (more_info_link)
		obs_data_set_string(settings, "more_info_link", more_info_link);
}

static inline json_t *find_service(json_t *root, const char *name,
				   const char **p_new_name)
{
	size_t index;
	json_t *service;

	if (p_new_name)
		*p_new_name = NULL;

	json_array_foreach (root, index, service) {
		const char *cur_name = get_string_val(service, "name");

		if (strcmp(name, cur_name) == 0)
			return service;

		/* check for alternate names */
		json_t *alt_names = json_object_get(service, "alt_names");
		size_t alt_name_idx;
		json_t *alt_name_obj;

		json_array_foreach (alt_names, alt_name_idx, alt_name_obj) {
			const char *alt_name = json_string_value(alt_name_obj);
			if (alt_name && strcmp(name, alt_name) == 0) {
				if (p_new_name)
					*p_new_name = cur_name;
				return service;
			}
		}
	}

	return NULL;
}

static bool service_selected(obs_properties_t *props, obs_property_t *p,
			     obs_data_t *settings)
{
	const char *name = obs_data_get_string(settings, "service");
	json_t *root = obs_properties_get_param(props);
	json_t *service;
	const char *new_name;

	if (!name || !*name)
		return false;

	service = find_service(root, name, &new_name);
	if (!service) {
		const char *server = obs_data_get_string(settings, "server");

		obs_property_list_insert_string(p, 0, name, name);
		obs_property_list_item_disable(p, 0, true);

		p = obs_properties_get(props, "server");
		obs_property_list_insert_string(p, 0, server, server);
		obs_property_list_item_disable(p, 0, true);
		return true;
	}
	if (new_name) {
		name = new_name;
		obs_data_set_string(settings, "service", name);
	}

	fill_servers(obs_properties_get(props, "server"), service, name);
	fill_more_info_link(service, settings);
	return true;
}

static bool show_all_services_toggled(obs_properties_t *ppts, obs_property_t *p,
				      obs_data_t *settings)
{
	const char *cur_service = obs_data_get_string(settings, "service");
	bool show_all = obs_data_get_bool(settings, "show_all");

	json_t *root = obs_properties_get_param(ppts);
	if (!root)
		return false;

	build_service_list(obs_properties_get(ppts, "service"), root, show_all,
			   cur_service);

	UNUSED_PARAMETER(p);
	return true;
}

static obs_properties_t *rtmp_common_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *ppts = obs_properties_create();
	obs_property_t *p;
	json_t *root;

	root = open_services_file();
	if (root)
		obs_properties_set_param(ppts, root, properties_data_destroy);

	p = obs_properties_add_list(ppts, "service", obs_module_text("Service"),
				    OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);

	obs_property_set_modified_callback(p, service_selected);

	p = obs_properties_add_bool(ppts, "show_all",
				    obs_module_text("ShowAll"));

	obs_property_set_modified_callback(p, show_all_services_toggled);

	obs_properties_add_list(ppts, "server", obs_module_text("Server"),
				OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	obs_properties_add_text(ppts, "key", obs_module_text("StreamKey"),
				OBS_TEXT_PASSWORD);
	return ppts;
}

static void apply_video_encoder_settings(obs_data_t *settings,
					 json_t *recommended)
{
	json_t *item = json_object_get(recommended, "keyint");
	if (json_is_integer(item)) {
		int keyint = (int)json_integer_value(item);
		obs_data_set_int(settings, "keyint_sec", keyint);
	}

	obs_data_set_string(settings, "rate_control", "CBR");

	item = json_object_get(recommended, "profile");
	obs_data_item_t *enc_item = obs_data_item_byname(settings, "profile");
	if (json_is_string(item) &&
	    obs_data_item_gettype(enc_item) == OBS_DATA_STRING) {
		const char *profile = json_string_value(item);
		obs_data_set_string(settings, "profile", profile);
	}

	obs_data_item_release(&enc_item);

	item = json_object_get(recommended, "max video bitrate");
	if (json_is_integer(item)) {
		int max_bitrate = (int)json_integer_value(item);
		if (obs_data_get_int(settings, "bitrate") > max_bitrate) {
			obs_data_set_int(settings, "bitrate", max_bitrate);
			obs_data_set_int(settings, "buffer_size", max_bitrate);
		}
	}

	item = json_object_get(recommended, "bframes");
	if (json_is_integer(item)) {
		int bframes = (int)json_integer_value(item);
		obs_data_set_int(settings, "bf", bframes);
	}

	item = json_object_get(recommended, "x264opts");
	if (json_is_string(item)) {
		const char *x264_settings = json_string_value(item);
		const char *cur_settings =
			obs_data_get_string(settings, "x264opts");
		struct dstr opts;

		dstr_init_copy(&opts, cur_settings);
		if (!dstr_is_empty(&opts))
			dstr_cat(&opts, " ");
		dstr_cat(&opts, x264_settings);

		obs_data_set_string(settings, "x264opts", opts.array);
		dstr_free(&opts);
	}
}

static void apply_audio_encoder_settings(obs_data_t *settings,
					 json_t *recommended)
{
	json_t *item = json_object_get(recommended, "max audio bitrate");
	if (json_is_integer(item)) {
		int max_bitrate = (int)json_integer_value(item);
		if (obs_data_get_int(settings, "bitrate") > max_bitrate)
			obs_data_set_int(settings, "bitrate", max_bitrate);
	}
}

static void initialize_output(struct rtmp_common *service, json_t *root,
			      obs_data_t *video_settings,
			      obs_data_t *audio_settings)
{
	json_t *json_service = find_service(root, service->service, NULL);
	json_t *recommended;

	if (!json_service) {
		if (service->service && *service->service)
			blog(LOG_WARNING,
			     "rtmp-common.c: [initialize_output] "
			     "Could not find service '%s'",
			     service->service);
		return;
	}

	recommended = json_object_get(json_service, "recommended");
	if (!recommended)
		return;

	if (video_settings)
		apply_video_encoder_settings(video_settings, recommended);
	if (audio_settings)
		apply_audio_encoder_settings(audio_settings, recommended);
}

static void rtmp_common_apply_settings(void *data, obs_data_t *video_settings,
				       obs_data_t *audio_settings)
{
	struct rtmp_common *service = data;
	json_t *root = open_services_file();

	if (root) {
		initialize_output(service, root, video_settings,
				  audio_settings);
		json_decref(root);
	}
}

static const char *rtmp_common_get_output_type(void *data)
{
	struct rtmp_common *service = data;
	return service->output;
}

static const char *rtmp_common_url(void *data)
{
	struct rtmp_common *service = data;

	if (service->service && strcmp(service->service, "Twitch") == 0) {
		if (service->server && strcmp(service->server, "auto") == 0) {
			struct twitch_ingest ing;

			twitch_ingests_refresh(3);

			twitch_ingests_lock();
			ing = twitch_ingest(0);
			twitch_ingests_unlock();

			return ing.url;
		}
	}

	if (service->service && strcmp(service->service, "YouNow") == 0) {
		if (service->server && service->key) {
			return younow_get_ingest(service->server, service->key);
		}
	}

	if (service->service && strcmp(service->service, "Nimo TV") == 0) {
		if (service->server && strcmp(service->server, "auto") == 0) {
			return nimotv_get_ingest(service->key);
		}
	}

	if (service->service && strcmp(service->service, "SHOWROOM") == 0) {
		if (service->server && service->key) {
			struct showroom_ingest *ingest;
			ingest = showroom_get_ingest(service->server,
						     service->key);
			return ingest->url;
		}
	}
	return service->server;
}

static const char *rtmp_common_key(void *data)
{
	struct rtmp_common *service = data;
	if (service->service && strcmp(service->service, "SHOWROOM") == 0) {
		if (service->server && service->key) {
			struct showroom_ingest *ingest;
			ingest = showroom_get_ingest(service->server,
						     service->key);
			return ingest->key;
		}
	}
	return service->key;
}

static bool supports_multitrack(void *data)
{
	struct rtmp_common *service = data;
	return service->supports_additional_audio_track;
}

static void rtmp_common_get_supported_resolutions(
	void *data, struct obs_service_resolution **resolutions, size_t *count)
{
	struct rtmp_common *service = data;
	*count = service->supported_resolutions_count;
	*resolutions = bmemdup(service->supported_resolutions,
			       *count * sizeof(struct obs_service_resolution));
}

static void rtmp_common_get_max_fps(void *data, int *fps)
{
	struct rtmp_common *service = data;
	*fps = service->max_fps;
}

static void rtmp_common_get_max_bitrate(void *data, int *video_bitrate,
					int *audio_bitrate)
{
	struct rtmp_common *service = data;
	json_t *root = open_services_file();
	json_t *item;

	if (!root)
		return;

	json_t *json_service = find_service(root, service->service, NULL);
	if (!json_service) {
		goto fail;
	}

	json_t *recommended = json_object_get(json_service, "recommended");
	if (!recommended) {
		goto fail;
	}

	if (audio_bitrate) {
		item = json_object_get(recommended, "max audio bitrate");
		if (json_is_integer(item))
			*audio_bitrate = (int)json_integer_value(item);
	}

	if (video_bitrate) {
		item = json_object_get(recommended, "max video bitrate");
		if (json_is_integer(item))
			*video_bitrate = (int)json_integer_value(item);
	}

fail:
	json_decref(root);
}

struct obs_service_info rtmp_common_service = {
	.id = "rtmp_common",
	.get_name = rtmp_common_getname,
	.create = rtmp_common_create,
	.destroy = rtmp_common_destroy,
	.update = rtmp_common_update,
	.get_properties = rtmp_common_properties,
	.get_url = rtmp_common_url,
	.get_key = rtmp_common_key,
	.apply_encoder_settings = rtmp_common_apply_settings,
	.get_output_type = rtmp_common_get_output_type,
	.get_supported_resolutions = rtmp_common_get_supported_resolutions,
	.get_max_fps = rtmp_common_get_max_fps,
	.get_max_bitrate = rtmp_common_get_max_bitrate,
};
