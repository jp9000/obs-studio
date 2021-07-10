#pragma once
#pragma once

#include "ipc-config.h"
#include "obs.hpp"
#include "interface.hpp"

#include <string>
#include <AclAPI.h>
#include <sddl.h>

SECURITY_DESCRIPTOR *CreateDescriptor();
void AddIPCInterface(IPCInterface *ipc);
bool DeleteIPCInterface(IPCInterface *ipc);
