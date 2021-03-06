#include <obs-module.h>

#include "const.h"
#include "util.hpp"

#include "DecklinkInput.hpp"
#include "decklink-device.hpp"
#include "decklink-device-discovery.hpp"
#include "decklink-devices.hpp"

static void decklink_enable_buffering(DeckLinkInput *decklink, bool enabled)
{
	obs_source_t *source = decklink->GetSource();
	obs_source_set_async_unbuffered(source, !enabled);
	decklink->buffering = enabled;
}

static void decklink_deactivate_when_not_showing(DeckLinkInput *decklink,
						 bool dwns)
{
	decklink->dwns = dwns;
}

static void *decklink_create(obs_data_t *settings, obs_source_t *source)
{
	DeckLinkInput *decklink = new DeckLinkInput(source, deviceEnum);

	obs_source_set_async_decoupled(source, true);
	decklink_enable_buffering(decklink,
				  obs_data_get_bool(settings, BUFFERING));

	obs_source_update(source, settings);
	return decklink;
}

static void decklink_destroy(void *data)
{
	DeckLinkInput *decklink = (DeckLinkInput *)data;
	delete decklink;
}

static void decklink_update(void *data, obs_data_t *settings)
{
	DeckLinkInput *decklink = (DeckLinkInput *)data;
	const char *hash = obs_data_get_string(settings, DEVICE_HASH);
	long long id = obs_data_get_int(settings, MODE_ID);
	BMDVideoConnection videoConnection =
		(BMDVideoConnection)obs_data_get_int(settings,
						     VIDEO_CONNECTION);
	BMDAudioConnection audioConnection =
		(BMDAudioConnection)obs_data_get_int(settings,
						     AUDIO_CONNECTION);
	BMDPixelFormat pixelFormat =
		(BMDPixelFormat)obs_data_get_int(settings, PIXEL_FORMAT);
	video_colorspace colorSpace =
		(video_colorspace)obs_data_get_int(settings, COLOR_SPACE);
	video_range_type colorRange =
		(video_range_type)obs_data_get_int(settings, COLOR_RANGE);
	int chFmtInt = (int)obs_data_get_int(settings, CHANNEL_FORMAT);

	if (chFmtInt == 7)
		chFmtInt = SPEAKERS_5POINT1;
	else if (chFmtInt < SPEAKERS_UNKNOWN || chFmtInt > SPEAKERS_7POINT1)
		chFmtInt = 2;

	speaker_layout channelFormat = (speaker_layout)chFmtInt;

	decklink_enable_buffering(decklink,
				  obs_data_get_bool(settings, BUFFERING));

	decklink_deactivate_when_not_showing(
		decklink, obs_data_get_bool(settings, DEACTIVATE_WNS));

	ComPtr<DeckLinkDevice> device;
	device.Set(deviceEnum->FindByHash(hash));

	decklink->SetPixelFormat(pixelFormat);
	decklink->SetColorSpace(colorSpace);
	decklink->SetColorRange(colorRange);
	decklink->SetChannelFormat(channelFormat);
	decklink->hash = std::string(hash);
	decklink->swap = obs_data_get_bool(settings, SWAP);
	decklink->allow10Bit = obs_data_get_bool(settings, ALLOW_10_BIT);
	decklink->Activate(device, id, videoConnection, audioConnection);
}

static void decklink_show(void *data)
{
	DeckLinkInput *decklink = (DeckLinkInput *)data;
	obs_source_t *source = decklink->GetSource();
	bool showing = obs_source_showing(source);
	if (decklink->dwns && showing && !decklink->Capturing()) {
		ComPtr<DeckLinkDevice> device;
		device.Set(deviceEnum->FindByHash(decklink->hash.c_str()));
		decklink->Activate(device, decklink->id,
				   decklink->videoConnection,
				   decklink->audioConnection);
	}
}
static void decklink_hide(void *data)
{
	DeckLinkInput *decklink = (DeckLinkInput *)data;
	obs_source_t *source = decklink->GetSource();
	bool showing = obs_source_showing(source);
	if (decklink->dwns && showing)
		decklink->Deactivate();
}

static void decklink_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, BUFFERING, false);
	obs_data_set_default_int(settings, PIXEL_FORMAT, bmdFormat8BitYUV);
	obs_data_set_default_int(settings, COLOR_SPACE, VIDEO_CS_DEFAULT);
	obs_data_set_default_int(settings, COLOR_RANGE, VIDEO_RANGE_DEFAULT);
	obs_data_set_default_int(settings, CHANNEL_FORMAT, SPEAKERS_STEREO);
	obs_data_set_default_bool(settings, SWAP, false);
}

