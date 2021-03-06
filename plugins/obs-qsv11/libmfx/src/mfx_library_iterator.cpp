/* ****************************************************************************** *\

Copyright (C) 2012-2018 Intel Corporation.  All rights reserved.

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

File Name: mfx_library_iterator.cpp

\* ****************************************************************************** */

#if defined(_WIN32) || defined(_WIN64)

#include "mfx_library_iterator.h"

#include "mfx_dispatcher.h"
#include "mfx_dispatcher_log.h"

#include "mfx_dxva2_device.h"
#include "mfx_load_dll.h"

#include <tchar.h>
#include <windows.h>

#include <vector>

namespace MFX
{

enum
{
    MFX_MAX_MERIT               = 0x7fffffff
};

//
// declare registry keys
//

const
wchar_t rootDispPath[] = L"Software\\Intel\\MediaSDK\\Dispatch";
const
wchar_t vendorIDKeyName[] = L"VendorID";
const
wchar_t deviceIDKeyName[] = L"DeviceID";
const
wchar_t meritKeyName[] = L"Merit";
const
wchar_t pathKeyName[] = L"Path";
const
wchar_t apiVersionName[] = L"APIVersion";

mfxStatus SelectImplementationType(const mfxU32 adapterNum, mfxIMPL *pImplInterface, mfxU32 *pVendorID, mfxU32 *pDeviceID)
{
    if (NULL == pImplInterface)
    {
        return MFX_ERR_NULL_PTR;
    }
    mfxIMPL impl_via = *pImplInterface;

    DXVA2Device dxvaDevice;
    if (MFX_IMPL_VIA_D3D9 == impl_via)
    {
        // try to create the Direct3D 9 device and find right adapter
        if (!dxvaDevice.InitD3D9(adapterNum))
        {
            DISPATCHER_LOG_INFO((("dxvaDevice.InitD3D9(%d) Failed "), adapterNum ));
            return MFX_ERR_UNSUPPORTED;
        }
    }
    else if (MFX_IMPL_VIA_D3D11 == impl_via)
    {
        // try to open DXGI 1.1 device to get hardware ID
        if (!dxvaDevice.InitDXGI1(adapterNum))
        {
            DISPATCHER_LOG_INFO((("dxvaDevice.InitDXGI1(%d) Failed "), adapterNum ));
            return MFX_ERR_UNSUPPORTED;
        }
    }
    else if (MFX_IMPL_VIA_ANY == impl_via)
    {
        // try the Direct3D 9 device
        if (dxvaDevice.InitD3D9(adapterNum))
        {
            *pImplInterface = MFX_IMPL_VIA_D3D9; // store value for GetImplementationType() call
        }
        // else try to open DXGI 1.1 device to get hardware ID
        else if (dxvaDevice.InitDXGI1(adapterNum))
        {
            *pImplInterface = MFX_IMPL_VIA_D3D11; // store value for GetImplementationType() call
        }
        else
        {
            DISPATCHER_LOG_INFO((("Unsupported adapter %d "), adapterNum ));
            return MFX_ERR_UNSUPPORTED;
        }
    }
    else
    {
        DISPATCHER_LOG_ERROR((("Unknown implementation type %d "), *pImplInterface ));
        return MFX_ERR_UNSUPPORTED;
    }

    // obtain card's parameters
    if (pVendorID && pDeviceID)
    {
        *pVendorID = dxvaDevice.GetVendorID();
        *pDeviceID = dxvaDevice.GetDeviceID();
    }

    return MFX_ERR_NONE;
}

MFXLibraryIterator::MFXLibraryIterator(void)
{
    m_implType = MFX_LIB_PSEUDO;
    m_implInterface = MFX_IMPL_UNSUPPORTED;

    m_vendorID = 0;
    m_deviceID = 0;

    m_lastLibIndex = 0;
    m_lastLibMerit = MFX_MAX_MERIT;

    m_bIsSubKeyValid = 0;
    m_StorageID = 0;

    m_SubKeyName[0] = 0;
} // MFXLibraryIterator::MFXLibraryIterator(void)

MFXLibraryIterator::~MFXLibraryIterator(void)
{
    Release();

} // MFXLibraryIterator::~MFXLibraryIterator(void)

void MFXLibraryIterator::Release(void)
{
    m_implType = MFX_LIB_PSEUDO;
    m_implInterface = MFX_IMPL_UNSUPPORTED;

    m_vendorID = 0;
    m_deviceID = 0;

    m_lastLibIndex = 0;
    m_lastLibMerit = MFX_MAX_MERIT;
    m_SubKeyName[0] = 0;

} // void MFXLibraryIterator::Release(void)

DECLSPEC_NOINLINE HMODULE GetThisDllModuleHandle()
{
  HMODULE hDll = HMODULE(-1);

  GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                      GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                      reinterpret_cast<LPCWSTR>(&GetThisDllModuleHandle), &hDll);
  return hDll;
}

