#include "interface.hpp"
#include "obs-ipc.hpp"
#include <vector>

std::map<std::string, uint32_t> signalInstances;

enum class IPCError {
	IPC_ERROR_SUCCESS,
	IPC_ERROR_UNKNOWN_COMMAND,
	IPC_ERROR_MISSING_PARAM,
	IPC_ERROR_BAD_PARAM,

	IPC_ERROR_UNKNOWN = 100
};

using namespace json11;
using namespace std;

#ifdef __APPLE__
#define INPUT_AUDIO_SOURCE "coreaudio_input_capture"
#define OUTPUT_AUDIO_SOURCE "coreaudio_output_capture"
#elif _WIN32
#define INPUT_AUDIO_SOURCE "wasapi_input_capture"
#define OUTPUT_AUDIO_SOURCE "wasapi_output_capture"
#else
#define INPUT_AUDIO_SOURCE "pulse_input_capture"
#define OUTPUT_AUDIO_SOURCE "pulse_output_capture"
#endif

IPCInterface::IPCInterface(signal_handler_t *plugin_handler)
{
	UUID uuid;
	char *strUuid;
	UuidCreateSequential(&uuid);
	if (UuidToStringA(&uuid, (RPC_CSTR *)&strUuid) == RPC_S_OK) {
		name = strUuid;
		RpcStringFreeA((RPC_CSTR *)&strUuid);
	} else {
		name = to_string(rand());
	}

	ipc_pipe_server_start_with_descriptor(&incoming, (name + "-in").c_str(),
					      this->ReadCallback, this,
					      CreateDescriptor());
	ipc_pipe_server_return_start_with_descriptor(
		&outgoing, (name + "-out").c_str(), CreateDescriptor());

	ipc_pipe_server_set_disconnect_callback(&incoming, DisconnectCallback,
						this);

	signal_handler = plugin_handler;
}

IPCInterface::~IPCInterface()
{
	ipc_pipe_server_free(&incoming);
	ipc_pipe_server_free(&outgoing);

	signal_handler_t *handler = obs_get_signal_handler();
	signal_handler_disconnect(handler, "source_create", source_create,
				  this);
	signal_handler_disconnect(handler, "source_remove", source_remove,
				  this);
	signal_handler_disconnect(handler, "source_activate", source_activate,
				  this);
	signal_handler_disconnect(handler, "source_deactivate",
				  source_deactivate, this);
	signal_handler_disconnect(handler, "source_show", source_show, this);
	signal_handler_disconnect(handler, "source_hide", source_hide, this);
	signal_handler_disconnect(handler, "source_rename", source_rename,
				  this);
	signal_handler_disconnect(handler, "source_volume", source_volume,
				  this);
	signal_handler_disconnect(signal_handler, "source_mute", source_mute,
				  this);
	signal_handler_disconnect(signal_handler, "source_unmute",
				  source_unmute, this);
}

bool IPCInterface::ClearSignal(string signal, void *data)
{
	IPCInterface *ipc = (IPCInterface *)data;
	signal_handler_t *handler = obs_get_signal_handler();

	if (signalInstances.find(signal) == signalInstances.end()) {
		return false;
	} else {
		signalInstances[signal]--;
		if (signalInstances[signal] != 0) {
			return true;
		}
	}

	if (signal == "source_create") {
		signal_handler_disconnect(handler, signal.c_str(),
					  ipc->source_create, ipc);
		return true;
	} else if (signal == "source_remove") {
		signal_handler_disconnect(handler, signal.c_str(),
					  ipc->source_remove, ipc);
		return true;
	} else if (signal == "source_activate") {
		signal_handler_disconnect(handler, signal.c_str(),
					  ipc->source_activate, ipc);
		return true;
	} else if (signal == "source_deactivate") {
		signal_handler_disconnect(handler, signal.c_str(),
					  ipc->source_deactivate, ipc);
		return true;
	} else if (signal == "source_show") {
		signal_handler_disconnect(handler, signal.c_str(),
					  ipc->source_show, ipc);
		return true;
	} else if (signal == "source_hide") {
		signal_handler_disconnect(handler, signal.c_str(),
					  ipc->source_hide, ipc);
		return true;
	} else if (signal == "source_rename") {
		signal_handler_disconnect(handler, signal.c_str(),
					  ipc->source_rename, ipc);
		return true;
	} else if (signal == "source_volume") {
		signal_handler_disconnect(handler, signal.c_str(),
					  ipc->source_volume, ipc);
		return true;
	} else if (signal == "source_mute") {
		signal_handler_disconnect(ipc->signal_handler, signal.c_str(),
					  ipc->source_mute, ipc);
		return true;
	} else if (signal == "source_unmute") {
		signal_handler_disconnect(ipc->signal_handler, signal.c_str(),
					  ipc->source_unmute, ipc);
		return true;
	} else if (signal == "source_audio_activate") {
		signal_handler_disconnect(ipc->signal_handler, signal.c_str(),
					  ipc->source_audio_activate, ipc);
		return true;
	} else if (signal == "source_audio_deactivate") {
		signal_handler_disconnect(ipc->signal_handler, signal.c_str(),
					  ipc->source_audio_deactivate, ipc);
		return true;
	}

	return false;
}

