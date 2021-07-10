#pragma once

#include "obs-ipc.hpp"
#include <ipc-util/pipe.h>

class ResponseServer {
public:
	ResponseServer(signal_handler_t *plugin_handler);
	~ResponseServer();

private:
	static void ReadCallback(void *param, uint8_t *data, size_t size);
	static bool DisconnectCallback(void *) { return true; };
	ipc_pipe_server_t pipe = {0};

	signal_handler_t *signal_handler;
};
