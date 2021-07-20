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

#ifndef AMF_Dump_h
#define AMF_Dump_h
#pragma once

#include "Platform.h"
#include "Result.h"
#include "Interface.h"

#if defined(__cplusplus)
namespace amf
{
#endif
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFDump : public AMFInterface
    {
    public:
        AMF_DECLARE_IID(0x75366ad4, 0x504c, 0x430b, 0xbb, 0xe2, 0xad, 0x21, 0x82, 0x8, 0xf, 0x72);


        virtual const wchar_t*  AMF_STD_CALL GetDumpBasePath() const = 0;             //  Get application dump base path
        virtual AMF_RESULT      AMF_STD_CALL SetDumpBasePath(const wchar_t* path) = 0;    //  Set application dump base path

        //  Enable/disable input and/or output stream dumps
        virtual bool            AMF_STD_CALL IsInputDumpEnabled() const = 0;
        virtual AMF_RESULT      AMF_STD_CALL EnableInputDump(bool enabled) = 0;     
        virtual const wchar_t*  AMF_STD_CALL GetInputDumpFullName() const = 0;  //  Get full name of dump file

        //  Enable/disable input and/or output stream dumps
        virtual bool            AMF_STD_CALL IsOutputDumpEnabled() const = 0;
        virtual AMF_RESULT      AMF_STD_CALL EnableOutputDump(bool enabled) = 0;     
        virtual const wchar_t*  AMF_STD_CALL GetOutputDumpFullName() const = 0;  //  Get full name of dump file

        //  When enabled, each new application session will create a subfolder with a time stamp in the base path tree (disabled by default)
        virtual bool            AMF_STD_CALL IsPerSessionDumpEnabled() const = 0;
        virtual void            AMF_STD_CALL EnablePerSessionDump(bool enabled) = 0;      
    };
    typedef AMFInterfacePtr_T<AMFDump> AMFDumpPtr;
#else // #if defined(__cplusplus)
        AMF_DECLARE_IID(AMFDump, 0x75366ad4, 0x504c, 0x430b, 0xbb, 0xe2, 0xad, 0x21, 0x82, 0x8, 0xf, 0x72);
    typedef struct AMFDump AMFDump;

    typedef struct AMFDumpVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFDump* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFDump* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFDump* pThis, const struct AMFGuid *interfaceID, void** ppInterface);
        
        // AMFDump interface
        const wchar_t*  (AMF_STD_CALL *GetDumpBasePath)(AMFDump* pThis) const;             //  Get application dump base path
        AMF_RESULT      (AMF_STD_CALL *SetDumpBasePath)(AMFDump* pThis, const wchar_t* path);    //  Set application dump base path
        
        //  Enable/disable input and/or output stream dumps
        bool            (AMF_STD_CALL *IsInputDumpEnabled)(AMFDump* pThis) const;
        AMF_RESULT      (AMF_STD_CALL *EnableInputDump)(AMFDump* pThis, bool enabled);     
        const wchar_t*  (AMF_STD_CALL *GetInputDumpFullName)(AMFDump* pThis) const;  //  Get full name of dump file

        //  Enable/disable input and/or output stream dumps
        bool            (AMF_STD_CALL *IsOutputDumpEnabled)(AMFDump* pThis) const;
        AMF_RESULT      (AMF_STD_CALL *EnableOutputDump)(AMFDump* pThis, bool enabled);     
        const wchar_t*  (AMF_STD_CALL *GetOutputDumpFullName)(AMFDump* pThis) const;  //  Get full name of dump file

        //  When enabled, each new application session will create a subfolder with a time stamp in the base path tree (disabled by default)
        bool            (AMF_STD_CALL *IsPerSessionDumpEnabled)(AMFDump* pThis) const;
        void            (AMF_STD_CALL *EnablePerSessionDump)(AMFDump* pThis, bool enabled);      

    } AMFDumpVtbl;

    struct AMFDump
    {
        const AMFDumpVtbl *pVtbl;
    };


#endif // #if defined(__cplusplus)
#if defined(__cplusplus)
} // namespace
#endif

#endif //AMF_Dump_h
