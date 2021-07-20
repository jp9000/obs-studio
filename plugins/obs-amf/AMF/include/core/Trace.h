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

#ifndef AMF_Trace_h
#define AMF_Trace_h
#pragma once

#include "Platform.h"
#include "Result.h"
#include "Surface.h"
#include "AudioBuffer.h"

#if defined(__cplusplus)
namespace amf
{
#endif
    //----------------------------------------------------------------------------------------------
    // trace levels
    //----------------------------------------------------------------------------------------------
    #define AMF_TRACE_ERROR     0
    #define AMF_TRACE_WARNING   1
    #define AMF_TRACE_INFO      2 // default in sdk
    #define AMF_TRACE_DEBUG     3
    #define AMF_TRACE_TRACE     4

    #define AMF_TRACE_TEST      5
    #define AMF_TRACE_NOLOG     100

    //----------------------------------------------------------------------------------------------
    // available trace writers
    //----------------------------------------------------------------------------------------------
    #define AMF_TRACE_WRITER_CONSOLE            L"Console"
    #define AMF_TRACE_WRITER_DEBUG_OUTPUT       L"DebugOutput"
    #define AMF_TRACE_WRITER_FILE               L"File"

    //----------------------------------------------------------------------------------------------
    // AMFTraceWriter interface - callback
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFTraceWriter
    {
    public:
        virtual void AMF_CDECL_CALL Write(const wchar_t* scope, const wchar_t* message) = 0;
        virtual void AMF_CDECL_CALL Flush() = 0;
    };
#else // #if defined(__cplusplus)
    typedef struct AMFTraceWriter AMFTraceWriter;

    typedef struct AMFTraceWriterVtbl
    {
        // AMFTraceWriter interface
        void (AMF_CDECL_CALL *Write)(AMFTraceWriter* pThis, const wchar_t* scope, const wchar_t* message);
        void (AMF_CDECL_CALL *Flush)(AMFTraceWriter* pThis);
    } AMFTraceWriterVtbl;

    struct AMFTraceWriter
    {
        const AMFTraceWriterVtbl *pVtbl;
    };

#endif // #if defined(__cplusplus)

    //----------------------------------------------------------------------------------------------
    // AMFTrace interface - singleton
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFTrace
    {
    public:
        virtual  void               AMF_STD_CALL TraceW(const wchar_t* src_path, amf_int32 line, amf_int32 level, const wchar_t* scope,amf_int32 countArgs, const wchar_t* format, ...) = 0;
        virtual  void               AMF_STD_CALL Trace(const wchar_t* src_path, amf_int32 line, amf_int32 level, const wchar_t* scope, const wchar_t* message, va_list* pArglist) = 0;

        virtual amf_int32           AMF_STD_CALL SetGlobalLevel(amf_int32 level) = 0;
        virtual amf_int32           AMF_STD_CALL GetGlobalLevel() = 0;

        virtual amf_bool            AMF_STD_CALL EnableWriter(const wchar_t* writerID, bool enable) = 0;
        virtual amf_bool            AMF_STD_CALL WriterEnabled(const wchar_t* writerID) = 0;
        virtual AMF_RESULT          AMF_STD_CALL TraceEnableAsync(amf_bool enable) = 0;
        virtual AMF_RESULT          AMF_STD_CALL TraceFlush() = 0;
        virtual AMF_RESULT          AMF_STD_CALL SetPath(const wchar_t* path) = 0;
        virtual AMF_RESULT          AMF_STD_CALL GetPath(wchar_t* path, amf_size* pSize) = 0;
        virtual amf_int32           AMF_STD_CALL SetWriterLevel(const wchar_t* writerID, amf_int32 level) = 0;
        virtual amf_int32           AMF_STD_CALL GetWriterLevel(const wchar_t* writerID) = 0;
        virtual amf_int32           AMF_STD_CALL SetWriterLevelForScope(const wchar_t* writerID, const wchar_t* scope, amf_int32 level) = 0;
        virtual amf_int32           AMF_STD_CALL GetWriterLevelForScope(const wchar_t* writerID, const wchar_t* scope) = 0;

        virtual amf_int32           AMF_STD_CALL GetIndentation() = 0;
        virtual void                AMF_STD_CALL Indent(amf_int32 addIndent) = 0;

        virtual void                AMF_STD_CALL RegisterWriter(const wchar_t* writerID, AMFTraceWriter* pWriter, amf_bool enable) = 0;
        virtual void                AMF_STD_CALL UnregisterWriter(const wchar_t* writerID) = 0;

        virtual const wchar_t*      AMF_STD_CALL GetResultText(AMF_RESULT res) = 0;
        virtual const wchar_t*      AMF_STD_CALL SurfaceGetFormatName(const AMF_SURFACE_FORMAT eSurfaceFormat) = 0;
        virtual AMF_SURFACE_FORMAT  AMF_STD_CALL SurfaceGetFormatByName(const wchar_t* name) = 0;

        virtual const wchar_t* const AMF_STD_CALL GetMemoryTypeName(const AMF_MEMORY_TYPE memoryType) = 0;
        virtual AMF_MEMORY_TYPE     AMF_STD_CALL GetMemoryTypeByName(const wchar_t* name) = 0;

        virtual const wchar_t* const AMF_STD_CALL GetSampleFormatName(const AMF_AUDIO_FORMAT eFormat) = 0;
        virtual AMF_AUDIO_FORMAT    AMF_STD_CALL GetSampleFormatByName(const wchar_t* name) = 0;
    };
#else // #if defined(__cplusplus)
    typedef struct AMFTrace AMFTrace;

