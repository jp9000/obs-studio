#include "common_directx9.h"
#include "device_directx9.h"

#include <objbase.h>
#include <initguid.h>
#include <d3d9.h>
#include <map>
#include <atlbase.h>

#define D3DFMT_NV12 (D3DFORMAT) MAKEFOURCC('N', 'V', '1', '2')
#define D3DFMT_YV12 (D3DFORMAT) MAKEFOURCC('Y', 'V', '1', '2')
#define D3DFMT_P010 (D3DFORMAT) MAKEFOURCC('P', '0', '1', '0')
#define MSDK_SAFE_FREE(X)         \
	{                         \
		if (X) {          \
			free(X);  \
			X = NULL; \
		}                 \
	}

std::map<mfxMemId *, mfxHDL> dx9_allocResponses;
std::map<mfxHDL, mfxFrameAllocResponse> dx9_allocDecodeResponses;
std::map<mfxHDL, int> dx9_allocDecodeRefCount;

CComPtr<IDirect3DDeviceManager9> m_manager;
CComPtr<IDirectXVideoDecoderService> m_decoderService;
CComPtr<IDirectXVideoProcessorService> m_processorService;
HANDLE m_hDecoder;
HANDLE m_hProcessor;
DWORD m_surfaceUsage;

CD3D9Device *g_hwdevice;

const struct {
	mfxIMPL impl;     // actual implementation
	mfxU32 adapterID; // device adapter number
} implTypes[] = {{MFX_IMPL_HARDWARE, 0},
		 {MFX_IMPL_HARDWARE2, 1},
		 {MFX_IMPL_HARDWARE3, 2},
		 {MFX_IMPL_HARDWARE4, 3}};

struct mfxAllocatorParams {
	virtual ~mfxAllocatorParams(){};
};

struct D3DAllocatorParams : mfxAllocatorParams {
	IDirect3DDeviceManager9 *pManager;
	DWORD surfaceUsage;

	D3DAllocatorParams() : pManager(), surfaceUsage() {}
};

mfxStatus DX9_Alloc_Init(D3DAllocatorParams *pParams)
{
	D3DAllocatorParams *pd3dParams = 0;
	pd3dParams = dynamic_cast<D3DAllocatorParams *>(pParams);
	if (!pd3dParams)
		return MFX_ERR_NOT_INITIALIZED;

	m_manager = pd3dParams->pManager;
	m_surfaceUsage = pd3dParams->surfaceUsage;

	return MFX_ERR_NONE;
}

mfxStatus DX9_CreateHWDevice(mfxSession session, mfxHDL *deviceHandle, HWND,
			     bool)
{
	mfxStatus result;

	g_hwdevice = new CD3D9Device;
	mfxU32 adapterNum = 0;
	mfxIMPL impl;

	MFXQueryIMPL(session, &impl);

	mfxIMPL baseImpl = MFX_IMPL_BASETYPE(
		impl); // Extract Media SDK base implementation type

	// get corresponding adapter number
	for (mfxU8 i = 0; i < sizeof(implTypes) / sizeof(implTypes[0]); i++) {
		if (implTypes[i].impl == baseImpl) {
			adapterNum = implTypes[i].adapterID;
			break;
		}
	}

	POINT point = {0, 0};
	HWND window = WindowFromPoint(point);

	result = g_hwdevice->Init(window, 0, adapterNum);
	if (result != MFX_ERR_NONE) {
		return result;
	}

	g_hwdevice->GetHandle(MFX_HANDLE_D3D9_DEVICE_MANAGER, deviceHandle);

	D3DAllocatorParams dx9_allocParam;
	dx9_allocParam.pManager =
		reinterpret_cast<IDirect3DDeviceManager9 *>(*deviceHandle);
	DX9_Alloc_Init(&dx9_allocParam);
	return MFX_ERR_NONE;
}

void DX9_CleanupHWDevice()
{
	if (g_hwdevice) {
		// g_hwdevice->Close();
		delete g_hwdevice;
		g_hwdevice = NULL;
	}
	if (m_manager && m_hDecoder) {
		m_manager->CloseDeviceHandle(m_hDecoder);
		m_manager = NULL;
		m_hDecoder = NULL;
	}

	if (m_manager && m_hProcessor) {
		m_manager->CloseDeviceHandle(m_hProcessor);
		m_manager = NULL;
		m_hProcessor = NULL;
	}

	if (m_decoderService) {
		// delete m_decoderService;
		m_decoderService = NULL;
	}

	if (m_processorService) {
		// delete m_processorService;
		m_processorService = NULL;
	}
}

