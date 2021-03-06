/* ****************************************************************************** *\

Copyright (C) 2013-2017 Intel Corporation.  All rights reserved.

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

File Name: mfx_dispatcher_defs.h

\* ****************************************************************************** */

#pragma once
#include "mfxdefs.h"
#include <cstring>

#if defined(MFX_DISPATCHER_LOG)
#include <string>
#include <string.h>
#endif

#define MAX_PLUGIN_PATH 4096
#define MAX_PLUGIN_NAME 4096

#if defined(_WIN32) || defined(_WIN64)
typedef wchar_t  msdk_disp_char;
#define MSDK2WIDE(x) x

#if _MSC_VER >= 1400
    #define msdk_disp_char_cpy_s(to, to_size, from) wcscpy_s(to,to_size, from)
#else
    #define msdk_disp_char_cpy_s(to, to_size, from) wcscpy(to, from)
#endif

#else
typedef char msdk_disp_char;
//#define msdk_disp_char_cpy_s(to, to_size, from) strcpy(to, from)

inline void msdk_disp_char_cpy_s(char * to, size_t to_size, const char * from)
{
    size_t source_len = strlen(from);
    size_t num_chars = (to_size - 1) < source_len ? (to_size - 1) : source_len;
    strncpy(to, from, num_chars);
    to[num_chars] = 0;
}

#if defined(MFX_DISPATCHER_LOG)
#define MSDK2WIDE(x) getWideString(x).c_str()

inline std::wstring getWideString(const char * string)
{
    size_t len = strlen(string);
    return std::wstring(string, string + len);
}
#else
    #define MSDK2WIDE(x) x  
#endif

#endif

#if defined(__GNUC__) && !defined(_WIN32) && !defined(_WIN64)
#define  sscanf_s  sscanf
#define  swscanf_s swscanf
#endif


// declare library module's handle
typedef void * mfxModuleHandle;

typedef void (MFX_CDECL * mfxFunctionPointer)(void);
