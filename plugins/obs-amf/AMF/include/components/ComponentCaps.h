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

#ifndef AMF_ComponentCaps_h
#define AMF_ComponentCaps_h

#pragma once

#include "../core/Interface.h"
#include "../core/PropertyStorage.h"
#include "../core/Surface.h"

#if defined(__cplusplus)
namespace amf
{
#endif
    typedef enum AMF_ACCELERATION_TYPE
    {
        AMF_ACCEL_NOT_SUPPORTED = -1,
        AMF_ACCEL_HARDWARE,
        AMF_ACCEL_GPU,
        AMF_ACCEL_SOFTWARE
    } AMF_ACCELERATION_TYPE;
    //----------------------------------------------------------------------------------------------
    // AMFIOCaps interface
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFIOCaps : public AMFInterface
    {
    public:
        //  Get supported resolution ranges in pixels/lines:
        virtual void AMF_STD_CALL GetWidthRange(amf_int32* minWidth, amf_int32* maxWidth) const = 0;
        virtual void AMF_STD_CALL GetHeightRange(amf_int32* minHeight, amf_int32* maxHeight) const = 0;

        //  Get memory alignment in lines: Vertical aligmnent should be multiples of this number
        virtual amf_int32 AMF_STD_CALL GetVertAlign() const = 0;
        
        //  Enumerate supported surface pixel formats
        virtual amf_int32 AMF_STD_CALL GetNumOfFormats() const = 0;
        virtual  AMF_RESULT AMF_STD_CALL GetFormatAt(amf_int32 index, AMF_SURFACE_FORMAT* format, amf_bool* native) const = 0;

        //  Enumerate supported memory types
        virtual amf_int32 AMF_STD_CALL GetNumOfMemoryTypes() const = 0;
        virtual AMF_RESULT AMF_STD_CALL GetMemoryTypeAt(amf_int32 index, AMF_MEMORY_TYPE* memType, amf_bool* native) const = 0;

        virtual amf_bool AMF_STD_CALL IsInterlacedSupported() const = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFIOCaps>    AMFIOCapsPtr;
#else // #if defined(__cplusplus)
    typedef struct AMFIOCaps AMFIOCaps;

    typedef struct AMFIOCapsVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFIOCaps* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFIOCaps* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFIOCaps* pThis, const struct AMFGuid *interfaceID, void** ppInterface);

        // AMFIOCaps interface
        //  Get supported resolution ranges in pixels/lines:
        void (AMF_STD_CALL *GetWidthRange)(AMFIOCaps* pThis, amf_int32* minWidth, amf_int32* maxWidth);
        void (AMF_STD_CALL *GetHeightRange)(AMFIOCaps* pThis, amf_int32* minHeight, amf_int32* maxHeight);

        //  Get memory alignment in lines: Vertical aligmnent should be multiples of this number
        amf_int32 (AMF_STD_CALL *GetVertAlign)(AMFIOCaps* pThis);
        
        //  Enumerate supported surface pixel formats
        amf_int32 (AMF_STD_CALL *GetNumOfFormats)(AMFIOCaps* pThis);
        AMF_RESULT (AMF_STD_CALL *GetFormatAt)(AMFIOCaps* pThis, amf_int32 index, AMF_SURFACE_FORMAT* format, amf_bool* native);

        //  Enumerate supported memory types
        amf_int32 (AMF_STD_CALL *GetNumOfMemoryTypes)(AMFIOCaps* pThis);
        AMF_RESULT (AMF_STD_CALL *GetMemoryTypeAt)(AMFIOCaps* pThis, amf_int32 index, AMF_MEMORY_TYPE* memType, amf_bool* native);

        amf_bool (AMF_STD_CALL *IsInterlacedSupported)(AMFIOCaps* pThis);
    } AMFIOCapsVtbl;

    struct AMFIOCaps
    {
        const AMFIOCapsVtbl *pVtbl;
    };

#endif // #if defined(__cplusplus)
   
    //----------------------------------------------------------------------------------------------
    // AMFCaps interface - base interface for every h/w module supported by Capability Manager
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFCaps : public AMFPropertyStorage
    {
    public:
        virtual AMF_ACCELERATION_TYPE AMF_STD_CALL GetAccelerationType() const = 0;
        virtual AMF_RESULT AMF_STD_CALL GetInputCaps(AMFIOCaps** input) = 0;
        virtual AMF_RESULT AMF_STD_CALL GetOutputCaps(AMFIOCaps** output) = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFCaps>  AMFCapsPtr;
#else // #if defined(__cplusplus)
    typedef struct AMFCaps AMFCaps;

    typedef struct AMFCapsVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFCaps* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFCaps* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFCaps* pThis, const struct AMFGuid *interfaceID, void** ppInterface);

        // AMFPropertyStorage interface
        AMF_RESULT          (AMF_STD_CALL *SetProperty)(AMFCaps* pThis, const wchar_t* name, AMFVariantStruct value);
        AMF_RESULT          (AMF_STD_CALL *GetProperty)(AMFCaps* pThis, const wchar_t* name, AMFVariantStruct* pValue);
        amf_bool            (AMF_STD_CALL *HasProperty)(AMFCaps* pThis, const wchar_t* name);
        amf_size            (AMF_STD_CALL *GetPropertyCount)(AMFCaps* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyAt)(AMFCaps* pThis, amf_size index, wchar_t* name, amf_size nameSize, AMFVariantStruct* pValue);
        AMF_RESULT          (AMF_STD_CALL *Clear)(AMFCaps* pThis);
        AMF_RESULT          (AMF_STD_CALL *AddTo)(AMFCaps* pThis, AMFPropertyStorage* pDest, amf_bool overwrite, amf_bool deep);
        AMF_RESULT          (AMF_STD_CALL *CopyTo)(AMFCaps* pThis, AMFPropertyStorage* pDest, amf_bool deep);
        void                (AMF_STD_CALL *AddObserver)(AMFCaps* pThis, AMFPropertyStorageObserver* pObserver);
        void                (AMF_STD_CALL *RemoveObserver)(AMFCaps* pThis, AMFPropertyStorageObserver* pObserver);

        // AMFCaps interface

        AMF_ACCELERATION_TYPE (AMF_STD_CALL *GetAccelerationType)(AMFCaps* pThis);
        AMF_RESULT (AMF_STD_CALL *GetInputCaps)(AMFCaps* pThis, AMFIOCaps** input);
        AMF_RESULT (AMF_STD_CALL *GetOutputCaps)(AMFCaps* pThis, AMFIOCaps** output);
    } AMFCapsVtbl;

    struct AMFCaps
    {
        const AMFCapsVtbl *pVtbl;
    };
#endif // #if defined(__cplusplus)

    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
}
#endif

#endif //#ifndef AMF_ComponentCaps_h
