#include <obs-module.h>
#include <obs-avc.h>

#include "const.h"

#include "DecklinkOutput.hpp"
#include "decklink-device.hpp"
#include "decklink-device-discovery.hpp"
#include "decklink-devices.hpp"

#include "../../libobs/media-io/video-scaler.h"
#include "../../libobs/util/util_uint64.h"

static void decklink_output_destroy(void *data)
{
	auto *decklink = (DeckLinkOutput *)data;
	delete decklink;
}

static void *decklink_output_create(obs_data_t *settings, obs_output_t *output)
{
	auto *decklinkOutput = new DeckLinkOutput(output, deviceEnum);

	decklinkOutput->deviceHash = obs_data_get_string(settings, DEVICE_HASH);
	decklinkOutput->modeID = obs_data_get_int(settings, MODE_ID);
	decklinkOutput->keyerMode = (int)obs_data_get_int(settings, KEYER);

	return decklinkOutput;
}

static void decklink_output_update(void *data, obs_data_t *settings)
{
	auto *decklink = (DeckLinkOutput *)data;

	decklink->deviceHash = obs_data_get_string(settings, DEVICE_HASH);
	decklink->modeID = obs_data_get_int(settings, MODE_ID);
	decklink->keyerMode = (int)obs_data_get_int(settings, KEYER);
}

static bool decklink_output_start(void *data)
{
	auto *decklink = (DeckLinkOutput *)data;
	struct obs_audio_info aoi;

	if (!obs_get_audio_info(&aoi)) {
		blog(LOG_WARNING, "No active audio");
		return false;
	}

	if (!decklink->deviceHash || !*decklink->deviceHash)
		return false;

	decklink->audio_samplerate = aoi.samples_per_sec;
	decklink->audio_planes = 2;
	decklink->audio_size =
		get_audio_size(AUDIO_FORMAT_16BIT, aoi.speakers, 1);

	decklink->start_timestamp = 0;

	ComPtr<DeckLinkDevice> device;

	device.Set(deviceEnum->FindByHash(decklink->deviceHash));

	if (!device)
		return false;

	DeckLinkDeviceMode *mode = device->FindOutputMode(decklink->modeID);

	decklink->SetSize(mode->GetWidth(), mode->GetHeight());

	struct video_scale_info to = {};

	if (decklink->keyerMode != 0) {
		to.format = VIDEO_FORMAT_BGRA;
	} else {
		to.format = VIDEO_FORMAT_UYVY;
	}
	to.width = mode->GetWidth();
	to.height = mode->GetHeight();

	obs_output_set_video_conversion(decklink->GetOutput(), &to);

	device->SetKeyerMode(decklink->keyerMode);

	if (!decklink->Activate(device, decklink->modeID))
		return false;

	struct audio_convert_info conversion = {};
	conversion.format = AUDIO_FORMAT_16BIT;
	conversion.speakers = SPEAKERS_STEREO;
	conversion.samples_per_sec = 48000; // Only format the decklink supports

	obs_output_set_audio_conversion(decklink->GetOutput(), &conversion);

	if (!obs_output_begin_data_capture(decklink->GetOutput(), 0))
		return false;

	return true;
}

static void decklink_output_stop(void *data, uint64_t)
{
	auto *decklink = (DeckLinkOutput *)data;

	obs_output_end_data_capture(decklink->GetOutput());

	ComPtr<DeckLinkDevice> device;

	device.Set(deviceEnum->FindByHash(decklink->deviceHash));

	decklink->Deactivate();
}

static void decklink_output_raw_video(void *data, struct video_data *frame)
{
	auto *decklink = (DeckLinkOutput *)data;

	if (!decklink->start_timestamp)
		decklink->start_timestamp = frame->timestamp;

	decklink->DisplayVideoFrame(frame);
}

static bool prepare_audio(DeckLinkOutput *decklink,
			  const struct audio_data *frame,
			  struct audio_data *output)
{
	*output = *frame;

	if (frame->timestamp < decklink->start_timestamp) {
		uint64_t duration = util_mul_div64(frame->frames, 1000000000ULL,
						   decklink->audio_samplerate);
		uint64_t end_ts = frame->timestamp + duration;
		uint64_t cutoff;

		if (end_ts <= decklink->start_timestamp)
			return false;

		cutoff = decklink->start_timestamp - frame->timestamp;
		output->timestamp += cutoff;

		cutoff = util_mul_div64(cutoff, decklink->audio_samplerate,
					1000000000ULL);

		for (size_t i = 0; i < decklink->audio_planes; i++)
			output->data[i] +=
				decklink->audio_size * (uint32_t)cutoff;

		output->frames -= (uint32_t)cutoff;
	}