bool IPCInterface::CreateSignal(string signal, void *data)
{
	IPCInterface *ipc = (IPCInterface *)data;
	signal_handler_t *handler = obs_get_signal_handler();

	if (signalInstances.find(signal) == signalInstances.end()) {
		signalInstances[signal] = 1;
	} else {
		signalInstances[signal]++;
		if (signalInstances[signal] != 1) {
			return true;
		}
	}

	if (signal == "source_create") {
		signal_handler_connect(handler, signal.c_str(),
				       ipc->source_create, ipc);
		return true;
	} else if (signal == "source_remove") {
		signal_handler_connect(handler, signal.c_str(),
				       ipc->source_remove, ipc);
		return true;
	} else if (signal == "source_activate") {
		signal_handler_connect(handler, signal.c_str(),
				       ipc->source_activate, ipc);
		return true;
	} else if (signal == "source_deactivate") {
		signal_handler_connect(handler, signal.c_str(),
				       ipc->source_deactivate, ipc);
		return true;
	} else if (signal == "source_show") {
		signal_handler_connect(handler, signal.c_str(),
				       ipc->source_show, ipc);
		return true;
	} else if (signal == "source_hide") {
		signal_handler_connect(handler, signal.c_str(),
				       ipc->source_hide, ipc);
		return true;
	} else if (signal == "source_rename") {
		signal_handler_connect(handler, signal.c_str(),
				       ipc->source_rename, ipc);
		return true;
	} else if (signal == "source_volume") {
		signal_handler_connect(handler, signal.c_str(),
				       ipc->source_volume, ipc);
		return true;
	} else if (signal == "source_mute") {
		signal_handler_connect(ipc->signal_handler, signal.c_str(),
				       ipc->source_mute, ipc);
		return true;
	} else if (signal == "source_unmute") {
		signal_handler_connect(ipc->signal_handler, signal.c_str(),
				       ipc->source_unmute, ipc);
		return true;
	} else if (signal == "source_audio_activate") {
		signal_handler_connect(ipc->signal_handler, signal.c_str(),
				       ipc->source_audio_activate, ipc);
		return true;
	} else if (signal == "source_audio_deactivate") {
		signal_handler_connect(ipc->signal_handler, signal.c_str(),
				       ipc->source_audio_deactivate, ipc);
		return true;
	}

	return false;
}

static Json CreateResponse(string command, IPCError result,
			   Json response = nullptr)
{
	Json::object res =
		Json::object{{"result", (int)result}, {"command", command}};

	if (response != nullptr)
		res["response"] = response;

	return res;
}

static string GetDeviceType(string id)
{
	if (id == INPUT_AUDIO_SOURCE)
		return "AudioInput";
	if (id == OUTPUT_AUDIO_SOURCE)
		return "AudioOutput";
	if (id == "dshow_input")
		return "AudioInput";

	return "AudioOutput";
}

