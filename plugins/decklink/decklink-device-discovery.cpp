#include "decklink-device-discovery.hpp"
#include "decklink-device.hpp"

#include <util/threading.h>

DeckLinkDeviceDiscovery::DeckLinkDeviceDiscovery()
{
	discovery = CreateDeckLinkDiscoveryInstance();
	if (discovery == nullptr)
		blog(LOG_INFO, "No blackmagic support");
}

DeckLinkDeviceDiscovery::~DeckLinkDeviceDiscovery(void)
{
	if (discovery != nullptr) {
		if (initialized)
			discovery->UninstallDeviceNotifications();
		for (DeckLinkDevice *device : devices)
			device->Release();
	}
}

bool DeckLinkDeviceDiscovery::Init(void)
{
	HRESULT result = E_FAIL;

	if (initialized)
		return false;

	if (discovery != nullptr)
		result = discovery->InstallDeviceNotifications(this);

	initialized = result == S_OK;
	if (!initialized)
		blog(LOG_DEBUG, "Failed to start search for DeckLink devices");

	return initialized;
}

DeckLinkDevice *DeckLinkDeviceDiscovery::FindByHash(const char *hash)
{
	DeckLinkDevice *ret = nullptr;

	deviceMutex.lock();
	for (DeckLinkDevice *device : devices) {
		if (device->GetHash().compare(hash) == 0) {
			ret = device;
			ret->AddRef();
			break;
		}
	}
	deviceMutex.unlock();

	return ret;
}

HRESULT STDMETHODCALLTYPE
DeckLinkDeviceDiscovery::DeckLinkDeviceArrived(IDeckLink *device)
{
	DeckLinkDevice *newDev = new DeckLinkDevice(device);
	if (!newDev->Init()) {
		delete newDev;
		return S_OK;
	}

	std::lock_guard<std::recursive_mutex> lock(deviceMutex);

	devices.push_back(newDev);

	for (DeviceChangeInfo &cb : callbacks)
		cb.callback(cb.param, newDev, true);

	return S_OK;
}

HRESULT STDMETHODCALLTYPE
DeckLinkDeviceDiscovery::DeckLinkDeviceRemoved(IDeckLink *device)
{
	std::lock_guard<std::recursive_mutex> lock(deviceMutex);

	for (size_t i = 0; i < devices.size(); i++) {
		if (devices[i]->IsDevice(device)) {

			for (DeviceChangeInfo &cb : callbacks)
				cb.callback(cb.param, devices[i], false);

			devices[i]->Release();
			devices.erase(devices.begin() + i);
			break;
		}
	}
	return S_OK;
}

ULONG STDMETHODCALLTYPE DeckLinkDeviceDiscovery::AddRef(void)
{
	return os_atomic_inc_long(&refCount);
}

HRESULT STDMETHODCALLTYPE DeckLinkDeviceDiscovery::QueryInterface(REFIID iid,
								  LPVOID *ppv)
{
	HRESULT result = E_NOINTERFACE;

	*ppv = nullptr;

	CFUUIDBytes unknown = CFUUIDGetUUIDBytes(IUnknownUUID);
	if (memcmp(&iid, &unknown, sizeof(REFIID)) == 0) {
		*ppv = this;
		AddRef();
		result = S_OK;
	} else if (memcmp(&iid, &IID_IDeckLinkDeviceNotificationCallback,
			  sizeof(REFIID)) == 0) {
		*ppv = (IDeckLinkDeviceNotificationCallback *)this;
		AddRef();
		result = S_OK;
	}

	return result;
}

ULONG STDMETHODCALLTYPE DeckLinkDeviceDiscovery::Release(void)
{
	const long newRefCount = os_atomic_dec_long(&refCount);
	if (newRefCount == 0) {
		delete this;
		return 0;
	}

	return newRefCount;
}
