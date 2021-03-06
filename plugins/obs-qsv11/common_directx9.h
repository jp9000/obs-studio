/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#pragma once

#include "common_utils.h"
#include <initguid.h>
#include <d3d9.h>
#include <dxva2api.h>
#include <dxva.h>
#include <windows.h>

#define VIDEO_MAIN_FORMAT D3DFMT_YUY2

class IGFXS3DControl;

/** Direct3D 9 device implementation.
@note Can be initialized for only 1 or two 2 views. Handle to
MFX_HANDLE_GFXS3DCONTROL must be set prior if initializing for 2 views.

@note Device always set D3DPRESENT_PARAMETERS::Windowed to TRUE.
*/
template<class T> class safe_array {
public:
	safe_array(T *ptr = 0)
		: m_ptr(ptr){
			  // construct from object pointer
		  };
	~safe_array() { reset(0); }
	T *get()
	{ // return wrapped pointer
		return m_ptr;
	}
	T *release()
	{ // return wrapped pointer and give up ownership
		T *ptr = m_ptr;
		m_ptr = 0;
		return ptr;
	}
	void reset(T *ptr)
	{ // destroy designated object and store new pointer
		if (m_ptr) {
			delete[] m_ptr;
		}
		m_ptr = ptr;
	}

protected:
	T *m_ptr; // the wrapped object pointer
};

mfxStatus dx9_simple_alloc(mfxHDL pthis, mfxFrameAllocRequest *request,
			   mfxFrameAllocResponse *response);
mfxStatus dx9_simple_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
mfxStatus dx9_simple_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
mfxStatus dx9_simple_gethdl(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
mfxStatus dx9_simple_free(mfxHDL pthis, mfxFrameAllocResponse *response);

mfxStatus DX9_CreateHWDevice(mfxSession session, mfxHDL *deviceHandle,
			     HWND hWnd, bool bCreateSharedHandles);
void DX9_CleanupHWDevice();