void IPCInterface::CommandResult(string command, Json params,
				 CommandResponse callback)
{
	Json success = CreateResponse(command, IPCError::IPC_ERROR_SUCCESS);

	if (command == "StartStreaming") {
		obs_frontend_streaming_start();
		callback(this, success);
		return;

	} else if (command == "StopStreaming") {
		obs_frontend_streaming_stop();
		callback(this, success);
		return;

	} else if (command == "StartRecording") {
		obs_frontend_recording_start();
		callback(this, success);
		return;

	} else if (command == "StopRecording") {
		obs_frontend_recording_stop();
		callback(this, success);
		return;

	} else if (command == "PauseRecording") {
		obs_frontend_recording_pause(true);
		callback(this, success);
		return;

	} else if (command == "UnpauseRecording") {
		obs_frontend_recording_pause(false);
		callback(this, success);
		return;

	} else if (command == "SaveReplay") {
		obs_frontend_replay_buffer_save();
		callback(this, success);
		return;

	} else if (command == "GetCurrentScene") {
		obs_source_t *scene = obs_frontend_get_current_scene();

		Json responseParams = Json::object{
			{"current_scene", obs_source_get_name(scene)}};
		Json response = CreateResponse(
			command, IPCError::IPC_ERROR_SUCCESS, responseParams);

		obs_source_release(scene);

		callback(this, response);
		return;

	} else if (command == "GetSources") {
		if (!params["scene"].is_null()) {
			string sceneName = params["scene"].string_value();

			obs_source_t *source =
				obs_get_source_by_name(sceneName.c_str());
			obs_scene_t *scene = obs_scene_from_source(source);

			vector<string> sourceNames;

			auto loop = [](obs_scene_t *, obs_sceneitem_t *item,
				       void *param) mutable {
				vector<string> *sourceNames =
					(vector<string> *)param;

				obs_source_t *source =
					obs_sceneitem_get_source(item);

				string sourceName = obs_source_get_name(source);

				if (find(sourceNames->begin(),
					 sourceNames->end(),
					 sourceName) == sourceNames->end())
					sourceNames->push_back(sourceName);

				return true;
			};

			obs_scene_enum_items(scene, loop, &sourceNames);

			Json responseParam =
				Json::object{{"sources", sourceNames}};
			Json response = CreateResponse(
				command, IPCError::IPC_ERROR_SUCCESS,
				responseParam);

			callback(this, response);
		} else {
			Json response = CreateResponse(
				command, IPCError::IPC_ERROR_MISSING_PARAM);
			callback(this, response);
		}
		return;

	} else if (command == "GetAudioSources") {
		Json::object sources = Json::object{};

		auto loop = [](void *param, obs_source_t *source) mutable {
			Json::object *sources = (Json::object *)param;
			string sourceName = obs_source_get_name(source);

			Json::object sourceData = Json::object{};

			uint32_t flags = obs_source_get_output_flags(source);

			if ((flags & OBS_SOURCE_AUDIO) != 0) {
				double volume = obs_mul_to_db(
					obs_source_get_volume(source));

				sourceData["active"] =
					obs_source_active(source);
				sourceData["volume"] =
					isnan(volume) || volume == -INFINITY
						? -96
						: volume;
				sourceData["muted"] = obs_source_muted(source);
				sourceData["device_type"] = GetDeviceType(
					obs_source_get_id(source));
				sourceData["type"] = obs_source_get_id(source);
				sourceData["audio_active"] =
					obs_source_audio_active(source);

				(*sources)[sourceName] = sourceData;
			}

			return true;
		};

		obs_enum_sources(loop, &sources);

		Json response = CreateResponse(
			command, IPCError::IPC_ERROR_SUCCESS, sources);

		callback(this, response);
		return;

	} else if (command == "GetSourceInfo") {
		if (!params["source"].is_null()) {
			string sourceName = params["source"].string_value();
			obs_source_t *source =
				obs_get_source_by_name(sourceName.c_str());

			uint32_t flags = obs_source_get_output_flags(source);

			Json::object sourceInfo = Json::object{};

			double volume =
				obs_mul_to_db(obs_source_get_volume(source));

			sourceInfo["audio_source"] =
				(flags & OBS_SOURCE_AUDIO) != 0;
			sourceInfo["volume"] =
				isnan(volume) || volume == -INFINITY ? -96
								     : volume;
			sourceInfo["muted"] = obs_source_muted(source);
			sourceInfo["type"] = obs_source_get_id(source);
			sourceInfo["active"] = obs_source_active(source);
			sourceInfo["audio_active"] =
				obs_source_audio_active(source);
			sourceInfo["showing"] = obs_source_showing(source);
			sourceInfo["device_type"] =
				GetDeviceType(obs_source_get_id(source));

			Json response = CreateResponse(
				command, IPCError::IPC_ERROR_SUCCESS,
				sourceInfo);
			callback(this, response);

			obs_source_release(source);

		} else {
			Json response = CreateResponse(
				command, IPCError::IPC_ERROR_MISSING_PARAM);
			callback(this, response);
		}
		return;

	} else if (command == "SetVolume") {
		if (!params["source"].is_null() ||
		    !params["volume"].is_null()) {
			string sourceName = params["source"].string_value();
			double volume = params["volume"].number_value();

			obs_source_t *source =
				obs_get_source_by_name(sourceName.c_str());
			obs_source_set_volume(source,
					      obs_db_to_mul((float)volume));
			obs_source_release(source);

			callback(this, success);

		} else {
			Json response = CreateResponse(
				command, IPCError::IPC_ERROR_MISSING_PARAM);
			callback(this, response);
		}
		return;

	} else if (command == "SetMuted") {
		if (!params["source"].is_null() || !params["mute"].is_null()) {
			string sourceName = params["source"].string_value();
			bool mute = params["mute"].bool_value();

			obs_source_t *source =
				obs_get_source_by_name(sourceName.c_str());
			obs_source_set_muted(source, mute);
			obs_source_release(source);

			callback(this, success);

		} else {
			Json response = CreateResponse(
				command, IPCError::IPC_ERROR_MISSING_PARAM);
			callback(this, response);
		}
		return;

	} else if (command == "GetStatus") {
		Json responseParams = Json::object{
			{"recording", obs_frontend_recording_active()},
			{"streaming", obs_frontend_streaming_active()},
			{"recordingPaused", obs_frontend_recording_paused()},
			{"replaybuffer", obs_frontend_replay_buffer_active()},
			{"canPause", obs_frontend_recording_can_pause()}};

		Json response = CreateResponse(
			command, IPCError::IPC_ERROR_SUCCESS, responseParams);

		callback(this, response);
		return;

	} else if (command == "GetWindowHandle") {
		Json responseParams = Json::object{
			{"window_handle",
			 PtrToLong(obs_frontend_get_main_window_handle())}};

		Json response = CreateResponse(
			command, IPCError::IPC_ERROR_SUCCESS, responseParams);

		callback(this, response);
		return;

	} else if (command == "ConnectSignal") {
		if (!params["signal"].is_null()) {
			string signal = params["signal"].string_value();

			bool created = CreateSignal(signal, this);
			Json response;

			if (!created) {
				response = CreateResponse(
					command, IPCError::IPC_ERROR_BAD_PARAM);
			} else {
				response = CreateResponse(
					command, IPCError::IPC_ERROR_SUCCESS);
			}

			callback(this, response);
		} else {
			Json response = CreateResponse(
				command, IPCError::IPC_ERROR_MISSING_PARAM);
			callback(this, response);
		}

		return;

	} else if (command == "DisconnectSignal") {
		if (!params["signal"].is_null()) {
			string signal = params["signal"].string_value();

			bool created = ClearSignal(signal, this);
			Json response;

			if (!created) {
				response = CreateResponse(
					command, IPCError::IPC_ERROR_BAD_PARAM);
			} else {
				response = CreateResponse(
					command, IPCError::IPC_ERROR_SUCCESS);
			}

			callback(this, response);

		} else {
			Json response = CreateResponse(
				command, IPCError::IPC_ERROR_MISSING_PARAM);
			callback(this, response);
		}

		return;

	} else if (command == "GetPreviewHandle") {
		if (!previewEvents) {
			previewEvents = true;

			preview.AddTexChangeCallback(TexChange, this);
			preview.AddPresentCallback(Present, this);
		}

		uint32_t handle = preview.GetHandle();
		Json responseParam = Json::object{{"handle", (int)handle}};
		Json response = CreateResponse(
			command, IPCError::IPC_ERROR_SUCCESS, responseParam);

		callback(this, response);

		return;
	} else if (command == "SetPreviewSize") {
		if (previewEvents) {
			if (!params["width"].is_null() &&
			    !params["height"].is_null()) {

				uint32_t width =
					(uint32_t)params["width"].number_value();
				uint32_t height = (uint32_t)params["height"]
							  .number_value();

				preview.SetSize(width, height);
				callback(this, success);
			} else {
				Json response = CreateResponse(
					command,
					IPCError::IPC_ERROR_MISSING_PARAM);
				callback(this, response);
			}
		} else {
			Json response = CreateResponse(
				command, IPCError::IPC_ERROR_UNKNOWN);
			callback(this, response);
		}

		return;
	}

	Json response =
		CreateResponse(command, IPCError::IPC_ERROR_UNKNOWN_COMMAND);

	callback(this, response);
}