D3DFORMAT ConvertMfxFourccToD3dFormat(mfxU32 fourcc)
{
	switch (fourcc) {
	case MFX_FOURCC_NV12:
		return D3DFMT_NV12;
	case MFX_FOURCC_YV12:
		return D3DFMT_YV12;
	case MFX_FOURCC_YUY2:
		return D3DFMT_YUY2;
	case MFX_FOURCC_RGB3:
		return D3DFMT_R8G8B8;
	case MFX_FOURCC_RGB4:
		return D3DFMT_A8R8G8B8;
	case MFX_FOURCC_P8:
		return D3DFMT_P8;
	case MFX_FOURCC_P010:
		return D3DFMT_P010;
	case MFX_FOURCC_A2RGB10:
		return D3DFMT_A2R10G10B10;
	default:
		return D3DFMT_UNKNOWN;
	}
}

mfxStatus dx9_simple_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
	pthis; // To suppress warning for this unused parameter

	if (!ptr || !mid)
		return MFX_ERR_NULL_PTR;

	mfxHDLPair *dxmid = (mfxHDLPair *)mid;
	IDirect3DSurface9 *pSurface =
		static_cast<IDirect3DSurface9 *>(dxmid->first);
	if (pSurface == 0)
		return MFX_ERR_INVALID_HANDLE;

	D3DSURFACE_DESC desc;
	HRESULT hr = pSurface->GetDesc(&desc);
	if (FAILED(hr))
		return MFX_ERR_LOCK_MEMORY;

	if (desc.Format != D3DFMT_NV12 && desc.Format != D3DFMT_YV12 &&
	    desc.Format != D3DFMT_YUY2 && desc.Format != D3DFMT_R8G8B8 &&
	    desc.Format != D3DFMT_A8R8G8B8 && desc.Format != D3DFMT_P8 &&
	    desc.Format != D3DFMT_P010 && desc.Format != D3DFMT_A2R10G10B10)
		return MFX_ERR_LOCK_MEMORY;

	D3DLOCKED_RECT locked;

	hr = pSurface->LockRect(&locked, 0, D3DLOCK_NOSYSLOCK);
	if (FAILED(hr))
		return MFX_ERR_LOCK_MEMORY;

	switch ((DWORD)desc.Format) {
	case D3DFMT_NV12:
		ptr->Pitch = (mfxU16)locked.Pitch;
		ptr->Y = (mfxU8 *)locked.pBits;
		ptr->U = (mfxU8 *)locked.pBits + desc.Height * locked.Pitch;
		ptr->V = ptr->U + 1;
		break;
	case D3DFMT_YV12:
		ptr->Pitch = (mfxU16)locked.Pitch;
		ptr->Y = (mfxU8 *)locked.pBits;
		ptr->V = ptr->Y + desc.Height * locked.Pitch;
		ptr->U = ptr->V + (desc.Height * locked.Pitch) / 4;
		break;
	case D3DFMT_YUY2:
		ptr->Pitch = (mfxU16)locked.Pitch;
		ptr->Y = (mfxU8 *)locked.pBits;
		ptr->U = ptr->Y + 1;
		ptr->V = ptr->Y + 3;
		break;
	case D3DFMT_R8G8B8:
		ptr->Pitch = (mfxU16)locked.Pitch;
		ptr->B = (mfxU8 *)locked.pBits;
		ptr->G = ptr->B + 1;
		ptr->R = ptr->B + 2;
		break;
	case D3DFMT_A8R8G8B8:
	case D3DFMT_A2R10G10B10:
		ptr->Pitch = (mfxU16)locked.Pitch;
		ptr->B = (mfxU8 *)locked.pBits;
		ptr->G = ptr->B + 1;
		ptr->R = ptr->B + 2;
		ptr->A = ptr->B + 3;
		break;
	case D3DFMT_P8:
		ptr->Pitch = (mfxU16)locked.Pitch;
		ptr->Y = (mfxU8 *)locked.pBits;
		ptr->U = 0;
		ptr->V = 0;
		break;
	case D3DFMT_P010:
		ptr->PitchHigh = (mfxU16)(locked.Pitch / (1 << 16));
		ptr->PitchLow = (mfxU16)(locked.Pitch % (1 << 16));
		ptr->Y = (mfxU8 *)locked.pBits;
		ptr->U = (mfxU8 *)locked.pBits + desc.Height * locked.Pitch;
		ptr->V = ptr->U + 1;
		break;
	}

	return MFX_ERR_NONE;
}

