#pragma once

#include <map>
#include <vector>
#include <mutex>

#include <obs-module.h>

#include "platform.hpp"

#include "decklink-device-discovery.hpp"
#include "decklink-device-instance.hpp"
#include "decklink-device-mode.hpp"

class DecklinkBase {

protected:
	DecklinkBase(DeckLinkDeviceDiscovery *discovery_);
	ComPtr<DeckLinkDeviceInstance> instance;
	DeckLinkDeviceDiscovery *discovery;
	std::recursive_mutex deviceMutex;
	volatile long activateRefs = 0;
	BMDPixelFormat pixelFormat = bmdFormat8BitYUV;
	video_colorspace colorSpace = VIDEO_CS_DEFAULT;
	video_range_type colorRange = VIDEO_RANGE_DEFAULT;
	speaker_layout channelFormat = SPEAKERS_STEREO;

public:
	virtual bool Activate(DeckLinkDevice *device, long long modeId);
	virtual bool Activate(DeckLinkDevice *device, long long modeId,
			      BMDVideoConnection bmdVideoConnection,
			      BMDAudioConnection bmdAudioConnection);
	virtual void Deactivate() = 0;

	DeckLinkDevice *GetDevice() const;
};