void IPCInterface::Respond(void *param, Json response)
{
	IPCInterface *ipc = (IPCInterface *)param;

	string resStr = response.dump();

	ipc_pipe_server_write(&ipc->outgoing, resStr.c_str(),
			      resStr.length() + 1);
}

void IPCInterface::ReadCallback(void *param, uint8_t *data, size_t size)
{
	IPCInterface *ipc = (IPCInterface *)param;

	Json response = Json();

	if (data && size) {
		const char *str = (const char *)data;

		std::string err;
		Json msg = Json::parse(str, err);

		if (!err.empty()) {
			blog(LOG_DEBUG, "%s", err.c_str());
			return;
		}

		if (!msg["command"].is_null()) {
			std::string com = msg["command"].string_value();
			Json params = msg["params"];

			ipc->CommandResult(com, params, ipc->Respond);
		}
	}
}

bool IPCInterface::DisconnectCallback(void *param)
{
	IPCInterface *ipc = (IPCInterface *)param;

	DeleteIPCInterface(ipc);

	return false;
}

string IPCInterface::GetPipeName()
{
	return name;
}

void IPCInterface::SendFrontendEvent(obs_frontend_event event)
{
	Json responseParams = Json::object{{"event", event}};
	Json response = CreateResponse(
		"FrontendEvent", IPCError::IPC_ERROR_SUCCESS, responseParams);

	this->Respond(this, response);
}

