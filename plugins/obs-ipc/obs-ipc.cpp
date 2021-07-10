/******************************************************************************
    Copyright (C) 2020 by Dillon Pentz <dillon@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <obs-module.h>
#include "obs-ipc.hpp"

#include <ipc-util/pipe.h>
#include <obs-frontend-api.h>

#include "ipc-config.h"
#include "responder.hpp"

#include <vector>
#include <algorithm>

using namespace std;

ResponseServer *server;
vector<IPCInterface *> interfaces;
signal_handler_t *signal_handler;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-ipc", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "IPC interface for OBS";
}

SECURITY_DESCRIPTOR *CreateDescriptor()
{
	void *sd = malloc(SECURITY_DESCRIPTOR_MIN_LENGTH);

	EXPLICIT_ACCESS ea[2];
	PSID everyone = NULL;
	PSID package = NULL;
	SID_IDENTIFIER_AUTHORITY worldAuth = SECURITY_WORLD_SID_AUTHORITY;
	PACL acl = NULL;

	if (!sd) {
		goto error;
	}

	if (!InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION)) {
		goto error;
	}

	if (!AllocateAndInitializeSid(&worldAuth, 1, SECURITY_WORLD_RID, 0, 0,
				      0, 0, 0, 0, 0, &everyone)) {
		goto error;
	}

	if (!ConvertStringSidToSidA(PACKAGE_SID, &package)) {
		goto error;
	}

	ea[0].grfAccessMode = SET_ACCESS;
	ea[0].grfAccessPermissions = GENERIC_ALL;
	ea[0].grfInheritance = NO_INHERITANCE;
	ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
	ea[0].Trustee.ptstrName = (LPWSTR)everyone;

	ea[1].grfAccessMode = SET_ACCESS;
	ea[1].grfAccessPermissions = GENERIC_ALL;
	ea[1].grfInheritance = NO_INHERITANCE;
	ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ea[1].Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
	ea[1].Trustee.ptstrName = (LPWSTR)package;

	SetEntriesInAcl(ARRAYSIZE(ea), ea, NULL, &acl);

	if (!SetSecurityDescriptorDacl(sd, true, acl, false)) {
		goto error;
	}

	return (SECURITY_DESCRIPTOR *)sd;

error:
	free(sd);
	return NULL;
}

void AddIPCInterface(IPCInterface *ipc)
{
	interfaces.push_back(ipc);
}

bool DeleteIPCInterface(IPCInterface *ipc)
{
	bool result = false;

	vector<IPCInterface *>::iterator iter =
		find(interfaces.begin(), interfaces.end(), ipc);

	if (iter != interfaces.end()) {
		interfaces.erase(iter);
		result = true;
	}

	delete ipc;
	return result;
}

void delete_interfaces()
{
	for (auto iter = interfaces.begin(); iter != interfaces.end();) {
		delete *iter;
		iter = interfaces.erase(iter);
	}
}

static inline void create_ipc_pipe()
{
	server = new ResponseServer(signal_handler);
}

static inline void destroy_ipc_pipe()
{
	delete server;
	server = nullptr;
}

static void frontend_events(obs_frontend_event event, void *private_data)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		create_ipc_pipe();
	}

	for (auto iter = interfaces.begin(); iter != interfaces.end(); ++iter) {
		(*iter)->SendFrontendEvent(event);
	}

	UNUSED_PARAMETER(private_data);
}

static void source_mute(void *data, calldata_t *cd)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	bool muted = calldata_bool(cd, "muted");

	struct calldata signal;
	uint8_t stack[128];

	calldata_init_fixed(&signal, stack, sizeof(stack));
	calldata_set_ptr(&signal, "source", source);

	if (muted)
		signal_handler_signal(signal_handler, "source_mute", &signal);
	else
		signal_handler_signal(signal_handler, "source_unmute", &signal);

	UNUSED_PARAMETER(data);
}

static void source_audio_activate(void *data, calldata_t *cd)
{
	signal_handler_signal(signal_handler, "source_audio_activate", cd);

	UNUSED_PARAMETER(data);
}
static void source_audio_deactivate(void *data, calldata_t *cd)
{
	signal_handler_signal(signal_handler, "source_audio_deactivate", cd);

	UNUSED_PARAMETER(data);
}

static void create_signals(void *param, calldata_t *cd)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	signal_handler_t *source_signals =
		obs_source_get_signal_handler(source);

	signal_handler_connect(source_signals, "mute", source_mute,
			       signal_handler);
	signal_handler_connect(source_signals, "audio_activate",
			       source_audio_activate, signal_handler);
	signal_handler_connect(source_signals, "audio_deactivate",
			       source_audio_deactivate, signal_handler);

	UNUSED_PARAMETER(param);
}

static const char *plugin_signals[] = {
	"void source_mute(ptr source)", "void source_unmute(ptr source)",
	"void source_audio_activate(ptr source)",
	"void source_audio_deactivate(ptr source)", NULL};

bool obs_module_load(void)
{
	signal_handler = signal_handler_create();
	signal_handler_add_array(signal_handler, plugin_signals);

	obs_frontend_add_event_callback(frontend_events, nullptr);

	signal_handler_connect(obs_get_signal_handler(), "source_create",
			       create_signals, nullptr);

	return true;
}

void obs_module_unload(void)
{
	delete_interfaces();
	signal_handler_destroy(signal_handler);
	obs_frontend_remove_event_callback(frontend_events, nullptr);
	destroy_ipc_pipe();
}
