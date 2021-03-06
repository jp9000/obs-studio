/* ****************************************************************************** *\

Copyright (C) 2013-2018 Intel Corporation.  All rights reserved.

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

File Name: mfx_plugin_hive.h

\* ****************************************************************************** */

#pragma once

#include "mfx_dispatcher_defs.h"
#include "mfxplugin.h"
#include "mfx_win_reg_key.h"
#include "mfx_vector.h"
#include <string.h>
#include <memory>
#include <stdio.h>

struct MFX_DISP_HANDLE;

namespace MFX {

    inline bool operator == (const mfxPluginUID &lhs, const mfxPluginUID & rhs) 
    {
        return !memcmp(lhs.Data, rhs.Data, sizeof(mfxPluginUID));
    }
    
    inline bool operator != (const mfxPluginUID &lhs, const mfxPluginUID & rhs) 
    {
        return !(lhs == rhs);
    }
#ifdef _WIN32
    //warning C4351: new behavior: elements of array 'MFX::PluginDescriptionRecord::sName' will be default initialized
    #pragma warning (disable: 4351)
#endif
    class PluginDescriptionRecord :  public mfxPluginParam 
    {
    public:
        msdk_disp_char sPath[MAX_PLUGIN_PATH];
        char sName[MAX_PLUGIN_NAME];
        //used for FS plugins that has poor description
        bool onlyVersionRegistered;
        bool Default;
        PluginDescriptionRecord()
            : mfxPluginParam()
            , sPath()
            , sName()
            , onlyVersionRegistered()
            , Default()
        {
        }
    };

    typedef MFXVector<PluginDescriptionRecord> MFXPluginStorage;

    class  MFXPluginStorageBase : public MFXPluginStorage 
    {
    protected:
        mfxVersion mCurrentAPIVersion;
    protected:
        MFXPluginStorageBase(mfxVersion currentAPIVersion) 
            : mCurrentAPIVersion(currentAPIVersion)
        {
        }
        void ConvertAPIVersion( mfxU32 APIVersion, PluginDescriptionRecord &descriptionRecord) const
        {
            descriptionRecord.APIVersion.Minor = static_cast<mfxU16> (APIVersion & 0x0ff);
            descriptionRecord.APIVersion.Major = static_cast<mfxU16> (APIVersion >> 8);
        }
    };

    //populated from registry
    class MFXPluginsInHive : public MFXPluginStorageBase
    {
    public:
        MFXPluginsInHive(int mfxStorageID, const msdk_disp_char *msdkLibSubKey, mfxVersion currentAPIVersion);
    };

#if defined(MEDIASDK_USE_CFGFILES) || (!defined(MEDIASDK_UWP_LOADER) && !defined(MEDIASDK_UWP_PROCTABLE))
    //plugins are loaded from FS close to executable
    class MFXPluginsInFS : public MFXPluginStorageBase
    {
        bool mIsVersionParsed;
        bool mIsAPIVersionParsed;
    public:
        MFXPluginsInFS(mfxVersion currentAPIVersion);
    private:
        bool ParseFile(FILE * f, PluginDescriptionRecord & des);
        bool ParseKVPair( msdk_disp_char *key, msdk_disp_char * value, PluginDescriptionRecord & des);
    };
#endif //#if defined(MEDIASDK_USE_CFGFILES) || (!defined(MEDIASDK_UWP_LOADER) && !defined(MEDIASDK_UWP_PROCTABLE))

    //plugins are loaded from FS close to Runtime library
    class MFXDefaultPlugins : public MFXPluginStorageBase
    {
    public:
        MFXDefaultPlugins(mfxVersion currentAPIVersion, MFX_DISP_HANDLE * hdl, int implType);
    private:
    };

}