// msdk_disp_char* sImplPath must be allocated with size not less then msdk_disp_path_len
bool GetImplPath(int storageID, msdk_disp_char* sImplPath)
{
    HMODULE hModule = NULL;

    sImplPath[0] = L'\0';

    switch (storageID) {
    case MFX_APP_FOLDER:
        hModule = 0;
        break;

#if defined(MEDIASDK_UWP_LOADER) || defined(MEDIASDK_UWP_PROCTABLE)
    case MFX_PATH_MSDK_FOLDER:
        hModule = GetThisDllModuleHandle();
        break;
#endif

    }

    if(hModule == HMODULE(-1)) {
        return false;
    }

    DWORD nSize = 0;
    DWORD allocSize = msdk_disp_path_len;

    nSize = GetModuleFileNameW(hModule, &sImplPath[0], allocSize);

    if (nSize  == 0 || nSize == allocSize) {
        // nSize == 0 meanse that system can't get this info for hModule
        // nSize == allocSize buffer is too small
        return false;
    }

    // for any case because WinXP implementation of GetModuleFileName does not add \0 to the end of string
    sImplPath[nSize] = L'\0';

    msdk_disp_char * dirSeparator = wcsrchr(sImplPath, L'\\');
    if (dirSeparator != NULL && dirSeparator < (sImplPath + msdk_disp_path_len))
    {
        *++dirSeparator = 0;
    }
    return true;
}

mfxStatus MFXLibraryIterator::Init(eMfxImplType implType, mfxIMPL implInterface, const mfxU32 adapterNum, int storageID)
{
    // check error(s)
    if ((MFX_LIB_SOFTWARE != implType) &&
        (MFX_LIB_HARDWARE != implType))
    {
        return MFX_ERR_UNSUPPORTED;
    }

    // release the object before initialization
    Release();
    m_StorageID = storageID;
    m_lastLibIndex = 0;

#if defined(MEDIASDK_USE_REGISTRY) || (!defined(MEDIASDK_UWP_LOADER) && !defined(MEDIASDK_UWP_PROCTABLE))
    if (storageID == MFX_CURRENT_USER_KEY || storageID == MFX_LOCAL_MACHINE_KEY)
    {
        return InitRegistry(implType, implInterface, adapterNum, storageID);
    }
#endif

    msdk_disp_char  sCurrentModulePath[msdk_disp_path_len];

    if(!GetImplPath(storageID, sCurrentModulePath)) {
        return MFX_ERR_UNSUPPORTED;
    }

    return InitFolder(implType, implInterface, adapterNum, sCurrentModulePath);

} // mfxStatus MFXLibraryIterator::Init(eMfxImplType implType, const mfxU32 adapterNum, int storageID)

