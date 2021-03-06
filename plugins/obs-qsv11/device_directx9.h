/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#pragma once

#if defined(_WIN32) || defined(_WIN64)

#include "common_utils.h"

#pragma warning(disable : 4201)
#include <initguid.h>
#include <d3d9.h>
#include <dxva2api.h>
#include <dxva.h>
#include <windows.h>

#define VIDEO_MAIN_FORMAT D3DFMT_YUY2

class IGFXS3DControl;

/// Base class for hw device
class CHWDevice {
public:
	virtual ~CHWDevice() {}
	/** Initializes device for requested processing.
    @param[in] hWindow Window handle to bundle device to.
    @param[in] nViews Number of views to process.
    @param[in] nAdapterNum Number of adapter to use
    */
	virtual mfxStatus Init(mfxHDL hWindow, mfxU16 nViews,
			       mfxU32 nAdapterNum) = 0;
	/// Reset device.
	virtual mfxStatus Reset() = 0;
	/// Get handle can be used for MFX session SetHandle calls
	virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl) = 0;
	/** Set handle.
    Particular device implementation may require other objects to operate.
    */
	virtual mfxStatus SetHandle(mfxHandleType type, mfxHDL hdl) = 0;
	virtual mfxStatus RenderFrame(mfxFrameSurface1 *pSurface,
				      mfxFrameAllocator *pmfxAlloc) = 0;
	virtual void Close() = 0;
};

enum { MFX_HANDLE_GFXS3DCONTROL =
	       0x100, /* A handle to the IGFXS3DControl instance */
       MFX_HANDLE_DEVICEWINDOW = 0x101 /* A handle to the render window */
};                                     //mfxHandleType

/** Direct3D 9 device implementation.
@note Can be initialized for only 1 or two 2 views. Handle to
MFX_HANDLE_GFXS3DCONTROL must be set prior if initializing for 2 views.

@note Device always set D3DPRESENT_PARAMETERS::Windowed to TRUE.
*/
class CD3D9Device : public CHWDevice {
public:
	CD3D9Device();
	virtual ~CD3D9Device();

	virtual mfxStatus Init(mfxHDL hWindow, mfxU16 nViews,
			       mfxU32 nAdapterNum);
	virtual mfxStatus Reset();
	virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl);
	virtual mfxStatus SetHandle(mfxHandleType type, mfxHDL hdl);
	virtual mfxStatus RenderFrame(mfxFrameSurface1 *pSurface,
				      mfxFrameAllocator *pmfxAlloc);
	virtual void UpdateTitle(double /*fps*/) {}
	virtual void Close();
	void DefineFormat(bool isA2rgb10)
	{
		m_bIsA2rgb10 = (isA2rgb10) ? TRUE : FALSE;
	}

protected:
	mfxStatus CreateVideoProcessors();
	bool CheckOverlaySupport();
	virtual mfxStatus FillD3DPP(mfxHDL hWindow, mfxU16 nViews,
				    D3DPRESENT_PARAMETERS &D3DPP);

private:
	IDirect3D9Ex *m_pD3D9;
	IDirect3DDevice9Ex *m_pD3DD9;
	IDirect3DDeviceManager9 *m_pDeviceManager9;
	D3DPRESENT_PARAMETERS m_D3DPP;
	UINT m_resetToken;

	mfxU16 m_nViews;
	IGFXS3DControl *m_pS3DControl;

	D3DSURFACE_DESC m_backBufferDesc;

	// service required to create video processors
	IDirectXVideoProcessorService *m_pDXVAVPS;
	//left channel processor
	IDirectXVideoProcessor *m_pDXVAVP_Left;
	// right channel processor
	IDirectXVideoProcessor *m_pDXVAVP_Right;

	// target rectangle
	RECT m_targetRect;

	// various structures for DXVA2 calls
	DXVA2_VideoDesc m_VideoDesc;
	DXVA2_VideoProcessBltParams m_BltParams;
	DXVA2_VideoSample m_Sample;

	BOOL m_bIsA2rgb10;
};

#endif // #if defined( _WIN32 ) || defined ( _WIN64 )