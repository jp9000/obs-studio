/*****************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or
nondisclosure agreement with Intel Corporation and may not be copied
or disclosed except in accordance with the terms of that agreement.
Copyright(c) 2005-2014 Intel Corporation. All Rights Reserved.

*****************************************************************************/

#pragma once

#include "common_utils.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <atlbase.h>

#define DEVICE_MGR_TYPE MFX_HANDLE_D3D11_DEVICE

// =================================================================
// DirectX functionality required to manage D3D surfaces
//

// Create DirectX 11 device context
// - Required when using D3D surfaces.
// - D3D Device created and handed to Intel Media SDK
// - Intel graphics device adapter will be determined automatically (does not have to be primary),
//   but with the following caveats:
//     - Device must be active (but monitor does NOT have to be attached)
//     - Device must be enabled in BIOS. Required for the case when used together with a discrete graphics card
//     - For switchable graphics solutions (mobile) make sure that Intel device is the active device
mfxStatus CreateHWDevice(mfxSession session, mfxHDL *deviceHandle, HWND hWnd,
			 bool bCreateSharedHandles);
void CleanupHWDevice();
void SetHWDeviceContext(CComPtr<ID3D11DeviceContext> devCtx);
CComPtr<ID3D11DeviceContext> GetHWDeviceContext();
void ClearYUVSurfaceD3D(mfxMemId memId);
void ClearRGBSurfaceD3D(mfxMemId memId);