mfxStatus dx9_simple_unlock(mfxHDL, mfxMemId mid, mfxFrameData *ptr)
{
	if (!mid)
		return MFX_ERR_NULL_PTR;

	mfxHDLPair *dxmid = (mfxHDLPair *)mid;
	IDirect3DSurface9 *pSurface =
		static_cast<IDirect3DSurface9 *>(dxmid->first);
	if (pSurface == 0)
		return MFX_ERR_INVALID_HANDLE;

	pSurface->UnlockRect();

	if (NULL != ptr) {
		ptr->Pitch = 0;
		ptr->Y = 0;
		ptr->U = 0;
		ptr->V = 0;
	}

	return MFX_ERR_NONE;
}

mfxStatus dx9_simple_gethdl(mfxHDL, mfxMemId mid, mfxHDL *handle)
{
	if (!mid || !handle)
		return MFX_ERR_NULL_PTR;

	mfxHDLPair *dxMid = (mfxHDLPair *)mid;
	*handle = dxMid->first;
	return MFX_ERR_NONE;
}

mfxStatus _dx9_simple_free(mfxFrameAllocResponse *response)
{
	if (!response)
		return MFX_ERR_NULL_PTR;

	mfxStatus sts = MFX_ERR_NONE;

	if (response->mids) {
		for (mfxU32 i = 0; i < response->NumFrameActual; i++) {
			if (response->mids[i]) {
				mfxHDLPair *dxMids =
					(mfxHDLPair *)response->mids[i];
				static_cast<IDirect3DSurface9 *>(dxMids->first)
					->Release();
			}
		}
		MSDK_SAFE_FREE(response->mids[0]);
	}
	MSDK_SAFE_FREE(response->mids);

	return sts;
}

mfxStatus dx9_simple_free(mfxHDL pthis, mfxFrameAllocResponse *response)
{
	if (NULL == response)
		return MFX_ERR_NULL_PTR;

	if (dx9_allocResponses.find(response->mids) ==
	    dx9_allocResponses.end()) {
		// Decode free response handling
		if (--dx9_allocDecodeRefCount[pthis] == 0) {
			_dx9_simple_free(response);
			dx9_allocDecodeResponses.erase(pthis);
			dx9_allocDecodeRefCount.erase(pthis);
		}
	} else {
		// Encode and VPP free response handling
		dx9_allocResponses.erase(response->mids);
		_dx9_simple_free(response);
	}

	return MFX_ERR_NONE;
}

