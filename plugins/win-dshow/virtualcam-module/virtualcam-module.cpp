#include "virtualcam-filter.hpp"
#include "virtualcam-guid.h"

/* ========================================================================= */

static const REGPINTYPES AMSMediaTypesV = {&MEDIATYPE_Video,
					   &MEDIASUBTYPE_NV12};

static const REGFILTERPINS AMSPinVideo = {L"Output", false, true,
					  false,     false, &CLSID_NULL,
					  nullptr,   1,     &AMSMediaTypesV};

HINSTANCE dll_inst = nullptr;
static volatile long locks = 0;

/* ========================================================================= */

class VCamFactory : public IClassFactory {
	volatile long refs = 1;
	CLSID cls;

public:
	inline VCamFactory(CLSID cls_) : cls(cls_) {}

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID riid, void **p_ptr);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	// IClassFactory
	STDMETHODIMP CreateInstance(LPUNKNOWN parent, REFIID riid,
				    void **p_ptr);
	STDMETHODIMP LockServer(BOOL lock);
};

STDMETHODIMP VCamFactory::QueryInterface(REFIID riid, void **p_ptr)
{
	if (!p_ptr) {
		return E_POINTER;
	}

	if ((riid == IID_IUnknown) || (riid == IID_IClassFactory)) {
		AddRef();
		*p_ptr = (void *)this;
		return S_OK;
	} else {
		*p_ptr = nullptr;
		return E_NOINTERFACE;
	}
}

STDMETHODIMP_(ULONG) VCamFactory::AddRef()
{
	return InterlockedIncrement(&refs);
}

STDMETHODIMP_(ULONG) VCamFactory::Release()
{
	long new_refs = InterlockedDecrement(&refs);
	if (new_refs == 0) {
		delete this;
		return 0;
	}

	return (ULONG)new_refs;
}

STDMETHODIMP VCamFactory::CreateInstance(LPUNKNOWN parent, REFIID, void **p_ptr)
{
	if (!p_ptr) {
		return E_POINTER;
	}

	*p_ptr = nullptr;

	/* don't bother supporting the "parent" functionality */
	if (parent) {
		return E_NOINTERFACE;
	}

	if (IsEqualCLSID(cls, CLSID_OBS_VirtualVideo)) {
		*p_ptr = (void *)new VCamFilter();
		return S_OK;
	}

	return E_NOINTERFACE;
}

STDMETHODIMP VCamFactory::LockServer(BOOL lock)
{
	if (lock) {
		InterlockedIncrement(&locks);
	} else {
		InterlockedDecrement(&locks);
	}

	return S_OK;
}

/* ========================================================================= */

static inline DWORD string_size(const wchar_t *str)
{
	return (DWORD)(wcslen(str) + 1) * sizeof(wchar_t);
}

static bool RegServer(const CLSID &cls, const wchar_t *desc,
		      const wchar_t *file, const wchar_t *model = L"Both",
		      const wchar_t *type = L"InprocServer32")
{
	wchar_t cls_str[CHARS_IN_GUID];
	wchar_t temp[MAX_PATH];
	HKEY key = nullptr;
	HKEY subkey = nullptr;
	bool success = false;

	StringFromGUID2(cls, cls_str, CHARS_IN_GUID);

	StringCbPrintf(temp, sizeof(temp), L"CLSID\\%s", cls_str);

	if (RegCreateKey(HKEY_CLASSES_ROOT, temp, &key) != ERROR_SUCCESS) {
		goto fail;
	}

	RegSetValueW(key, nullptr, REG_SZ, desc, string_size(desc));

	if (RegCreateKey(key, type, &subkey) != ERROR_SUCCESS) {
		goto fail;
	}

	RegSetValueW(subkey, nullptr, REG_SZ, file, string_size(file));
	RegSetValueExW(subkey, L"ThreadingModel", 0, REG_SZ,
		       (const BYTE *)model, string_size(model));

	success = true;

fail:
	if (key) {
		RegCloseKey(key);
	}
	if (key) {
		RegCloseKey(subkey);
	}

	return success;
}