mfxStatus MFXLibraryIterator::InitRegistry(eMfxImplType implType, mfxIMPL implInterface, const mfxU32 adapterNum, int storageID)
{
#if defined(MEDIASDK_USE_REGISTRY) || (!defined(MEDIASDK_UWP_LOADER) && !defined(MEDIASDK_UWP_PROCTABLE))
    HKEY rootHKey;
    bool bRes;

    // open required registry key
    rootHKey = (MFX_LOCAL_MACHINE_KEY == storageID) ? (HKEY_LOCAL_MACHINE) : (HKEY_CURRENT_USER);
    bRes = m_baseRegKey.Open(rootHKey, rootDispPath, KEY_READ);
    if (false == bRes)
    {
        DISPATCHER_LOG_WRN((("Can't open %s\\%S : RegOpenKeyExA()==0x%x\n"),
            (MFX_LOCAL_MACHINE_KEY == storageID) ? ("HKEY_LOCAL_MACHINE") : ("HKEY_CURRENT_USER"),
            rootDispPath, GetLastError()))
            return MFX_ERR_UNKNOWN;
    }

    // set the required library's implementation type
    m_implType = implType;
    m_implInterface = implInterface != 0
        ? implInterface
        : MFX_IMPL_VIA_ANY;

    //deviceID and vendorID are not actual for SW library loading
    if (m_implType != MFX_LIB_SOFTWARE)
    {
        mfxStatus mfxRes = MFX::SelectImplementationType(adapterNum, &m_implInterface, &m_vendorID, &m_deviceID);
        if (MFX_ERR_NONE != mfxRes)
        {
            return mfxRes;
        }
    }

    DISPATCHER_LOG_INFO((("Inspecting %s\\%S\n"),
        (MFX_LOCAL_MACHINE_KEY == storageID) ? ("HKEY_LOCAL_MACHINE") : ("HKEY_CURRENT_USER"),
        rootDispPath))

    return MFX_ERR_NONE;
#else
    (void) storageID;
    (void) adapterNum;
    (void) implInterface;
    (void) implType;
    return MFX_ERR_UNSUPPORTED;
#endif // #if !defined(MEDIASDK_UWP_LOADER) && !defined(MEDIASDK_UWP_PROCTABLE)

} // mfxStatus MFXLibraryIterator::InitRegistry(eMfxImplType implType, mfxIMPL implInterface, const mfxU32 adapterNum, int storageID)

mfxStatus MFXLibraryIterator::InitFolder(eMfxImplType implType, mfxIMPL implInterface, const mfxU32 adapterNum, const msdk_disp_char * path)
{
     const int maxPathLen = sizeof(m_path)/sizeof(m_path[0]);
     m_path[0] = 0;
     msdk_disp_char_cpy_s(m_path, maxPathLen, path);
     size_t pathLen = wcslen(m_path);

#if !defined(MEDIASDK_UWP_LOADER) && !defined(MEDIASDK_UWP_PROCTABLE)
     // we looking for runtime in application folder, it should be named libmfxsw64 or libmfxsw32
     mfx_get_default_dll_name(m_path + pathLen, msdk_disp_path_len - pathLen,  MFX_LIB_SOFTWARE);
#else
     mfx_get_default_dll_name(m_path + pathLen, msdk_disp_path_len - pathLen, implType);
#endif

     // set the required library's implementation type
     m_implType = implType;
     m_implInterface = implInterface != 0
         ? implInterface
         : MFX_IMPL_VIA_ANY;

     //deviceID and vendorID are not actual for SW library loading
     if (m_implType != MFX_LIB_SOFTWARE)
     {
         mfxStatus mfxRes = MFX::SelectImplementationType(adapterNum, &m_implInterface, &m_vendorID, &m_deviceID);
         if (MFX_ERR_NONE != mfxRes)
         {
             return mfxRes;
         }
     }
     return MFX_ERR_NONE;
} // mfxStatus MFXLibraryIterator::InitFolder(eMfxImplType implType, mfxIMPL implInterface, const mfxU32 adapterNum, const msdk_disp_char * path)

