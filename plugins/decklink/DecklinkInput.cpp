#include "DecklinkInput.hpp"

#include <util/threading.h>

DeckLinkInput::DeckLinkInput(obs_source_t *source,
			     DeckLinkDeviceDiscovery *discovery_)
	: DecklinkBase(discovery_), source(source)
{
	discovery->AddCallback(DeckLinkInput::DevicesChanged, this);
}

DeckLinkInput::~DeckLinkInput(void)
{
	discovery->RemoveCallback(DeckLinkInput::DevicesChanged, this);
	Deactivate();
}

void DeckLinkInput::DevicesChanged(void *param, DeckLinkDevice *device,
				   bool added)
{
	DeckLinkInput *decklink = reinterpret_cast<DeckLinkInput *>(param);
	std::lock_guard<std::recursive_mutex> lock(decklink->deviceMutex);

	obs_source_update_properties(decklink->source);

	if (added && !decklink->instance) {
		const char *hash;
		long long mode;
		obs_data_t *settings;
		BMDVideoConnection videoConnection;
		BMDAudioConnection audioConnection;

		settings = obs_source_get_settings(decklink->source);
		hash = obs_data_get_string(settings, "device_hash");
		videoConnection = (BMDVideoConnection)obs_data_get_int(
			settings, "video_connection");
		audioConnection = (BMDAudioConnection)obs_data_get_int(
			settings, "audio_connection");
		mode = obs_data_get_int(settings, "mode_id");
		obs_data_release(settings);

		if (device->GetHash().compare(hash) == 0) {
			if (!decklink->activateRefs)
				return;
			if (decklink->Activate(device, mode, videoConnection,
					       audioConnection))
				os_atomic_dec_long(&decklink->activateRefs);
		}

	} else if (!added && decklink->instance) {
		if (decklink->instance->GetDevice() == device) {
			os_atomic_inc_long(&decklink->activateRefs);
			decklink->Deactivate();
		}
	}
}

bool DeckLinkInput::Activate(DeckLinkDevice *device, long long modeId,
			     BMDVideoConnection bmdVideoConnection,
			     BMDAudioConnection bmdAudioConnection)
{
	std::lock_guard<std::recursive_mutex> lock(deviceMutex);
	DeckLinkDevice *curDevice = GetDevice();
	const bool same = device == curDevice;
	const bool isActive = instance != nullptr;

	if (same) {
		if (!isActive)
			return false;
		if (instance->GetActiveModeId() == modeId &&
		    instance->GetVideoConnection() == bmdVideoConnection &&
		    instance->GetAudioConnection() == bmdAudioConnection &&
		    instance->GetActivePixelFormat() == pixelFormat &&
		    instance->GetActiveColorSpace() == colorSpace &&
		    instance->GetActiveColorRange() == colorRange &&
		    instance->GetActiveChannelFormat() == channelFormat &&
		    instance->GetActiveSwapState() == swap)
			return false;
	}

	if (isActive)
		instance->StopCapture();

	isCapturing = false;
	if (!same)
		instance.Set(new DeckLinkDeviceInstance(this, device));

	if (instance == nullptr)
		return false;

	if (GetDevice() == nullptr) {
		LOG(LOG_ERROR,
		    "Tried to activate an input with nullptr device.");
		return false;
	}

	DeckLinkDeviceMode *mode = GetDevice()->FindInputMode(modeId);
	if (mode == nullptr) {
		instance = nullptr;
		return false;
	}

	if (!instance->StartCapture(mode, allow10Bit, bmdVideoConnection,
				    bmdAudioConnection)) {
		instance = nullptr;
		return false;
	}

	os_atomic_inc_long(&activateRefs);
	SaveSettings();
	id = modeId;
	isCapturing = true;
	return true;
}

void DeckLinkInput::Deactivate(void)
{
	std::lock_guard<std::recursive_mutex> lock(deviceMutex);
	if (instance)
		instance->StopCapture();
	isCapturing = false;
	instance = nullptr;

	os_atomic_dec_long(&activateRefs);
}

bool DeckLinkInput::Capturing(void)
{
	return isCapturing;
}

void DeckLinkInput::SaveSettings()
{
	if (!instance)
		return;

	DeckLinkDevice *device = instance->GetDevice();
	DeckLinkDeviceMode *mode = instance->GetMode();

	obs_data_t *settings = obs_source_get_settings(source);

	obs_data_set_string(settings, "device_hash", device->GetHash().c_str());
	obs_data_set_string(settings, "device_name",
			    device->GetDisplayName().c_str());
	obs_data_set_int(settings, "mode_id", instance->GetActiveModeId());
	obs_data_set_string(settings, "mode_name", mode->GetName().c_str());

	obs_data_release(settings);
}

obs_source_t *DeckLinkInput::GetSource(void) const
{
	return source;
}
