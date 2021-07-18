#include "service-factory.hpp"

#include "protocols.hpp"
#include "plugin.hpp"
#include "service-instance.hpp"

extern "C" {
#include <obs-module.h>
}

// XXX: Add the server list for each protocols
void service_factory::create_server_lists(obs_properties_t *props)
{
	obs_property_t *rtmp, *hls;
	rtmp = obs_properties_add_list(props, "server_rtmp",
				       obs_module_text("Server"),
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
#if !RTMPS_DISABLED
	obs_property_t *rtmps = obs_properties_add_list(
		props, "server_rtmps", obs_module_text("Server"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
#endif
	hls = obs_properties_add_list(props, "server_hls",
				      obs_module_text("Server"),
				      OBS_COMBO_TYPE_LIST,
				      OBS_COMBO_FORMAT_STRING);
#if !FTL_DISABLED
	obs_property_t *ftl = obs_properties_add_list(props, "server_ftl",
						      obs_module_text("Server"),
						      OBS_COMBO_TYPE_LIST,
						      OBS_COMBO_FORMAT_STRING);
#endif

	for (size_t idx = 0; idx < servers.size(); idx++) {
		if (strcmp(servers[idx].protocol, "RTMP") == 0)
			obs_property_list_add_string(
				rtmp, obs_module_text(servers[idx].name),
				servers[idx].url);

#if !RTMPS_DISABLED
		if (strcmp(servers[idx].protocol, "RTMPS") == 0)
			obs_property_list_add_string(
				rtmps, obs_module_text(servers[idx].name),
				servers[idx].url);
#endif

		if (strcmp(servers[idx].protocol, "HLS") == 0)
			obs_property_list_add_string(
				hls, obs_module_text(servers[idx].name),
				servers[idx].url);

#if !FTL_DISABLED
		if (strcmp(servers[idx].protocol, "FTL") == 0)
			obs_property_list_add_string(
				ftl, obs_module_text(servers[idx].name),
				servers[idx].url);
#endif
	}
}

/* ----------------------------------------------------------------- */

// XXX: Add maximum default settings for each protocols
void service_factory::add_maximum_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "max_video_bitrate_rtmp",
				 maximum["RTMP"].video_bitrate);
	obs_data_set_default_int(settings, "max_audio_bitrate_rtmp",
				 maximum["RTMP"].audio_bitrate);
	obs_data_set_default_int(settings, "max_fps_rtmp", maximum["RTMP"].fps);

#if !RTMPS_DISABLED
	obs_data_set_default_int(settings, "max_video_bitrate_rtmps",
				 maximum["RTMPS"].video_bitrate);
	obs_data_set_default_int(settings, "max_audio_bitrate_rtmps",
				 maximum["RTMPS"].audio_bitrate);
	obs_data_set_default_int(settings, "max_fps_rtmps",
				 maximum["RTMPS"].fps);
#endif

	obs_data_set_default_int(settings, "max_video_bitrate_hls",
				 maximum["HLS"].video_bitrate);
	obs_data_set_default_int(settings, "max_audio_bitrate_hls",
				 maximum["HLS"].audio_bitrate);
	obs_data_set_default_int(settings, "max_fps_hls", maximum["HLS"].fps);

#if !FTL_DISABLED
	obs_data_set_default_int(settings, "max_video_bitrate_ftl",
				 maximum["FTL"].video_bitrate);
	obs_data_set_default_int(settings, "max_audio_bitrate_ftl",
				 maximum["FTL"].audio_bitrate);
	obs_data_set_default_int(settings, "max_fps_ftl", maximum["FTL"].fps);
#endif

	obs_data_set_default_bool(settings, "ignore_maximum", false);
}

// XXX: Add maximum settings for each protocols
void service_factory::add_maximum_infos(obs_properties_t *props)
{
	obs_properties_add_info_bitrate(props, "max_video_bitrate_rtmp",
					obs_module_text("MaxVideoBitrate"));
	obs_properties_add_info_bitrate(props, "max_audio_bitrate_rtmp",
					obs_module_text("MaxAudioBitrate"));
	obs_properties_add_info_fps(props, "max_fps_rtmp",
				    obs_module_text("MaxFPS"));

#if !RTMPS_DISABLED
	obs_properties_add_info_bitrate(props, "max_video_bitrate_rtmps",
					obs_module_text("MaxVideoBitrate"));
	obs_properties_add_info_bitrate(props, "max_audio_bitrate_rtmps",
					obs_module_text("MaxAudioBitrate"));
	obs_properties_add_info_fps(props, "max_fps_rtmps",
				    obs_module_text("MaxFPS"));
#endif

	obs_properties_add_info_bitrate(props, "max_video_bitrate_hls",
					obs_module_text("MaxVideoBitrate"));
	obs_properties_add_info_bitrate(props, "max_audio_bitrate_hls",
					obs_module_text("MaxAudioBitrate"));
	obs_properties_add_info_fps(props, "max_fps_hls",
				    obs_module_text("MaxFPS"));

#if !FTL_DISABLED
	obs_properties_add_info_bitrate(props, "max_video_bitrate_ftl",
					obs_module_text("MaxVideoBitrate"));
	obs_properties_add_info_bitrate(props, "max_audio_bitrate_ftl",
					obs_module_text("MaxAudioBitrate"));
	obs_properties_add_info_fps(props, "max_fps_ftl",
				    obs_module_text("MaxFPS"));
#endif

	obs_properties_add_bool(props, "ignore_maximum",
				obs_module_text("IgnoreMaximum"));
}

/* ----------------------------------------------------------------- */

static inline int get_int_val(json_t *service, const char *key)
{
	json_t *integer_val = json_object_get(service, key);
	if (!integer_val || !json_is_integer(integer_val))
		return -1;

	return (int)json_integer_value(integer_val);
}

static inline const char *get_string_val(json_t *service, const char *key)
{
	json_t *str_val = json_object_get(service, key);
	if (!str_val || !json_is_string(str_val))
		return NULL;

	return json_string_value(str_val);
}

const char *service_factory::_get_name(void *type_data) noexcept
try {
	if (type_data)
		return reinterpret_cast<service_factory *>(type_data)
			->get_name();
	return nullptr;
} catch (const std::exception &ex) {
	blog(LOG_ERROR, "Unexpected exception in function %s: %s", __func__,
	     ex.what());
	return nullptr;
} catch (...) {
	blog(LOG_ERROR, "Unexpected exception in function %s", __func__);
	return nullptr;
}

void *service_factory::_create(obs_data_t *settings,
			       obs_service_t *service) noexcept
try {
	service_factory *fac = reinterpret_cast<service_factory *>(
		obs_service_get_type_data(service));
	return fac->create(settings, service);
} catch (const std::exception &ex) {
	blog(LOG_ERROR, "Unexpected exception in function %s: %s", __func__,
	     ex.what());
	return nullptr;
} catch (...) {
	blog(LOG_ERROR, "Unexpected exception in function %s", __func__);
	return nullptr;
}

void service_factory::_destroy(void *data) noexcept
try {
	if (data)
		delete reinterpret_cast<service_instance *>(data);
} catch (const std::exception &ex) {
	blog(LOG_ERROR, "Unexpected exception in function %s: %s", __func__,
	     ex.what());
} catch (...) {
	blog(LOG_ERROR, "Unexpected exception in function %s", __func__);
}

void service_factory::_update(void *data, obs_data_t *settings) noexcept
try {
	service_instance *priv = reinterpret_cast<service_instance *>(data);
	if (priv) {
		priv->update(settings);
	}

} catch (const std::exception &ex) {
	blog(LOG_ERROR, "Unexpected exception in function %s: %s", __func__,
	     ex.what());
} catch (...) {
	blog(LOG_ERROR, "Unexpected exception in function %s", __func__);
}

void service_factory::_get_defaults2(obs_data_t *settings,
				     void *type_data) noexcept
try {
	if (type_data)
		reinterpret_cast<service_factory *>(type_data)->get_defaults2(
			settings);
} catch (const std::exception &ex) {
	blog(LOG_ERROR, "Unexpected exception in function %s: %s", __func__,
	     ex.what());
} catch (...) {
	blog(LOG_ERROR, "Unexpected exception in function %s", __func__);
}

obs_properties_t *service_factory::_get_properties2(void *data,
						    void *type_data) noexcept
try {
	if (type_data)
		return reinterpret_cast<service_factory *>(type_data)
			->get_properties2(data);
	return nullptr;
} catch (const std::exception &ex) {
	blog(LOG_ERROR, "Unexpected exception in function %s: %s", __func__,
	     ex.what());
	return nullptr;
} catch (...) {
	blog(LOG_ERROR, "Unexpected exception in function %s", __func__);
	return nullptr;
}

const char *service_factory::_get_protocol(void *data) noexcept
try {
	service_instance *priv = reinterpret_cast<service_instance *>(data);
	if (priv)
		return priv->get_protocol();
	return nullptr;
} catch (const std::exception &ex) {
	blog(LOG_ERROR, "Unexpected exception in function %s: %s", __func__,
	     ex.what());
	return nullptr;
} catch (...) {
	blog(LOG_ERROR, "Unexpected exception in function %s", __func__);
	return nullptr;
}

const char *service_factory::_get_url(void *data) noexcept
try {
	service_instance *priv = reinterpret_cast<service_instance *>(data);
	if (priv)
		return priv->get_url();
	return nullptr;
} catch (const std::exception &ex) {
	blog(LOG_ERROR, "Unexpected exception in function %s: %s", __func__,
	     ex.what());
	return nullptr;
} catch (...) {
	blog(LOG_ERROR, "Unexpected exception in function %s", __func__);
	return nullptr;
}

const char *service_factory::_get_key(void *data) noexcept
try {
	service_instance *priv = reinterpret_cast<service_instance *>(data);
	if (priv)
		return priv->get_key();
	return nullptr;
} catch (const std::exception &ex) {
	blog(LOG_ERROR, "Unexpected exception in function %s: %s", __func__,
	     ex.what());
	return nullptr;
} catch (...) {
	blog(LOG_ERROR, "Unexpected exception in function %s", __func__);
	return nullptr;
}

void service_factory::_get_max_fps(void *data, int *fps) noexcept
try {
	service_instance *priv = reinterpret_cast<service_instance *>(data);
	if (priv)
		priv->get_max_fps(fps);
} catch (const std::exception &ex) {
	blog(LOG_ERROR, "Unexpected exception in function %s: %s", __func__,
	     ex.what());
} catch (...) {
	blog(LOG_ERROR, "Unexpected exception in function %s", __func__);
}

void service_factory::_get_max_bitrate(void *data, int *video,
				       int *audio) noexcept
try {
	service_instance *priv = reinterpret_cast<service_instance *>(data);
	if (priv)
		priv->get_max_bitrate(video, audio);
} catch (const std::exception &ex) {
	blog(LOG_ERROR, "Unexpected exception in function %s: %s", __func__,
	     ex.what());
} catch (...) {
	blog(LOG_ERROR, "Unexpected exception in function %s", __func__);
}

/* ----------------------------------------------------------------- */

service_factory::service_factory(json_t *service)
{
	// XXX: Initialize maximum settings for each protocols
	// Clang disabled to keep it readable a a list
	/* clang-format off */
	maximum.insert({
		{"RTMP", {}},
#if !RTMPS_DISABLED
		{"RTMPS", {}},
#endif
		{"HLS", {}},
#if !FTL_DISABLED
		{"FTL", {}},
#endif
	});
	/* clang-format on */

	/** JSON data extraction **/

	_id = get_string_val(service, "id");
	_name = get_string_val(service, "name");

	const char *info_link = get_string_val(service, "more_info_link");
	if (info_link != NULL)
		more_info_link = info_link;

	const char *key_link = get_string_val(service, "stream_key_link");
	if (key_link != NULL)
		stream_key_link = key_link;

	json_t *object;
	size_t idx;
	json_t *element;

	/* Available protocols extraction */

	object = json_object_get(service, "available_protocols");
	//If not provided set only RTMP by default
	if (!object)
		protocols.push_back("RTMP");
	else {
		json_incref(object);

		json_array_foreach (object, idx, element) {
			const char *prtcl = json_string_value(element);
			if (prtcl != NULL)
				protocols.push_back(prtcl);
		}

		json_decref(object);
	}

	/* Servers extraction */

	object = json_object_get(service, "servers");
	if (object) {
		json_incref(object);

		json_array_foreach (object, idx, element) {
			const char *prtcl = get_string_val(element, "protocol");
			const char *url = get_string_val(element, "url");
			const char *name = get_string_val(element, "name");

			if ((url != NULL) && (name != NULL)) {
				//If not provided set RTMP by default
				if (prtcl != NULL)
					servers.push_back({strdup(prtcl),
							   strdup(url),
							   strdup(name)});
				else
					servers.push_back({"RTMP", strdup(url),
							   strdup(name)});
			}
		}

		json_decref(object);
	}

	/* Maximum output settings extraction */

	object = json_object_get(service, "maximum");
	if (object) {
		json_incref(object);

		json_array_foreach (object, idx, element) {
			const char *prtcl = get_string_val(element, "protocol");
			int video_bitrate =
				get_int_val(element, "video_bitrate");
			int audio_bitrate =
				get_int_val(element, "audio_bitrate");
			int fps = get_int_val(element, "fps");

			if (prtcl != NULL) {
				if (video_bitrate != -1)
					maximum[prtcl].video_bitrate =
						video_bitrate;
				if (audio_bitrate != -1)
					maximum[prtcl].audio_bitrate =
						audio_bitrate;
				if (fps != -1)
					maximum[prtcl].fps = fps;
			}
		}

		json_decref(object);
	}

	/* Supported resolutions extraction */

	object = json_object_get(service, "supported_resolutions");
	if (object) {
		json_incref(object);

		json_array_foreach (object, idx, element) {
			const char *res_str = json_string_value(element);
			obs_service_resolution res;

			if (res_str != NULL) {
				if (sscanf(res_str, "%dx%d", &res.cx,
					   &res.cy) == 2) {
					supported_resolutions_str.push_back(
						res_str);
					supported_resolutions.push_back(res);
				}
			}
		}

		json_decref(object);
	}

	/** Service implementation **/

	_info.type_data = this;
	_info.id = _id.c_str();

	_info.get_name = _get_name;

	_info.create = _create;
	_info.destroy = _destroy;

	_info.update = _update;

	_info.get_defaults2 = _get_defaults2;

	_info.get_properties2 = _get_properties2;

	_info.get_protocol = _get_protocol;
	_info.get_url = _get_url;
	_info.get_key = _get_key;

	_info.get_max_fps = _get_max_fps;
	_info.get_max_bitrate = _get_max_bitrate;

#if RTMPS_DISABLED
	if ((protocols.size() == 1) && (protocols[0].compare("RTMPS") == 0))
		return;
#endif

#if FTL_DISABLED
	if ((protocols.size() == 1) && (protocols[0].compare("FTL") == 0))
		return;
#endif

	obs_register_service(&_info);
}

service_factory::~service_factory()
{
	protocols.clear();
	servers.clear();
	maximum.clear();
	supported_resolutions_str.clear();
	supported_resolutions.clear();
}

const char *service_factory::get_name()
{
	return _name.c_str();
}

void *service_factory::create(obs_data_t *settings, obs_service_t *service)
{
	return reinterpret_cast<void *>(
		new service_instance(settings, service));
}

void service_factory::get_defaults2(obs_data_t *settings)
{
	if (!more_info_link.empty())
		obs_data_set_default_string(settings, "info_link",
					    more_info_link.c_str());

	if (!stream_key_link.empty())
		obs_data_set_default_string(settings, "key_link",
					    stream_key_link.c_str());

	obs_data_set_default_string(settings, "protocol", protocols[0].c_str());

	add_maximum_defaults(settings);

	obs_data_set_default_bool(settings, "ignore_supported_resolutions",
				  false);
}

static inline void set_visible_maximum(obs_properties_t *props,
				       obs_data_t *settings, const char *name,
				       bool prtcl)
{
	obs_property_set_visible(obs_properties_get(props, name),
				 prtcl && (obs_data_get_int(settings, name) !=
					   -1));
}

static inline void
set_visible_ignore_maximum(obs_properties_t *props, obs_data_t *settings,
			   const char *max1, const char *max2, const char *max3)
{
	obs_property_set_visible(
		obs_properties_get(props, "ignore_maximum"),
		(obs_data_get_int(settings, max1) != -1) ||
			(obs_data_get_int(settings, max2) != -1) ||
			(obs_data_get_int(settings, max3) != -1));
}

// XXX: Set the visibility of server lists and maximum settings for each protocols
static bool modified_protocol(obs_properties_t *props, obs_property_t *,
			      obs_data_t *settings) noexcept
try {
	const char *protocol = obs_data_get_string(settings, "protocol");

	bool rtmp = strcmp(protocol, "RTMP") == 0;
	obs_property_set_visible(obs_properties_get(props, "server_rtmp"),
				 rtmp);
	set_visible_maximum(props, settings, "max_video_bitrate_rtmp", rtmp);
	set_visible_maximum(props, settings, "max_audio_bitrate_rtmp", rtmp);
	set_visible_maximum(props, settings, "max_fps_rtmp", rtmp);

#if !RTMPS_DISABLED
	bool rtmps = strcmp(protocol, "RTMPS") == 0;
	obs_property_set_visible(obs_properties_get(props, "server_rtmps"),
				 rtmps);
	set_visible_maximum(props, settings, "max_video_bitrate_rtmps", rtmps);
	set_visible_maximum(props, settings, "max_audio_bitrate_rtmps", rtmps);
	set_visible_maximum(props, settings, "max_fps_rtmps", rtmps);
#endif

	bool hls = strcmp(protocol, "HLS") == 0;
	obs_property_set_visible(obs_properties_get(props, "server_hls"), hls);
	set_visible_maximum(props, settings, "max_video_bitrate_hls", hls);
	set_visible_maximum(props, settings, "max_audio_bitrate_hls", hls);
	set_visible_maximum(props, settings, "max_fps_hls", hls);

#if !FTL_DISABLED
	bool ftl = strcmp(protocol, "FTL") == 0;
	obs_property_set_visible(obs_properties_get(props, "server_ftl"), ftl);
	set_visible_maximum(props, settings, "max_video_bitrate_ftl", ftl);
	set_visible_maximum(props, settings, "max_audio_bitrate_ftl", ftl);
	set_visible_maximum(props, settings, "max_fps_ftl", ftl);
#endif

	//Ignore Max Toggle
	if (rtmp)
		set_visible_ignore_maximum(props, settings,
					   "max_video_bitrate_rtmp",
					   "max_audio_bitrate_rtmp",
					   "max_fps_rtmp");
#if !RTMPS_DISABLED
	if (rtmps)
		set_visible_ignore_maximum(props, settings,
					   "max_video_bitrate_rtmps",
					   "max_audio_bitrate_rtmps",
					   "max_fps_rtmps");
#endif
	if (hls)
		set_visible_ignore_maximum(props, settings,
					   "max_video_bitrate_hls",
					   "max_audio_bitrate_hls",
					   "max_fps_hls");
#if !FTL_DISABLED
	if (ftl)
		set_visible_ignore_maximum(props, settings,
					   "max_video_bitrate_ftl",
					   "max_audio_bitrate_ftl",
					   "max_fps_ftl");
#endif

	return true;
} catch (const std::exception &ex) {
	blog(LOG_ERROR, "Unexpected exception in function %s: %s", __func__,
	     ex.what());
	return false;
} catch (...) {
	blog(LOG_ERROR, "Unexpected exception in function %s", __func__);
	return false;
}

obs_properties_t *service_factory::get_properties2(void *data)
{
	UNUSED_PARAMETER(data);

	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;

	if (!more_info_link.empty())
		obs_properties_add_open_url(props, "info_link",
					    obs_module_text("MoreInfo"));

	p = obs_properties_add_list(props, "protocol",
				    obs_module_text("Protocol"),
				    OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);

	obs_property_set_modified_callback(p, modified_protocol);

	for (size_t idx = 0; idx < protocols.size(); idx++) {
#if RTMPS_DISABLED
		if (protocols[idx].compare("RTMPS") == 0)
			continue;
#endif
#if FTL_DISABLED
		if (protocols[idx].compare("FTL") == 0)
			continue;
#endif
		obs_property_list_add_string(p, protocols[idx].c_str(),
					     protocols[idx].c_str());
	}

	create_server_lists(props);

	obs_properties_add_text(props, "key", obs_module_text("StreamKey"),
				OBS_TEXT_PASSWORD);

	if (!stream_key_link.empty())
		obs_properties_add_open_url(props, "key_link",
					    obs_module_text("StreamKeyLink"));

	add_maximum_infos(props);

	if (!supported_resolutions_str.empty()) {
		std::string label = obs_module_text("SupportedResolutions");
		label += " ";

		for (size_t idx = 0; idx < supported_resolutions_str.size();
		     idx++) {
			label += supported_resolutions_str[idx];
			if ((supported_resolutions_str.size() - idx) != 1)
				label += ", ";
		}

		obs_properties_add_info(props, "resolutions", label.c_str());
		obs_properties_add_bool(
			props, "ignore_supported_resolutions",
			obs_module_text("IgnoreSupportedResolutions"));
	}

	return props;
}
