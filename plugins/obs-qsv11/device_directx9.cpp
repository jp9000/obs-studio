/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2015 Intel Corporation. All Rights Reserved.

**********************************************************************************/

// #include "mfx_samples_config.h"

#if defined(WIN32) || defined(WIN64)

//prefast signature used in combaseapi.h
#ifndef _PREFAST_
#pragma warning(disable : 4068)
#endif

#include "device_directx9.h"
// #include "igfx_s3dcontrol.h"

#include "atlbase.h"

// Macros
#define MSDK_ZERO_MEMORY(VAR)                 \
	{                                     \
		memset(&VAR, 0, sizeof(VAR)); \
	}
#define MSDK_MEMCPY_VAR(dstVarName, src, count) \
	memcpy_s(&(dstVarName), sizeof(dstVarName), (src), (count))

CD3D9Device::CD3D9Device()
{
	m_pD3D9 = NULL;
	m_pD3DD9 = NULL;
	m_pDeviceManager9 = NULL;
	MSDK_ZERO_MEMORY(m_D3DPP);
	m_resetToken = 0;

	m_nViews = 0;
	m_pS3DControl = NULL;

	MSDK_ZERO_MEMORY(m_backBufferDesc);
	m_pDXVAVPS = NULL;
	m_pDXVAVP_Left = NULL;
	m_pDXVAVP_Right = NULL;

	MSDK_ZERO_MEMORY(m_targetRect);

	MSDK_ZERO_MEMORY(m_VideoDesc);
	MSDK_ZERO_MEMORY(m_BltParams);
	MSDK_ZERO_MEMORY(m_Sample);

	// Initialize DXVA structures

	DXVA2_AYUVSample16 color = {
		0x8000, // Cr
		0x8000, // Cb
		0x1000, // Y
		0xffff  // Alpha
	};

	DXVA2_ExtendedFormat format = {
		// DestFormat
		DXVA2_SampleProgressiveFrame,       // SampleFormat
		DXVA2_VideoChromaSubsampling_MPEG2, // VideoChromaSubsampling
		DXVA_NominalRange_0_255,            // NominalRange
		DXVA2_VideoTransferMatrix_BT709,    // VideoTransferMatrix
		DXVA2_VideoLighting_bright,         // VideoLighting
		DXVA2_VideoPrimaries_BT709,         // VideoPrimaries
		DXVA2_VideoTransFunc_709            // VideoTransferFunction
	};

	// init m_VideoDesc structure
	MSDK_MEMCPY_VAR(m_VideoDesc.SampleFormat, &format,
			sizeof(DXVA2_ExtendedFormat));
	m_VideoDesc.SampleWidth = 0;
	m_VideoDesc.SampleHeight = 0;
	m_VideoDesc.InputSampleFreq.Numerator = 60;
	m_VideoDesc.InputSampleFreq.Denominator = 1;
	m_VideoDesc.OutputFrameFreq.Numerator = 60;
	m_VideoDesc.OutputFrameFreq.Denominator = 1;

	// init m_BltParams structure
	MSDK_MEMCPY_VAR(m_BltParams.DestFormat, &format,
			sizeof(DXVA2_ExtendedFormat));
	MSDK_MEMCPY_VAR(m_BltParams.BackgroundColor, &color,
			sizeof(DXVA2_AYUVSample16));

	// init m_Sample structure
	m_Sample.Start = 0;
	m_Sample.End = 1;
	m_Sample.SampleFormat = format;
	m_Sample.PlanarAlpha.Fraction = 0;
	m_Sample.PlanarAlpha.Value = 1;

	m_bIsA2rgb10 = FALSE;
}