mfxStatus _dx9_simple_alloc(mfxFrameAllocRequest *request,
			    mfxFrameAllocResponse *response)
{
	HRESULT hr;

	MSDK_CHECK_POINTER(request, MFX_ERR_NULL_PTR);
	if (request->NumFrameSuggested == 0)
		return MFX_ERR_UNKNOWN;

	D3DFORMAT format = ConvertMfxFourccToD3dFormat(request->Info.FourCC);

	if (format == D3DFMT_UNKNOWN)
		return MFX_ERR_UNSUPPORTED;

	DWORD target;

	if (MFX_MEMTYPE_DXVA2_DECODER_TARGET & request->Type) {
		target = DXVA2_VideoDecoderRenderTarget;
	} else if (MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET & request->Type) {
		target = DXVA2_VideoProcessorRenderTarget;
	} else
		return MFX_ERR_UNSUPPORTED;

	IDirectXVideoAccelerationService *videoService = NULL;

	if (target == DXVA2_VideoProcessorRenderTarget) {
		if (!m_hProcessor) {
			hr = m_manager->OpenDeviceHandle(&m_hProcessor);
			if (FAILED(hr))
				return MFX_ERR_MEMORY_ALLOC;

			hr = m_manager->GetVideoService(
				m_hProcessor, IID_IDirectXVideoProcessorService,
				(void **)&m_processorService);
			if (FAILED(hr))
				return MFX_ERR_MEMORY_ALLOC;
		}
		videoService = m_processorService;
	} else {
		if (!m_hDecoder) {
			hr = m_manager->OpenDeviceHandle(&m_hDecoder);
			if (FAILED(hr))
				return MFX_ERR_MEMORY_ALLOC;

			hr = m_manager->GetVideoService(
				m_hDecoder, IID_IDirectXVideoDecoderService,
				(void **)&m_decoderService);
			if (FAILED(hr))
				return MFX_ERR_MEMORY_ALLOC;
		}
		videoService = m_decoderService;
	}

	mfxHDLPair *dxMids = NULL, **dxMidPtrs = NULL;
	dxMids = (mfxHDLPair *)calloc(request->NumFrameSuggested,
				      sizeof(mfxHDLPair));
	dxMidPtrs = (mfxHDLPair **)calloc(request->NumFrameSuggested,
					  sizeof(mfxHDLPair *));

	if (!dxMids || !dxMidPtrs) {
		MSDK_SAFE_FREE(dxMids);
		MSDK_SAFE_FREE(dxMidPtrs);
		return MFX_ERR_MEMORY_ALLOC;
	}

	response->mids = (mfxMemId *)dxMidPtrs;
	response->NumFrameActual = request->NumFrameSuggested;

	if (request->Type & MFX_MEMTYPE_EXTERNAL_FRAME) {
		for (int i = 0; i < request->NumFrameSuggested; i++) {
			hr = videoService->CreateSurface(
				request->Info.Width, request->Info.Height, 0,
				format, D3DPOOL_DEFAULT, m_surfaceUsage, target,
				(IDirect3DSurface9 **)&dxMids[i].first,
				&dxMids[i].second);
			if (FAILED(hr)) {
				_dx9_simple_free(response);
				MSDK_SAFE_FREE(dxMids);
				return MFX_ERR_MEMORY_ALLOC;
			}
			dxMidPtrs[i] = &dxMids[i];
		}
	} else {
		safe_array<IDirect3DSurface9 *> dxSrf(
			new IDirect3DSurface9 *[request->NumFrameSuggested]);
		if (!dxSrf.get()) {
			MSDK_SAFE_FREE(dxMids);
			return MFX_ERR_MEMORY_ALLOC;
		}
		hr = videoService->CreateSurface(
			request->Info.Width, request->Info.Height,
			request->NumFrameSuggested - 1, format, D3DPOOL_DEFAULT,
			m_surfaceUsage, target, dxSrf.get(), NULL);
		if (FAILED(hr)) {
			MSDK_SAFE_FREE(dxMids);
			return MFX_ERR_MEMORY_ALLOC;
		}

		for (int i = 0; i < request->NumFrameSuggested; i++) {
			dxMids[i].first = dxSrf.get()[i];
			dxMidPtrs[i] = &dxMids[i];
		}
	}
	return MFX_ERR_NONE;
}

mfxStatus dx9_simple_alloc(mfxHDL pthis, mfxFrameAllocRequest *request,
			   mfxFrameAllocResponse *response)
{
	mfxStatus sts = MFX_ERR_NONE;

	if (request->Type & MFX_MEMTYPE_SYSTEM_MEMORY)
		return MFX_ERR_UNSUPPORTED;

	if (dx9_allocDecodeResponses.find(pthis) !=
		    dx9_allocDecodeResponses.end() &&
	    MFX_MEMTYPE_EXTERNAL_FRAME & request->Type &&
	    MFX_MEMTYPE_FROM_DECODE & request->Type) {
		// Memory for this request was already allocated during manual allocation stage. Return saved response
		//   When decode acceleration device (DXVA) is created it requires a list of d3d surfaces to be passed.
		//   Therefore Media SDK will ask for the surface info/mids again at Init() stage, thus requiring us to return the saved response
		//   (No such restriction applies to Encode or VPP)
		*response = dx9_allocDecodeResponses[pthis];
		dx9_allocDecodeRefCount[pthis]++;
	} else {
		sts = _dx9_simple_alloc(request, response);

		if (MFX_ERR_NONE == sts) {
			if (MFX_MEMTYPE_EXTERNAL_FRAME & request->Type &&
			    MFX_MEMTYPE_FROM_DECODE & request->Type) {
				// Decode alloc response handling
				dx9_allocDecodeResponses[pthis] = *response;
				dx9_allocDecodeRefCount[pthis]++;
			} else {
				// Encode and VPP alloc response handling
				dx9_allocResponses[response->mids] = pthis;
			}
		}
	}

	return sts;
}
