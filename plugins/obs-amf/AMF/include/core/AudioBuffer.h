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

#ifndef AMF_AudioBuffer_h
#define AMF_AudioBuffer_h
#pragma once

#include "Data.h"
#if defined(_MSC_VER)
    #pragma warning( push )
    #pragma warning(disable : 4263)
    #pragma warning(disable : 4264)
#endif
#if defined(__cplusplus)
namespace amf
{
#endif
    typedef enum AMF_AUDIO_FORMAT
    {
        AMFAF_UNKNOWN   =-1,
        AMFAF_U8        = 0,               // amf_uint8
        AMFAF_S16       = 1,               // amf_int16
        AMFAF_S32       = 2,               // amf_int32
        AMFAF_FLT       = 3,               // amf_float
        AMFAF_DBL       = 4,               // amf_double

        AMFAF_U8P       = 5,               // amf_uint8
        AMFAF_S16P      = 6,               // amf_int16
        AMFAF_S32P      = 7,               // amf_int32
        AMFAF_FLTP      = 8,               // amf_float
        AMFAF_DBLP      = 9,               // amf_double
        AMFAF_FIRST     = AMFAF_U8,
        AMFAF_LAST      = AMFAF_DBLP,
    } AMF_AUDIO_FORMAT;

    //----------------------------------------------------------------------------------------------
    // AMFAudioBufferObserver interface - callback
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMFAudioBuffer;
    class AMF_NO_VTABLE AMFAudioBufferObserver
    {
    public:
        virtual void                AMF_STD_CALL OnBufferDataRelease(AMFAudioBuffer* pBuffer) = 0;
    };
#else // #if defined(__cplusplus)
    typedef struct AMFAudioBuffer AMFAudioBuffer;
    typedef struct AMFAudioBufferObserver AMFAudioBufferObserver;
    typedef struct AMFAudioBufferObserverVtbl
    {
        void                (AMF_STD_CALL *OnBufferDataRelease)(AMFAudioBufferObserver* pThis, AMFAudioBuffer* pBuffer);
    } AMFAudioBufferObserverVtbl;

    struct AMFAudioBufferObserver
    {
        const AMFAudioBufferObserverVtbl *pVtbl;
    };

#endif // #if defined(__cplusplus)
    //----------------------------------------------------------------------------------------------
    // AudioBuffer interface
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFAudioBuffer : public AMFData
    {
    public:
        AMF_DECLARE_IID(0x2212ff8, 0x6107, 0x430b, 0xb6, 0x3c, 0xc7, 0xe5, 0x40, 0xe5, 0xf8, 0xeb)

        virtual amf_int32           AMF_STD_CALL GetSampleCount() = 0;
        virtual amf_int32           AMF_STD_CALL GetSampleRate() = 0;
        virtual amf_int32           AMF_STD_CALL GetChannelCount() = 0;
        virtual AMF_AUDIO_FORMAT    AMF_STD_CALL GetSampleFormat() = 0;
        virtual amf_int32           AMF_STD_CALL GetSampleSize() = 0;
        virtual amf_uint32          AMF_STD_CALL GetChannelLayout() = 0;
        virtual void*               AMF_STD_CALL GetNative() = 0;
        virtual amf_size            AMF_STD_CALL GetSize() = 0;

        // Observer management
        virtual void                AMF_STD_CALL AddObserver(AMFAudioBufferObserver* pObserver) = 0;
        virtual void                AMF_STD_CALL RemoveObserver(AMFAudioBufferObserver* pObserver) = 0;

    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFAudioBuffer> AMFAudioBufferPtr;
    //----------------------------------------------------------------------------------------------
#else // #if defined(__cplusplus)
        AMF_DECLARE_IID(AMFAudioBuffer, 0x2212ff8, 0x6107, 0x430b, 0xb6, 0x3c, 0xc7, 0xe5, 0x40, 0xe5, 0xf8, 0xeb)

    typedef struct AMFAudioBufferVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFAudioBuffer* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFAudioBuffer* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFAudioBuffer* pThis, const struct AMFGuid *interfaceID, void** ppInterface);