    typedef struct AMFTraceVtbl
    {
        // AMFTrace interface
         void               (AMF_STD_CALL *TraceW)(AMFTrace* pThis, const wchar_t* src_path, amf_int32 line, amf_int32 level, const wchar_t* scope,amf_int32 countArgs, const wchar_t* format, ...);
         void               (AMF_STD_CALL *Trace)(AMFTrace* pThis, const wchar_t* src_path, amf_int32 line, amf_int32 level, const wchar_t* scope, const wchar_t* message, va_list* pArglist);

        amf_int32           (AMF_STD_CALL *SetGlobalLevel)(AMFTrace* pThis, amf_int32 level);
        amf_int32           (AMF_STD_CALL *GetGlobalLevel)(AMFTrace* pThis);

        amf_bool            (AMF_STD_CALL *EnableWriter)(AMFTrace* pThis, const wchar_t* writerID, amf_bool enable);
        amf_bool            (AMF_STD_CALL *WriterEnabled)(AMFTrace* pThis, const wchar_t* writerID);
        AMF_RESULT          (AMF_STD_CALL *TraceEnableAsync)(AMFTrace* pThis, amf_bool enable);
        AMF_RESULT          (AMF_STD_CALL *TraceFlush)(AMFTrace* pThis);
        AMF_RESULT          (AMF_STD_CALL *SetPath)(AMFTrace* pThis, const wchar_t* path);
        AMF_RESULT          (AMF_STD_CALL *GetPath)(AMFTrace* pThis, wchar_t* path, amf_size* pSize);
        amf_int32           (AMF_STD_CALL *SetWriterLevel)(AMFTrace* pThis, const wchar_t* writerID, amf_int32 level);
        amf_int32           (AMF_STD_CALL *GetWriterLevel)(AMFTrace* pThis, const wchar_t* writerID);
        amf_int32           (AMF_STD_CALL *SetWriterLevelForScope)(AMFTrace* pThis, const wchar_t* writerID, const wchar_t* scope, amf_int32 level);
        amf_int32           (AMF_STD_CALL *GetWriterLevelForScope)(AMFTrace* pThis, const wchar_t* writerID, const wchar_t* scope);

        amf_int32           (AMF_STD_CALL *GetIndentation)(AMFTrace* pThis);
        void                (AMF_STD_CALL *Indent)(AMFTrace* pThis, amf_int32 addIndent);

        void                (AMF_STD_CALL *RegisterWriter)(AMFTrace* pThis, const wchar_t* writerID, AMFTraceWriter* pWriter, amf_bool enable);
        void                (AMF_STD_CALL *UnregisterWriter)(AMFTrace* pThis, const wchar_t* writerID);

        const wchar_t*      (AMF_STD_CALL *GetResultText)(AMFTrace* pThis, AMF_RESULT res);
        const wchar_t*      (AMF_STD_CALL *SurfaceGetFormatName)(AMFTrace* pThis, const AMF_SURFACE_FORMAT eSurfaceFormat);
        AMF_SURFACE_FORMAT  (AMF_STD_CALL *SurfaceGetFormatByName)(AMFTrace* pThis, const wchar_t* name);

        const wchar_t* const (AMF_STD_CALL *GetMemoryTypeName)(AMFTrace* pThis, const AMF_MEMORY_TYPE memoryType);
        AMF_MEMORY_TYPE     (AMF_STD_CALL *GetMemoryTypeByName)(AMFTrace* pThis, const wchar_t* name);

        const wchar_t* const (AMF_STD_CALL *GetSampleFormatName)(AMFTrace* pThis, const AMF_AUDIO_FORMAT eFormat);
        AMF_AUDIO_FORMAT    (AMF_STD_CALL *GetSampleFormatByName)(AMFTrace* pThis, const wchar_t* name);
    } AMFTraceVtbl;

    struct AMFTrace
    {
        const AMFTraceVtbl *pVtbl;
    };

#endif
#if defined(__cplusplus)
}
#endif


#endif // AMF_Trace_h
