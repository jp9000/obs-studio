/* ****************************************************************************** *\

Copyright (C) 2013-2016 Intel Corporation.  All rights reserved.

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

File Name: mfx_load_plugin.h

\* ****************************************************************************** */

#pragma once
#include "mfxplugin.h"
#include "mfx_dispatcher_defs.h"
#include "mfx_plugin_hive.h"

namespace MFX
{
    typedef mfxStatus (MFX_CDECL *CreatePluginPtr_t)(mfxPluginUID uid, mfxPlugin* plugin);

    class PluginModule
    {
        mfxModuleHandle mHmodule;
        CreatePluginPtr_t mCreatePluginPtr;
        msdk_disp_char mPath[MAX_PLUGIN_PATH];
        
    public:
        PluginModule();
        PluginModule(const msdk_disp_char * path);
        PluginModule(const PluginModule & that) ;
        PluginModule & operator = (const PluginModule & that);
        bool Create(mfxPluginUID guid, mfxPlugin&);
        ~PluginModule(void);

    private:
        void Tidy();
    };

    class MFXPluginFactory {
        struct FactoryRecord {
            mfxPluginParam plgParams;
            PluginModule module;
            mfxPlugin plugin;
            FactoryRecord ()
                : plugin()
            {}
            FactoryRecord(const mfxPluginParam &plgParams,
                          PluginModule &module,
                          mfxPlugin plugin) 
                : plgParams(plgParams) 
                , module(module)
                , plugin(plugin) {
            }
        };
        MFXVector<FactoryRecord> mPlugins;
        mfxU32 nPlugins;
        mfxSession mSession;
    public:
        MFXPluginFactory(mfxSession session);
        void Close();
        mfxStatus Create(const PluginDescriptionRecord &);
        bool Destroy(const mfxPluginUID &);
        
        ~MFXPluginFactory();
    protected:
        void DestroyPlugin( FactoryRecord & );
        static bool RunVerification( const mfxPlugin & plg, const PluginDescriptionRecord &dsc, mfxPluginParam &pluginParams );
        static bool VerifyEncoder( const mfxVideoCodecPlugin &videoCodec );
        static bool VerifyAudioEncoder( const mfxAudioCodecPlugin &audioCodec );
        static bool VerifyEnc( const mfxVideoCodecPlugin &videoEnc );
        static bool VerifyVpp( const mfxVideoCodecPlugin &videoCodec );
        static bool VerifyDecoder( const mfxVideoCodecPlugin &videoCodec );
        static bool VerifyAudioDecoder( const mfxAudioCodecPlugin &audioCodec );
        static bool VerifyCodecCommon( const mfxVideoCodecPlugin & Video );
    };
}