bool CD3D9Device::CheckOverlaySupport()
{
	D3DCAPS9 d3d9caps;
	D3DOVERLAYCAPS d3doverlaycaps = {0};
	IDirect3D9ExOverlayExtension *d3d9overlay = NULL;
	bool overlaySupported = false;

	memset(&d3d9caps, 0, sizeof(d3d9caps));
	HRESULT hr = m_pD3D9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
					    &d3d9caps);
	if (FAILED(hr) || !(d3d9caps.Caps & D3DCAPS_OVERLAY)) {
		overlaySupported = false;
	} else {
		hr = m_pD3D9->QueryInterface(IID_PPV_ARGS(&d3d9overlay));
		if (FAILED(hr) || (d3d9overlay == NULL)) {
			overlaySupported = false;
		} else {
			hr = d3d9overlay->CheckDeviceOverlayType(
				D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
				m_D3DPP.BackBufferWidth,
				m_D3DPP.BackBufferHeight,
				m_D3DPP.BackBufferFormat, NULL,
				D3DDISPLAYROTATION_IDENTITY, &d3doverlaycaps);
			MSDK_SAFE_RELEASE(d3d9overlay);

			if (FAILED(hr)) {
				overlaySupported = false;
			} else {
				overlaySupported = true;
			}
		}
	}

	return overlaySupported;
}

mfxStatus CD3D9Device::FillD3DPP(mfxHDL hWindow, mfxU16 nViews,
				 D3DPRESENT_PARAMETERS &D3DPP)
{
	mfxStatus sts = MFX_ERR_NONE;

	D3DPP.Windowed = true;
	D3DPP.hDeviceWindow = (HWND)hWindow;

	D3DPP.Flags = D3DPRESENTFLAG_VIDEO;
	D3DPP.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	D3DPP.PresentationInterval =
		D3DPRESENT_INTERVAL_ONE; // note that this setting leads to an implicit timeBeginPeriod call
	D3DPP.BackBufferCount = 1;
	D3DPP.BackBufferFormat = (m_bIsA2rgb10) ? D3DFMT_A2R10G10B10
						: D3DFMT_X8R8G8B8;

	if (hWindow) {
		RECT r;
		GetClientRect((HWND)hWindow, &r);
		int x = GetSystemMetrics(SM_CXSCREEN);
		int y = GetSystemMetrics(SM_CYSCREEN);
		D3DPP.BackBufferWidth = min(r.right - r.left, x);
		D3DPP.BackBufferHeight = min(r.bottom - r.top, y);
	} else {
		D3DPP.BackBufferWidth = GetSystemMetrics(SM_CYSCREEN);
		D3DPP.BackBufferHeight = GetSystemMetrics(SM_CYSCREEN);
	}
	//
	// Mark the back buffer lockable if software DXVA2 could be used.
	// This is because software DXVA2 device requires a lockable render target
	// for the optimal performance.
	//
	{
		D3DPP.Flags |= D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
	}

	bool isOverlaySupported = CheckOverlaySupport();
	if (2 == nViews && !isOverlaySupported)
		return MFX_ERR_UNSUPPORTED;

	bool needOverlay = (2 == nViews) ? true : false;

	D3DPP.SwapEffect = needOverlay ? D3DSWAPEFFECT_OVERLAY
				       : D3DSWAPEFFECT_DISCARD;

	return sts;
}

mfxStatus CD3D9Device::Init(mfxHDL hWindow, mfxU16 nViews, mfxU32 nAdapterNum)
{
	mfxStatus sts = MFX_ERR_NONE;

	if (2 < nViews)
		return MFX_ERR_UNSUPPORTED;

	m_nViews = nViews;

	HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &m_pD3D9);
	if (!m_pD3D9 || FAILED(hr))
		return MFX_ERR_DEVICE_FAILED;

	ZeroMemory(&m_D3DPP, sizeof(m_D3DPP));
	sts = FillD3DPP(hWindow, nViews, m_D3DPP);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	hr = m_pD3D9->CreateDeviceEx(nAdapterNum, D3DDEVTYPE_HAL, (HWND)hWindow,
				     D3DCREATE_SOFTWARE_VERTEXPROCESSING |
					     D3DCREATE_MULTITHREADED |
					     D3DCREATE_FPU_PRESERVE,
				     &m_D3DPP, NULL, &m_pD3DD9);
	if (FAILED(hr))
		return MFX_ERR_NULL_PTR;

	if (hWindow) {
		hr = m_pD3DD9->ResetEx(&m_D3DPP, NULL);
		if (FAILED(hr))
			return MFX_ERR_UNDEFINED_BEHAVIOR;
		hr = m_pD3DD9->Clear(0, NULL, D3DCLEAR_TARGET,
				     D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
		if (FAILED(hr))
			return MFX_ERR_UNDEFINED_BEHAVIOR;
	}
	UINT resetToken = 0;

	hr = DXVA2CreateDirect3DDeviceManager9(&resetToken, &m_pDeviceManager9);
	if (FAILED(hr))
		return MFX_ERR_NULL_PTR;

	hr = m_pDeviceManager9->ResetDevice(m_pD3DD9, resetToken);
	if (FAILED(hr))
		return MFX_ERR_UNDEFINED_BEHAVIOR;

	m_resetToken = resetToken;

	return sts;
}

