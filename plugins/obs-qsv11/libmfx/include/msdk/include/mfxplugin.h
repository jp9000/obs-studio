/******************************************************************************* *\

Copyright (C) 2007-2018 Intel Corporation.  All rights reserved.

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

File Name: mfxplugin.h

*******************************************************************************/
#ifndef __MFXPLUGIN_H__
#define __MFXPLUGIN_H__
#include "mfxvideo.h"
#include "mfxaudio.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

typedef struct {
    mfxU8  Data[16];
} mfxPluginUID;

static const mfxPluginUID  MFX_PLUGINID_HEVCD_SW        = {{0x15, 0xdd, 0x93, 0x68, 0x25, 0xad, 0x47, 0x5e, 0xa3, 0x4e, 0x35, 0xf3, 0xf5, 0x42, 0x17, 0xa6}};
static const mfxPluginUID  MFX_PLUGINID_HEVCD_HW        = {{0x33, 0xa6, 0x1c, 0x0b, 0x4c, 0x27, 0x45, 0x4c, 0xa8, 0xd8, 0x5d, 0xde, 0x75, 0x7c, 0x6f, 0x8e}};
static const mfxPluginUID  MFX_PLUGINID_HEVCE_SW        = {{0x2f, 0xca, 0x99, 0x74, 0x9f, 0xdb, 0x49, 0xae, 0xb1, 0x21, 0xa5, 0xb6, 0x3e, 0xf5, 0x68, 0xf7}};
static const mfxPluginUID  MFX_PLUGINID_HEVCE_GACC      = {{0xe5, 0x40, 0x0a, 0x06, 0xc7, 0x4d, 0x41, 0xf5, 0xb1, 0x2d, 0x43, 0x0b, 0xba, 0xa2, 0x3d, 0x0b}};
static const mfxPluginUID  MFX_PLUGINID_HEVCE_DP_GACC   = {{0x2b, 0xad, 0x6f, 0x9d, 0x77, 0x54, 0x41, 0x2d, 0xbf, 0x63, 0x03, 0xed, 0x4b, 0xb5, 0x09, 0x68}};
static const mfxPluginUID  MFX_PLUGINID_HEVCE_HW        = {{0x6f, 0xad, 0xc7, 0x91, 0xa0, 0xc2, 0xeb, 0x47, 0x9a, 0xb6, 0xdc, 0xd5, 0xea, 0x9d, 0xa3, 0x47}};
static const mfxPluginUID  MFX_PLUGINID_VP8D_HW         = {{0xf6, 0x22, 0x39, 0x4d, 0x8d, 0x87, 0x45, 0x2f, 0x87, 0x8c, 0x51, 0xf2, 0xfc, 0x9b, 0x41, 0x31}};
static const mfxPluginUID  MFX_PLUGINID_VP8E_HW         = {{0xbf, 0xfc, 0x51, 0x8c, 0xde, 0x13, 0x4d, 0xf9, 0x8a, 0x96, 0xf4, 0xcf, 0x81, 0x6c, 0x0f, 0xac}};
static const mfxPluginUID  MFX_PLUGINID_VP9E_HW         = {{0xce, 0x44, 0xef, 0x6f, 0x1a, 0x6d, 0x22, 0x46, 0xb4, 0x12, 0xbb, 0x38, 0xd6, 0xe4, 0x51, 0x82}};
static const mfxPluginUID  MFX_PLUGINID_VP9D_HW         = {{0xa9, 0x22, 0x39, 0x4d, 0x8d, 0x87, 0x45, 0x2f, 0x87, 0x8c, 0x51, 0xf2, 0xfc, 0x9b, 0x41, 0x31}};
static const mfxPluginUID  MFX_PLUGINID_CAMERA_HW       = {{0x54, 0x54, 0x26, 0x16, 0x24, 0x33, 0x41, 0xe6, 0x93, 0xae, 0x89, 0x99, 0x42, 0xce, 0x73, 0x55}};
static const mfxPluginUID  MFX_PLUGINID_CAPTURE_HW      = {{0x22, 0xd6, 0x2c, 0x07, 0xe6, 0x72, 0x40, 0x8f, 0xbb, 0x4c, 0xc2, 0x0e, 0xd7, 0xa0, 0x53, 0xe4}};
static const mfxPluginUID  MFX_PLUGINID_ITELECINE_HW    = {{0xe7, 0x44, 0x75, 0x3a, 0xcd, 0x74, 0x40, 0x2e, 0x89, 0xa2, 0xee, 0x06, 0x35, 0x49, 0x61, 0x79}};
static const mfxPluginUID  MFX_PLUGINID_H264LA_HW       = {{0x58, 0x8f, 0x11, 0x85, 0xd4, 0x7b, 0x42, 0x96, 0x8d, 0xea, 0x37, 0x7b, 0xb5, 0xd0, 0xdc, 0xb4}};
static const mfxPluginUID  MFX_PLUGINID_AACD            = {{0xe9, 0x34, 0x67, 0x25, 0xac, 0x2f, 0x4c, 0x93, 0xaa, 0x58, 0x5c, 0x11, 0xc7, 0x08, 0x7c, 0xf4}};
static const mfxPluginUID  MFX_PLUGINID_AACE            = {{0xb2, 0xa2, 0xa0, 0x5a, 0x4e, 0xac, 0x46, 0xbf, 0xa9, 0xde, 0x7e, 0x80, 0xc9, 0x8d, 0x2e, 0x18}};
static const mfxPluginUID  MFX_PLUGINID_HEVCE_FEI_HW    = {{0x87, 0xe0, 0xe8, 0x02, 0x07, 0x37, 0x52, 0x40, 0x85, 0x25, 0x15, 0xcf, 0x4a, 0x5e, 0xdd, 0xe6}};
static const mfxPluginUID  MFX_PLUGINID_HEVC_FEI_ENCODE = {{0x54, 0x18, 0xa7, 0x06, 0x66, 0xf9, 0x4d, 0x5c, 0xb4, 0xf7, 0xb1, 0xca, 0xee, 0x86, 0x33, 0x9b}};