static bool UnregServer(const CLSID &cls)
{
	wchar_t cls_str[CHARS_IN_GUID];
	wchar_t temp[MAX_PATH];

	StringFromGUID2(cls, cls_str, CHARS_IN_GUID);
	StringCbPrintf(temp, sizeof(temp), L"CLSID\\%s", cls_str);

	return RegDeleteTreeW(HKEY_CLASSES_ROOT, temp) == ERROR_SUCCESS;
}

static bool RegServers(bool reg)
{
	wchar_t file[MAX_PATH];

	if (!GetModuleFileNameW(dll_inst, file, MAX_PATH)) {
		return false;
	}

	if (reg) {
		return RegServer(CLSID_OBS_VirtualVideo, L"OBS Virtual Camera",
				 file);
	} else {
		return UnregServer(CLSID_OBS_VirtualVideo);
	}
}

static bool RegFilters(bool reg)
{
	ComPtr<IFilterMapper2> fm;
	HRESULT hr;

	hr = CoCreateInstance(CLSID_FilterMapper2, nullptr,
			      CLSCTX_INPROC_SERVER, IID_IFilterMapper2,
			      (void **)&fm);
	if (FAILED(hr)) {
		return false;
	}

	if (reg) {
		ComPtr<IMoniker> moniker;
		REGFILTER2 rf2;
		rf2.dwVersion = 1;
		rf2.dwMerit = MERIT_DO_NOT_USE;
		rf2.cPins = 1;
		rf2.rgPins = &AMSPinVideo;

		hr = fm->RegisterFilter(CLSID_OBS_VirtualVideo,
					L"OBS Virtual Camera", &moniker,
					&CLSID_VideoInputDeviceCategory,
					nullptr, &rf2);
		if (FAILED(hr)) {
			return false;
		}
	} else {
		hr = fm->UnregisterFilter(&CLSID_VideoInputDeviceCategory, 0,
					  CLSID_OBS_VirtualVideo);
		if (FAILED(hr)) {
			return false;
		}
	}

	return true;
}

/* ========================================================================= */

STDAPI DllRegisterServer()
{
	if (!RegServers(true)) {
		RegServers(false);
		return E_FAIL;
	}

	CoInitialize(0);

	if (!RegFilters(true)) {
		RegFilters(false);
		RegServers(false);
		CoUninitialize();
		return E_FAIL;
	}

	CoUninitialize();
	return S_OK;
}

STDAPI DllUnregisterServer()
{
	CoInitialize(0);
	RegFilters(false);
	RegServers(false);
	CoUninitialize();
	return S_OK;
}

STDAPI DllInstall(BOOL install, LPCWSTR)
{
	if (!install) {
		return DllUnregisterServer();
	} else {
		return DllRegisterServer();
	}
}

STDAPI DllCanUnloadNow()
{
	return InterlockedOr(&locks, 0) == 0 ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID cls, REFIID riid, void **p_ptr)
{
	if (!p_ptr) {
		return E_POINTER;
	}

	*p_ptr = nullptr;

	if (riid != IID_IClassFactory && riid != IID_IUnknown) {
		return E_NOINTERFACE;
	}
	if (!IsEqualCLSID(cls, CLSID_OBS_VirtualVideo)) {
		return E_INVALIDARG;
	}

	*p_ptr = (void *)new VCamFactory(cls);
	return S_OK;
}

//#define ENABLE_LOGGING

#ifdef ENABLE_LOGGING
void logcallback(DShow::LogType, const wchar_t *msg, void *)
{
	OutputDebugStringW(msg);
	OutputDebugStringW(L"\n");
}
#endif

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(inst);
#ifdef ENABLE_LOGGING
		DShow::SetLogCallback(logcallback, nullptr);
#endif
		dll_inst = inst;
	}

	return true;
}
