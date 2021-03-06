/* ****************************************************************************** *\

Copyright (C) 2007-2016 Intel Corporation.  All rights reserved.

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


File Name: mfxplugin++.h

\* ****************************************************************************** */

#ifndef __MFXPLUGINPLUSPLUS_H
#define __MFXPLUGINPLUSPLUS_H

#include "mfxplugin.h"

// base class for MFXVideoUSER/MFXAudioUSER API

class MFXBaseUSER {
public:
    explicit MFXBaseUSER(mfxSession session = NULL)
        : m_session(session){}

    virtual ~MFXBaseUSER() {};

    virtual mfxStatus Register(mfxU32 type, const mfxPlugin *par) = 0;
    virtual mfxStatus Unregister(mfxU32 type) = 0;
    virtual mfxStatus ProcessFrameAsync(const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxSyncPoint *syncp) = 0;

protected:
    mfxSession m_session;
};

//c++ wrapper over only 3 exposed functions from MFXVideoUSER module
class MFXVideoUSER: public MFXBaseUSER {
public:
    explicit MFXVideoUSER(mfxSession session = NULL)
        : MFXBaseUSER(session){}

    virtual mfxStatus Register(mfxU32 type, const mfxPlugin *par) {
        return MFXVideoUSER_Register(m_session, type, par);
    }
    virtual mfxStatus Unregister(mfxU32 type) {
        return MFXVideoUSER_Unregister(m_session, type);
    }
    virtual mfxStatus ProcessFrameAsync(const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxSyncPoint *syncp) {
        return MFXVideoUSER_ProcessFrameAsync(m_session, in, in_num, out, out_num, syncp);
    }
};

//c++ wrapper over only 3 exposed functions from MFXAudioUSER module
class MFXAudioUSER: public MFXBaseUSER {
public:
    explicit MFXAudioUSER(mfxSession session = NULL)
        : MFXBaseUSER(session){}

    virtual mfxStatus Register(mfxU32 type, const mfxPlugin *par) {
        return MFXAudioUSER_Register(m_session, type, par);
    }
    virtual mfxStatus Unregister(mfxU32 type) {
        return MFXAudioUSER_Unregister(m_session, type);
    }
    virtual mfxStatus ProcessFrameAsync(const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxSyncPoint *syncp) {
        return MFXAudioUSER_ProcessFrameAsync(m_session, in, in_num, out, out_num, syncp);
    }
};


//initialize mfxPlugin struct
class MFXPluginParam {
    mfxPluginParam m_param;

public:
    MFXPluginParam(mfxU32 CodecId, mfxU32  Type, mfxPluginUID uid, mfxThreadPolicy ThreadPolicy = MFX_THREADPOLICY_SERIAL, mfxU32  MaxThreadNum = 1)
        : m_param() {
        m_param.PluginUID = uid;
        m_param.Type = Type;
        m_param.CodecId = CodecId;
        m_param.MaxThreadNum = MaxThreadNum;
        m_param.ThreadPolicy = ThreadPolicy;
    }
    operator const mfxPluginParam& () const {
        return m_param;
    }
    operator mfxPluginParam& () {
        return m_param;
    }
};

//common interface part for every plugin: decoder/encoder and generic
struct MFXPlugin
{
    virtual ~MFXPlugin() {};
    //init function always required for any transform or codec plugins, for codec plugins it maps to callback from MediaSDK
    //for generic plugin application should call it
    //MediaSDK mfxPlugin API mapping
    virtual mfxStatus PluginInit(mfxCoreInterface *core) = 0;
    //release CoreInterface, and destroy plugin state, not destroy plugin instance
    virtual mfxStatus PluginClose() = 0;
    virtual mfxStatus GetPluginParam(mfxPluginParam *par) = 0;
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a) = 0;
    virtual mfxStatus FreeResources(mfxThreadTask task, mfxStatus sts) = 0;
    //destroy plugin due to shared module distribution model plugin wont support virtual destructor
    virtual void      Release() = 0;
    //release resources associated with current instance of plugin, but do not release CoreInterface related resource set in pluginInit
    virtual mfxStatus Close()  = 0;
    //communication protocol between particular version of plugin and application
    virtual mfxStatus SetAuxParams(void* auxParam, int auxParamSize) = 0;
};