typedef enum {
    MFX_PLUGINTYPE_VIDEO_GENERAL   = 0,
    MFX_PLUGINTYPE_VIDEO_DECODE    = 1,
    MFX_PLUGINTYPE_VIDEO_ENCODE    = 2,
    MFX_PLUGINTYPE_VIDEO_VPP       = 3,
    MFX_PLUGINTYPE_VIDEO_ENC       = 4,
    MFX_PLUGINTYPE_AUDIO_DECODE    = 5,
    MFX_PLUGINTYPE_AUDIO_ENCODE    = 6
} mfxPluginType;

typedef enum {
    MFX_THREADPOLICY_SERIAL    = 0,
    MFX_THREADPOLICY_PARALLEL  = 1
} mfxThreadPolicy;

typedef struct mfxPluginParam {
    mfxU32          reserved[6];
    mfxU16          reserved1;
    mfxU16          PluginVersion;
    mfxVersion      APIVersion;
    mfxPluginUID    PluginUID;
    mfxU32          Type;
    mfxU32          CodecId;
    mfxThreadPolicy ThreadPolicy;
    mfxU32          MaxThreadNum;
} mfxPluginParam;

typedef struct mfxCoreParam{
    mfxU32     reserved[13];
    mfxIMPL    Impl;
    mfxVersion Version;
    mfxU32     NumWorkingThread;
} mfxCoreParam;

