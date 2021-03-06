/* ****************************************************************************** *\

Copyright (C) 2007-2017 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfxvideo.h

\* ****************************************************************************** */
#ifndef __MFXVIDEO_H__
#define __MFXVIDEO_H__
#include "mfxsession.h"
#include "mfxvstructures.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* MFXVideoCORE */
typedef struct {
    mfxU32      reserved[4];
    mfxHDL      pthis;
    mfxStatus  (MFX_CDECL *Alloc)    (mfxHDL pthis, mfxU32 nbytes, mfxU16 type, mfxMemId *mid);
    mfxStatus  (MFX_CDECL *Lock)     (mfxHDL pthis, mfxMemId mid, mfxU8 **ptr);
    mfxStatus  (MFX_CDECL *Unlock)   (mfxHDL pthis, mfxMemId mid);
    mfxStatus  (MFX_CDECL *Free)     (mfxHDL pthis, mfxMemId mid);
} mfxBufferAllocator;

typedef struct {
    mfxU32      reserved[4];
    mfxHDL      pthis;

    mfxStatus  (MFX_CDECL  *Alloc)    (mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    mfxStatus  (MFX_CDECL  *Lock)     (mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
    mfxStatus  (MFX_CDECL  *Unlock)   (mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
    mfxStatus  (MFX_CDECL  *GetHDL)   (mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
    mfxStatus  (MFX_CDECL  *Free)     (mfxHDL pthis, mfxFrameAllocResponse *response);
} mfxFrameAllocator;

/* VideoCORE */
mfxStatus MFX_CDECL MFXVideoCORE_SetBufferAllocator(mfxSession session, mfxBufferAllocator *allocator);
mfxStatus MFX_CDECL MFXVideoCORE_SetFrameAllocator(mfxSession session, mfxFrameAllocator *allocator);
mfxStatus MFX_CDECL MFXVideoCORE_SetHandle(mfxSession session, mfxHandleType type, mfxHDL hdl);
mfxStatus MFX_CDECL MFXVideoCORE_GetHandle(mfxSession session, mfxHandleType type, mfxHDL *hdl);
mfxStatus MFX_CDECL MFXVideoCORE_QueryPlatform(mfxSession session, mfxPlatform* platform);
mfxStatus MFX_CDECL MFXVideoCORE_SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait);

/* VideoENCODE */
mfxStatus MFX_CDECL MFXVideoENCODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out);
mfxStatus MFX_CDECL MFXVideoENCODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request);
mfxStatus MFX_CDECL MFXVideoENCODE_Init(mfxSession session, mfxVideoParam *par);
mfxStatus MFX_CDECL MFXVideoENCODE_Reset(mfxSession session, mfxVideoParam *par);
mfxStatus MFX_CDECL MFXVideoENCODE_Close(mfxSession session);

mfxStatus MFX_CDECL MFXVideoENCODE_GetVideoParam(mfxSession session, mfxVideoParam *par);
mfxStatus MFX_CDECL MFXVideoENCODE_GetEncodeStat(mfxSession session, mfxEncodeStat *stat);
mfxStatus MFX_CDECL MFXVideoENCODE_EncodeFrameAsync(mfxSession session, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp);

/* VideoDECODE */
mfxStatus MFX_CDECL MFXVideoDECODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out);
mfxStatus MFX_CDECL MFXVideoDECODE_DecodeHeader(mfxSession session, mfxBitstream *bs, mfxVideoParam *par);
mfxStatus MFX_CDECL MFXVideoDECODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request);
mfxStatus MFX_CDECL MFXVideoDECODE_Init(mfxSession session, mfxVideoParam *par);
mfxStatus MFX_CDECL MFXVideoDECODE_Reset(mfxSession session, mfxVideoParam *par);
mfxStatus MFX_CDECL MFXVideoDECODE_Close(mfxSession session);

mfxStatus MFX_CDECL MFXVideoDECODE_GetVideoParam(mfxSession session, mfxVideoParam *par);
mfxStatus MFX_CDECL MFXVideoDECODE_GetDecodeStat(mfxSession session, mfxDecodeStat *stat);
mfxStatus MFX_CDECL MFXVideoDECODE_SetSkipMode(mfxSession session, mfxSkipMode mode);
mfxStatus MFX_CDECL MFXVideoDECODE_GetPayload(mfxSession session, mfxU64 *ts, mfxPayload *payload);
mfxStatus MFX_CDECL MFXVideoDECODE_DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp);

/* VideoVPP */
mfxStatus MFX_CDECL MFXVideoVPP_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out);
mfxStatus MFX_CDECL MFXVideoVPP_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest request[2]);
mfxStatus MFX_CDECL MFXVideoVPP_Init(mfxSession session, mfxVideoParam *par);
mfxStatus MFX_CDECL MFXVideoVPP_Reset(mfxSession session, mfxVideoParam *par);
mfxStatus MFX_CDECL MFXVideoVPP_Close(mfxSession session);

mfxStatus MFX_CDECL MFXVideoVPP_GetVideoParam(mfxSession session, mfxVideoParam *par);
mfxStatus MFX_CDECL MFXVideoVPP_GetVPPStat(mfxSession session, mfxVPPStat *stat);
mfxStatus MFX_CDECL MFXVideoVPP_RunFrameVPPAsync(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp);
mfxStatus MFX_CDECL MFXVideoVPP_RunFrameVPPAsyncEx(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
