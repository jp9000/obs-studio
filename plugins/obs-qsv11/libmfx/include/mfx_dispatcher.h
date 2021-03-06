/* ****************************************************************************** *\

Copyright (C) 2012-2017 Intel Corporation.  All rights reserved.

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

File Name: mfx_dispatcher.h

\* ****************************************************************************** */

#if !defined(__MFX_DISPATCHER_H)
#define __MFX_DISPATCHER_H

#include <mfxvideo.h>
#include <mfxaudio.h>
#include <mfxplugin.h>
#include <stddef.h>
#include "mfx_dispatcher_defs.h"
#include "mfx_load_plugin.h"
#include "mfxenc.h"
#include "mfxpak.h"

#define INTEL_VENDOR_ID 0x8086

mfxStatus MFXQueryVersion(mfxSession session, mfxVersion *version);

enum
{
    // to avoid code changing versions are just inherited
    // from the API header file.
    DEFAULT_API_VERSION_MAJOR   = MFX_VERSION_MAJOR,
    DEFAULT_API_VERSION_MINOR   = MFX_VERSION_MINOR
};

//
// declare functions' integer identifiers.
//

#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) \
    e##func_name,

enum eFunc
{
    eMFXInit,
    eMFXClose,
    eMFXQueryIMPL,
    eMFXQueryVersion,
    eMFXJoinSession,
    eMFXDisjoinSession,
    eMFXCloneSession,
    eMFXSetPriority,
    eMFXGetPriority,
    eMFXInitEx,
#include "mfx_exposed_functions_list.h"
    eVideoFuncTotal
};

enum ePluginFunc
{
    eMFXVideoUSER_Load,
    eMFXVideoUSER_LoadByPath,
    eMFXVideoUSER_UnLoad,
    eMFXAudioUSER_Load,
    eMFXAudioUSER_UnLoad,
    ePluginFuncTotal
};

enum eAudioFunc
{
    eFakeAudioEnum = eMFXGetPriority,
#include "mfxaudio_exposed_functions_list.h"
    eAudioFuncTotal
};

// declare max buffer length for regsitry key name
enum
{
    MFX_MAX_REGISTRY_KEY_NAME = 256
};

// declare the maximum DLL path
enum
{
    MFX_MAX_DLL_PATH = 1024
};

// declare library's implementation types
enum eMfxImplType
{
    MFX_LIB_HARDWARE            = 0,
    MFX_LIB_SOFTWARE            = 1,
    MFX_LIB_PSEUDO              = 2,

    MFX_LIB_IMPL_TYPES
};

// declare dispatcher's version
enum
{
    MFX_DISPATCHER_VERSION_MAJOR = 1,
    MFX_DISPATCHER_VERSION_MINOR = 2
};

struct _mfxSession
{
    // A real handle from MFX engine passed to a called function
    mfxSession session;

    mfxFunctionPointer callTable[eVideoFuncTotal];
    mfxFunctionPointer callPlugInsTable[ePluginFuncTotal];
    mfxFunctionPointer callAudioTable[eAudioFuncTotal];

    // Current library's implementation (exact implementation)
    mfxIMPL impl;
};

// declare a dispatcher's handle
struct MFX_DISP_HANDLE : public _mfxSession
{
    // Default constructor
    MFX_DISP_HANDLE(const mfxVersion requiredVersion);
    // Destructor
    ~MFX_DISP_HANDLE(void);

    // Load the library's module
    mfxStatus LoadSelectedDLL(const msdk_disp_char *pPath, eMfxImplType implType, mfxIMPL impl, mfxIMPL implInterface, mfxInitParam &par);
    // Unload the library's module
    mfxStatus UnLoadSelectedDLL(void);

    // Close the handle
    mfxStatus Close(void);

    // NOTE: changing order of struct's members can make different version of
    // dispatchers incompatible. Think of different modules (e.g. MFT filters)
    // within a single application.

    // Library's implementation type (hardware or software)
    eMfxImplType implType;
    // Current library's VIA interface
    mfxIMPL implInterface;
    // Dispatcher's version. If version is 1.1 or lower, then old dispatcher's
    // architecture is used. Otherwise it means current dispatcher's version.
    mfxVersion dispVersion;
    // Required API version of session initialized
    const mfxVersion apiVersion;
    // Actual library API version
    mfxVersion actualApiVersion;
    // Status of loaded dll
    mfxStatus loadStatus;
    // Resgistry subkey name for windows version
    msdk_disp_char subkeyName[MFX_MAX_REGISTRY_KEY_NAME];
    // Storage ID for windows version
    int storageID;

    // Library's module handle
    mfxModuleHandle hModule;

    MFX::MFXPluginStorage pluginHive;
    MFX::MFXPluginFactory pluginFactory;

private:
    // Declare assignment operator and copy constructor to prevent occasional assignment
    MFX_DISP_HANDLE(const MFX_DISP_HANDLE &);
    MFX_DISP_HANDLE & operator = (const MFX_DISP_HANDLE &);

};

// declare comparison operator
inline
bool operator == (const mfxVersion &one, const mfxVersion &two)
{
    return (one.Version == two.Version);

} // bool operator == (const mfxVersion &one, const mfxVersion &two)

inline
bool operator < (const mfxVersion &one, const mfxVersion &two)
{
    return (one.Major == two.Major) && (one.Minor < two.Minor);

} // bool operator < (const mfxVersion &one, const mfxVersion &two)

inline
bool operator <= (const mfxVersion &one, const mfxVersion &two)
{
    return (one == two) || (one < two);
} // bool operator <= (const mfxVersion &one, const mfxVersion &two)


//
// declare a table with functions descriptions
//

typedef
struct FUNCTION_DESCRIPTION
{
    // Literal function's name
    const char *pName;
    // API version when function appeared first time
    mfxVersion apiVersion;
} FUNCTION_DESCRIPTION;

extern const
FUNCTION_DESCRIPTION APIFunc[eVideoFuncTotal];

extern const
FUNCTION_DESCRIPTION APIAudioFunc[eAudioFuncTotal];
#endif // __MFX_DISPATCHER_H
