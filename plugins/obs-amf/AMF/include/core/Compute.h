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

/**
 ***************************************************************************************************
 * @file  Compute.h
 * @brief AMFCompute interface declaration
 ***************************************************************************************************
 */
#ifndef AMF_Compute_h
#define AMF_Compute_h
#pragma once

#include "Buffer.h"
#include "Surface.h"

#if defined(__cplusplus)
namespace amf
{
#endif
    typedef amf_uint64 AMF_KERNEL_ID;

    //----------------------------------------------------------------------------------------------
    // enumerations for plane conversion
    //----------------------------------------------------------------------------------------------
    typedef enum AMF_CHANNEL_ORDER
    {
        AMF_CHANNEL_ORDER_INVALID       = 0,
        AMF_CHANNEL_ORDER_R             = 1,
        AMF_CHANNEL_ORDER_RG            = 2,
        AMF_CHANNEL_ORDER_BGRA          = 3,
        AMF_CHANNEL_ORDER_RGBA          = 4,
        AMF_CHANNEL_ORDER_ARGB          = 5,
        AMF_CHANNEL_ORDER_YUY2          = 6,
    } AMF_CHANNEL_ORDER;
    //----------------------------------------------------------------------------------------------
    typedef enum AMF_CHANNEL_TYPE
    {
        AMF_CHANNEL_INVALID             = 0,
        AMF_CHANNEL_UNSIGNED_INT8       = 1,
        AMF_CHANNEL_UNSIGNED_INT32      = 2,
        AMF_CHANNEL_UNORM_INT8          = 3,
        AMF_CHANNEL_UNORM_INT16         = 4,
        AMF_CHANNEL_SNORM_INT16         = 5,
        AMF_CHANNEL_FLOAT               = 6,
        AMF_CHANNEL_FLOAT16             = 7,
        AMF_CHANNEL_UNSIGNED_INT16      = 8,
        AMF_CHANNEL_UNORM_INT_101010    = 9,
} AMF_CHANNEL_TYPE;
    //----------------------------------------------------------------------------------------------
#define AMF_STRUCTURED_BUFFER_FORMAT        L"StructuredBufferFormat"                                                             // amf_int64(AMF_CHANNEL_TYPE), default - AMF_CHANNEL_UNSIGNED_INT32; to be set on AMFBuffer objects
#if defined(_WIN32)
    AMF_WEAK GUID  AMFStructuredBufferFormatGUID = { 0x90c5d674, 0xe90, 0x4181, 0xbd, 0xef, 0x26, 0x13, 0xc1, 0xdf, 0xa3, 0xbd }; // UINT(DXGI_FORMAT), default - DXGI_FORMAT_R32_UINT; to be set on ID3D11Buffer or ID3D11Texture2D objects when used natively
#endif
    //----------------------------------------------------------------------------------------------
    // enumeration argument type
    //----------------------------------------------------------------------------------------------
    typedef enum AMF_ARGUMENT_ACCESS_TYPE
    {
        AMF_ARGUMENT_ACCESS_READ        = 0,
        AMF_ARGUMENT_ACCESS_WRITE       = 1,
        AMF_ARGUMENT_ACCESS_READWRITE   = 2,
        AMF_ARGUMENT_ACCESS_READWRITE_MASK  = 0xFFFF,
        //Sampler parameters
        AMF_ARGUMENT_SAMPLER_LINEAR        = 0x10000000, 
        AMF_ARGUMENT_SAMPLER_NORM_COORD    = 0x20000000, 
        AMF_ARGUMENT_SAMPLER_POINT         = 0x40000000,
        AMF_ARGUMENT_SAMPLER_MASK          = 0xFFFF0000,
    } AMF_ARGUMENT_ACCESS_TYPE;
    //----------------------------------------------------------------------------------------------
    // AMFComputeKernel interface
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFComputeKernel : public AMFInterface
    {
    public:
        AMF_DECLARE_IID(0x94815701, 0x6c84, 0x4ba6, 0xa9, 0xfe, 0xe9, 0xad, 0x40, 0xf8, 0x8, 0x8)

        virtual void*               AMF_STD_CALL GetNative() = 0;
        virtual const wchar_t*      AMF_STD_CALL GetIDName() = 0;

        virtual AMF_RESULT          AMF_STD_CALL SetArgPlaneNative(amf_size index, void* pPlane, AMF_ARGUMENT_ACCESS_TYPE eAccess) = 0;
        virtual AMF_RESULT          AMF_STD_CALL SetArgBufferNative(amf_size index, void* pBuffer, AMF_ARGUMENT_ACCESS_TYPE eAccess) = 0;

        virtual AMF_RESULT          AMF_STD_CALL SetArgPlane(amf_size index, AMFPlane* pPlane, AMF_ARGUMENT_ACCESS_TYPE eAccess) = 0;
        virtual AMF_RESULT          AMF_STD_CALL SetArgBuffer(amf_size index, AMFBuffer* pBuffer, AMF_ARGUMENT_ACCESS_TYPE eAccess) = 0;

        virtual AMF_RESULT          AMF_STD_CALL SetArgInt32(amf_size index, amf_int32 data) = 0;
        virtual AMF_RESULT          AMF_STD_CALL SetArgInt64(amf_size index, amf_int64 data) = 0;
        virtual AMF_RESULT          AMF_STD_CALL SetArgFloat(amf_size index, amf_float data) = 0;
        virtual AMF_RESULT          AMF_STD_CALL SetArgBlob(amf_size index, amf_size dataSize, const void* pData) = 0;

        virtual AMF_RESULT          AMF_STD_CALL GetCompileWorkgroupSize(amf_size workgroupSize[3]) = 0;

        virtual AMF_RESULT          AMF_STD_CALL Enqueue(amf_size dimension, amf_size globalOffset[3], amf_size globalSize[3], amf_size localSize[3]) = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFComputeKernel> AMFComputeKernelPtr;
#else // #if defined(__cplusplus)
    AMF_DECLARE_IID(AMFComputeKernel, 0x94815701, 0x6c84, 0x4ba6, 0xa9, 0xfe, 0xe9, 0xad, 0x40, 0xf8, 0x8, 0x8)
    typedef struct AMFComputeKernel AMFComputeKernel;

    typedef struct AMFComputeKernelVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFComputeKernel* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFComputeKernel* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFComputeKernel* pThis, const struct AMFGuid *interfaceID, void** ppInterface);

