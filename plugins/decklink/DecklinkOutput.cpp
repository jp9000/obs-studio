#include "DecklinkOutput.hpp"

#include <util/threading.h>

DeckLinkOutput::DeckLinkOutput(obs_output_t *output,
			       DeckLinkDeviceDiscovery *discovery_)
	: DecklinkBase(discovery_), output(output)
{
	discovery->AddCallback(DeckLinkOutput::DevicesChanged, this);
}

DeckLinkOutput::~DeckLinkOutput(void)
{
	discovery->RemoveCallback(DeckLinkOutput::DevicesChanged, this);
	Deactivate();
}

void DeckLinkOutput::DevicesChanged(void *param, DeckLinkDevice *device, bool)
{
	auto *decklink = reinterpret_cast<DeckLinkOutput *>(param);
	std::lock_guard<std::recursive_mutex> lock(decklink->deviceMutex);

	blog(LOG_DEBUG, "%s", device->GetHash().c_str());
}

bool DeckLinkOutput::Activate(DeckLinkDevice *device, long long modeId)
{
	std::lock_guard<std::recursive_mutex> lock(deviceMutex);
	DeckLinkDevice *curDevice = GetDevice();
	const bool same = device == curDevice;
	const bool isActive = instance != nullptr;

	if (same) {
		if (!isActive)
			return false;

		if (instance->GetActiveModeId() == modeId &&
		    instance->GetActivePixelFormat() == pixelFormat &&
		    instance->GetActiveColorSpace() == colorSpace &&
		    instance->GetActiveColorRange() == colorRange &&
		    instance->GetActiveChannelFormat() == channelFormat)
			return false;
	}

	if (isActive)
		instance->StopOutput();

	if (!same)
		instance.Set(new DeckLinkDeviceInstance(this, device));

	if (instance == nullptr)
		return false;

	DeckLinkDeviceMode *mode = GetDevice()->FindOutputMode(modeId);
	if (mode == nullptr) {
		instance = nullptr;
		return false;
	}

	if (!instance->StartOutput(mode)) {
		instance = nullptr;
		return false;
	}

	os_atomic_inc_long(&activateRefs);
	return true;
}

void DeckLinkOutput::Deactivate(void)
{
	std::lock_guard<std::recursive_mutex> lock(deviceMutex);
	if (instance)
		instance->StopOutput();

	instance = nullptr;

	os_atomic_dec_long(&activateRefs);
}

obs_output_t *DeckLinkOutput::GetOutput(void) const
{
	return output;
}

void DeckLinkOutput::DisplayVideoFrame(video_data *frame)
{
	instance->DisplayVideoFrame(frame);
}

void DeckLinkOutput::WriteAudio(audio_data *frames)
{
	instance->WriteAudio(frames);
}

void DeckLinkOutput::SetSize(int width, int height)
{
	this->width = width;
	this->height = height;
}

int DeckLinkOutput::GetWidth()
{
	return width;
}

int DeckLinkOutput::GetHeight()
{
	return height;
}
