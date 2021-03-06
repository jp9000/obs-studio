#pragma once

#include "decklink-device-mode.hpp"

#include <map>
#include <string>
#include <vector>
#include <stdint.h>

class DeckLinkDevice {
	ComPtr<IDeckLink> device;
	std::map<long long, DeckLinkDeviceMode *> inputModeIdMap;
	std::vector<DeckLinkDeviceMode *> inputModes;
	std::map<long long, DeckLinkDeviceMode *> outputModeIdMap;
	std::vector<DeckLinkDeviceMode *> outputModes;
	std::string name;
	std::string displayName;
	std::string hash;
	int32_t maxChannel = 0;
	decklink_bool_t supportsExternalKeyer = false;
	decklink_bool_t supportsInternalKeyer = false;
	int64_t subDeviceIndex = 0;
	int64_t numSubDevices = 0;
	int64_t supportedVideoInputConnections = -1;
	int64_t supportedVideoOutputConnections = -1;
	int64_t supportedAudioInputConnections = -1;
	int64_t supportedAudioOutputConnections = -1;
	int keyerMode = 0;
	volatile long refCount = 1;

public:
	DeckLinkDevice(IDeckLink *device);
	~DeckLinkDevice(void);

	ULONG AddRef(void);
	ULONG Release(void);

	bool Init();

	DeckLinkDeviceMode *FindInputMode(long long id);
	DeckLinkDeviceMode *FindOutputMode(long long id);
	const std::string &GetDisplayName(void);
	const std::string &GetHash(void) const;
	const std::vector<DeckLinkDeviceMode *> &GetInputModes(void) const;
	const std::vector<DeckLinkDeviceMode *> &GetOutputModes(void) const;
	int64_t GetVideoInputConnections();
	int64_t GetAudioInputConnections();
	bool GetSupportsExternalKeyer(void) const;
	bool GetSupportsInternalKeyer(void) const;
	int64_t GetSubDeviceCount();
	int64_t GetSubDeviceIndex();
	int GetKeyerMode(void);
	void SetKeyerMode(int newKeyerMode);
	const std::string &GetName(void) const;
	int32_t GetMaxChannel(void) const;

	bool GetInput(IDeckLinkInput **input);
	bool GetOutput(IDeckLinkOutput **output);
	bool GetKeyer(IDeckLinkKeyer **keyer);

	inline bool IsDevice(IDeckLink *device_) { return device_ == device; }
};
