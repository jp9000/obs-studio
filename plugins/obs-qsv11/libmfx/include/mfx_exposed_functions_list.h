/* ****************************************************************************** *\

Copyright (C) 2012-2016 Intel Corporation.  All rights reserved.

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

File Name: mfx_exposed_functions_list.h

\* ****************************************************************************** */

//
// WARNING:
// this file doesn't contain an include guard by intension.
// The file may be included into a source file many times.
// That is why this header doesn't contain any include directive.
// Please, do no try to fix it.
//



// Use define API_VERSION to set the API of functions listed further
// When new functions are added new section with functions declarations must be started with updated define

//
// API version 1.0 functions
//

// API version where a function is added. Minor value should precedes the major value
#define API_VERSION {{0, 1}}

// CORE interface functions
FUNCTION(mfxStatus, MFXVideoCORE_SetBufferAllocator, (mfxSession session, mfxBufferAllocator *allocator), (session, allocator))
FUNCTION(mfxStatus, MFXVideoCORE_SetFrameAllocator, (mfxSession session, mfxFrameAllocator *allocator), (session, allocator))
FUNCTION(mfxStatus, MFXVideoCORE_SetHandle, (mfxSession session, mfxHandleType type, mfxHDL hdl), (session, type, hdl))
FUNCTION(mfxStatus, MFXVideoCORE_GetHandle, (mfxSession session, mfxHandleType type, mfxHDL *hdl), (session, type, hdl))

FUNCTION(mfxStatus, MFXVideoCORE_SyncOperation, (mfxSession session, mfxSyncPoint syncp, mfxU32 wait), (session, syncp, wait))

// ENCODE interface functions
FUNCTION(mfxStatus, MFXVideoENCODE_Query, (mfxSession session, mfxVideoParam *in, mfxVideoParam *out), (session, in, out))
FUNCTION(mfxStatus, MFXVideoENCODE_QueryIOSurf, (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request), (session, par, request))
FUNCTION(mfxStatus, MFXVideoENCODE_Init, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoENCODE_Reset, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoENCODE_Close, (mfxSession session), (session))

FUNCTION(mfxStatus, MFXVideoENCODE_GetVideoParam, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoENCODE_GetEncodeStat, (mfxSession session, mfxEncodeStat *stat), (session, stat))
FUNCTION(mfxStatus, MFXVideoENCODE_EncodeFrameAsync, (mfxSession session, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp), (session, ctrl, surface, bs, syncp))

// DECODE interface functions
FUNCTION(mfxStatus, MFXVideoDECODE_Query, (mfxSession session, mfxVideoParam *in, mfxVideoParam *out), (session, in, out))
FUNCTION(mfxStatus, MFXVideoDECODE_DecodeHeader, (mfxSession session, mfxBitstream *bs, mfxVideoParam *par), (session, bs, par))
FUNCTION(mfxStatus, MFXVideoDECODE_QueryIOSurf, (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request), (session, par, request))
FUNCTION(mfxStatus, MFXVideoDECODE_Init, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoDECODE_Reset, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoDECODE_Close, (mfxSession session), (session))

FUNCTION(mfxStatus, MFXVideoDECODE_GetVideoParam, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoDECODE_GetDecodeStat, (mfxSession session, mfxDecodeStat *stat), (session, stat))
FUNCTION(mfxStatus, MFXVideoDECODE_SetSkipMode, (mfxSession session, mfxSkipMode mode), (session, mode))
FUNCTION(mfxStatus, MFXVideoDECODE_GetPayload, (mfxSession session, mfxU64 *ts, mfxPayload *payload), (session, ts, payload))
FUNCTION(mfxStatus, MFXVideoDECODE_DecodeFrameAsync, (mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp), (session, bs, surface_work, surface_out, syncp))