//common extension interface that codec plugins should expose additionally to MFXPlugin
struct MFXCodecPlugin : MFXPlugin
{
    virtual mfxStatus Init(mfxVideoParam *par) = 0;
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out) = 0;
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) =0;
    virtual mfxStatus Reset(mfxVideoParam *par) = 0;
    virtual mfxStatus GetVideoParam(mfxVideoParam *par) = 0;
};

//common extension interface that audio codec plugins should expose additionally to MFXPlugin
struct MFXAudioCodecPlugin : MFXPlugin
{
    virtual mfxStatus Init(mfxAudioParam *par) = 0;
    virtual mfxStatus Query(mfxAudioParam *in, mfxAudioParam *out) =0;
    virtual mfxStatus QueryIOSize(mfxAudioParam *par, mfxAudioAllocRequest *request) = 0;
    virtual mfxStatus Reset(mfxAudioParam *par) = 0;
    virtual mfxStatus GetAudioParam(mfxAudioParam *par) = 0;
};

//general purpose transform plugin interface, not a codec plugin
struct MFXGenericPlugin : MFXPlugin
{
    virtual mfxStatus Init(mfxVideoParam *par) = 0;
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out) = 0;
    virtual mfxStatus Submit(const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxThreadTask *task) = 0;
};

//decoder plugins may only support this interface
struct MFXDecoderPlugin : MFXCodecPlugin
{
    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par) = 0;
    virtual mfxStatus GetPayload(mfxU64 *ts, mfxPayload *payload) = 0;
    virtual mfxStatus DecodeFrameSubmit(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,  mfxThreadTask *task) = 0;
};

//audio decoder plugins may only support this interface
struct MFXAudioDecoderPlugin : MFXAudioCodecPlugin
{
    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxAudioParam *par) = 0;
//    virtual mfxStatus GetPayload(mfxU64 *ts, mfxPayload *payload) = 0;
    virtual mfxStatus DecodeFrameSubmit(mfxBitstream *in, mfxAudioFrame *out, mfxThreadTask *task) = 0;
};

//encoder plugins may only support this interface
struct MFXEncoderPlugin : MFXCodecPlugin
{
    virtual mfxStatus EncodeFrameSubmit(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *task) = 0;
};

//audio encoder plugins may only support this interface
struct MFXAudioEncoderPlugin : MFXAudioCodecPlugin
{
    virtual mfxStatus EncodeFrameSubmit(mfxAudioFrame *aFrame, mfxBitstream *out, mfxThreadTask *task) = 0;
};

//vpp plugins may only support this interface
struct MFXVPPPlugin : MFXCodecPlugin
{
    virtual mfxStatus VPPFrameSubmit(mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_out, mfxExtVppAuxData *aux, mfxThreadTask *task) = 0;
    virtual mfxStatus VPPFrameSubmitEx(mfxFrameSurface1 *in, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxThreadTask *task) = 0;
};

struct MFXEncPlugin : MFXCodecPlugin
{
    virtual mfxStatus EncFrameSubmit(mfxENCInput *in, mfxENCOutput *out, mfxThreadTask *task) = 0;
};




class MFXCoreInterface
{
protected:
    mfxCoreInterface m_core;
public:

    MFXCoreInterface()
        : m_core() {
    }
    MFXCoreInterface(const mfxCoreInterface & pCore)
        : m_core(pCore) {
    }

