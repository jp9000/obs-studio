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

//-------------------------------------------------------------------------------------------------
// Capture interface declaration
//-------------------------------------------------------------------------------------------------
#ifndef __Capture_h__
#define __Capture_h__
#pragma once

#include "../../../public/include/components/Component.h"

typedef enum AMF_CAPTURE_DEVICE_TYPE_ENUM
{
    AMF_CAPTURE_DEVICE_UNKNOWN              = 0,
    AMF_CAPTURE_DEVICE_MEDIAFOUNDATION      = 1,
    AMF_CAPTURE_DEVICE_WASAPI               = 2,
    AMF_CAPTURE_DEVICE_SDI                  = 3,
    AMF_CAPTURE_DEVICE_SCREEN_DUPLICATION   = 4,
} AMF_CAPTURE_DEVICE_TYPE_ENUM;

// device properties
#define AMF_CAPTURE_DEVICE_TYPE                     L"DeviceType"               // amf_int64( AMF_CAPTURE_DEVICE_TYPE_ENUM )
#define AMF_CAPTURE_DEVICE_NAME                     L"DeviceName"               // wchar_t* : name of the device



#if defined(__cplusplus)
namespace amf
{
#endif

    //----------------------------------------------------------------------------------------------
    // AMFCaptureDevice interface
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFCaptureDevice : public AMFComponentEx
    {
    public:
        AMF_DECLARE_IID (0x5bfd1b17, 0x9f2a, 0x43c4, 0x9c, 0xdd, 0x2c, 0x3, 0x88, 0x43, 0xb5, 0xf3)

        virtual AMF_RESULT          AMF_STD_CALL Start() = 0;
        virtual AMF_RESULT          AMF_STD_CALL Stop() = 0;

        // TODO add callback interface for disconnected / lost / changed device notification
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFCaptureDevice> AMFCaptureDevicePtr;
    //----------------------------------------------------------------------------------------------
#else // #if defined(__cplusplus)
        AMF_DECLARE_IID(AMFCaptureDevice, 0x5bfd1b17, 0x9f2a, 0x43c4, 0x9c, 0xdd, 0x2c, 0x3, 0x88, 0x43, 0xb5, 0xf3)

    typedef struct AMFCaptureDeviceVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFCaptureDevice* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFCaptureDevice* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFCaptureDevice* pThis, const struct AMFGuid *interfaceID, void** ppInterface);

        // AMFPropertyStorage interface
        AMF_RESULT          (AMF_STD_CALL *SetProperty)(AMFCaptureDevice* pThis, const wchar_t* name, AMFVariantStruct value);
        AMF_RESULT          (AMF_STD_CALL *GetProperty)(AMFCaptureDevice* pThis, const wchar_t* name, AMFVariantStruct* pValue);
        amf_bool            (AMF_STD_CALL *HasProperty)(AMFCaptureDevice* pThis, const wchar_t* name);
        amf_size            (AMF_STD_CALL *GetPropertyCount)(AMFCaptureDevice* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyAt)(AMFCaptureDevice* pThis, amf_size index, wchar_t* name, amf_size nameSize, AMFVariantStruct* pValue);
        AMF_RESULT          (AMF_STD_CALL *Clear)(AMFCaptureDevice* pThis);
        AMF_RESULT          (AMF_STD_CALL *AddTo)(AMFCaptureDevice* pThis, AMFPropertyStorage* pDest, amf_bool overwrite, amf_bool deep);
        AMF_RESULT          (AMF_STD_CALL *CopyTo)(AMFCaptureDevice* pThis, AMFPropertyStorage* pDest, amf_bool deep);
        void                (AMF_STD_CALL *AddObserver)(AMFCaptureDevice* pThis, AMFPropertyStorageObserver* pObserver);
        void                (AMF_STD_CALL *RemoveObserver)(AMFCaptureDevice* pThis, AMFPropertyStorageObserver* pObserver);

        // AMFPropertyStorageEx interface