mfxStatus CD3D9Device::Reset()
{
	HRESULT hr = NO_ERROR;
	MSDK_CHECK_POINTER(m_pD3DD9, MFX_ERR_NULL_PTR);

	if (m_D3DPP.Windowed) {
		RECT r;
		GetClientRect((HWND)m_D3DPP.hDeviceWindow, &r);
		int x = GetSystemMetrics(SM_CXSCREEN);
		int y = GetSystemMetrics(SM_CYSCREEN);
		m_D3DPP.BackBufferWidth = min(r.right - r.left, x);
		m_D3DPP.BackBufferHeight = min(r.bottom - r.top, y);
	} else {
		m_D3DPP.BackBufferWidth = GetSystemMetrics(SM_CXSCREEN);
		m_D3DPP.BackBufferHeight = GetSystemMetrics(SM_CYSCREEN);
	}

	// Reset will change the parameters, so use a copy instead.
	D3DPRESENT_PARAMETERS d3dpp = m_D3DPP;
	hr = m_pD3DD9->ResetEx(&d3dpp, NULL);

	if (FAILED(hr))
		return MFX_ERR_UNDEFINED_BEHAVIOR;

	hr = m_pDeviceManager9->ResetDevice(m_pD3DD9, m_resetToken);
	if (FAILED(hr))
		return MFX_ERR_UNDEFINED_BEHAVIOR;

	return MFX_ERR_NONE;
}

void CD3D9Device::Close()
{
	MSDK_SAFE_RELEASE(m_pDXVAVP_Left);
	MSDK_SAFE_RELEASE(m_pDXVAVP_Right);
	MSDK_SAFE_RELEASE(m_pDXVAVPS);

	MSDK_SAFE_RELEASE(m_pDeviceManager9);
	MSDK_SAFE_RELEASE(m_pD3DD9);
	MSDK_SAFE_RELEASE(m_pD3D9);
	m_pS3DControl = NULL;
}

CD3D9Device::~CD3D9Device()
{
	Close();
}

mfxStatus CD3D9Device::GetHandle(mfxHandleType type, mfxHDL *pHdl)
{
	if (MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9 == type && pHdl != NULL) {
		*pHdl = m_pDeviceManager9;

		return MFX_ERR_NONE;
	} else if (MFX_HANDLE_GFXS3DCONTROL == type && pHdl != NULL) {
		*pHdl = m_pS3DControl;

		return MFX_ERR_NONE;
	}
	return MFX_ERR_UNSUPPORTED;
}

mfxStatus CD3D9Device::SetHandle(mfxHandleType type, mfxHDL hdl)
{
	if (MFX_HANDLE_GFXS3DCONTROL == type && hdl != NULL) {
		m_pS3DControl = (IGFXS3DControl *)hdl;
		return MFX_ERR_NONE;
	} else if (MFX_HANDLE_DEVICEWINDOW == type &&
		   hdl != NULL) //for render window handle
	{
		m_D3DPP.hDeviceWindow = (HWND)hdl;
		return MFX_ERR_NONE;
	}
	return MFX_ERR_UNSUPPORTED;
}