        // AMFComputeKernel interface 

    } AMFComputeKernelVtbl;

    struct AMFComputeKernel
    {
        const AMFComputeKernelVtbl *pVtbl;
    };

#endif //#if defined(__cplusplus)

    //----------------------------------------------------------------------------------------------
    // AMFComputeSyncPoint interface
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFComputeSyncPoint : public AMFInterface
    {
    public:
        AMF_DECLARE_IID(0x66f33fe6, 0xaae, 0x4e65, 0xba, 0x3, 0xea, 0x8b, 0xa3, 0x60, 0x11, 0x2)

        virtual amf_bool            AMF_STD_CALL IsCompleted() = 0;
        virtual void                AMF_STD_CALL Wait() = 0;
    };
    typedef AMFInterfacePtr_T<AMFComputeSyncPoint> AMFComputeSyncPointPtr;
#else // #if defined(__cplusplus)
    AMF_DECLARE_IID(AMFComputeSyncPoint, 0x66f33fe6, 0xaae, 0x4e65, 0xba, 0x3, 0xea, 0x8b, 0xa3, 0x60, 0x11, 0x2)
    typedef struct AMFComputeSyncPoint AMFComputeSyncPoint;

    typedef struct AMFComputeSyncPointVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFComputeSyncPoint* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFComputeSyncPoint* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFComputeSyncPoint* pThis, const struct AMFGuid *interfaceID, void** ppInterface);

        // AMFComputeSyncPoint interface 
        amf_bool            (AMF_STD_CALL *IsCompleted)(AMFComputeSyncPoint* pThis);
        void                (AMF_STD_CALL *Wait)(AMFComputeSyncPoint* pThis);

    } AMFComputeSyncPointVtbl;

    struct AMFComputeSyncPoint
    {
        const AMFComputeSyncPointVtbl *pVtbl;
    };