// VPP interface functions
FUNCTION(mfxStatus, MFXVideoVPP_Query, (mfxSession session, mfxVideoParam *in, mfxVideoParam *out), (session, in, out))
FUNCTION(mfxStatus, MFXVideoVPP_QueryIOSurf, (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request), (session, par, request))
FUNCTION(mfxStatus, MFXVideoVPP_Init, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoVPP_Reset, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoVPP_Close, (mfxSession session), (session))

FUNCTION(mfxStatus, MFXVideoVPP_GetVideoParam, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoVPP_GetVPPStat, (mfxSession session, mfxVPPStat *stat), (session, stat))
FUNCTION(mfxStatus, MFXVideoVPP_RunFrameVPPAsync, (mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp), (session, in, out, aux, syncp))

#undef API_VERSION

//
// API version 1.1 functions
//

#define API_VERSION {{1, 1}}

FUNCTION(mfxStatus, MFXVideoUSER_Register, (mfxSession session, mfxU32 type, const mfxPlugin *par), (session, type, par))
FUNCTION(mfxStatus, MFXVideoUSER_Unregister, (mfxSession session, mfxU32 type), (session, type))
FUNCTION(mfxStatus, MFXVideoUSER_ProcessFrameAsync, (mfxSession session, const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxSyncPoint *syncp), (session, in, in_num, out, out_num, syncp))

#undef API_VERSION

//
// API version 1.10 functions
//

#define API_VERSION {{10, 1}}

FUNCTION(mfxStatus, MFXVideoENC_Query,(mfxSession session, mfxVideoParam *in, mfxVideoParam *out), (session,in,out))
FUNCTION(mfxStatus, MFXVideoENC_QueryIOSurf,(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request), (session,par,request))
FUNCTION(mfxStatus, MFXVideoENC_Init,(mfxSession session, mfxVideoParam *par), (session,par))
FUNCTION(mfxStatus, MFXVideoENC_Reset,(mfxSession session, mfxVideoParam *par), (session,par))
FUNCTION(mfxStatus, MFXVideoENC_Close,(mfxSession session),(session))
FUNCTION(mfxStatus, MFXVideoENC_ProcessFrameAsync,(mfxSession session, mfxENCInput *in, mfxENCOutput *out, mfxSyncPoint *syncp),(session,in,out,syncp))

FUNCTION(mfxStatus, MFXVideoVPP_RunFrameVPPAsyncEx, (mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *work, mfxFrameSurface1 **out, mfxSyncPoint *syncp), (session, in, work, out, syncp))

#undef API_VERSION

#define API_VERSION {{13, 1}}

FUNCTION(mfxStatus, MFXVideoPAK_Query, (mfxSession session, mfxVideoParam *in, mfxVideoParam *out), (session, in, out))
FUNCTION(mfxStatus, MFXVideoPAK_QueryIOSurf, (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request), (session, par, request))
FUNCTION(mfxStatus, MFXVideoPAK_Init, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoPAK_Reset, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoPAK_Close, (mfxSession session), (session))
FUNCTION(mfxStatus, MFXVideoPAK_ProcessFrameAsync, (mfxSession session, mfxPAKInput *in, mfxPAKOutput *out, mfxSyncPoint *syncp), (session, in, out, syncp))

#undef API_VERSION

#define API_VERSION {{14, 1}}

// FUNCTION(mfxStatus, MFXInitEx, (mfxInitParam par, mfxSession session), (par, session))
FUNCTION(mfxStatus, MFXDoWork, (mfxSession session), (session))

#undef API_VERSION

#define API_VERSION {{19, 1}}

FUNCTION(mfxStatus, MFXVideoENC_GetVideoParam, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoPAK_GetVideoParam, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoCORE_QueryPlatform, (mfxSession session, mfxPlatform* platform), (session, platform))
FUNCTION(mfxStatus, MFXVideoUSER_GetPlugin, (mfxSession session, mfxU32 type, mfxPlugin *par), (session, type, par))

#undef API_VERSION