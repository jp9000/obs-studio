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

File Name: mfx_load_dll.cpp

\* ****************************************************************************** */

#if defined(_WIN32) || defined(_WIN64)

#include "mfx_dispatcher.h"
#include "mfx_load_dll.h"

#include <wchar.h>
#include <string.h>
#include <windows.h>

#if !defined(_DEBUG)

#if defined(_WIN64)
const
wchar_t * const defaultDLLName[2] = {L"libmfxhw64.dll",
                                     L"libmfxsw64.dll"};
const
wchar_t * const defaultAudioDLLName[2] = {L"libmfxaudiosw64.dll",
                                          L"libmfxaudiosw64.dll"};

const 
wchar_t  * const defaultPluginDLLName[2] = {L"mfxplugin64_hw.dll",
                                            L"mfxplugin64_sw.dll"};

#elif defined(_WIN32)
const
wchar_t * const defaultDLLName[2] = {L"libmfxhw32.dll",
                                     L"libmfxsw32.dll"};

const
wchar_t * const defaultAudioDLLName[2] = {L"libmfxaudiosw32.dll",
                                          L"libmfxaudiosw32.dll"};

const 
wchar_t  * const defaultPluginDLLName[2] = {L"mfxplugin32_hw.dll",
                                            L"mfxplugin32_sw.dll"};

#endif // (defined(_WIN64))

#else // defined(_DEBUG)

#if defined(_WIN64)
const
wchar_t * const defaultDLLName[2] = {L"libmfxhw64_d.dll",
                                     L"libmfxsw64_d.dll"};
const
wchar_t * const defaultAudioDLLName[2] = {L"libmfxaudiosw64_d.dll",
                                          L"libmfxaudiosw64_d.dll"};

const 
wchar_t  * const defaultPluginDLLName[2] = {L"mfxplugin64_hw_d.dll",
                                            L"mfxplugin64_sw_d.dll"};

#elif defined(WIN32)
const
wchar_t * const defaultDLLName[2] = {L"libmfxhw32_d.dll",
                                     L"libmfxsw32_d.dll"};


const
wchar_t * const defaultAudioDLLName[2] = {L"libmfxaudiosw32_d.dll",
                                          L"libmfxaudiosw32_d.dll"};

const 
wchar_t  * const defaultPluginDLLName[2] = {L"mfxplugin32_hw_d.dll",
                                            L"mfxplugin32_sw_d.dll"};

#endif // (defined(_WIN64))

#endif // !defined(_DEBUG)

