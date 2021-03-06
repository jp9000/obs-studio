#include "../../obs-internal.h"

#include "wasapi-output.h"

#include <propsys.h>

#ifdef __MINGW32__

#ifdef DEFINE_PROPERTYKEY
#undef DEFINE_PROPERTYKEY
#endif
#define DEFINE_PROPERTYKEY(id, a, b, c, d, e, f, g, h, i, j, k, l) \
	const PROPERTYKEY id = {{a,                                \
				 b,                                \
				 c,                                \
				 {                                 \
					 d,                        \
					 e,                        \
					 f,                        \
					 g,                        \
					 h,                        \
					 i,                        \
					 j,                        \
					 k,                        \
				 }},                               \
				l};
DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName, 0xa45c254e, 0xdf1c, 0x4efd, 0x80,
		   0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);

#else

#include <functiondiscoverykeys_devpkey.h>

#endif

static bool get_device_info(obs_enum_audio_device_cb cb, void *data,
			    IMMDeviceCollection *collection, UINT idx)
{
	IPropertyStore *store = NULL;
	IMMDevice *device = NULL;
	PROPVARIANT name_var;
	char utf8_name[512];
	WCHAR *w_id = NULL;
	char utf8_id[512];
	bool cont = true;
	HRESULT hr;

	hr = collection->lpVtbl->Item(collection, idx, &device);
	if (FAILED(hr)) {
		goto fail;
	}

	hr = device->lpVtbl->GetId(device, &w_id);
	if (FAILED(hr)) {
		goto fail;
	}

	hr = device->lpVtbl->OpenPropertyStore(device, STGM_READ, &store);
	if (FAILED(hr)) {
		goto fail;
	}

	PropVariantInit(&name_var);
	hr = store->lpVtbl->GetValue(store, &PKEY_Device_FriendlyName,
				     &name_var);
	if (FAILED(hr)) {
		goto fail;
	}

	os_wcs_to_utf8(w_id, 0, utf8_id, 512);
	os_wcs_to_utf8(name_var.pwszVal, 0, utf8_name, 512);

	cont = cb(data, utf8_name, utf8_id);

fail:
	safe_release(store);
	safe_release(device);
	if (w_id)
		CoTaskMemFree(w_id);
	return cont;
}

void obs_enum_audio_monitoring_devices(obs_enum_audio_device_cb cb, void *data)
{
	IMMDeviceEnumerator *enumerator = NULL;
	IMMDeviceCollection *collection = NULL;
	UINT count;
	HRESULT hr;

	hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
			      &IID_IMMDeviceEnumerator, &enumerator);
	if (FAILED(hr)) {
		goto fail;
	}

	hr = enumerator->lpVtbl->EnumAudioEndpoints(
		enumerator, eRender, DEVICE_STATE_ACTIVE, &collection);
	if (FAILED(hr)) {
		goto fail;
	}

	hr = collection->lpVtbl->GetCount(collection, &count);
	if (FAILED(hr)) {
		goto fail;
	}

	for (UINT i = 0; i < count; i++) {
		if (!get_device_info(cb, data, collection, i)) {
			break;
		}
	}

fail:
	safe_release(enumerator);
	safe_release(collection);
}

static void get_default_id(char **p_id)
{
	IMMDeviceEnumerator *immde = NULL;
	IMMDevice *device = NULL;
	WCHAR *w_id = NULL;
	HRESULT hr;

	if (*p_id)
		return;

	hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
			      &IID_IMMDeviceEnumerator, &immde);
	if (FAILED(hr)) {
		goto fail;
	}

	hr = immde->lpVtbl->GetDefaultAudioEndpoint(immde, eRender, eConsole,
						    &device);
	if (FAILED(hr)) {
		goto fail;
	}

	hr = device->lpVtbl->GetId(device, &w_id);
	if (FAILED(hr)) {
		goto fail;
	}

	os_wcs_to_utf8_ptr(w_id, 0, p_id);

fail:
	if (!*p_id)
		*p_id = bzalloc(1);
	if (immde)
		immde->lpVtbl->Release(immde);
	if (device)
		device->lpVtbl->Release(device);
	if (w_id)
		CoTaskMemFree(w_id);
}

bool devices_match(const char *id1, const char *id2)
{
	char *default_id = NULL;
	bool match;

	if (!id1 || !id2)
		return false;

	if (strcmp(id1, "default") == 0) {
		get_default_id(&default_id);
		id1 = default_id;
	}
	if (strcmp(id2, "default") == 0) {
		get_default_id(&default_id);
		id2 = default_id;
	}

	match = strcmp(id1, id2) == 0;
	bfree(default_id);

	return match;
}