mfxStatus CD3D9Device::RenderFrame(mfxFrameSurface1 *pSurface,
				   mfxFrameAllocator *pmfxAlloc)
{
	HRESULT hr = S_OK;

	if (!(1 == m_nViews || (2 == m_nViews && NULL != m_pS3DControl)))
		return MFX_ERR_UNDEFINED_BEHAVIOR;

	MSDK_CHECK_POINTER(pSurface, MFX_ERR_NULL_PTR);
	MSDK_CHECK_POINTER(m_pDeviceManager9, MFX_ERR_NOT_INITIALIZED);
	MSDK_CHECK_POINTER(pmfxAlloc, MFX_ERR_NULL_PTR);

	// don't try to render second view if output rect changed since first view
	if (2 == m_nViews && (0 != pSurface->Info.FrameId.ViewId))
		return MFX_ERR_NONE;

	hr = m_pD3DD9->TestCooperativeLevel();

	switch (hr) {
	case D3D_OK:
		break;

	case D3DERR_DEVICELOST: {
		return MFX_ERR_DEVICE_LOST;
	}

	case D3DERR_DEVICENOTRESET: {
		return MFX_ERR_UNKNOWN;
	}

	default: {
		return MFX_ERR_UNKNOWN;
	}
	}

	CComPtr<IDirect3DSurface9> pBackBuffer;
	hr = m_pD3DD9->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO,
				     &pBackBuffer);

	mfxHDLPair *dxMemId = (mfxHDLPair *)pSurface->Data.MemId;

	hr = m_pD3DD9->StretchRect((IDirect3DSurface9 *)dxMemId->first, NULL,
				   pBackBuffer, NULL, D3DTEXF_LINEAR);
	if (FAILED(hr)) {
		return MFX_ERR_UNKNOWN;
	}

	if (SUCCEEDED(hr) &&
	    (1 == m_nViews || pSurface->Info.FrameId.ViewId == 1)) {
		hr = m_pD3DD9->Present(NULL, NULL, NULL, NULL);
	}

	return SUCCEEDED(hr) ? MFX_ERR_NONE : MFX_ERR_DEVICE_FAILED;
}

/*
mfxStatus CD3D9Device::CreateVideoProcessors()
{
    if (!(1 == m_nViews || (2 == m_nViews && NULL != m_pS3DControl)))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

   MSDK_SAFE_RELEASE(m_pDXVAVP_Left);
   MSDK_SAFE_RELEASE(m_pDXVAVP_Right);

   HRESULT hr ;

   if (2 == m_nViews && NULL != m_pS3DControl)
   {
       hr = m_pS3DControl->SetDevice(m_pDeviceManager9);
       if (FAILED(hr))
       {
           return MFX_ERR_DEVICE_FAILED;
       }
   }

   ZeroMemory(&m_backBufferDesc, sizeof(m_backBufferDesc));
   IDirect3DSurface9 *backBufferTmp = NULL;
   hr = m_pD3DD9->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBufferTmp);
   if (NULL != backBufferTmp)
       backBufferTmp->GetDesc(&m_backBufferDesc);
   MSDK_SAFE_RELEASE(backBufferTmp);

   if (SUCCEEDED(hr))
   {
       // Create DXVA2 Video Processor Service.
       hr = DXVA2CreateVideoService(m_pD3DD9,
           IID_IDirectXVideoProcessorService,
           (void**)&m_pDXVAVPS);
   }

   if (2 == m_nViews)
   {
        // Activate L channel
        if (SUCCEEDED(hr))
        {
           hr = m_pS3DControl->SelectLeftView();
        }

        if (SUCCEEDED(hr))
        {
           // Create VPP device for the L channel
           hr = m_pDXVAVPS->CreateVideoProcessor(DXVA2_VideoProcProgressiveDevice,
               &m_VideoDesc,
               m_D3DPP.BackBufferFormat,
               1,
               &m_pDXVAVP_Left);
        }

        // Activate R channel
        if (SUCCEEDED(hr))
        {
           hr = m_pS3DControl->SelectRightView();
        }

   }
   if (SUCCEEDED(hr))
   {
       hr = m_pDXVAVPS->CreateVideoProcessor(DXVA2_VideoProcProgressiveDevice,
           &m_VideoDesc,
           m_D3DPP.BackBufferFormat,
           1,
           &m_pDXVAVP_Right);
   }

   return SUCCEEDED(hr) ? MFX_ERR_NONE : MFX_ERR_DEVICE_FAILED;
}
*/

#endif // #if defined(WIN32) || defined(WIN64)