    MFXCoreInterface(const MFXCoreInterface & that)
        : m_core(that.m_core) {
    }
    MFXCoreInterface &operator = (const MFXCoreInterface & that)
    {
        m_core = that.m_core;
        return *this;
    }
    bool IsCoreSet() {
        return m_core.pthis != 0;
    }
    mfxStatus GetCoreParam(mfxCoreParam *par) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.GetCoreParam(m_core.pthis, par);
    }
    mfxStatus GetHandle (mfxHandleType type, mfxHDL *handle) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.GetHandle(m_core.pthis, type, handle);
    }
    mfxStatus IncreaseReference (mfxFrameData *fd) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.IncreaseReference(m_core.pthis, fd);
    }
    mfxStatus DecreaseReference (mfxFrameData *fd) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.DecreaseReference(m_core.pthis, fd);
    }
    mfxStatus CopyFrame (mfxFrameSurface1 *dst, mfxFrameSurface1 *src) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.CopyFrame(m_core.pthis, dst, src);
    }
    mfxStatus CopyBuffer(mfxU8 *dst, mfxU32 size, mfxFrameSurface1 *src) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.CopyBuffer(m_core.pthis, dst, size, src);
    }
    mfxStatus MapOpaqueSurface(mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.MapOpaqueSurface(m_core.pthis, num, type, op_surf);
    }
    mfxStatus UnmapOpaqueSurface(mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.UnmapOpaqueSurface(m_core.pthis, num, type, op_surf);
    }
    mfxStatus GetRealSurface(mfxFrameSurface1 *op_surf, mfxFrameSurface1 **surf) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.GetRealSurface(m_core.pthis, op_surf, surf);
    }
    mfxStatus GetOpaqueSurface(mfxFrameSurface1 *surf, mfxFrameSurface1 **op_surf) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.GetOpaqueSurface(m_core.pthis, surf, op_surf);
    }
    mfxStatus CreateAccelerationDevice(mfxHandleType type, mfxHDL *handle) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.CreateAccelerationDevice(m_core.pthis, type, handle);
    }
    mfxFrameAllocator & FrameAllocator() {
        return m_core.FrameAllocator;
    }
    mfxStatus GetFrameHandle(mfxFrameData *fd, mfxHDL *handle) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.GetFrameHandle(m_core.pthis, fd, handle);
    }
    mfxStatus QueryPlatform(mfxPlatform *platform) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.QueryPlatform(m_core.pthis, platform);
    }
} ;

/* Class adapter between "C" structure mfxPlugin and C++ interface MFXPlugin */

namespace detail
{
    template <class T>
    class MFXPluginAdapterBase
    {
    protected:
        mfxPlugin m_mfxAPI;
    public:
        MFXPluginAdapterBase( T *plugin, mfxVideoCodecPlugin *pCodec = NULL)
            : m_mfxAPI()
        {
            SetupCallbacks(plugin, pCodec);
        }

        MFXPluginAdapterBase( T *plugin, mfxAudioCodecPlugin *pCodec)
            : m_mfxAPI()
        {
            SetupCallbacks(plugin, pCodec);
        }

        operator  mfxPlugin () const {
            return m_mfxAPI;
        }
        void SetupCallbacks(T *plugin) {
            m_mfxAPI.pthis = plugin;
            m_mfxAPI.PluginInit = _PluginInit;
            m_mfxAPI.PluginClose = _PluginClose;
            m_mfxAPI.GetPluginParam = _GetPluginParam;
            m_mfxAPI.Submit = 0;
            m_mfxAPI.Execute = _Execute;
            m_mfxAPI.FreeResources = _FreeResources;
        }

        void SetupCallbacks( T *plugin, mfxVideoCodecPlugin *pCodec) {
            SetupCallbacks(plugin);
            m_mfxAPI.Video = pCodec;
        }

        void SetupCallbacks( T *plugin, mfxAudioCodecPlugin *pCodec) {
            SetupCallbacks(plugin);
            m_mfxAPI.Audio = pCodec;
        }
    private:

        static mfxStatus _PluginInit(mfxHDL pthis, mfxCoreInterface *core) {
            return reinterpret_cast<T*>(pthis)->PluginInit(core);
        }
        static mfxStatus _PluginClose(mfxHDL pthis) {
            return reinterpret_cast<T*>(pthis)->PluginClose();
        }
        static mfxStatus _GetPluginParam(mfxHDL pthis, mfxPluginParam *par) {
            return reinterpret_cast<T*>(pthis)->GetPluginParam(par);
        }
        static mfxStatus _Execute(mfxHDL pthis, mfxThreadTask task, mfxU32 thread_id, mfxU32 call_count) {
            return reinterpret_cast<T*>(pthis)->Execute(task, thread_id, call_count);
        }
        static mfxStatus _FreeResources(mfxHDL pthis, mfxThreadTask task, mfxStatus sts) {
            return reinterpret_cast<T*>(pthis)->FreeResources(task, sts);
        }
    };

