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

File Name: mfx_win_reg_key.cpp

\* ****************************************************************************** */

#if defined(_WIN32) || defined(_WIN64)

#include "mfx_win_reg_key.h"
#include "mfx_dispatcher_log.h"

#define TRACE_WINREG_ERROR(str, ...) DISPATCHER_LOG_ERROR((("[WINREG]: " str), __VA_ARGS__))

namespace MFX
{

WinRegKey::WinRegKey(void)
{
    m_hKey = (HKEY) 0;

} // WinRegKey::WinRegKey(void)

WinRegKey::~WinRegKey(void)
{
    Release();

} // WinRegKey::~WinRegKey(void)

void WinRegKey::Release(void)
{
    // close the opened key
    if (m_hKey)
    {
        RegCloseKey(m_hKey);
    }

    m_hKey = (HKEY) 0;

} // void WinRegKey::Release(void)

bool WinRegKey::Open(HKEY hRootKey, const wchar_t *pSubKey, REGSAM samDesired)
{
    LONG lRes;
    HKEY hTemp;

    //
    // All operation are performed in this order by intention.
    // It makes possible to reopen the keys, using itself as a base.
    //

    // try to the open registry key
    lRes = RegOpenKeyExW(hRootKey, pSubKey, 0, samDesired, &hTemp);
    if (ERROR_SUCCESS != lRes)
    {
        DISPATCHER_LOG_OPERATION(SetLastError(lRes));
        TRACE_WINREG_ERROR("Opening key \"%s\\%S\" : RegOpenKeyExW()==0x%x\n"
            , (HKEY_LOCAL_MACHINE == hRootKey) ? ("HKEY_LOCAL_MACHINE") 
            : (HKEY_CURRENT_USER == hRootKey)  ? ("HKEY_CURRENT_USER") 
            : "UNSUPPORTED_KEY", pSubKey, GetLastError());
        return false;
    }

    // release the object before initialization
    Release();

    // save the handle
    m_hKey = hTemp;

    return true;

} // bool WinRegKey::Open(HKEY hRootKey, const wchar_t *pSubKey, REGSAM samDesired)

bool WinRegKey::Open(WinRegKey &rootKey, const wchar_t *pSubKey, REGSAM samDesired)
{
    return Open(rootKey.m_hKey, pSubKey, samDesired);

} // bool WinRegKey::Open(WinRegKey &rootKey, const wchar_t *pSubKey, REGSAM samDesired)

bool WinRegKey::QueryValueSize(const wchar_t *pValueName, DWORD type, LPDWORD pcbData) {
    DWORD keyType = type;
    LONG lRes;

    // query the value
    lRes = RegQueryValueExW(m_hKey, pValueName, NULL, &keyType, 0, pcbData);
    if (ERROR_SUCCESS != lRes)
    {
        DISPATCHER_LOG_OPERATION(SetLastError(lRes));
        TRACE_WINREG_ERROR("Querying \"%S\" : RegQueryValueExA()==0x%x\n", pValueName, GetLastError());
        return false;
    }

    return true;
}

bool WinRegKey::Query(const wchar_t *pValueName, DWORD type, LPBYTE pData, LPDWORD pcbData)
{
    DWORD keyType = type;
    LONG lRes;
    DWORD dstSize = (pcbData) ? (*pcbData) : (0);

    // query the value
    lRes = RegQueryValueExW(m_hKey, pValueName, NULL, &keyType, pData, pcbData);
    if (ERROR_SUCCESS != lRes)
    {
        DISPATCHER_LOG_OPERATION(SetLastError(lRes));
        TRACE_WINREG_ERROR("Querying \"%S\" : RegQueryValueExA()==0x%x\n", pValueName, GetLastError());
        return false;
    }

    // check the type
    if (keyType != type)
    {
        TRACE_WINREG_ERROR("Querying \"%S\" : expectedType=%d, returned=%d\n", pValueName, type, keyType);
        return false;
    }

    // terminate the string only if pointers not NULL
    if ((REG_SZ == type || REG_EXPAND_SZ == type) && NULL != pData && NULL != pcbData)
    {
        wchar_t *pString = (wchar_t *) pData;
        size_t NullEndingSizeBytes = sizeof(wchar_t); // size of string termination null character
        if (dstSize < NullEndingSizeBytes)
        {
            TRACE_WINREG_ERROR("Querying \"%S\" : buffer is too small for null-terminated string", pValueName);
            return false;
        }
        size_t maxStringLengthBytes = dstSize - NullEndingSizeBytes;
        size_t maxStringIndex = dstSize / sizeof(wchar_t) - 1;

        size_t lastIndex = (maxStringLengthBytes < *pcbData) ? (maxStringIndex) : (*pcbData) / sizeof(wchar_t);

        pString[lastIndex] = (wchar_t) 0;
    }
    else if(REG_MULTI_SZ == type && NULL != pData && NULL != pcbData)
    {
        wchar_t *pString = (wchar_t *) pData;
        size_t NullEndingSizeBytes = sizeof(wchar_t)*2; // size of string termination null characters
        if (dstSize < NullEndingSizeBytes)
        {
            TRACE_WINREG_ERROR("Querying \"%S\" : buffer is too small for multi-line null-terminated string", pValueName);
            return false;
        }
        size_t maxStringLengthBytes = dstSize - NullEndingSizeBytes;
        size_t maxStringIndex = dstSize / sizeof(wchar_t) - 1;

        size_t lastIndex = (maxStringLengthBytes < *pcbData) ? (maxStringIndex) : (*pcbData) / sizeof(wchar_t) + 1;

        // last 2 bytes should be 0 in case of REG_MULTI_SZ
        pString[lastIndex] = pString[lastIndex - 1] = (wchar_t) 0;
    }

    return true;

} // bool WinRegKey::Query(const wchar_t *pValueName, DWORD type, LPBYTE pData, LPDWORD pcbData)

bool WinRegKey::EnumValue(DWORD index, wchar_t *pValueName, LPDWORD pcchValueName, LPDWORD pType)
{
    LONG lRes;

    // enum the values
    lRes = RegEnumValueW(m_hKey, index, pValueName, pcchValueName, 0, pType, NULL, NULL);
    if (ERROR_SUCCESS != lRes)
    {
        DISPATCHER_LOG_OPERATION(SetLastError(lRes));
        return false;
    }

    return true;

} // bool WinRegKey::EnumValue(DWORD index, wchar_t *pValueName, LPDWORD pcchValueName, LPDWORD pType)

bool WinRegKey::EnumKey(DWORD index, wchar_t *pValueName, LPDWORD pcchValueName)
{
    LONG lRes;

    // enum the keys
    lRes = RegEnumKeyExW(m_hKey, index, pValueName, pcchValueName, NULL, NULL, NULL, NULL);
    if (ERROR_SUCCESS != lRes)
    {
        DISPATCHER_LOG_OPERATION(SetLastError(lRes));
        TRACE_WINREG_ERROR("EnumKey with index=%d: RegEnumKeyExW()==0x%x\n", index, GetLastError());
        return false;
    }

    return true;

} // bool WinRegKey::EnumKey(DWORD index, wchar_t *pValueName, LPDWORD pcchValueName)

bool WinRegKey::QueryInfo(LPDWORD lpcSubkeys)
{
    LONG lRes;

    lRes = RegQueryInfoKeyW(m_hKey, NULL, 0, 0, lpcSubkeys, 0, 0, 0, 0, 0, 0, 0);
    if (ERROR_SUCCESS != lRes) {
        TRACE_WINREG_ERROR("RegQueryInfoKeyW()==0x%x\n", lRes);
        return false;
    }
    return true;

} //bool QueryInfo(LPDWORD lpcSubkeys);

} // namespace MFX

#endif // #if defined(_WIN32) || defined(_WIN64)
