#pragma once

#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <propsys.h>

#ifdef __MINGW32__

#ifdef DEFINE_PROPERTYKEY
#undef DEFINE_PROPERTYKEY
#endif

/* clang-format off */

#define DEFINE_PROPERTYKEY(id, a, b, c, d, e, f, g, h, i, j, k, l) \
	const PROPERTYKEY id = { { a,b,c, { d,e,f,g,h,i,j,k, } }, l };
DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName, \
	0xa45c254e, 0xdf1c, 0x4efd, 0x80, \
	0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);

/* clang-format on */

#else

#include <functiondiscoverykeys_devpkey.h>

#endif

#include <vector>
#include <string>

struct AudioDeviceInfo {
	std::string name;
	std::string id;
};

std::string GetDeviceName(IMMDevice *device);
void GetWASAPIAudioDevices(std::vector<AudioDeviceInfo> &devices, bool input);