    template<class T>
    class MFXCodecPluginAdapterBase : public MFXPluginAdapterBase<T>
    {
    protected:
        //stub to feed mediasdk plugin API
        mfxVideoCodecPlugin   m_codecPlg;
    public:
        MFXCodecPluginAdapterBase(T * pCodecPlg)
            : MFXPluginAdapterBase<T>(pCodecPlg, &m_codecPlg)
            , m_codecPlg()
        {
            m_codecPlg.Query = _Query;
            m_codecPlg.QueryIOSurf = _QueryIOSurf ;
            m_codecPlg.Init = _Init;
            m_codecPlg.Reset = _Reset;
            m_codecPlg.Close = _Close;
            m_codecPlg.GetVideoParam = _GetVideoParam;
        }
        MFXCodecPluginAdapterBase(const MFXCodecPluginAdapterBase<T> & that)
            : MFXPluginAdapterBase<T>(reinterpret_cast<T*>(that.m_mfxAPI.pthis), &m_codecPlg)
            , m_codecPlg() {
            SetupCallbacks();
        }
        MFXCodecPluginAdapterBase<T>& operator = (const MFXCodecPluginAdapterBase<T> & that) {
            MFXPluginAdapterBase<T> :: SetupCallbacks(reinterpret_cast<T*>(that.m_mfxAPI.pthis), &m_codecPlg);
            SetupCallbacks();
            return *this;
        }

    private:
        void SetupCallbacks() {
            m_codecPlg.Query = _Query;
            m_codecPlg.QueryIOSurf = _QueryIOSurf ;
            m_codecPlg.Init = _Init;
            m_codecPlg.Reset = _Reset;
            m_codecPlg.Close = _Close;
            m_codecPlg.GetVideoParam = _GetVideoParam;
        }
        static mfxStatus _Query(mfxHDL pthis, mfxVideoParam *in, mfxVideoParam *out) {
            return reinterpret_cast<T*>(pthis)->Query(in, out);
        }
        static mfxStatus _QueryIOSurf(mfxHDL pthis, mfxVideoParam *par, mfxFrameAllocRequest *in,  mfxFrameAllocRequest *out){
            return reinterpret_cast<T*>(pthis)->QueryIOSurf(par, in, out);
        }
        static mfxStatus _Init(mfxHDL pthis, mfxVideoParam *par){
            return reinterpret_cast<T*>(pthis)->Init(par);
        }
        static mfxStatus _Reset(mfxHDL pthis, mfxVideoParam *par){
            return reinterpret_cast<T*>(pthis)->Reset(par);
        }
        static mfxStatus _Close(mfxHDL pthis) {
            return reinterpret_cast<T*>(pthis)->Close();
        }
        static mfxStatus _GetVideoParam(mfxHDL pthis, mfxVideoParam *par) {
            return reinterpret_cast<T*>(pthis)->GetVideoParam(par);
        }
    };

    template<class T>
    class MFXAudioCodecPluginAdapterBase : public MFXPluginAdapterBase<T>
    {
    protected:
        //stub to feed mediasdk plugin API
        mfxAudioCodecPlugin   m_codecPlg;
    public:
        MFXAudioCodecPluginAdapterBase(T * pCodecPlg)
            : MFXPluginAdapterBase<T>(pCodecPlg, &m_codecPlg)
            , m_codecPlg()
        {
            m_codecPlg.Query = _Query;
            m_codecPlg.QueryIOSize = _QueryIOSize ;
            m_codecPlg.Init = _Init;
            m_codecPlg.Reset = _Reset;
            m_codecPlg.Close = _Close;
            m_codecPlg.GetAudioParam = _GetAudioParam;
        }
        MFXAudioCodecPluginAdapterBase(const MFXCodecPluginAdapterBase<T> & that)
            : MFXPluginAdapterBase<T>(reinterpret_cast<T*>(that.m_mfxAPI.pthis), &m_codecPlg)
            , m_codecPlg() {
            SetupCallbacks();
        }
        MFXAudioCodecPluginAdapterBase<T>& operator = (const MFXAudioCodecPluginAdapterBase<T> & that) {
            MFXPluginAdapterBase<T> :: SetupCallbacks(reinterpret_cast<T*>(that.m_mfxAPI.pthis), &m_codecPlg);
            SetupCallbacks();
            return *this;
        }

