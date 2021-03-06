#include <CoreFoundation/CFString.h>
#include <CoreAudio/CoreAudio.h>

#include "../../obs-internal.h"
#include "../../util/dstr.h"
#include "../../util/apple/cfstring-utils.h"

#include "mac-helpers.h"

static bool obs_enum_audio_monitoring_device(obs_enum_audio_device_cb cb,
					     void *data, AudioDeviceID id,
					     bool allow_inputs)
{
	UInt32 size = 0;
	CFStringRef cf_name = NULL;
	CFStringRef cf_uid = NULL;
	char *name = NULL;
	char *uid = NULL;
	OSStatus stat;
	bool cont = true;

	AudioObjectPropertyAddress addr = {kAudioDevicePropertyStreams,
					   kAudioDevicePropertyScopeOutput,
					   kAudioObjectPropertyElementMaster};

	/* Check if the device is capable of audio output. */
	AudioObjectGetPropertyDataSize(id, &addr, 0, NULL, &size);
	if (!allow_inputs && !size)
		return true;

	size = sizeof(CFStringRef);

	addr.mSelector = kAudioDevicePropertyDeviceUID;
	stat = AudioObjectGetPropertyData(id, &addr, 0, NULL, &size, &cf_uid);
	if (!success(stat, "get audio device UID"))
		goto fail;

	addr.mSelector = kAudioDevicePropertyDeviceNameCFString;
	stat = AudioObjectGetPropertyData(id, &addr, 0, NULL, &size, &cf_name);
	if (!success(stat, "get audio device name"))
		goto fail;

	name = cfstr_copy_cstr(cf_name, kCFStringEncodingUTF8);
	if (!name) {
		blog(LOG_WARNING, "%s: failed to convert name", __FUNCTION__);
		goto fail;
	}

	uid = cfstr_copy_cstr(cf_uid, kCFStringEncodingUTF8);
	if (!uid) {
		blog(LOG_WARNING, "%s: failed to convert uid", __FUNCTION__);
		goto fail;
	}

	cont = cb(data, name, uid);

fail:
	bfree(name);
	bfree(uid);
	if (cf_name)
		CFRelease(cf_name);
	if (cf_uid)
		CFRelease(cf_uid);
	return cont;
}

static void enum_audio_devices(obs_enum_audio_device_cb cb, void *data,
			       bool allow_inputs)
{
	AudioObjectPropertyAddress addr = {kAudioHardwarePropertyDevices,
					   kAudioObjectPropertyScopeGlobal,
					   kAudioObjectPropertyElementMaster};

	UInt32 size = 0;
	UInt32 count;
	OSStatus stat;
	AudioDeviceID *ids;

	stat = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr,
					      0, NULL, &size);
	if (!success(stat, "get data size"))
		return;

	ids = malloc(size);
	count = size / sizeof(AudioDeviceID);

	stat = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0,
					  NULL, &size, ids);
	if (success(stat, "get data")) {
		for (UInt32 i = 0; i < count; i++) {
			if (!obs_enum_audio_monitoring_device(cb, data, ids[i],
							      allow_inputs))
				break;
		}
	}

	free(ids);
}

void obs_enum_audio_monitoring_devices(obs_enum_audio_device_cb cb, void *data)
{
	enum_audio_devices(cb, data, false);
}

static bool alloc_default_id(void *data, const char *name, const char *id)
{
	char **p_id = data;
	UNUSED_PARAMETER(name);

	*p_id = bstrdup(id);
	return false;
}

static void get_default_id(char **p_id)
{
	AudioObjectPropertyAddress addr = {
		kAudioHardwarePropertyDefaultSystemOutputDevice,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster};

	if (*p_id)
		return;

	OSStatus stat;
	AudioDeviceID id = 0;
	UInt32 size = sizeof(id);

	stat = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0,
					  NULL, &size, &id);
	if (success(stat, "AudioObjectGetPropertyData"))
		obs_enum_audio_monitoring_device(alloc_default_id, p_id, id,
						 true);
	if (!*p_id)
		*p_id = bzalloc(1);
}

struct device_name_info {
	const char *id;
	char *name;
};

static bool enum_device_name(void *data, const char *name, const char *id)
{
	struct device_name_info *info = data;

	if (strcmp(info->id, id) == 0) {
		info->name = bstrdup(name);
		return false;
	}

	return true;
}

bool devices_match(const char *id1, const char *id2)
{
	struct device_name_info info = {0};
	char *default_id = NULL;
	char *name1 = NULL;
	char *name2 = NULL;
	bool match;

	if (!id1 || !id2)
		return false;

	if (strcmp(id1, "default") == 0) {
		get_default_id(&default_id);
		id1 = default_id;
	}
	if (strcmp(id2, "default") == 0) {
		get_default_id(&default_id);
		id2 = default_id;
	}

	info.id = id1;
	enum_audio_devices(enum_device_name, &info, true);
	name1 = info.name;

	info.name = NULL;
	info.id = id2;
	enum_audio_devices(enum_device_name, &info, true);
	name2 = info.name;

	match = name1 && name2 && strcmp(name1, name2) == 0;
	bfree(default_id);
	bfree(name1);
	bfree(name2);

	return match;
}
