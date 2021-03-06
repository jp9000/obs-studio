/******************************************************************************* *\

Copyright (C) 2014-2017 Intel Corporation.  All rights reserved.

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

File Name: mfxpak.h

*******************************************************************************/
#ifndef __MFXPAK_H__
#define __MFXPAK_H__
#include "mfxdefs.h"
#include "mfxvstructures.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

typedef struct {
    mfxU16 reserved[32];

    mfxFrameSurface1 *InSurface;

    mfxU16  NumFrameL0;
    mfxFrameSurface1 **L0Surface;
    mfxU16  NumFrameL1;
    mfxFrameSurface1 **L1Surface;

    mfxU16  NumExtParam;
    mfxExtBuffer    **ExtParam;

    mfxU16 NumPayload;
    mfxPayload      **Payload;
} mfxPAKInput;

typedef struct {
    mfxU16  reserved[32];

    mfxBitstream     *Bs;

    mfxFrameSurface1 *OutSurface;

    mfxU16            NumExtParam;
    mfxExtBuffer    **ExtParam;
} mfxPAKOutput;

typedef struct _mfxSession *mfxSession;
mfxStatus MFX_CDECL MFXVideoPAK_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out);
mfxStatus MFX_CDECL MFXVideoPAK_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest request[2]);
mfxStatus MFX_CDECL MFXVideoPAK_Init(mfxSession session, mfxVideoParam *par);
mfxStatus MFX_CDECL MFXVideoPAK_Reset(mfxSession session, mfxVideoParam *par);
mfxStatus MFX_CDECL MFXVideoPAK_Close(mfxSession session);

mfxStatus MFX_CDECL MFXVideoPAK_ProcessFrameAsync(mfxSession session, mfxPAKInput *in, mfxPAKOutput *out,  mfxSyncPoint *syncp);

mfxStatus MFX_CDECL MFXVideoPAK_GetVideoParam(mfxSession session, mfxVideoParam *par);

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */


#endif