    private:
        void SetupCallbacks() {
            m_codecPlg.Query = _Query;
            m_codecPlg.QueryIOSize = _QueryIOSize;
            m_codecPlg.Init = _Init;
            m_codecPlg.Reset = _Reset;
            m_codecPlg.Close = _Close;
            m_codecPlg.GetAudioParam = _GetAudioParam;
        }
        static mfxStatus _Query(mfxHDL pthis, mfxAudioParam *in, mfxAudioParam *out) {
            return reinterpret_cast<T*>(pthis)->Query(in, out);
        }
        static mfxStatus _QueryIOSize(mfxHDL pthis, mfxAudioParam *par, mfxAudioAllocRequest *request){
            return reinterpret_cast<T*>(pthis)->QueryIOSize(par, request);
        }
        static mfxStatus _Init(mfxHDL pthis, mfxAudioParam *par){
            return reinterpret_cast<T*>(pthis)->Init(par);
        }
        static mfxStatus _Reset(mfxHDL pthis, mfxAudioParam *par){
            return reinterpret_cast<T*>(pthis)->Reset(par);
        }
        static mfxStatus _Close(mfxHDL pthis) {
            return reinterpret_cast<T*>(pthis)->Close();
        }
        static mfxStatus _GetAudioParam(mfxHDL pthis, mfxAudioParam *par) {
            return reinterpret_cast<T*>(pthis)->GetAudioParam(par);
        }
    };

    template <class T>
    struct MFXPluginAdapterInternal{};
    template<>
    class MFXPluginAdapterInternal<MFXGenericPlugin> : public MFXPluginAdapterBase<MFXGenericPlugin>
    {
    public:
        MFXPluginAdapterInternal(MFXGenericPlugin *pPlugin)
            : MFXPluginAdapterBase<MFXGenericPlugin>(pPlugin)
        {
            m_mfxAPI.Submit = _Submit;
        }
        MFXPluginAdapterInternal(const MFXPluginAdapterInternal & that )
            : MFXPluginAdapterBase<MFXGenericPlugin>(that) {
            m_mfxAPI.Submit = that._Submit;
        }
        MFXPluginAdapterInternal<MFXGenericPlugin>& operator = (const MFXPluginAdapterInternal<MFXGenericPlugin> & that) {
            MFXPluginAdapterBase<MFXGenericPlugin>::operator=(that);
            m_mfxAPI.Submit = that._Submit;
            return *this;
        }

    private:
        static mfxStatus _Submit(mfxHDL pthis, const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxThreadTask *task) {
            return reinterpret_cast<MFXGenericPlugin*>(pthis)->Submit(in, in_num, out, out_num, task);
        }
    };

    template<>
    class MFXPluginAdapterInternal<MFXDecoderPlugin> : public MFXCodecPluginAdapterBase<MFXDecoderPlugin>
    {
    public:
        MFXPluginAdapterInternal(MFXDecoderPlugin *pPlugin)
            : MFXCodecPluginAdapterBase<MFXDecoderPlugin>(pPlugin)
        {
            SetupCallbacks();
        }

        MFXPluginAdapterInternal(const MFXPluginAdapterInternal & that)
        : MFXCodecPluginAdapterBase<MFXDecoderPlugin>(that) {
            SetupCallbacks();
        }

        MFXPluginAdapterInternal<MFXDecoderPlugin>& operator = (const MFXPluginAdapterInternal<MFXDecoderPlugin> & that) {
            MFXCodecPluginAdapterBase<MFXDecoderPlugin>::operator=(that);
            SetupCallbacks();
            return *this;
        }

    private:
        void SetupCallbacks() {
            m_codecPlg.DecodeHeader = _DecodeHeader;
            m_codecPlg.GetPayload = _GetPayload;
            m_codecPlg.DecodeFrameSubmit = _DecodeFrameSubmit;
        }
        static mfxStatus _DecodeHeader(mfxHDL pthis, mfxBitstream *bs, mfxVideoParam *par) {
            return reinterpret_cast<MFXDecoderPlugin*>(pthis)->DecodeHeader(bs, par);
        }
        static mfxStatus _GetPayload(mfxHDL pthis, mfxU64 *ts, mfxPayload *payload) {
            return reinterpret_cast<MFXDecoderPlugin*>(pthis)->GetPayload(ts, payload);
        }
        static mfxStatus _DecodeFrameSubmit(mfxHDL pthis, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,  mfxThreadTask *task) {
            return reinterpret_cast<MFXDecoderPlugin*>(pthis)->DecodeFrameSubmit(bs, surface_work, surface_out, task);
        }
    };