static const char *decklink_get_name(void *)
{
	return obs_module_text("BlackmagicDevice");
}

static bool decklink_device_changed(obs_properties_t *props,
				    obs_property_t *list, obs_data_t *settings)
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

	obs_property_t *videoConnectionList =
		obs_properties_get(props, VIDEO_CONNECTION);
	obs_property_t *audioConnectionList =
		obs_properties_get(props, AUDIO_CONNECTION);
	obs_property_t *modeList = obs_properties_get(props, MODE_ID);
	obs_property_t *channelList = obs_properties_get(props, CHANNEL_FORMAT);

	obs_property_list_clear(videoConnectionList);
	obs_property_list_clear(audioConnectionList);

	obs_property_list_clear(modeList);

	obs_property_list_clear(channelList);
	obs_property_list_add_int(channelList, TEXT_CHANNEL_FORMAT_NONE,
				  SPEAKERS_UNKNOWN);
	obs_property_list_add_int(channelList, TEXT_CHANNEL_FORMAT_2_0CH,
				  SPEAKERS_STEREO);

	ComPtr<DeckLinkDevice> device;
	device.Set(deviceEnum->FindByHash(hash));

	if (!device) {
		obs_property_list_item_disable(videoConnectionList, 0, true);
		obs_property_list_item_disable(audioConnectionList, 0, true);
		obs_property_list_add_int(modeList, mode, modeId);
		obs_property_list_item_disable(modeList, 0, true);
	} else {
		const BMDVideoConnection BMDVideoConnections[] = {
			bmdVideoConnectionSDI,
			bmdVideoConnectionHDMI,
			bmdVideoConnectionOpticalSDI,
			bmdVideoConnectionComponent,
			bmdVideoConnectionComposite,
			bmdVideoConnectionSVideo};

		for (BMDVideoConnection conn : BMDVideoConnections) {
			if ((device->GetVideoInputConnections() & conn) ==
			    conn) {
				obs_property_list_add_int(
					videoConnectionList,
					bmd_video_connection_to_name(conn),
					conn);
			}
		}

		const BMDAudioConnection BMDAudioConnections[] = {
			bmdAudioConnectionEmbedded,
			bmdAudioConnectionAESEBU,
			bmdAudioConnectionAnalog,
			bmdAudioConnectionAnalogXLR,
			bmdAudioConnectionAnalogRCA,
			bmdAudioConnectionMicrophone,
			bmdAudioConnectionHeadphones};

		for (BMDAudioConnection conn : BMDAudioConnections) {
			if ((device->GetAudioInputConnections() & conn) ==
			    conn) {
				obs_property_list_add_int(
					audioConnectionList,
					bmd_audio_connection_to_name(conn),
					conn);
			}
		}

		const std::vector<DeckLinkDeviceMode *> &modes =
			device->GetInputModes();

		for (DeckLinkDeviceMode *mode : modes) {
			obs_property_list_add_int(modeList,
						  mode->GetName().c_str(),
						  mode->GetId());
		}

		if (device->GetMaxChannel() >= 8) {
			obs_property_list_add_int(channelList,
						  TEXT_CHANNEL_FORMAT_2_1CH,
						  SPEAKERS_2POINT1);
			obs_property_list_add_int(channelList,
						  TEXT_CHANNEL_FORMAT_4_0CH,
						  SPEAKERS_4POINT0);
			obs_property_list_add_int(channelList,
						  TEXT_CHANNEL_FORMAT_4_1CH,
						  SPEAKERS_4POINT1);
			obs_property_list_add_int(channelList,
						  TEXT_CHANNEL_FORMAT_5_1CH,
						  SPEAKERS_5POINT1);
			obs_property_list_add_int(channelList,
						  TEXT_CHANNEL_FORMAT_7_1CH,
						  SPEAKERS_7POINT1);
		}
	}

	return true;
}

static bool mode_id_changed(obs_properties_t *props, obs_property_t *list,
			    obs_data_t *settings)
{
	long long id = obs_data_get_int(settings, MODE_ID);

	list = obs_properties_get(props, PIXEL_FORMAT);
	obs_property_set_visible(list, id != MODE_ID_AUTO);

	auto allow10BitProp = obs_properties_get(props, ALLOW_10_BIT);
	obs_property_set_visible(allow10BitProp, id == MODE_ID_AUTO);

	return true;
}