namespace MFX
{


mfxStatus mfx_get_default_dll_name(msdk_disp_char *pPath, size_t pathSize, eMfxImplType implType)
{
    if (!pPath)
    {
        return MFX_ERR_NULL_PTR;
    }
    

    // there are only 2 implementation with default DLL names
#if _MSC_VER >= 1400
    return 0 == wcscpy_s(pPath, pathSize, defaultDLLName[implType & 1])
        ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
#else    
    wcscpy(pPath, defaultDLLName[implType & 1]);
    return MFX_ERR_NONE;
#endif
} // mfxStatus mfx_get_default_dll_name(wchar_t *pPath, size_t pathSize, eMfxImplType implType)

mfxStatus mfx_get_default_plugin_name(msdk_disp_char *pPath, size_t pathSize, eMfxImplType implType)
{
    if (!pPath)
    {
        return MFX_ERR_NULL_PTR;
    }


    // there are only 2 implementation with default DLL names
#if _MSC_VER >= 1400
    return 0 == wcscpy_s(pPath, pathSize, defaultPluginDLLName[implType & 1])
        ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
#else    
    wcscpy(pPath, defaultPluginDLLName[implType & 1]);
    return MFX_ERR_NONE;
#endif
}

mfxStatus mfx_get_default_audio_dll_name(msdk_disp_char *pPath, size_t pathSize, eMfxImplType implType)
{
    if (!pPath)
    {
        return MFX_ERR_NULL_PTR;
    }
    
    // there are only 2 implementation with default DLL names
#if _MSC_VER >= 1400
    return 0 == wcscpy_s(pPath, pathSize, defaultAudioDLLName[implType & 1])
        ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
#else    
    wcscpy(pPath, defaultAudioDLLName[implType & 1]);
    return MFX_ERR_NONE;
#endif
} // mfxStatus mfx_get_default_audio_dll_name(wchar_t *pPath, size_t pathSize, eMfxImplType implType)

mfxModuleHandle mfx_dll_load(const msdk_disp_char *pFileName)
{
    mfxModuleHandle hModule = (mfxModuleHandle) 0;

    // check error(s)
    if (NULL == pFileName)
    {
        return NULL;
    }
#if !defined(MEDIASDK_UWP_LOADER) && !defined(MEDIASDK_UWP_PROCTABLE)
    // set the silent error mode
    DWORD prevErrorMode = 0;
#if (_WIN32_WINNT >= 0x0600) && !(__GNUC__)
    SetThreadErrorMode(SEM_FAILCRITICALERRORS, &prevErrorMode);
#else
    prevErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
#endif
#endif // !defined(MEDIASDK_UWP_LOADER) && !defined(MEDIASDK_UWP_PROCTABLE)

        // load the library's module
        hModule = LoadLibraryExW(pFileName, NULL, 0);

#if !defined(MEDIASDK_UWP_LOADER) && !defined(MEDIASDK_UWP_PROCTABLE)
        // set the previous error mode
#if (_WIN32_WINNT >= 0x0600) && !(__GNUC__)
    SetThreadErrorMode(prevErrorMode, NULL);
#else
    SetErrorMode(prevErrorMode);
#endif
#endif // !defined(MEDIASDK_UWP_LOADER) && !defined(MEDIASDK_UWP_PROCTABLE)

    return hModule;

} // mfxModuleHandle mfx_dll_load(const wchar_t *pFileName)

mfxFunctionPointer mfx_dll_get_addr(mfxModuleHandle handle, const char *pFunctionName)
{
    if (NULL == handle)
    {
        return NULL;
    }

    return (mfxFunctionPointer) GetProcAddress((HMODULE) handle, pFunctionName);
} // mfxFunctionPointer mfx_dll_get_addr(mfxModuleHandle handle, const char *pFunctionName)

bool mfx_dll_free(mfxModuleHandle handle)
{
    if (NULL == handle)
    {
        return true;
    }

    BOOL bRes = FreeLibrary((HMODULE)handle);

    return !!bRes;
} // bool mfx_dll_free(mfxModuleHandle handle)

#if !defined(MEDIASDK_UWP_LOADER) && !defined(MEDIASDK_UWP_PROCTABLE)
mfxModuleHandle mfx_get_dll_handle(const msdk_disp_char *pFileName)
{
    mfxModuleHandle hModule = (mfxModuleHandle) 0;

    // check error(s)
    if (NULL == pFileName)
    {
        return NULL;
    }

    // set the silent error mode
    DWORD prevErrorMode = 0;
#if (_WIN32_WINNT >= 0x0600) && !(__GNUC__)
    SetThreadErrorMode(SEM_FAILCRITICALERRORS, &prevErrorMode); 
#else
    prevErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
#endif
    // load the library's module
    GetModuleHandleExW(0, pFileName, (HMODULE*) &hModule);
    // set the previous error mode
#if (_WIN32_WINNT >= 0x0600) && !(__GNUC__)
    SetThreadErrorMode(prevErrorMode, NULL);
#else
    SetErrorMode(prevErrorMode);
#endif
    return hModule;
}
#endif //!defined(MEDIASDK_UWP_LOADER) && !defined(MEDIASDK_UWP_PROCTABLE)


} // namespace MFX

#endif // #if defined(_WIN32) || defined(_WIN64)