    template<>
    class MFXPluginAdapterInternal<MFXAudioDecoderPlugin> : public MFXAudioCodecPluginAdapterBase<MFXAudioDecoderPlugin>
    {
    public:
        MFXPluginAdapterInternal(MFXAudioDecoderPlugin *pPlugin)
            : MFXAudioCodecPluginAdapterBase<MFXAudioDecoderPlugin>(pPlugin)
        {
            SetupCallbacks();
        }

        MFXPluginAdapterInternal(const MFXPluginAdapterInternal & that)
        : MFXAudioCodecPluginAdapterBase<MFXAudioDecoderPlugin>(that) {
            SetupCallbacks();
        }

        MFXPluginAdapterInternal<MFXAudioDecoderPlugin>& operator = (const MFXPluginAdapterInternal<MFXAudioDecoderPlugin> & that) {
            MFXAudioCodecPluginAdapterBase<MFXAudioDecoderPlugin>::operator=(that);
            SetupCallbacks();
            return *this;
        }

    private:
        void SetupCallbacks() {
            m_codecPlg.DecodeHeader = _DecodeHeader;
//            m_codecPlg.GetPayload = _GetPayload;
            m_codecPlg.DecodeFrameSubmit = _DecodeFrameSubmit;
        }
        static mfxStatus _DecodeHeader(mfxHDL pthis, mfxBitstream *bs, mfxAudioParam *par) {
            return reinterpret_cast<MFXAudioDecoderPlugin*>(pthis)->DecodeHeader(bs, par);
        }
//        static mfxStatus _GetPayload(mfxHDL pthis, mfxU64 *ts, mfxPayload *payload) {
  //          return reinterpret_cast<MFXAudioDecoderPlugin*>(pthis)->GetPayload(ts, payload);
    //    }
        static mfxStatus _DecodeFrameSubmit(mfxHDL pthis, mfxBitstream *in, mfxAudioFrame *out, mfxThreadTask *task) {
            return reinterpret_cast<MFXAudioDecoderPlugin*>(pthis)->DecodeFrameSubmit(in, out, task);
        }
    };

    template<>
    class MFXPluginAdapterInternal<MFXEncoderPlugin> : public MFXCodecPluginAdapterBase<MFXEncoderPlugin>
    {
    public:
        MFXPluginAdapterInternal(MFXEncoderPlugin *pPlugin)
            : MFXCodecPluginAdapterBase<MFXEncoderPlugin>(pPlugin)
        {
            m_codecPlg.EncodeFrameSubmit = _EncodeFrameSubmit;
        }
        MFXPluginAdapterInternal(const MFXPluginAdapterInternal & that)
            : MFXCodecPluginAdapterBase<MFXEncoderPlugin>(that) {
            m_codecPlg.EncodeFrameSubmit = _EncodeFrameSubmit;
        }

        MFXPluginAdapterInternal<MFXEncoderPlugin>& operator = (const MFXPluginAdapterInternal<MFXEncoderPlugin> & that) {
            MFXCodecPluginAdapterBase<MFXEncoderPlugin>::operator = (that);
            m_codecPlg.EncodeFrameSubmit = _EncodeFrameSubmit;
            return *this;
        }

    private:
        static mfxStatus _EncodeFrameSubmit(mfxHDL pthis, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *task) {
            return reinterpret_cast<MFXEncoderPlugin*>(pthis)->EncodeFrameSubmit(ctrl, surface, bs, task);
        }
    };

    template<>
    class MFXPluginAdapterInternal<MFXAudioEncoderPlugin> : public MFXAudioCodecPluginAdapterBase<MFXAudioEncoderPlugin>
    {
    public:
        MFXPluginAdapterInternal(MFXAudioEncoderPlugin *pPlugin)
            : MFXAudioCodecPluginAdapterBase<MFXAudioEncoderPlugin>(pPlugin)
        {
            SetupCallbacks();
        }

        MFXPluginAdapterInternal(const MFXPluginAdapterInternal & that)
        : MFXAudioCodecPluginAdapterBase<MFXAudioEncoderPlugin>(that) {
            SetupCallbacks();
        }

        MFXPluginAdapterInternal<MFXAudioEncoderPlugin>& operator = (const MFXPluginAdapterInternal<MFXAudioEncoderPlugin> & that) {
            MFXAudioCodecPluginAdapterBase<MFXAudioEncoderPlugin>::operator=(that);
            SetupCallbacks();
            return *this;
        }