#endif // #if defined(__cplusplus)

    //----------------------------------------------------------------------------------------------
    // AMFCompute interface
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFCompute : public AMFInterface
    {
    public:
        AMF_DECLARE_IID(0x3846233a, 0x3f43, 0x443f, 0x8a, 0x45, 0x75, 0x22, 0x11, 0xa9, 0xfb, 0xd5)

        virtual AMF_MEMORY_TYPE     AMF_STD_CALL GetMemoryType() = 0;

        virtual void*               AMF_STD_CALL GetNativeContext() = 0;
        virtual void*               AMF_STD_CALL GetNativeDeviceID() = 0;
        virtual void*               AMF_STD_CALL GetNativeCommandQueue() = 0;

        virtual AMF_RESULT          AMF_STD_CALL GetKernel(AMF_KERNEL_ID kernelID, AMFComputeKernel** kernel) = 0;

        virtual AMF_RESULT          AMF_STD_CALL PutSyncPoint(AMFComputeSyncPoint** ppSyncPoint) = 0;
        virtual AMF_RESULT          AMF_STD_CALL FinishQueue() = 0;
        virtual AMF_RESULT          AMF_STD_CALL FlushQueue() = 0;

        virtual AMF_RESULT          AMF_STD_CALL FillPlane(AMFPlane *pPlane, const amf_size origin[3], const amf_size region[3], const void* pColor) = 0;
        virtual AMF_RESULT          AMF_STD_CALL FillBuffer(AMFBuffer* pBuffer, amf_size dstOffset, amf_size dstSize, const void* pSourcePattern, amf_size patternSize) = 0;
        virtual AMF_RESULT          AMF_STD_CALL ConvertPlaneToBuffer(AMFPlane *pSrcPlane, AMFBuffer** ppDstBuffer) = 0;

        virtual AMF_RESULT          AMF_STD_CALL CopyBuffer(AMFBuffer* pSrcBuffer, amf_size srcOffset, amf_size size, AMFBuffer* pDstBuffer, amf_size dstOffset) = 0;
        virtual AMF_RESULT          AMF_STD_CALL CopyPlane(AMFPlane *pSrcPlane, const amf_size srcOrigin[3], const amf_size region[3], AMFPlane *pDstPlane, const amf_size dstOrigin[3]) = 0;

        virtual AMF_RESULT          AMF_STD_CALL CopyBufferToHost(AMFBuffer* pSrcBuffer, amf_size srcOffset, amf_size size, void* pDest, amf_bool blocking) = 0;
        virtual AMF_RESULT          AMF_STD_CALL CopyBufferFromHost(const void* pSource, amf_size size, AMFBuffer* pDstBuffer, amf_size dstOffsetInBytes, amf_bool blocking) = 0;

        virtual AMF_RESULT          AMF_STD_CALL CopyPlaneToHost(AMFPlane *pSrcPlane, const amf_size origin[3], const amf_size region[3], void* pDest, amf_size dstPitch, amf_bool blocking) = 0;
        virtual AMF_RESULT          AMF_STD_CALL CopyPlaneFromHost(void* pSource, const amf_size origin[3], const amf_size region[3], amf_size srcPitch, AMFPlane *pDstPlane, amf_bool blocking) = 0;

        virtual AMF_RESULT          AMF_STD_CALL ConvertPlaneToPlane(AMFPlane* pSrcPlane, AMFPlane** ppDstPlane, AMF_CHANNEL_ORDER order, AMF_CHANNEL_TYPE type) = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFCompute> AMFComputePtr;
#else // #if defined(__cplusplus)
        AMF_DECLARE_IID(AMFCompute, 0x3846233a, 0x3f43, 0x443f, 0x8a, 0x45, 0x75, 0x22, 0x11, 0xa9, 0xfb, 0xd5)
    typedef struct AMFCompute AMFCompute;

    typedef struct AMFComputeVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFCompute* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFCompute* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFCompute* pThis, const struct AMFGuid *interfaceID, void** ppInterface);

        // AMFCompute interface 
        AMF_MEMORY_TYPE     (AMF_STD_CALL *GetMemoryType)(AMFCompute* pThis);
        void*               (AMF_STD_CALL *GetNativeContext)(AMFCompute* pThis);
        void*               (AMF_STD_CALL *GetNativeDeviceID)(AMFCompute* pThis);
        void*               (AMF_STD_CALL *GetNativeCommandQueue)(AMFCompute* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetKernel)(AMFCompute* pThis, AMF_KERNEL_ID kernelID, AMFComputeKernel** kernel);
        AMF_RESULT          (AMF_STD_CALL *PutSyncPoint)(AMFCompute* pThis, AMFComputeSyncPoint** ppSyncPoint);
        AMF_RESULT          (AMF_STD_CALL *FinishQueue)(AMFCompute* pThis);
        AMF_RESULT          (AMF_STD_CALL *FlushQueue)(AMFCompute* pThis);
        AMF_RESULT          (AMF_STD_CALL *FillPlane)(AMFCompute* pThis, AMFPlane *pPlane, const amf_size origin[3], const amf_size region[3], const void* pColor);
        AMF_RESULT          (AMF_STD_CALL *FillBuffer)(AMFCompute* pThis, AMFBuffer* pBuffer, amf_size dstOffset, amf_size dstSize, const void* pSourcePattern, amf_size patternSize);
        AMF_RESULT          (AMF_STD_CALL *ConvertPlaneToBuffer)(AMFCompute* pThis, AMFPlane *pSrcPlane, AMFBuffer** ppDstBuffer);
        AMF_RESULT          (AMF_STD_CALL *CopyBuffer)(AMFCompute* pThis, AMFBuffer* pSrcBuffer, amf_size srcOffset, amf_size size, AMFBuffer* pDstBuffer, amf_size dstOffset);
        AMF_RESULT          (AMF_STD_CALL *CopyPlane)(AMFCompute* pThis, AMFPlane *pSrcPlane, const amf_size srcOrigin[3], const amf_size region[3], AMFPlane *pDstPlane, const amf_size dstOrigin[3]);
        AMF_RESULT          (AMF_STD_CALL *CopyBufferToHost)(AMFCompute* pThis, AMFBuffer* pSrcBuffer, amf_size srcOffset, amf_size size, void* pDest, amf_bool blocking);
        AMF_RESULT          (AMF_STD_CALL *CopyBufferFromHost)(AMFCompute* pThis, const void* pSource, amf_size size, AMFBuffer* pDstBuffer, amf_size dstOffsetInBytes, amf_bool blocking);
        AMF_RESULT          (AMF_STD_CALL *CopyPlaneToHost)(AMFCompute* pThis, AMFPlane *pSrcPlane, const amf_size origin[3], const amf_size region[3], void* pDest, amf_size dstPitch, amf_bool blocking);
        AMF_RESULT          (AMF_STD_CALL *CopyPlaneFromHost)(AMFCompute* pThis, void* pSource, const amf_size origin[3], const amf_size region[3], amf_size srcPitch, AMFPlane *pDstPlane, amf_bool blocking);
        AMF_RESULT          (AMF_STD_CALL *ConvertPlaneToPlane)(AMFCompute* pThis, AMFPlane* pSrcPlane, AMFPlane** ppDstPlane, AMF_CHANNEL_ORDER order, AMF_CHANNEL_TYPE type);
    } AMFComputeVtbl;

    struct AMFCompute
    {
        const AMFComputeVtbl *pVtbl;
    };

