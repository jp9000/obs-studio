#include "obs-amf.hpp"
#include <mutex>
#include <dxgi.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <atlbase.h>
#include "AMF/include/components/VideoEncoderVCE.h"
#include "AMF/include/components/VideoEncoderHEVC.h"
#include "AMF/include/core/Factory.h"
#include "AMF/include/core/Data.h"
#include "AMF/include/core/PropertyStorageEx.h"
using namespace amf;

static AMF *__instance;
static std::mutex __instance_mutex;
void AMF::Initialize()
{
	const std::lock_guard<std::mutex> lock(__instance_mutex);
	if (!__instance)
		__instance = new AMF();
}

AMF *AMF::Instance()
{
	const std::lock_guard<std::mutex> lock(__instance_mutex);
	return __instance;
}

void AMF::Finalize()
{
	const std::lock_guard<std::mutex> lock(__instance_mutex);
	if (__instance)
		delete __instance;
	__instance = nullptr;
}

AMF::AMF()
{
	AMF_RESULT result = AMF_FAIL;
	amf_version_fun = nullptr;
	amf_init_fun = nullptr;
	amf_version = 0;
	amf_module = LoadLibraryW(AMF_DLL_NAME);
	if (!amf_module) {
		result = AMF_FAIL;
		return;
	}

	amf_init_fun = (AMFInit_Fn)(
		GetProcAddress(amf_module, AMF_INIT_FUNCTION_NAME));
	if (!amf_module) {
		result = AMF_FAIL;
		AMF_LOG_WARNING("Load Library Failed");
		return;
	}
	result = amf_init_fun(AMF_FULL_VERSION, &factory);
	if (result != AMF_OK) {
		AMF_LOG_WARNING("Init Failed");
		return;
	}
	result = factory->GetTrace(&trace);
	if (result != AMF_OK) {
		AMF_LOG_WARNING("AMF: Failed to GetTrace.");
		return;
	}
	amf_version_fun = (AMFQueryVersion_Fn)GetProcAddress(
		amf_module, AMF_QUERY_VERSION_FUNCTION_NAME);
	if (!amf_version_fun) {
		AMF_LOG_WARNING(
			"Incompatible AMF Runtime (could not find '%s'), error code %ld.",
			AMF_QUERY_VERSION_FUNCTION_NAME, GetLastError());
		return;
	} else {
		result = amf_version_fun(&amf_version);

		if (result != AMF_OK) {
			AMF_LOG_WARNING(
				"Querying Version failed, error code %d.",
				result);
			return;
		}
	}
	trace_writer = std::unique_ptr<obs_amf_trace_writer>(
		new obs_amf_trace_writer());
	trace->RegisterWriter(OBS_AMF_TRACE_WRITER, trace_writer.get(), true);
}

AMF::~AMF()
{
	if (amf_module) {
		if (trace) {
			trace->TraceFlush();
			trace->UnregisterWriter(OBS_AMF_TRACE_WRITER);
		}
		FreeLibrary(amf_module);
	}
	amf_version = 0;
	amf_module = 0;

	factory = nullptr;
	trace = nullptr;
	amf_version_fun = nullptr;
	amf_init_fun = nullptr;
}

AMFFactory *AMF::GetFactory()
{
	return factory;
}
AMFTrace *AMF::GetTrace()
{
	return trace;
}
uint64_t AMF::GetRuntimeVersion()
{
	return amf_version;
}
void AMF::FillCaps()
{
	h264_caps.clear();
	hevc_caps.clear();
	ATL::CComPtr<IDXGIFactory> pFactory;
	ATL::CComPtr<ID3D11Device> pD3D11Device;
	ATL::CComPtr<ID3D11DeviceContext> pD3D11Context;
	HRESULT hr;
	uint32_t adapterIndex = 0;
	DXGI_ADAPTER_DESC desc;
	AMF_RESULT result;

	hr = CreateDXGIFactory1(__uuidof(IDXGIFactory), (void **)(&pFactory));
	if (FAILED(hr)) {
		AMF_LOG_WARNING("CreateDXGIFactory1 failed");
		return;
	}

	INT count = -1;
	while (true) {
		count++;
		ATL::CComPtr<IDXGIAdapter> pAdapter;

		if (pFactory->EnumAdapters(count, &pAdapter) ==
		    DXGI_ERROR_NOT_FOUND) {
			break;
		}

		pAdapter->GetDesc(&desc);

		if (desc.VendorId != 0x1002) {
			continue;
		}
		ATL::CComPtr<IDXGIOutput> pOutput;
		if (pAdapter->EnumOutputs(0, &pOutput) ==
		    DXGI_ERROR_NOT_FOUND) {
			continue;
		}

		hr = D3D11CreateDevice(pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL,
				       0, NULL, 0, D3D11_SDK_VERSION,
				       &pD3D11Device, NULL, &pD3D11Context);
		if (FAILED(hr)) {
			AMF_LOG_WARNING("D3D11CreateDevice failed");
			continue;
		}

		AMFContextPtr pContext;
		result = factory->CreateContext(&pContext);
		if (result != AMF_OK) {
			continue;
		}
		result = pContext->InitDX11(pD3D11Device);
		if (result != AMF_OK) {
			continue;
		}
		AMFComponentPtr encoder;
		result = factory->CreateComponent(
			pContext, AMFVideoEncoderVCE_AVC, &encoder);
		if (result == AMF_OK) {
			const AMFPropertyInfo *pParamInfo = NULL;

			result = encoder->GetPropertyInfo(
				AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD,
				&pParamInfo);
			if (result == AMF_OK) {
				EncoderCaps caps;
				const AMFEnumDescriptionEntry *enumDesc =
					pParamInfo->pEnumDescription;
				while (enumDesc->name) {
					EncoderCaps::NameValuePair pair;
					pair.value = enumDesc->value;
					pair.name = enumDesc->name;
					caps.rate_control_methods.push_back(
						pair);
					enumDesc++;
				}
				h264_caps.insert({count, caps});
			}
		}

		encoder.Release();
		result = factory->CreateComponent(
			pContext, AMFVideoEncoder_HEVC, &encoder);
		if (result == AMF_OK) {
			const AMFPropertyInfo *pParamInfo = NULL;

			result = encoder->GetPropertyInfo(
				AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_METHOD,
				&pParamInfo);
			if (result == AMF_OK) {
				EncoderCaps caps;
				const AMFEnumDescriptionEntry *enumDesc =
					pParamInfo->pEnumDescription;
				while (enumDesc->name) {
					EncoderCaps::NameValuePair pair;
					pair.value = enumDesc->value;
					pair.name = enumDesc->name;
					caps.rate_control_methods.push_back(
						pair);
					enumDesc++;
				}
				hevc_caps.insert({count, caps});
			}
		}
	}
}

EncoderCaps AMF::GetH264Caps(int deviceNum) const
{
	std::map<int, EncoderCaps>::const_iterator it;
	it = h264_caps.find(deviceNum);

	if (it != h264_caps.end())
		return (it->second);
	return EncoderCaps();
}
EncoderCaps AMF::GetHEVCCaps(int deviceNum) const
{
	std::map<int, EncoderCaps>::const_iterator it;
	it = hevc_caps.find(deviceNum);

	if (it != hevc_caps.end())
		return (it->second);
	return EncoderCaps();
}

bool AMF::H264Available() const
{
	return h264_caps.size() > 0;
}

bool AMF::HEVCAvailable() const
{
	return hevc_caps.size() > 0;
}