    private:
        void SetupCallbacks() {
            m_codecPlg.EncodeFrameSubmit = _EncodeFrameSubmit;
        }
        static mfxStatus _EncodeFrameSubmit(mfxHDL pthis, mfxAudioFrame *aFrame, mfxBitstream *out, mfxThreadTask *task) {
            return reinterpret_cast<MFXAudioEncoderPlugin*>(pthis)->EncodeFrameSubmit(aFrame, out, task);
        }
    };

    template<>
    class MFXPluginAdapterInternal<MFXEncPlugin> : public MFXCodecPluginAdapterBase<MFXEncPlugin>
    {
    public:
        MFXPluginAdapterInternal(MFXEncPlugin *pPlugin)
            : MFXCodecPluginAdapterBase<MFXEncPlugin>(pPlugin)
        {
            m_codecPlg.ENCFrameSubmit = _ENCFrameSubmit;
        }
        MFXPluginAdapterInternal(const MFXPluginAdapterInternal & that)
            : MFXCodecPluginAdapterBase<MFXEncPlugin>(that) {
            m_codecPlg.ENCFrameSubmit = _ENCFrameSubmit;
        }

        MFXPluginAdapterInternal<MFXEncPlugin>& operator = (const MFXPluginAdapterInternal<MFXEncPlugin> & that) {
            MFXCodecPluginAdapterBase<MFXEncPlugin>::operator = (that);
            m_codecPlg.ENCFrameSubmit = _ENCFrameSubmit;
            return *this;
        }

    private:
        static mfxStatus _ENCFrameSubmit(mfxHDL pthis,mfxENCInput *in, mfxENCOutput *out, mfxThreadTask *task) {
            return reinterpret_cast<MFXEncPlugin*>(pthis)->EncFrameSubmit(in, out, task);
        }
    };


    template<>
    class MFXPluginAdapterInternal<MFXVPPPlugin> : public MFXCodecPluginAdapterBase<MFXVPPPlugin>
    {
    public:
        MFXPluginAdapterInternal(MFXVPPPlugin *pPlugin)
            : MFXCodecPluginAdapterBase<MFXVPPPlugin>(pPlugin)
        {
            SetupCallbacks();
        }
        MFXPluginAdapterInternal(const MFXPluginAdapterInternal & that)
            : MFXCodecPluginAdapterBase<MFXVPPPlugin>(that) {
            SetupCallbacks();
        }

        MFXPluginAdapterInternal<MFXVPPPlugin>& operator = (const MFXPluginAdapterInternal<MFXVPPPlugin> & that) {
            MFXCodecPluginAdapterBase<MFXVPPPlugin>::operator = (that);
            SetupCallbacks();
            return *this;
        }

    private:
        void SetupCallbacks() {
            m_codecPlg.VPPFrameSubmit = _VPPFrameSubmit;
            m_codecPlg.VPPFrameSubmitEx = _VPPFrameSubmitEx;
        }
        static mfxStatus _VPPFrameSubmit(mfxHDL pthis, mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_out, mfxExtVppAuxData *aux, mfxThreadTask *task) {
            return reinterpret_cast<MFXVPPPlugin*>(pthis)->VPPFrameSubmit(surface_in, surface_out, aux, task);
        }
        static mfxStatus _VPPFrameSubmitEx(mfxHDL pthis, mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxThreadTask *task) {
            return reinterpret_cast<MFXVPPPlugin*>(pthis)->VPPFrameSubmitEx(surface_in, surface_work, surface_out, task);
        }
    };
}

/* adapter for particular plugin type*/
template<class T>
class MFXPluginAdapter
{
public:
    detail::MFXPluginAdapterInternal<T> m_Adapter;

    operator  mfxPlugin () const {
        return m_Adapter.operator mfxPlugin();
    }

    MFXPluginAdapter(T* pPlugin = NULL)
        : m_Adapter(pPlugin)
    {
    }
};

template<class T>
inline MFXPluginAdapter<T> make_mfx_plugin_adapter(T* pPlugin) {

    MFXPluginAdapter<T> adapt(pPlugin);
    return adapt;
}

#endif // __MFXPLUGINPLUSPLUS_H
