#include "responder.hpp"
#include "interface.hpp"
#include "json11.hpp"

#include <util/dstr.hpp>

using namespace std;
using namespace json11;

ResponseServer::ResponseServer(signal_handler_t *plugin_handler)
{
	ipc_pipe_server_start_with_descriptor(&pipe, "obs-ipc-interface",
					      ReadCallback, this,
					      CreateDescriptor());

	ipc_pipe_server_set_disconnect_callback(&pipe, DisconnectCallback,
						this);

	signal_handler = plugin_handler;
}

ResponseServer::~ResponseServer()
{
	ipc_pipe_server_free(&pipe);
}

void ResponseServer::ReadCallback(void *param, uint8_t *data, size_t size)
{
	ResponseServer *server = (ResponseServer *)param;

	if (data && size) {
		const char *str = (const char *)data;

		std::string err;
		Json msg = Json::parse(str, err);

		if (!err.empty()) {
			blog(LOG_DEBUG, "%s", err.c_str());
		}

		if (!msg["command"].is_null()) {
			std::string com = msg["command"].string_value();

			if (com == "connect") {
				IPCInterface *ipc = new IPCInterface(
					server->signal_handler);
				AddIPCInterface(ipc);

				Json res = Json::object{
					{"result", 0},
					{"response",
					 Json::object{{"pipeName",
						       ipc->GetPipeName()}}}};

				string msg = res.dump();

				ipc_pipe_server_write(&server->pipe,
						      msg.c_str(),
						      msg.length() + 1);
			}
		}
	}

	ipc_pipe_server_disconnect(&server->pipe);
}