typedef struct mfxCoreInterface {
    mfxHDL pthis;

    mfxHDL reserved1[2];
    mfxFrameAllocator FrameAllocator;
    mfxBufferAllocator reserved3;

    mfxStatus (MFX_CDECL *GetCoreParam)(mfxHDL pthis, mfxCoreParam *par);
    mfxStatus (MFX_CDECL *GetHandle) (mfxHDL pthis, mfxHandleType type, mfxHDL *handle);
    mfxStatus (MFX_CDECL *IncreaseReference) (mfxHDL pthis, mfxFrameData *fd);
    mfxStatus (MFX_CDECL *DecreaseReference) (mfxHDL pthis, mfxFrameData *fd);
    mfxStatus (MFX_CDECL *CopyFrame) (mfxHDL pthis, mfxFrameSurface1 *dst, mfxFrameSurface1 *src);
    mfxStatus (MFX_CDECL *CopyBuffer)(mfxHDL pthis, mfxU8 *dst, mfxU32 size, mfxFrameSurface1 *src);

    mfxStatus (MFX_CDECL *MapOpaqueSurface)(mfxHDL pthis, mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf);
    mfxStatus (MFX_CDECL *UnmapOpaqueSurface)(mfxHDL pthis, mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf);

    mfxStatus (MFX_CDECL *GetRealSurface)(mfxHDL pthis, mfxFrameSurface1 *op_surf, mfxFrameSurface1 **surf);
    mfxStatus (MFX_CDECL *GetOpaqueSurface)(mfxHDL pthis, mfxFrameSurface1 *surf, mfxFrameSurface1 **op_surf);

    mfxStatus (MFX_CDECL *CreateAccelerationDevice)(mfxHDL pthis, mfxHandleType type, mfxHDL *handle);
    mfxStatus (MFX_CDECL *GetFrameHandle) (mfxHDL pthis, mfxFrameData *fd, mfxHDL *handle);
    mfxStatus (MFX_CDECL *QueryPlatform) (mfxHDL pthis, mfxPlatform *platform);

    mfxHDL reserved4[1];
} mfxCoreInterface;

/* video codec plugin extension */
typedef struct _mfxENCInput mfxENCInput;
typedef struct _mfxENCOutput mfxENCOutput;
typedef struct mfxVideoCodecPlugin{
    mfxStatus (MFX_CDECL *Query)(mfxHDL pthis, mfxVideoParam *in, mfxVideoParam *out);
    mfxStatus (MFX_CDECL *QueryIOSurf)(mfxHDL pthis, mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out);
    mfxStatus (MFX_CDECL *Init)(mfxHDL pthis, mfxVideoParam *par);
    mfxStatus (MFX_CDECL *Reset)(mfxHDL pthis, mfxVideoParam *par);
    mfxStatus (MFX_CDECL *Close)(mfxHDL pthis);
    mfxStatus (MFX_CDECL *GetVideoParam)(mfxHDL pthis, mfxVideoParam *par);

    mfxStatus (MFX_CDECL *EncodeFrameSubmit)(mfxHDL pthis, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *task);

    mfxStatus (MFX_CDECL *DecodeHeader)(mfxHDL pthis, mfxBitstream *bs, mfxVideoParam *par);
    mfxStatus (MFX_CDECL *GetPayload)(mfxHDL pthis, mfxU64 *ts, mfxPayload *payload);
    mfxStatus (MFX_CDECL *DecodeFrameSubmit)(mfxHDL pthis, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,  mfxThreadTask *task);

    mfxStatus (MFX_CDECL *VPPFrameSubmit)(mfxHDL pthis,  mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxThreadTask *task);
    mfxStatus (MFX_CDECL *VPPFrameSubmitEx)(mfxHDL pthis,  mfxFrameSurface1 *in, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxThreadTask *task);

    mfxStatus (MFX_CDECL *ENCFrameSubmit)(mfxHDL pthis,  mfxENCInput *in, mfxENCOutput *out, mfxThreadTask *task);

    mfxHDL reserved1[3];
    mfxU32 reserved2[8];
} mfxVideoCodecPlugin;

