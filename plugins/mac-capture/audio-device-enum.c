#include <CoreFoundation/CFString.h>
#include <CoreAudio/CoreAudio.h>

#include <util/apple/cfstring-utils.h>

#include "audio-device-enum.h"

/* ugh, because mac has no means of capturing output, we have to basically
 * mark soundflower, wavtap and sound siphon as output devices. */
static inline bool device_is_input(char *device)
{
	return astrstri(device, "soundflower") == NULL &&
	       astrstri(device, "wavtap") == NULL &&
	       astrstri(device, "soundsiphon") == NULL &&
	       astrstri(device, "ishowu") == NULL &&
	       astrstri(device, "blackhole") == NULL &&
	       astrstri(device, "loopback") == NULL;
}

static inline bool enum_success(OSStatus stat, const char *msg)
{
	if (stat != noErr) {
		blog(LOG_WARNING, "[coreaudio_enum_devices] %s failed: %d", msg,
		     (int)stat);
		return false;
	}

	return true;
}

typedef bool (*enum_device_proc_t)(void *param, CFStringRef cf_name,
				   CFStringRef cf_uid, AudioDeviceID id);

static bool coreaudio_enum_device(enum_device_proc_t proc, void *param,
				  AudioDeviceID id)
{
	UInt32 size = 0;
	CFStringRef cf_name = NULL;
	CFStringRef cf_uid = NULL;
	bool enum_next = true;
	OSStatus stat;

	AudioObjectPropertyAddress addr = {kAudioDevicePropertyStreams,
					   kAudioDevicePropertyScopeInput,
					   kAudioObjectPropertyElementMaster};

	/* check to see if it's a mac input device */
	AudioObjectGetPropertyDataSize(id, &addr, 0, NULL, &size);
	if (!size)
		return true;

	size = sizeof(CFStringRef);

	addr.mSelector = kAudioDevicePropertyDeviceUID;
	stat = AudioObjectGetPropertyData(id, &addr, 0, NULL, &size, &cf_uid);
	if (!enum_success(stat, "get audio device UID"))
		return true;

	addr.mSelector = kAudioDevicePropertyDeviceNameCFString;
	stat = AudioObjectGetPropertyData(id, &addr, 0, NULL, &size, &cf_name);
	if (!enum_success(stat, "get audio device name"))
		goto fail;

	enum_next = proc(param, cf_name, cf_uid, id);

fail:
	if (cf_name)
		CFRelease(cf_name);
	if (cf_uid)
		CFRelease(cf_uid);
	return enum_next;
}

static void enum_devices(enum_device_proc_t proc, void *param)
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
	if (!enum_success(stat, "get kAudioObjectSystemObject data size"))
		return;

	ids = bmalloc(size);
	count = size / sizeof(AudioDeviceID);

	stat = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0,
					  NULL, &size, ids);

	if (enum_success(stat, "get kAudioObjectSystemObject data"))
		for (UInt32 i = 0; i < count; i++)
			if (!coreaudio_enum_device(proc, param, ids[i]))
				break;

	bfree(ids);
}

struct add_data {
	struct device_list *list;
	bool input;
};

static bool coreaudio_enum_add_device(void *param, CFStringRef cf_name,
				      CFStringRef cf_uid, AudioDeviceID id)
{
	struct add_data *data = param;
	struct device_item item;

	memset(&item, 0, sizeof(item));

	if (!cfstr_copy_dstr(cf_name, kCFStringEncodingUTF8, &item.name))
		goto fail;
	if (!cfstr_copy_dstr(cf_uid, kCFStringEncodingUTF8, &item.value))
		goto fail;

	if (data->input || !device_is_input(item.value.array))
		device_list_add(data->list, &item);

fail:
	device_item_free(&item);

	UNUSED_PARAMETER(id);
	return true;
}

void coreaudio_enum_devices(struct device_list *list, bool input)
{
	struct add_data data = {list, input};
	enum_devices(coreaudio_enum_add_device, &data);
}

struct device_id_data {
	CFStringRef uid;
	AudioDeviceID *id;
	bool found;
};

static bool get_device_id(void *param, CFStringRef cf_name, CFStringRef cf_uid,
			  AudioDeviceID id)
{
	struct device_id_data *data = param;

	if (CFStringCompare(cf_uid, data->uid, 0) == 0) {
		*data->id = id;
		data->found = true;
		return false;
	}

	UNUSED_PARAMETER(cf_name);
	return true;
}

bool coreaudio_get_device_id(CFStringRef uid, AudioDeviceID *id)
{
	struct device_id_data data = {uid, id, false};
	enum_devices(get_device_id, &data);
	return data.found;
}
