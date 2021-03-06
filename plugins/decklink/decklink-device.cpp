#include <sstream>

#include "decklink-device.hpp"

#include <util/threading.h>

DeckLinkDevice::DeckLinkDevice(IDeckLink *device_) : device(device_) {}

DeckLinkDevice::~DeckLinkDevice(void)
{
	for (DeckLinkDeviceMode *mode : inputModes)
		delete mode;

	for (DeckLinkDeviceMode *mode : outputModes)
		delete mode;
}

ULONG DeckLinkDevice::AddRef()
{
	return os_atomic_inc_long(&refCount);
}

ULONG DeckLinkDevice::Release()
{
	long ret = os_atomic_dec_long(&refCount);
	if (ret == 0)
		delete this;
	return ret;
}

bool DeckLinkDevice::Init()
{
	ComPtr<IDeckLinkProfileAttributes> attributes;
	const HRESULT result = device->QueryInterface(
		IID_IDeckLinkProfileAttributes, (void **)&attributes);
	if (result == S_OK) {
		decklink_bool_t detectable = false;
		if (attributes->GetFlag(BMDDeckLinkSupportsInputFormatDetection,
					&detectable) == S_OK &&
		    !!detectable) {
			DeckLinkDeviceMode *mode =
				new DeckLinkDeviceMode("Auto", MODE_ID_AUTO);
			inputModes.push_back(mode);
			inputModeIdMap[MODE_ID_AUTO] = mode;
		}
	}

	// Find input modes
	ComPtr<IDeckLinkInput> input;
	if (device->QueryInterface(IID_IDeckLinkInput, (void **)&input) ==
	    S_OK) {
		IDeckLinkDisplayModeIterator *modeIterator;
		if (input->GetDisplayModeIterator(&modeIterator) == S_OK) {
			IDeckLinkDisplayMode *displayMode;
			long long modeId = 1;

			while (modeIterator->Next(&displayMode) == S_OK) {
				if (displayMode == nullptr)
					continue;

				DeckLinkDeviceMode *mode =
					new DeckLinkDeviceMode(displayMode,
							       modeId);
				inputModes.push_back(mode);
				inputModeIdMap[modeId] = mode;
				displayMode->Release();
				++modeId;
			}

			modeIterator->Release();
		}
	}

	// Get supported video connections
	attributes->GetInt(BMDDeckLinkVideoInputConnections,
			   &supportedVideoInputConnections);
	attributes->GetInt(BMDDeckLinkVideoOutputConnections,
			   &supportedVideoOutputConnections);

	// Get supported audio connections
	attributes->GetInt(BMDDeckLinkAudioInputConnections,
			   &supportedAudioInputConnections);
	attributes->GetInt(BMDDeckLinkAudioOutputConnections,
			   &supportedAudioOutputConnections);

	// find output modes
	ComPtr<IDeckLinkOutput> output;
	if (device->QueryInterface(IID_IDeckLinkOutput, (void **)&output) ==
	    S_OK) {

		IDeckLinkDisplayModeIterator *modeIterator;
		if (output->GetDisplayModeIterator(&modeIterator) == S_OK) {
			IDeckLinkDisplayMode *displayMode;
			long long modeId = 1;

			while (modeIterator->Next(&displayMode) == S_OK) {
				if (displayMode == nullptr)
					continue;

				DeckLinkDeviceMode *mode =
					new DeckLinkDeviceMode(displayMode,
							       modeId);
				outputModes.push_back(mode);
				outputModeIdMap[modeId] = mode;
				displayMode->Release();
				++modeId;
			}

			modeIterator->Release();
		}
	}

	// get keyer support
	attributes->GetFlag(BMDDeckLinkSupportsExternalKeying,
			    &supportsExternalKeyer);
	attributes->GetFlag(BMDDeckLinkSupportsInternalKeying,
			    &supportsInternalKeyer);

	// Sub Device Counts
	attributes->GetInt(BMDDeckLinkSubDeviceIndex, &subDeviceIndex);
	attributes->GetInt(BMDDeckLinkNumberOfSubDevices, &numSubDevices);

	decklink_string_t decklinkModelName;
	decklink_string_t decklinkDisplayName;

	if (device->GetModelName(&decklinkModelName) != S_OK)
		return false;
	DeckLinkStringToStdString(decklinkModelName, name);

	if (device->GetDisplayName(&decklinkDisplayName) != S_OK)
		return false;
	DeckLinkStringToStdString(decklinkDisplayName, displayName);

	hash = displayName;

	if (result != S_OK)
		return true;

	int64_t channels;
	/* Intensity Shuttle for Thunderbolt return 2; however, it supports 8 channels */
	if (name == "Intensity Shuttle Thunderbolt")
		maxChannel = 8;
	else if (attributes->GetInt(BMDDeckLinkMaximumAudioChannels,
				    &channels) == S_OK)
		maxChannel = (int32_t)channels;
	else
		maxChannel = 2;

	/* http://forum.blackmagicdesign.com/viewtopic.php?f=12&t=33967
	 * BMDDeckLinkTopologicalID for older devices
	 * BMDDeckLinkPersistentID for newer ones */

	int64_t value;
	if (attributes->GetInt(BMDDeckLinkPersistentID, &value) != S_OK &&
	    attributes->GetInt(BMDDeckLinkTopologicalID, &value) != S_OK)
		return true;

	std::ostringstream os;
	os << value << "_" << name;
	hash = os.str();
	return true;
}

