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

#ifndef AMF_ComputeFactory_h
#define AMF_ComputeFactory_h
#pragma once

#include "Compute.h"

#if defined(__cplusplus)
namespace amf
{
#endif
// compute device audio capabilities accessed via GetProperties() from AMFComputeDevice
#define AMF_DEVICE_NAME                         L"DeviceName"                       // char*, string, device name
#define AMF_DRIVER_VERSION_NAME                 L"DriverVersion"                       // char*, string, driver version
#define AMF_AUDIO_CONVOLUTION_MAX_STREAMS       L"ConvolutionMaxStreams"            // amf_int64, maximum number of audio streams supported in realtime
#define AMF_AUDIO_CONVOLUTION_LENGTH            L"ConvolutionLength"                // amf_int64, length of convolution in samples
#define AMF_AUDIO_CONVOLUTION_BUFFER_SIZE       L"ConvolutionBufferSize"            // amf_int64, buffer size in samples
#define AMF_AUDIO_CONVOLUTION_SAMPLE_RATE       L"ConvolutionSampleRate"            // amf_int64, sample rate

#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFComputeDevice  : public AMFPropertyStorage
    {
    public:
        AMF_DECLARE_IID(0xb79d7cf6, 0x2c5c, 0x4deb, 0xb8, 0x96, 0xa2, 0x9e, 0xbe, 0xa6, 0xe3, 0x97);

        virtual void*               AMF_STD_CALL GetNativePlatform() = 0;
        virtual void*               AMF_STD_CALL GetNativeDeviceID() = 0;
        virtual void*               AMF_STD_CALL GetNativeContext() = 0;

        virtual AMF_RESULT          AMF_STD_CALL CreateCompute(void *reserved, AMFCompute **ppCompute) = 0;
        virtual AMF_RESULT          AMF_STD_CALL CreateComputeEx(void* pCommandQueue, AMFCompute **ppCompute) = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFComputeDevice> AMFComputeDevicePtr;
#else // #if defined(__cplusplus)
    AMF_DECLARE_IID(AMFComputeDevice, 0xb79d7cf6, 0x2c5c, 0x4deb, 0xb8, 0x96, 0xa2, 0x9e, 0xbe, 0xa6, 0xe3, 0x97);
    typedef struct AMFComputeDevice AMFComputeDevice;

    typedef struct AMFComputeDeviceVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFComputeDevice* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFComputeDevice* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFComputeDevice* pThis, const struct AMFGuid *interfaceID, void** ppInterface);

        // AMFPropertyStorage interface
        AMF_RESULT          (AMF_STD_CALL *SetProperty)(AMFComputeDevice* pThis, const wchar_t* name, AMFVariantStruct value);
        AMF_RESULT          (AMF_STD_CALL *GetProperty)(AMFComputeDevice* pThis, const wchar_t* name, AMFVariantStruct* pValue);
        amf_bool            (AMF_STD_CALL *HasProperty)(AMFComputeDevice* pThis, const wchar_t* name);
        amf_size            (AMF_STD_CALL *GetPropertyCount)(AMFComputeDevice* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyAt)(AMFComputeDevice* pThis, amf_size index, wchar_t* name, amf_size nameSize, AMFVariantStruct* pValue);
        AMF_RESULT          (AMF_STD_CALL *Clear)(AMFComputeDevice* pThis);
        AMF_RESULT          (AMF_STD_CALL *AddTo)(AMFComputeDevice* pThis, AMFPropertyStorage* pDest, amf_bool overwrite, amf_bool deep);
        AMF_RESULT          (AMF_STD_CALL *CopyTo)(AMFComputeDevice* pThis, AMFPropertyStorage* pDest, amf_bool deep);
        void                (AMF_STD_CALL *AddObserver)(AMFComputeDevice* pThis, AMFPropertyStorageObserver* pObserver);
        void                (AMF_STD_CALL *RemoveObserver)(AMFComputeDevice* pThis, AMFPropertyStorageObserver* pObserver);

        // AMFComputeDevice interface
       void*               (AMF_STD_CALL *GetNativePlatform)(AMFComputeDevice* pThis);
       void*               (AMF_STD_CALL *GetNativeDeviceID)(AMFComputeDevice* pThis);
       void*               (AMF_STD_CALL *GetNativeContext)(AMFComputeDevice* pThis);

       AMF_RESULT          (AMF_STD_CALL *CreateCompute)(AMFComputeDevice* pThis, void *reserved, AMFCompute **ppCompute);
       AMF_RESULT          (AMF_STD_CALL *CreateComputeEx)(AMFComputeDevice* pThis, void* pCommandQueue, AMFCompute **ppCompute);

    } AMFComputeDeviceVtbl;

    struct AMFComputeDevice
    {
        const AMFComputeDeviceVtbl *pVtbl;
    };
#endif // #if defined(__cplusplus)
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFComputeFactory : public AMFInterface
    {
    public:
        AMF_DECLARE_IID(0xe3c24bd7, 0x2d83, 0x416c, 0x8c, 0x4e, 0xfd, 0x13, 0xca, 0x86, 0xf4, 0xd0);

        virtual amf_int32           AMF_STD_CALL GetDeviceCount() = 0;
        virtual AMF_RESULT          AMF_STD_CALL GetDeviceAt(amf_int32 index, AMFComputeDevice **ppDevice) = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFComputeFactory> AMFComputeFactoryPtr;
#else // #if defined(__cplusplus)
    AMF_DECLARE_IID(AMFComputeFactory, 0xe3c24bd7, 0x2d83, 0x416c, 0x8c, 0x4e, 0xfd, 0x13, 0xca, 0x86, 0xf4, 0xd0);
    typedef struct AMFComputeFactory AMFComputeFactory;

    typedef struct AMFComputeFactoryVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFComputeFactory* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFComputeFactory* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFComputeFactory* pThis, const struct AMFGuid *interfaceID, void** ppInterface);

        // AMFComputeFactory interface
        amf_int32           (AMF_STD_CALL *GetDeviceCount)(AMFComputeFactory* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetDeviceAt)(AMFComputeFactory* pThis, amf_int32 index, AMFComputeDevice **ppDevice);
    } AMFComputeFactoryVtbl;

    struct AMFComputeFactory
    {
        const AMFComputeFactoryVtbl *pVtbl;
    };
#endif // #if defined(__cplusplus)

    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
}; // namespace amf
#endif

#endif // AMF_ComputeFactory_h
