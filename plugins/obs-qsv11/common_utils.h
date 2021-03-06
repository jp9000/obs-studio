/*****************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or
nondisclosure agreement with Intel Corporation and may not be copied
or disclosed except in accordance with the terms of that agreement.
Copyright(c) 2005-2014 Intel Corporation. All Rights Reserved.

*****************************************************************************/

#pragma once

#include <stdio.h>

#include "mfxvideo++.h"

// =================================================================
// OS-specific definitions of types, macro, etc...
// The following should be defined:
//  - mfxTime
//  - MSDK_FOPEN
//  - MSDK_SLEEP
#if defined(_WIN32) || defined(_WIN64)
#include "bits/windows_defs.h"
#elif defined(__linux__)
#include "bits/linux_defs.h"
#endif

// =================================================================
// Helper macro definitions...
#define MSDK_PRINT_RET_MSG(ERR)                          \
	{                                                \
		PrintErrString(ERR, __FILE__, __LINE__); \
	}
#define MSDK_CHECK_RESULT(P, X, ERR)             \
	{                                        \
		if ((X) > (P)) {                 \
			MSDK_PRINT_RET_MSG(ERR); \
			return ERR;              \
		}                                \
	}
#define MSDK_CHECK_POINTER(P, ERR)               \
	{                                        \
		if (!(P)) {                      \
			MSDK_PRINT_RET_MSG(ERR); \
			return ERR;              \
		}                                \
	}
#define MSDK_CHECK_ERROR(P, X, ERR)              \
	{                                        \
		if ((X) == (P)) {                \
			MSDK_PRINT_RET_MSG(ERR); \
			return ERR;              \
		}                                \
	}
#define MSDK_IGNORE_MFX_STS(P, X)         \
	{                                 \
		if ((X) == (P)) {         \
			P = MFX_ERR_NONE; \
		}                         \
	}
#define MSDK_BREAK_ON_ERROR(P)           \
	{                                \
		if (MFX_ERR_NONE != (P)) \
			break;           \
	}
#define MSDK_SAFE_DELETE_ARRAY(P)   \
	{                           \
		if (P) {            \
			delete[] P; \
			P = NULL;   \
		}                   \
	}
#define MSDK_ALIGN32(X) (((mfxU32)((X) + 31)) & (~(mfxU32)31))
#define MSDK_ALIGN16(value) (((value + 15) >> 4) << 4)
#define MSDK_SAFE_RELEASE(X)          \
	{                             \
		if (X) {              \
			X->Release(); \
			X = NULL;     \
		}                     \
	}
#define MSDK_MAX(A, B) (((A) > (B)) ? (A) : (B))

// Usage of the following two macros are only required for certain Windows DirectX11 use cases
#define WILL_READ 0x1000
#define WILL_WRITE 0x2000

// =================================================================
// Intel Media SDK memory allocator entrypoints....
// Implementation of this functions is OS/Memory type specific.
mfxStatus simple_alloc(mfxHDL pthis, mfxFrameAllocRequest *request,
		       mfxFrameAllocResponse *response);
mfxStatus simple_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
mfxStatus simple_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
mfxStatus simple_gethdl(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
mfxStatus simple_free(mfxHDL pthis, mfxFrameAllocResponse *response);
mfxStatus simple_copytex(mfxHDL pthis, mfxMemId mid, mfxU32 tex_handle,
			 mfxU64 lock_key, mfxU64 *next_key);

// =================================================================
// Utility functions, not directly tied to Media SDK functionality
//

void PrintErrString(int err, const char *filestr, int line);

// LoadRawFrame: Reads raw frame from YUV file (YV12) into NV12 surface
// - YV12 is a more common format for YUV files than NV12 (therefore the conversion during read and write)
// - For the simulation case (fSource = NULL), the surface is filled with default image data
// LoadRawRGBFrame: Reads raw RGB32 frames from file into RGB32 surface
// - For the simulation case (fSource = NULL), the surface is filled with default image data

mfxStatus LoadRawFrame(mfxFrameSurface1 *pSurface, FILE *fSource);
mfxStatus LoadRawRGBFrame(mfxFrameSurface1 *pSurface, FILE *fSource);

// Write raw YUV (NV12) surface to YUV (YV12) file
mfxStatus WriteRawFrame(mfxFrameSurface1 *pSurface, FILE *fSink);

// Write bit stream data for frame to file
mfxStatus WriteBitStreamFrame(mfxBitstream *pMfxBitstream, FILE *fSink);
// Read bit stream data from file. Stream is read as large chunks (= many frames)
mfxStatus ReadBitStreamData(mfxBitstream *pBS, FILE *fSource);

void ClearYUVSurfaceSysMem(mfxFrameSurface1 *pSfc, mfxU16 width, mfxU16 height);
void ClearYUVSurfaceVMem(mfxMemId memId);
void ClearRGBSurfaceVMem(mfxMemId memId);

// Get free raw frame surface
int GetFreeSurfaceIndex(mfxFrameSurface1 **pSurfacesPool, mfxU16 nPoolSize);

// For use with asynchronous task management
typedef struct {
	mfxBitstream mfxBS;
	mfxSyncPoint syncp;
} Task;

// Get free task
int GetFreeTaskIndex(Task *pTaskPool, mfxU16 nPoolSize);

// Initialize Intel Media SDK Session, device/display and memory manager
mfxStatus Initialize(mfxIMPL impl, mfxVersion ver, MFXVideoSession *pSession,
		     mfxFrameAllocator *pmfxAllocator,
		     mfxHDL *deviceHandle = NULL,
		     bool bCreateSharedHandles = false, bool dx9hack = false);

// Release resources (device/display)
void Release();

// Convert frame type to string
char mfxFrameTypeString(mfxU16 FrameType);

void mfxGetTime(mfxTime *timestamp);

//void mfxInitTime();  might need this for Windows
double TimeDiffMsec(mfxTime tfinish, mfxTime tstart);
