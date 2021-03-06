#pragma once

#include <obs-module.h>
#include "platform.hpp"

#include <vector>
#include <mutex>

class DeckLinkDevice;

typedef void (*DeviceChangeCallback)(void *param, DeckLinkDevice *device,
				     bool added);

struct DeviceChangeInfo {
	DeviceChangeCallback callback;
	void *param;
};

class DeckLinkDeviceDiscovery : public IDeckLinkDeviceNotificationCallback {
protected:
	ComPtr<IDeckLinkDiscovery> discovery;
	long refCount = 1;
	bool initialized = false;

	std::recursive_mutex deviceMutex;
	std::vector<DeckLinkDevice *> devices;
	std::vector<DeviceChangeInfo> callbacks;

public:
	DeckLinkDeviceDiscovery();
	virtual ~DeckLinkDeviceDiscovery(void);

	bool Init();

	HRESULT STDMETHODCALLTYPE DeckLinkDeviceArrived(IDeckLink *device);
	HRESULT STDMETHODCALLTYPE DeckLinkDeviceRemoved(IDeckLink *device);

	inline void AddCallback(DeviceChangeCallback callback, void *param)
	{
		std::lock_guard<std::recursive_mutex> lock(deviceMutex);
		DeviceChangeInfo info;

		info.callback = callback;
		info.param = param;

		for (DeviceChangeInfo &curCB : callbacks) {
			if (curCB.callback == callback && curCB.param == param)
				return;
		}

		callbacks.push_back(info);
	}

	inline void RemoveCallback(DeviceChangeCallback callback, void *param)
	{
		std::lock_guard<std::recursive_mutex> lock(deviceMutex);

		for (size_t i = 0; i < callbacks.size(); i++) {
			DeviceChangeInfo &curCB = callbacks[i];

			if (curCB.callback == callback &&
			    curCB.param == param) {
				callbacks.erase(callbacks.begin() + i);
				return;
			}
		}
	}

	DeckLinkDevice *FindByHash(const char *hash);

	inline void Lock() { deviceMutex.lock(); }
	inline void Unlock() { deviceMutex.unlock(); }
	inline const std::vector<DeckLinkDevice *> &GetDevices() const
	{
		return devices;
	}

	ULONG STDMETHODCALLTYPE AddRef(void);
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv);
	ULONG STDMETHODCALLTYPE Release(void);
};