        amf_size            (AMF_STD_CALL *GetPropertiesInfoCount)(AMFCaptureDevice* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyInfoAt)(AMFCaptureDevice* pThis, amf_size index, const AMFPropertyInfo** ppInfo);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyInfo)(AMFCaptureDevice* pThis, const wchar_t* name, const AMFPropertyInfo** ppInfo);
        AMF_RESULT          (AMF_STD_CALL *ValidateProperty)(AMFCaptureDevice* pThis, const wchar_t* name, AMFVariantStruct value, AMFVariantStruct* pOutValidated);

        // AMFComponent interface

        AMF_RESULT          (AMF_STD_CALL *Init)(AMFCaptureDevice* pThis, AMF_SURFACE_FORMAT format,amf_int32 width,amf_int32 height);
        AMF_RESULT          (AMF_STD_CALL *ReInit)(AMFCaptureDevice* pThis, amf_int32 width,amf_int32 height);
        AMF_RESULT          (AMF_STD_CALL *Terminate)(AMFCaptureDevice* pThis);
        AMF_RESULT          (AMF_STD_CALL *Drain)(AMFCaptureDevice* pThis);
        AMF_RESULT          (AMF_STD_CALL *Flush)(AMFCaptureDevice* pThis);

        AMF_RESULT          (AMF_STD_CALL *SubmitInput)(AMFCaptureDevice* pThis, AMFData* pData);
        AMF_RESULT          (AMF_STD_CALL *QueryOutput)(AMFCaptureDevice* pThis, AMFData** ppData);
        AMFContext*         (AMF_STD_CALL *GetContext)(AMFCaptureDevice* pThis);
        AMF_RESULT          (AMF_STD_CALL *SetOutputDataAllocatorCB)(AMFCaptureDevice* pThis, AMFDataAllocatorCB* callback);

        AMF_RESULT          (AMF_STD_CALL *GetCaps)(AMFCaptureDevice* pThis, AMFCaps** ppCaps);
        AMF_RESULT          (AMF_STD_CALL *Optimize)(AMFCaptureDevice* pThis, AMFComponentOptimizationCallback* pCallback);

        // AMFComponentEx interface

        amf_int32           (AMF_STD_CALL *GetInputCount)(AMFCaptureDevice* pThis);
        amf_int32           (AMF_STD_CALL *GetOutputCount)(AMFCaptureDevice* pThis);

        AMF_RESULT          (AMF_STD_CALL *GetInput)(AMFCaptureDevice* pThis, amf_int32 index, AMFInput** ppInput);
        AMF_RESULT          (AMF_STD_CALL *GetOutput)(AMFCaptureDevice* pThis, amf_int32 index, AMFOutput** ppOutput);

        // AMFCaptureDevice interface

        AMF_RESULT          (AMF_STD_CALL *Start)(AMFCaptureDevice* pThis);
        AMF_RESULT          (AMF_STD_CALL *Stop)(AMFCaptureDevice* pThis);

    } AMFCaptureVtbl;

    struct AMFCapture
    {
        const AMFCaptureVtbl *pVtbl;
    };
#endif // #if defined(__cplusplus)

    //----------------------------------------------------------------------------------------------
    // AMFCaptureManager interface
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFCaptureManager : public AMFInterface
    {
    public:
        AMF_DECLARE_IID ( 0xf64d2f0d, 0xad16, 0x4ce7, 0x80, 0x5f, 0xa1, 0xe7, 0x3b, 0x0, 0xf4, 0x28)

        virtual AMF_RESULT          AMF_STD_CALL Update() = 0;
        virtual amf_int32           AMF_STD_CALL GetDeviceCount() = 0;
        virtual AMF_RESULT          AMF_STD_CALL GetDevice(amf_int32 index,AMFCaptureDevice **pDevice) = 0;

    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFCaptureManager> AMFCaptureManagerPtr;
    //----------------------------------------------------------------------------------------------
#else // #if defined(__cplusplus)
        AMF_DECLARE_IID(AMFCaptureManager, 0xf64d2f0d, 0xad16, 0x4ce7, 0x80, 0x5f, 0xa1, 0xe7, 0x3b, 0x0, 0xf4, 0x28)

    typedef struct AMFCaptureManagerVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFCaptureManager* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFCaptureManager* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFCaptureManager* pThis, const struct AMFGuid *interfaceID, void** ppInterface);


        // AMFCaptureManager interface
        AMF_RESULT          (AMF_STD_CALL *Update)((AMFCaptureManager* pThis);
        amf_int32           (AMF_STD_CALL *GetDeviceCount)(AMFCaptureManager* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetDevice)(AMFCaptureManager* pThis, amf_int32 index,AMFCaptureDevice **pDevice);

    } AMFCaptureManagerVtbl;

    struct AMFCaptureManager
    {
        const AMFCaptureManagerVtbl *pVtbl;
    };
#endif // #if defined(__cplusplus)
#if defined(__cplusplus)
} // namespace
#endif

extern "C"
{
    AMF_RESULT AMF_CDECL_CALL AMFCreateCaptureManager(amf::AMFContext* pContext, amf::AMFCaptureManager** ppManager);
}

#endif // __Capture_h__