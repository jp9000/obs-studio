// 
// Notice Regarding Standards.  AMD does not provide a license or sublicense to
// any Intellectual Property Rights relating to any standards, including but not
// limited to any audio and/or video codec technologies such as MPEG-2, MPEG-4;
// AVC/H.264; HEVC/H.265; AAC decode/FFMPEG; AAC encode/FFMPEG; VC-1; and MP3
// (collectively, the "Media Technologies"). For clarity, you will pay any
// royalties due for such third party technologies, which may include the Media
// Technologies that are owed as a result of AMD providing the Software to you.
// 
// MIT license 
// 
// Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#ifndef AMF_Debug_h
#define AMF_Debug_h
#pragma once

#include "Platform.h"
#include "Result.h"

#if defined(__cplusplus)
namespace amf
{
#endif
    //----------------------------------------------------------------------------------------------
    // AMFDebug interface - singleton
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFDebug
    {
    public:
        virtual  void               AMF_STD_CALL EnablePerformanceMonitor(amf_bool enable) = 0;
        virtual  amf_bool           AMF_STD_CALL PerformanceMonitorEnabled() = 0;
        virtual  void               AMF_STD_CALL AssertsEnable(amf_bool enable) = 0;
        virtual  amf_bool           AMF_STD_CALL AssertsEnabled() = 0;
    };
#else // #if defined(__cplusplus)
    typedef struct AMFDebug AMFDebug;
    typedef struct AMFDebugVtbl
    {
        // AMFDebug interface
        void               (AMF_STD_CALL *EnablePerformanceMonitor)(AMFDebug* pThis, amf_bool enable);
        amf_bool           (AMF_STD_CALL *PerformanceMonitorEnabled)(AMFDebug* pThis);
        void               (AMF_STD_CALL *AssertsEnable)(AMFDebug* pThis, amf_bool enable);
        amf_bool           (AMF_STD_CALL *AssertsEnabled)(AMFDebug* pThis);
    } AMFDebugVtbl;

    struct AMFDebug
    {
        const AMFDebugVtbl *pVtbl;
    };

#endif // #if defined(__cplusplus)
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
}
#endif

#endif // AMF_Debug_h
