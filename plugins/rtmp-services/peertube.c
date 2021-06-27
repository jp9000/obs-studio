#include <obs-module.h>
#include <util/dstr.h>

struct peertube {
	char *instance, *server, *key;
	bool perma_live;
};

static const char *peertube_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("PeerTubeStreamingService");
}

static void peertube_update(void *data, obs_data_t *settings)
{
	struct peertube *service = data;

	bfree(service->instance);
	bfree(service->server);
	bfree(service->key);

	service->instance = bstrdup(obs_data_get_string(settings, "instance"));
	service->server = bstrdup(obs_data_get_string(settings, "server"));
	service->key = bstrdup(obs_data_get_string(settings, "key"));
	service->perma_live = obs_data_get_bool(settings, "perma_live");
}

static void peertube_destroy(void *data)
{
	struct peertube *service = data;

	bfree(service->instance);
	bfree(service->server);
	bfree(service->key);
	bfree(service);
}

static void *peertube_create(obs_data_t *settings, obs_service_t *service)
{
	struct peertube *data = bzalloc(sizeof(struct peertube));
	peertube_update(data, settings);

	UNUSED_PARAMETER(service);
	return data;
}

static obs_properties_t *peertube_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *ppts = obs_properties_create();

	obs_properties_add_text(ppts, "instance", "URL", OBS_TEXT_DEFAULT);

	obs_properties_add_text(ppts, "server", "URL", OBS_TEXT_DEFAULT);

	obs_properties_add_text(ppts, "key", obs_module_text("StreamKey"),
				OBS_TEXT_PASSWORD);
	obs_properties_add_bool(ppts, "perma_live",
				obs_module_text("PermaLive"));

	return ppts;
}

static const char *peertube_url(void *data)
{
	struct peertube *service = data;
	return service->server;
}

static const char *peertube_key(void *data)
{
	struct peertube *service = data;
	return service->key;
}

#define RTMP_PROTOCOL "rtmp"

static void peertube_apply_settings(void *data, obs_data_t *video_settings,
				    obs_data_t *audio_settings)
{
	UNUSED_PARAMETER(audio_settings);

	struct peertube *service = data;
	if (service->instance != NULL && video_settings != NULL &&
	    strncmp(service->instance, RTMP_PROTOCOL, strlen(RTMP_PROTOCOL)) !=
		    0) {
		obs_data_set_bool(video_settings, "repeat_headers", true);
	}
}

struct obs_service_info peertube_service = {
	.id = "peertube",
	.get_name = peertube_name,
	.create = peertube_create,
	.destroy = peertube_destroy,
	.update = peertube_update,
	.get_properties = peertube_properties,
	.get_url = peertube_url,
	.get_key = peertube_key,
	.apply_encoder_settings = peertube_apply_settings,
};
