#pragma once

#include <ipc-util/pipe.h>
#include <string>
#include "json11.hpp"
#include <obs-frontend-api.h>
#include "preview.hpp"

typedef void (*CommandResponse)(void *param, json11::Json res);

class IPCInterface {
public:
	IPCInterface(signal_handler_t *plugin_handler);
	~IPCInterface();

	std::string GetPipeName();
	void SendFrontendEvent(obs_frontend_event event);

	bool ipc_gone = false;

private:
	void CommandResult(std::string command, json11::Json,
			   CommandResponse callback);

	static bool CreateSignal(std::string, void *data);
	static bool ClearSignal(std::string, void *data);

	static void ReadCallback(void *param, uint8_t *data, size_t size);
	static bool DisconnectCallback(void *param);
	static void Respond(void *param, json11::Json res);
	ipc_pipe_server_t incoming = {0};
	ipc_pipe_server_t outgoing = {0};

	signal_handler_t *signal_handler;

	static void source_create(void *data, calldata_t *cd);
	static void source_remove(void *data, calldata_t *cd);
	static void source_activate(void *data, calldata_t *cd);
	static void source_deactivate(void *data, calldata_t *cd);
	static void source_show(void *data, calldata_t *cd);
	static void source_hide(void *data, calldata_t *cd);
	static void source_rename(void *data, calldata_t *cd);
	static void source_volume(void *data, calldata_t *cd);
	static void source_mute(void *data, calldata_t *cd);
	static void source_unmute(void *data, calldata_t *cd);
	static void source_audio_activate(void *data, calldata_t *cd);
	static void source_audio_deactivate(void *data, calldata_t *cd);

	std::string name;

	Preview preview;
	bool previewEvents = false;
	static void TexChange(uint32_t handle, void *param);
	static void Present(void *param);
};