void IPCInterface::source_create(void *data, calldata_t *cd)
{
	IPCInterface *ipc = (IPCInterface *)data;
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");

	const char *sourceName = obs_source_get_name(source);

	Json responseParams = Json::object{{"signal", "source_create"},
					   {"source", sourceName}};
	Json response = CreateResponse("Signal", IPCError::IPC_ERROR_SUCCESS,
				       responseParams);

	ipc->Respond(ipc, response);
}

void IPCInterface::source_remove(void *data, calldata_t *cd)
{
	IPCInterface *ipc = (IPCInterface *)data;
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");

	const char *sourceName = obs_source_get_name(source);

	Json responseParams = Json::object{{"signal", "source_remove"},
					   {"source", sourceName}};
	Json response = CreateResponse("Signal", IPCError::IPC_ERROR_SUCCESS,
				       responseParams);

	ipc->Respond(ipc, response);
}

void IPCInterface::source_activate(void *data, calldata_t *cd)
{
	IPCInterface *ipc = (IPCInterface *)data;
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");

	const char *sourceName = obs_source_get_name(source);

	Json responseParams = Json::object{{"signal", "source_activate"},
					   {"source", sourceName}};
	Json response = CreateResponse("Signal", IPCError::IPC_ERROR_SUCCESS,
				       responseParams);

	ipc->Respond(ipc, response);
}

void IPCInterface::source_deactivate(void *data, calldata_t *cd)
{
	IPCInterface *ipc = (IPCInterface *)data;
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");

	const char *sourceName = obs_source_get_name(source);

	Json responseParams = Json::object{{"signal", "source_deactivate"},
					   {"source", sourceName}};
	Json response = CreateResponse("Signal", IPCError::IPC_ERROR_SUCCESS,
				       responseParams);

	ipc->Respond(ipc, response);
}

void IPCInterface::source_show(void *data, calldata_t *cd)
{
	IPCInterface *ipc = (IPCInterface *)data;
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");

	const char *sourceName = obs_source_get_name(source);

	Json responseParams =
		Json::object{{"signal", "source_show"}, {"source", sourceName}};
	Json response = CreateResponse("Signal", IPCError::IPC_ERROR_SUCCESS,
				       responseParams);

	ipc->Respond(ipc, response);
}

