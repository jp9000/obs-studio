#include "service-instance.hpp"

#include <util/util.hpp>

service_instance::service_instance(obs_data_t *settings, obs_service_t *self)
	: _factory(reinterpret_cast<service_factory *>(
		  obs_service_get_type_data(self)))
{
	update(settings);
}

// XXX: Add server updater for each protocols
void service_instance::update(obs_data_t *settings)
{
	protocol = obs_data_get_string(settings, "protocol");

	if (protocol.compare("RTMP") == 0) {
		server = obs_data_get_string(settings, "server_rtmp");
		max_fps = obs_data_get_int(settings, "max_fps_rtmp");
		max_video_bitrate =
			obs_data_get_int(settings, "max_video_bitrate_rtmp");
		max_audio_bitrate =
			obs_data_get_int(settings, "max_audio_bitrate_rtmp");
	}
	if (protocol.compare("RTMPS") == 0) {
		server = obs_data_get_string(settings, "server_rtmps");
		max_fps = obs_data_get_int(settings, "max_fps_rtmps");
		max_video_bitrate =
			obs_data_get_int(settings, "max_video_bitrate_rtmps");
		max_audio_bitrate =
			obs_data_get_int(settings, "max_audio_bitrate_rtmps");
	}
	if (protocol.compare("HLS") == 0) {
		server = obs_data_get_string(settings, "server_hls");
		max_fps = obs_data_get_int(settings, "max_fps_hls");
		max_video_bitrate =
			obs_data_get_int(settings, "max_video_bitrate_hls");
		max_audio_bitrate =
			obs_data_get_int(settings, "max_audio_bitrate_hls");
	}
	if (protocol.compare("FTL") == 0) {
		server = obs_data_get_string(settings, "server_ftl");
		max_fps = obs_data_get_int(settings, "max_fps_ftl");
		max_video_bitrate =
			obs_data_get_int(settings, "max_video_bitrate_ftl");
		max_audio_bitrate =
			obs_data_get_int(settings, "max_audio_bitrate_ftl");
	}

	key = obs_data_get_string(settings, "key");
}

const char *service_instance::get_protocol()
{
	return protocol.c_str();
}

const char *service_instance::get_url()
{
	return server.c_str();
}

const char *service_instance::get_key()
{
	return key.c_str();
}

void service_instance::get_max_fps(int *fps)
{
	if (max_fps != -1)
		*fps = max_fps;
}

void service_instance::get_max_bitrate(int *video, int *audio)
{
	if (max_video_bitrate != -1)
		*video = max_video_bitrate;
	if (max_audio_bitrate != -1)
		*audio = max_audio_bitrate;
}
