#include "decklink-devices.hpp"

DeckLinkDeviceDiscovery *deviceEnum = nullptr;

void fill_out_devices(obs_property_t *list)
{
	deviceEnum->Lock();

	const std::vector<DeckLinkDevice *> &devices = deviceEnum->GetDevices();
	for (DeckLinkDevice *device : devices) {
		obs_property_list_add_string(list,
					     device->GetDisplayName().c_str(),
					     device->GetHash().c_str());
	}

	deviceEnum->Unlock();
}