        // AMFPropertyStorage interface
        AMF_RESULT          (AMF_STD_CALL *SetProperty)(AMFAudioBuffer* pThis, const wchar_t* name, AMFVariantStruct value);
        AMF_RESULT          (AMF_STD_CALL *GetProperty)(AMFAudioBuffer* pThis, const wchar_t* name, AMFVariantStruct* pValue);
        amf_bool            (AMF_STD_CALL *HasProperty)(AMFAudioBuffer* pThis, const wchar_t* name);
        amf_size            (AMF_STD_CALL *GetPropertyCount)(AMFAudioBuffer* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyAt)(AMFAudioBuffer* pThis, amf_size index, wchar_t* name, amf_size nameSize, AMFVariantStruct* pValue);
        AMF_RESULT          (AMF_STD_CALL *Clear)(AMFAudioBuffer* pThis);
        AMF_RESULT          (AMF_STD_CALL *AddTo)(AMFAudioBuffer* pThis, AMFPropertyStorage* pDest, amf_bool overwrite, amf_bool deep);
        AMF_RESULT          (AMF_STD_CALL *CopyTo)(AMFAudioBuffer* pThis, AMFPropertyStorage* pDest, amf_bool deep);
        void                (AMF_STD_CALL *AddObserver)(AMFAudioBuffer* pThis, AMFPropertyStorageObserver* pObserver);
        void                (AMF_STD_CALL *RemoveObserver)(AMFAudioBuffer* pThis, AMFPropertyStorageObserver* pObserver);

        // AMFData interface

        AMF_MEMORY_TYPE     (AMF_STD_CALL *GetMemoryType)(AMFAudioBuffer* pThis);

        AMF_RESULT          (AMF_STD_CALL *Duplicate)(AMFAudioBuffer* pThis, AMF_MEMORY_TYPE type, AMFData** ppData);
        AMF_RESULT          (AMF_STD_CALL *Convert)(AMFAudioBuffer* pThis, AMF_MEMORY_TYPE type); // optimal interop if possilble. Copy through host memory if needed
        AMF_RESULT          (AMF_STD_CALL *Interop)(AMFAudioBuffer* pThis, AMF_MEMORY_TYPE type); // only optimal interop if possilble. No copy through host memory for GPU objects

        AMF_DATA_TYPE       (AMF_STD_CALL *GetDataType)(AMFAudioBuffer* pThis);

        amf_bool            (AMF_STD_CALL *IsReusable)(AMFAudioBuffer* pThis);

        void                (AMF_STD_CALL *SetPts)(AMFAudioBuffer* pThis, amf_pts pts);
        amf_pts             (AMF_STD_CALL *GetPts)(AMFAudioBuffer* pThis);
        void                (AMF_STD_CALL *SetDuration)(AMFAudioBuffer* pThis, amf_pts duration);
        amf_pts             (AMF_STD_CALL *GetDuration)(AMFAudioBuffer* pThis);

        // AMFAudioBuffer interface

        amf_int32           (AMF_STD_CALL *GetSampleCount)(AMFAudioBuffer* pThis);
        amf_int32           (AMF_STD_CALL *GetSampleRate)(AMFAudioBuffer* pThis);
        amf_int32           (AMF_STD_CALL *GetChannelCount)(AMFAudioBuffer* pThis);
        AMF_AUDIO_FORMAT    (AMF_STD_CALL *GetSampleFormat)(AMFAudioBuffer* pThis);
        amf_int32           (AMF_STD_CALL *GetSampleSize)(AMFAudioBuffer* pThis);
        amf_uint32          (AMF_STD_CALL *GetChannelLayout)(AMFAudioBuffer* pThis);
        void*               (AMF_STD_CALL *GetNative)(AMFAudioBuffer* pThis);
        amf_size            (AMF_STD_CALL *GetSize)(AMFAudioBuffer* pThis);

        // Observer management
        void                (AMF_STD_CALL *AddObserver_AudioBuffer)(AMFAudioBuffer* pThis, AMFAudioBufferObserver* pObserver);
        void                (AMF_STD_CALL *RemoveObserver_AudioBuffer)(AMFAudioBuffer* pThis, AMFAudioBufferObserver* pObserver);

    } AMFAudioBufferVtbl;

    struct AMFAudioBuffer
    {
        const AMFAudioBufferVtbl *pVtbl;
    };
#endif // #if defined(__cplusplus)
#if defined(__cplusplus)
} // namespace
#endif
#if defined(_MSC_VER)
    #pragma warning( pop )
#endif
#endif //#ifndef AMF_AudioBuffer_h