bool DeckLinkDevice::GetInput(IDeckLinkInput **input)
{
	if (device->QueryInterface(IID_IDeckLinkInput, (void **)input) != S_OK)
		return false;
	return true;
}

bool DeckLinkDevice::GetOutput(IDeckLinkOutput **output)
{
	if (device->QueryInterface(IID_IDeckLinkOutput, (void **)output) !=
	    S_OK)
		return false;

	return true;
}

bool DeckLinkDevice::GetKeyer(IDeckLinkKeyer **deckLinkKeyer)
{
	if (device->QueryInterface(IID_IDeckLinkKeyer,
				   (void **)deckLinkKeyer) != S_OK) {
		fprintf(stderr,
			"Could not obtain the IDeckLinkKeyer interface\n");
		return false;
	}

	return true;
}

void DeckLinkDevice::SetKeyerMode(int newKeyerMode)
{
	keyerMode = newKeyerMode;
}

int DeckLinkDevice::GetKeyerMode(void)
{
	return keyerMode;
}

DeckLinkDeviceMode *DeckLinkDevice::FindInputMode(long long id)
{
	return inputModeIdMap[id];
}

DeckLinkDeviceMode *DeckLinkDevice::FindOutputMode(long long id)
{
	return outputModeIdMap[id];
}

const std::string &DeckLinkDevice::GetDisplayName(void)
{
	return displayName;
}

const std::string &DeckLinkDevice::GetHash(void) const
{
	return hash;
}

const std::vector<DeckLinkDeviceMode *> &
DeckLinkDevice::GetInputModes(void) const
{
	return inputModes;
}

const std::vector<DeckLinkDeviceMode *> &
DeckLinkDevice::GetOutputModes(void) const
{
	return outputModes;
}

int64_t DeckLinkDevice::GetVideoInputConnections()
{
	return supportedVideoInputConnections;
}

int64_t DeckLinkDevice::GetAudioInputConnections()
{
	return supportedAudioInputConnections;
}

bool DeckLinkDevice::GetSupportsExternalKeyer(void) const
{
	return supportsExternalKeyer;
}

bool DeckLinkDevice::GetSupportsInternalKeyer(void) const
{
	return supportsInternalKeyer;
}

int64_t DeckLinkDevice::GetSubDeviceCount()
{
	return numSubDevices;
}

int64_t DeckLinkDevice::GetSubDeviceIndex()
{
	return subDeviceIndex;
}

const std::string &DeckLinkDevice::GetName(void) const
{
	return name;
}

int32_t DeckLinkDevice::GetMaxChannel(void) const
{
	return maxChannel;
}