mfxStatus MFXLibraryIterator::SelectDLLVersion(wchar_t *pPath
                                             , size_t pathSize
                                             , eMfxImplType *pImplType, mfxVersion minVersion)
{
    UNREFERENCED_PARAMETER(minVersion);

    if (m_StorageID == MFX_APP_FOLDER)
    {
        if (m_lastLibIndex != 0)
            return MFX_ERR_NOT_FOUND;
        if (m_vendorID != INTEL_VENDOR_ID)
            return MFX_ERR_UNKNOWN;

        m_lastLibIndex = 1;
        msdk_disp_char_cpy_s(pPath, pathSize, m_path);
        *pImplType = MFX_LIB_SOFTWARE;
        return MFX_ERR_NONE;
    }

#if defined(MEDIASDK_UWP_LOADER) || defined(MEDIASDK_UWP_PROCTABLE)

    if (m_StorageID == MFX_PATH_MSDK_FOLDER) {

        if (m_lastLibIndex != 0)
            return MFX_ERR_NOT_FOUND;
        if (m_vendorID != INTEL_VENDOR_ID)
            return MFX_ERR_UNKNOWN;

        m_lastLibIndex = 1;
        msdk_disp_char_cpy_s(pPath, pathSize, m_path);
        // do not change impl type
        //*pImplType = MFX_LIB_HARDWARE;
        return MFX_ERR_NONE;
    }

#endif

#if defined(MEDIASDK_USE_REGISTRY) || (!defined(MEDIASDK_UWP_LOADER) && !defined(MEDIASDK_UWP_PROCTABLE))
    wchar_t libPath[MFX_MAX_DLL_PATH] = L"";
    DWORD libIndex = 0;
    DWORD libMerit = 0;
    DWORD index;
    bool enumRes;

    // main query cycle
    index = 0;
    m_bIsSubKeyValid = false;
    do
    {
        WinRegKey subKey;
        wchar_t subKeyName[MFX_MAX_REGISTRY_KEY_NAME] = { 0 };
        DWORD subKeyNameSize = sizeof(subKeyName) / sizeof(subKeyName[0]);

        // query next value name
        enumRes = m_baseRegKey.EnumKey(index, subKeyName, &subKeyNameSize);
        if (!enumRes)
        {
            DISPATCHER_LOG_WRN((("no more subkeys : RegEnumKeyExA()==0x%x\n"), GetLastError()))
        }
        else
        {
            DISPATCHER_LOG_INFO((("found subkey: %S\n"), subKeyName))

            bool bRes;

            // open the sub key
            bRes = subKey.Open(m_baseRegKey, subKeyName, KEY_READ);
            if (!bRes)
            {
                DISPATCHER_LOG_WRN((("error opening key %S :RegOpenKeyExA()==0x%x\n"), subKeyName, GetLastError()));
            }
            else
            {
                DISPATCHER_LOG_INFO((("opened key: %S\n"), subKeyName));

                mfxU32 vendorID = 0, deviceID = 0, merit = 0;
                DWORD size;

                // query vendor and device IDs
                size = sizeof(vendorID);
                bRes = subKey.Query(vendorIDKeyName, REG_DWORD, (LPBYTE) &vendorID, &size);
                DISPATCHER_LOG_OPERATION({
                    if (bRes)
                    {
                        DISPATCHER_LOG_INFO((("loaded %S : 0x%x\n"), vendorIDKeyName, vendorID));
                    }
                    else
                    {
                        DISPATCHER_LOG_WRN((("querying %S : RegQueryValueExA()==0x%x\n"), vendorIDKeyName, GetLastError()));
                    }
                })

                if (bRes)
                {
                    size = sizeof(deviceID);
                    bRes = subKey.Query(deviceIDKeyName, REG_DWORD, (LPBYTE) &deviceID, &size);
                    DISPATCHER_LOG_OPERATION({
                        if (bRes)
                        {
                            DISPATCHER_LOG_INFO((("loaded %S : 0x%x\n"), deviceIDKeyName, deviceID));
                        }
                        else
                        {
                            DISPATCHER_LOG_WRN((("querying %S : RegQueryValueExA()==0x%x\n"), deviceIDKeyName, GetLastError()));
                        }
                    })
                }
                // query merit value
                if (bRes)
                {
                    size = sizeof(merit);
                    bRes = subKey.Query(meritKeyName, REG_DWORD, (LPBYTE) &merit, &size);
                    DISPATCHER_LOG_OPERATION({
                        if (bRes)
                        {
                            DISPATCHER_LOG_INFO((("loaded %S : %d\n"), meritKeyName, merit));
                        }
                        else
                        {
                            DISPATCHER_LOG_WRN((("querying %S : RegQueryValueExA()==0x%x\n"), meritKeyName, GetLastError()));
                        }
                    })
                }

                // if the library fits required parameters,
                // query the library's path
                if (bRes)
                {
                    // compare device's and library's IDs
                    if (MFX_LIB_HARDWARE == m_implType)
                    {
                        if (m_vendorID != vendorID)
                        {
                            bRes = false;
                            DISPATCHER_LOG_WRN((("%S conflict, actual = 0x%x : required = 0x%x\n"), vendorIDKeyName, m_vendorID, vendorID));
                        }
                        if (bRes && m_deviceID != deviceID)
                        {
                            bRes = false;
                            DISPATCHER_LOG_WRN((("%S conflict, actual = 0x%x : required = 0x%x\n"), deviceIDKeyName, m_deviceID, deviceID));
                        }
                    }

                    DISPATCHER_LOG_OPERATION({
                    if (bRes)
                    {
                        if (!(((m_lastLibMerit > merit) || ((m_lastLibMerit == merit) && (m_lastLibIndex < index))) &&
                             (libMerit < merit)))
                        {
                            DISPATCHER_LOG_WRN((("merit conflict: lastMerit = 0x%x, requiredMerit = 0x%x, libraryMerit = 0x%x, lastindex = %d, index = %d\n")
                                        , m_lastLibMerit, merit, libMerit, m_lastLibIndex, index));
                        }
                    }})

                    if ((bRes) &&
                        ((m_lastLibMerit > merit) || ((m_lastLibMerit == merit) && (m_lastLibIndex < index))) &&
                        (libMerit < merit))
                    {
                        wchar_t tmpPath[MFX_MAX_DLL_PATH];
                        DWORD tmpPathSize = sizeof(tmpPath);

                        bRes = subKey.Query(pathKeyName, REG_SZ, (LPBYTE) tmpPath, &tmpPathSize);
                        if (!bRes)
                        {
                            DISPATCHER_LOG_WRN((("error querying %S : RegQueryValueExA()==0x%x\n"), pathKeyName, GetLastError()));
                        }
                        else
                        {
                            DISPATCHER_LOG_INFO((("loaded %S : %S\n"), pathKeyName, tmpPath));

                            msdk_disp_char_cpy_s(libPath, sizeof(libPath) / sizeof(libPath[0]), tmpPath);
                            msdk_disp_char_cpy_s(m_SubKeyName, sizeof(m_SubKeyName) / sizeof(m_SubKeyName[0]), subKeyName);

                            libMerit = merit;
                            libIndex = index;

                            // set the library's type
                            if ((0 == vendorID) || (0 == deviceID))
                            {
                                *pImplType = MFX_LIB_SOFTWARE;
                                DISPATCHER_LOG_INFO((("Library type is MFX_LIB_SOFTWARE\n")));
                            }
                            else
                            {
                                *pImplType = MFX_LIB_HARDWARE;
                                DISPATCHER_LOG_INFO((("Library type is MFX_LIB_HARDWARE\n")));
                            }
                        }
                    }
                }
            }
        }

        // advance key index
        index += 1;

    } while (enumRes);

    // if the library's path was successfully read,
    // the merit variable holds valid value
    if (0 == libMerit)
    {
        return MFX_ERR_NOT_FOUND;
    }

    msdk_disp_char_cpy_s(pPath, pathSize, libPath);

    m_lastLibIndex = libIndex;
    m_lastLibMerit = libMerit;
    m_bIsSubKeyValid = true;

#endif

    return MFX_ERR_NONE;

} // mfxStatus MFXLibraryIterator::SelectDLLVersion(wchar_t *pPath, size_t pathSize, eMfxImplType *pImplType, mfxVersion minVersion)

mfxIMPL MFXLibraryIterator::GetImplementationType()
{
    return m_implInterface;
} // mfxIMPL MFXLibraryIterator::GetImplementationType()

bool MFXLibraryIterator::GetSubKeyName(msdk_disp_char *subKeyName, size_t length) const
{
    msdk_disp_char_cpy_s(subKeyName, length, m_SubKeyName);
    return m_bIsSubKeyValid;
}
} // namespace MFX
#endif // #if defined(_WIN32) || defined(_WIN64)