	return true;
}

static void decklink_output_raw_audio(void *data, struct audio_data *frames)
{
	auto *decklink = (DeckLinkOutput *)data;
	struct audio_data in;

	if (!decklink->start_timestamp)
		return;

	if (!prepare_audio(decklink, frames, &in))
		return;

	decklink->WriteAudio(&in);
}

static bool decklink_output_device_changed(obs_properties_t *props,
					   obs_property_t *list,
					   obs_data_t *settings)
{
	const char *name = obs_data_get_string(settings, DEVICE_NAME);
	const char *hash = obs_data_get_string(settings, DEVICE_HASH);
	const char *mode = obs_data_get_string(settings, MODE_NAME);
	long long modeId = obs_data_get_int(settings, MODE_ID);

	size_t itemCount = obs_property_list_item_count(list);
	bool itemFound = false;

	for (size_t i = 0; i < itemCount; i++) {
		const char *curHash = obs_property_list_item_string(list, i);
		if (strcmp(hash, curHash) == 0) {
			itemFound = true;
			break;
		}
	}

	if (!itemFound) {
		obs_property_list_insert_string(list, 0, name, hash);
		obs_property_list_item_disable(list, 0, true);
	}

	obs_property_t *modeList = obs_properties_get(props, MODE_ID);
	obs_property_t *keyerList = obs_properties_get(props, KEYER);

	obs_property_list_clear(modeList);
	obs_property_list_clear(keyerList);

	ComPtr<DeckLinkDevice> device;
	device.Set(deviceEnum->FindByHash(hash));

	if (!device) {
		obs_property_list_add_int(modeList, mode, modeId);
		obs_property_list_item_disable(modeList, 0, true);
		obs_property_list_item_disable(keyerList, 0, true);
	} else {
		const std::vector<DeckLinkDeviceMode *> &modes =
			device->GetOutputModes();

		for (DeckLinkDeviceMode *mode : modes) {
			obs_property_list_add_int(modeList,
						  mode->GetName().c_str(),
						  mode->GetId());
		}

		obs_property_list_add_int(keyerList, "Disabled", 0);

		if (device->GetSupportsExternalKeyer()) {
			obs_property_list_add_int(keyerList, "External", 1);
		}

		if (device->GetSupportsInternalKeyer()) {
			obs_property_list_add_int(keyerList, "Internal", 2);
		}
	}

	return true;
}

static obs_properties_t *decklink_output_properties(void *unused)
{
	UNUSED_PARAMETER(unused);
	obs_properties_t *props = obs_properties_create();

	obs_property_t *list = obs_properties_add_list(props, DEVICE_HASH,
						       TEXT_DEVICE,
						       OBS_COMBO_TYPE_LIST,
						       OBS_COMBO_FORMAT_STRING);
	obs_property_set_modified_callback(list,
					   decklink_output_device_changed);

	fill_out_devices(list);

	obs_properties_add_list(props, MODE_ID, TEXT_MODE, OBS_COMBO_TYPE_LIST,
				OBS_COMBO_FORMAT_INT);

	obs_properties_add_bool(props, AUTO_START, TEXT_AUTO_START);

	obs_properties_add_list(props, KEYER, TEXT_ENABLE_KEYER,
				OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	return props;
}

static const char *decklink_output_get_name(void *)
{
	return obs_module_text("BlackmagicDevice");
}

struct obs_output_info create_decklink_output_info()
{
	struct obs_output_info decklink_output_info = {};

	decklink_output_info.id = "decklink_output";
	decklink_output_info.flags = OBS_OUTPUT_AV;
	decklink_output_info.get_name = decklink_output_get_name;
	decklink_output_info.create = decklink_output_create;
	decklink_output_info.destroy = decklink_output_destroy;
	decklink_output_info.start = decklink_output_start;
	decklink_output_info.stop = decklink_output_stop;
	decklink_output_info.get_properties = decklink_output_properties;
	decklink_output_info.raw_video = decklink_output_raw_video;
	decklink_output_info.raw_audio = decklink_output_raw_audio;
	decklink_output_info.update = decklink_output_update;

	return decklink_output_info;
}