#endif // #if defined(__cplusplus)

    //----------------------------------------------------------------------------------------------
    // AMFPrograms interface - singleton
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFPrograms
    {
    public:
        virtual AMF_RESULT          AMF_STD_CALL RegisterKernelSourceFile(AMF_KERNEL_ID* pKernelID, const wchar_t* kernelid_name, const char* kernelName, const wchar_t* filepath, const char* options) = 0;
        virtual AMF_RESULT          AMF_STD_CALL RegisterKernelSource(AMF_KERNEL_ID* pKernelID, const wchar_t* kernelid_name, const char* kernelName, amf_size dataSize, const amf_uint8* data, const char* options) = 0;
        virtual AMF_RESULT          AMF_STD_CALL RegisterKernelBinary(AMF_KERNEL_ID* pKernelID, const wchar_t* kernelid_name, const char* kernelName, amf_size dataSize, const amf_uint8* data, const char* options) = 0;
        virtual AMF_RESULT          AMF_STD_CALL RegisterKernelSource1(AMF_MEMORY_TYPE eMemoryType, AMF_KERNEL_ID* pKernelID, const wchar_t* kernelid_name, const char* kernelName, amf_size dataSize, const amf_uint8* data, const char* options) = 0;
        virtual AMF_RESULT          AMF_STD_CALL RegisterKernelBinary1(AMF_MEMORY_TYPE eMemoryType, AMF_KERNEL_ID* pKernelID, const wchar_t* kernelid_name, const char* kernelName, amf_size dataSize, const amf_uint8* data, const char* options) = 0;
    };
#else // #if defined(__cplusplus)
    typedef struct AMFPrograms AMFPrograms;
    typedef struct AMFProgramsVtbl
    {
        AMF_RESULT          (AMF_STD_CALL *RegisterKernelSourceFile)(AMFPrograms* pThis, AMF_KERNEL_ID* pKernelID, const wchar_t* kernelid_name, const char* kernelName, const wchar_t* filepath, const char* options);
        AMF_RESULT          (AMF_STD_CALL *RegisterKernelSource)(AMFPrograms* pThis, AMF_KERNEL_ID* pKernelID, const wchar_t* kernelid_name, const char* kernelName, amf_size dataSize, const amf_uint8* data, const char* options);
        AMF_RESULT          (AMF_STD_CALL *RegisterKernelBinary)(AMFPrograms* pThis, AMF_KERNEL_ID* pKernelID, const wchar_t* kernelid_name, const char* kernelName, amf_size dataSize, const amf_uint8* data, const char* options);
        AMF_RESULT          (AMF_STD_CALL *RegisterKernelSource1)(AMFPrograms* pThis, AMF_MEMORY_TYPE eMemoryType, AMF_KERNEL_ID* pKernelID, const wchar_t* kernelid_name, const char* kernelName, amf_size dataSize, const amf_uint8* data, const char* options);
        AMF_RESULT          (AMF_STD_CALL *RegisterKernelBinary1)(AMFPrograms* pThis, AMF_MEMORY_TYPE eMemoryType, AMF_KERNEL_ID* pKernelID, const wchar_t* kernelid_name, const char* kernelName, amf_size dataSize, const amf_uint8* data, const char* options);
    } AMFProgramsVtbl;

    struct AMFPrograms
    {
        const AMFProgramsVtbl *pVtbl;
    };
#endif // #if defined(__cplusplus)


#if defined(__cplusplus)
} // namespace amf
#endif

#endif // AMF_Compute_h