typedef struct mfxAudioCodecPlugin{
    mfxStatus (MFX_CDECL *Query)(mfxHDL pthis, mfxAudioParam *in, mfxAudioParam *out);
    mfxStatus (MFX_CDECL *QueryIOSize)(mfxHDL pthis, mfxAudioParam *par, mfxAudioAllocRequest *request);
    mfxStatus (MFX_CDECL *Init)(mfxHDL pthis, mfxAudioParam *par);
    mfxStatus (MFX_CDECL *Reset)(mfxHDL pthis, mfxAudioParam *par);
    mfxStatus (MFX_CDECL *Close)(mfxHDL pthis);
    mfxStatus (MFX_CDECL *GetAudioParam)(mfxHDL pthis, mfxAudioParam *par);

    mfxStatus (MFX_CDECL *EncodeFrameSubmit)(mfxHDL pthis, mfxAudioFrame *aFrame, mfxBitstream *out, mfxThreadTask *task);

    mfxStatus (MFX_CDECL *DecodeHeader)(mfxHDL pthis, mfxBitstream *bs, mfxAudioParam *par);
//    mfxStatus (MFX_CDECL *GetPayload)(mfxHDL pthis, mfxU64 *ts, mfxPayload *payload);
    mfxStatus (MFX_CDECL *DecodeFrameSubmit)(mfxHDL pthis, mfxBitstream *in, mfxAudioFrame *out, mfxThreadTask *task);

    mfxHDL reserved1[6];
    mfxU32 reserved2[8];
} mfxAudioCodecPlugin;

typedef struct mfxPlugin{
    mfxHDL pthis;

    mfxStatus (MFX_CDECL *PluginInit) (mfxHDL pthis, mfxCoreInterface *core);
    mfxStatus (MFX_CDECL *PluginClose) (mfxHDL pthis);

    mfxStatus (MFX_CDECL *GetPluginParam)(mfxHDL pthis, mfxPluginParam *par);

    mfxStatus (MFX_CDECL *Submit)(mfxHDL pthis, const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxThreadTask *task);
    mfxStatus (MFX_CDECL *Execute)(mfxHDL pthis, mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a);
    mfxStatus (MFX_CDECL *FreeResources)(mfxHDL pthis, mfxThreadTask task, mfxStatus sts);

    union {
        mfxVideoCodecPlugin  *Video;
        mfxAudioCodecPlugin  *Audio;
    };

    mfxHDL reserved[8];
} mfxPlugin;



mfxStatus MFX_CDECL MFXVideoUSER_Register(mfxSession session, mfxU32 type, const mfxPlugin *par);
mfxStatus MFX_CDECL MFXVideoUSER_Unregister(mfxSession session, mfxU32 type);
mfxStatus MFX_CDECL MFXVideoUSER_GetPlugin(mfxSession session, mfxU32 type, mfxPlugin *par);
mfxStatus MFX_CDECL MFXVideoUSER_ProcessFrameAsync(mfxSession session, const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxSyncPoint *syncp);

mfxStatus MFX_CDECL MFXVideoUSER_Load(mfxSession session, const mfxPluginUID *uid, mfxU32 version);
mfxStatus MFX_CDECL MFXVideoUSER_LoadByPath(mfxSession session, const mfxPluginUID *uid, mfxU32 version, const mfxChar *path, mfxU32 len);
mfxStatus MFX_CDECL MFXVideoUSER_UnLoad(mfxSession session, const mfxPluginUID *uid);

mfxStatus MFX_CDECL MFXAudioUSER_Register(mfxSession session, mfxU32 type, const mfxPlugin *par);
mfxStatus MFX_CDECL MFXAudioUSER_Unregister(mfxSession session, mfxU32 type);
mfxStatus MFX_CDECL MFXAudioUSER_ProcessFrameAsync(mfxSession session, const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxSyncPoint *syncp);

mfxStatus MFX_CDECL MFXAudioUSER_Load(mfxSession session, const mfxPluginUID *uid, mfxU32 version);
mfxStatus MFX_CDECL MFXAudioUSER_UnLoad(mfxSession session, const mfxPluginUID *uid);

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */

#endif /* __MFXPLUGIN_H__ */