static obs_properties_t *decklink_get_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *list = obs_properties_add_list(props, DEVICE_HASH,
						       TEXT_DEVICE,
						       OBS_COMBO_TYPE_LIST,
						       OBS_COMBO_FORMAT_STRING);
	obs_property_set_modified_callback(list, decklink_device_changed);

	fill_out_devices(list);

	obs_properties_add_list(props, VIDEO_CONNECTION, TEXT_VIDEO_CONNECTION,
				OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_properties_add_list(props, AUDIO_CONNECTION, TEXT_AUDIO_CONNECTION,
				OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	list = obs_properties_add_list(props, MODE_ID, TEXT_MODE,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_INT);
	obs_property_set_modified_callback(list, mode_id_changed);

	list = obs_properties_add_list(props, PIXEL_FORMAT, TEXT_PIXEL_FORMAT,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(list, "8-bit YUV", bmdFormat8BitYUV);
	obs_property_list_add_int(list, "10-bit YUV", bmdFormat10BitYUV);
	obs_property_list_add_int(list, "8-bit BGRA", bmdFormat8BitBGRA);

	list = obs_properties_add_list(props, COLOR_SPACE, TEXT_COLOR_SPACE,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(list, TEXT_COLOR_SPACE_DEFAULT,
				  VIDEO_CS_DEFAULT);
	obs_property_list_add_int(list, "BT.601", VIDEO_CS_601);
	obs_property_list_add_int(list, "BT.709", VIDEO_CS_709);

	list = obs_properties_add_list(props, COLOR_RANGE, TEXT_COLOR_RANGE,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(list, TEXT_COLOR_RANGE_DEFAULT,
				  VIDEO_RANGE_DEFAULT);
	obs_property_list_add_int(list, TEXT_COLOR_RANGE_PARTIAL,
				  VIDEO_RANGE_PARTIAL);
	obs_property_list_add_int(list, TEXT_COLOR_RANGE_FULL,
				  VIDEO_RANGE_FULL);

	list = obs_properties_add_list(props, CHANNEL_FORMAT,
				       TEXT_CHANNEL_FORMAT, OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(list, TEXT_CHANNEL_FORMAT_NONE,
				  SPEAKERS_UNKNOWN);
	obs_property_list_add_int(list, TEXT_CHANNEL_FORMAT_2_0CH,
				  SPEAKERS_STEREO);
	obs_property_list_add_int(list, TEXT_CHANNEL_FORMAT_2_1CH,
				  SPEAKERS_2POINT1);
	obs_property_list_add_int(list, TEXT_CHANNEL_FORMAT_4_0CH,
				  SPEAKERS_4POINT0);
	obs_property_list_add_int(list, TEXT_CHANNEL_FORMAT_4_1CH,
				  SPEAKERS_4POINT1);
	obs_property_list_add_int(list, TEXT_CHANNEL_FORMAT_5_1CH,
				  SPEAKERS_5POINT1);
	obs_property_list_add_int(list, TEXT_CHANNEL_FORMAT_7_1CH,
				  SPEAKERS_7POINT1);

	obs_property_t *swap = obs_properties_add_bool(props, SWAP, TEXT_SWAP);
	obs_property_set_long_description(swap, TEXT_SWAP_TOOLTIP);

	obs_properties_add_bool(props, BUFFERING, TEXT_BUFFERING);

	obs_properties_add_bool(props, DEACTIVATE_WNS, TEXT_DWNS);

	obs_properties_add_bool(props, ALLOW_10_BIT, TEXT_ALLOW_10_BIT);

	UNUSED_PARAMETER(data);
	return props;
}

struct obs_source_info create_decklink_source_info()
{
	struct obs_source_info decklink_source_info = {};
	decklink_source_info.id = "decklink-input";
	decklink_source_info.type = OBS_SOURCE_TYPE_INPUT;
	decklink_source_info.output_flags =
		OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO |
		OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_CEA_708;
	decklink_source_info.create = decklink_create;
	decklink_source_info.destroy = decklink_destroy;
	decklink_source_info.get_defaults = decklink_get_defaults;
	decklink_source_info.get_name = decklink_get_name;
	decklink_source_info.get_properties = decklink_get_properties;
	decklink_source_info.update = decklink_update;
	decklink_source_info.show = decklink_show;
	decklink_source_info.hide = decklink_hide;
	decklink_source_info.icon_type = OBS_ICON_TYPE_CAMERA;

	return decklink_source_info;
}