void IPCInterface::source_hide(void *data, calldata_t *cd)
{
	IPCInterface *ipc = (IPCInterface *)data;
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");

	const char *sourceName = obs_source_get_name(source);

	Json responseParams =
		Json::object{{"signal", "source_hide"}, {"source", sourceName}};
	Json response = CreateResponse("Signal", IPCError::IPC_ERROR_SUCCESS,
				       responseParams);

	ipc->Respond(ipc, response);
}

void IPCInterface::source_rename(void *data, calldata_t *cd)
{
	IPCInterface *ipc = (IPCInterface *)data;
	const char *newName = calldata_string(cd, "new_name");
	const char *prevName = calldata_string(cd, "prev_name");

	Json responseParams = Json::object{{"signal", "source_rename"},
					   {"new_name", newName},
					   {"prev_name", prevName}};
	Json response = CreateResponse("Signal", IPCError::IPC_ERROR_SUCCESS,
				       responseParams);

	ipc->Respond(ipc, response);
}

void IPCInterface::source_volume(void *data, calldata_t *cd)
{
	IPCInterface *ipc = (IPCInterface *)data;
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	float volume = obs_mul_to_db((float)calldata_float(cd, "volume"));

	if (volume == -INFINITY || isnan(volume)) {
		volume = -96.0f;
	}

	const char *sourceName = obs_source_get_name(source);

	Json responseParams = Json::object{{"signal", "source_volume"},
					   {"source", sourceName},
					   {"volume", volume}};
	Json response = CreateResponse("Signal", IPCError::IPC_ERROR_SUCCESS,
				       responseParams);

	ipc->Respond(ipc, response);
}

void IPCInterface::source_mute(void *data, calldata_t *cd)
{
	IPCInterface *ipc = (IPCInterface *)data;
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	const char *sourceName = obs_source_get_name(source);

	Json responseParams =
		Json::object{{"signal", "source_mute"}, {"source", sourceName}};
	Json response = CreateResponse("Signal", IPCError::IPC_ERROR_SUCCESS,
				       responseParams);

	ipc->Respond(ipc, response);
}

void IPCInterface::source_unmute(void *data, calldata_t *cd)
{
	IPCInterface *ipc = (IPCInterface *)data;
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	const char *sourceName = obs_source_get_name(source);

	Json responseParams = Json::object{{"signal", "source_unmute"},
					   {"source", sourceName}};
	Json response = CreateResponse("Signal", IPCError::IPC_ERROR_SUCCESS,
				       responseParams);

	ipc->Respond(ipc, response);
}

void IPCInterface::source_audio_activate(void *data, calldata_t *cd)
{
	IPCInterface *ipc = (IPCInterface *)data;
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	const char *sourceName = obs_source_get_name(source);

	Json responseParams = Json::object{{"signal", "source_audio_activate"},
					   {"source", sourceName}};
	Json response = CreateResponse("Signal", IPCError::IPC_ERROR_SUCCESS,
				       responseParams);

	ipc->Respond(ipc, response);
}

void IPCInterface::source_audio_deactivate(void *data, calldata_t *cd)
{
	IPCInterface *ipc = (IPCInterface *)data;
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	const char *sourceName = obs_source_get_name(source);

	Json responseParams = Json::object{
		{"signal", "source_audio_deactivate"}, {"source", sourceName}};
	Json response = CreateResponse("Signal", IPCError::IPC_ERROR_SUCCESS,
				       responseParams);

	ipc->Respond(ipc, response);
}

void IPCInterface::TexChange(uint32_t handle, void *param)
{
	IPCInterface *ipc = (IPCInterface *)param;

	Json responseParams = Json::object{{"handle", (int)handle}};
	Json response = CreateResponse("GetPreviewHandle",
				       IPCError::IPC_ERROR_SUCCESS,
				       responseParams);

	ipc->Respond(ipc, response);
}

void IPCInterface::Present(void *param)
{

	IPCInterface *ipc = (IPCInterface *)param;
	Json response =
		CreateResponse("PreviewPresent", IPCError::IPC_ERROR_SUCCESS);

	ipc->Respond(ipc, response);
}
