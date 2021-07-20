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

#ifndef AMF_Context_h
#define AMF_Context_h
#pragma once

#include "Buffer.h"
#include "AudioBuffer.h"
#include "Surface.h"
#include "Compute.h"
#include "ComputeFactory.h"

#if defined(__cplusplus)
namespace amf
{
#endif
    //----------------------------------------------------------------------------------------------
    // AMFContext interface
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFContext : public AMFPropertyStorage
    {
    public:
        AMF_DECLARE_IID(0xa76a13f0, 0xd80e, 0x4fcc, 0xb5, 0x8, 0x65, 0xd0, 0xb5, 0x2e, 0xd9, 0xee)
        
        // Cleanup
        virtual AMF_RESULT          AMF_STD_CALL Terminate() = 0;

        // DX9
        virtual AMF_RESULT          AMF_STD_CALL InitDX9(void* pDX9Device) = 0;
        virtual void*               AMF_STD_CALL GetDX9Device(AMF_DX_VERSION dxVersionRequired = AMF_DX9) = 0;
        virtual AMF_RESULT          AMF_STD_CALL LockDX9() = 0;
        virtual AMF_RESULT          AMF_STD_CALL UnlockDX9() = 0;
        class AMFDX9Locker;

        // DX11
        virtual AMF_RESULT          AMF_STD_CALL InitDX11(void* pDX11Device, AMF_DX_VERSION dxVersionRequired = AMF_DX11_0) = 0;
        virtual void*               AMF_STD_CALL GetDX11Device(AMF_DX_VERSION dxVersionRequired = AMF_DX11_0) = 0;
        virtual AMF_RESULT          AMF_STD_CALL LockDX11() = 0;
        virtual AMF_RESULT          AMF_STD_CALL UnlockDX11() = 0;
        class AMFDX11Locker;

        // OpenCL
        virtual AMF_RESULT          AMF_STD_CALL InitOpenCL(void* pCommandQueue = NULL) = 0;
        virtual void*               AMF_STD_CALL GetOpenCLContext() = 0;
        virtual void*               AMF_STD_CALL GetOpenCLCommandQueue() = 0;
        virtual void*               AMF_STD_CALL GetOpenCLDeviceID() = 0;
        virtual AMF_RESULT          AMF_STD_CALL GetOpenCLComputeFactory(AMFComputeFactory **ppFactory) = 0; // advanced compute - multiple queries
        virtual AMF_RESULT          AMF_STD_CALL InitOpenCLEx(AMFComputeDevice *pDevice) = 0;
        virtual AMF_RESULT          AMF_STD_CALL LockOpenCL() = 0;
        virtual AMF_RESULT          AMF_STD_CALL UnlockOpenCL() = 0;
        class AMFOpenCLLocker;

        // OpenGL
        virtual AMF_RESULT          AMF_STD_CALL InitOpenGL(amf_handle hOpenGLContext, amf_handle hWindow, amf_handle hDC) = 0;
        virtual amf_handle          AMF_STD_CALL GetOpenGLContext() = 0;
        virtual amf_handle          AMF_STD_CALL GetOpenGLDrawable() = 0;
        virtual AMF_RESULT          AMF_STD_CALL LockOpenGL() = 0;
        virtual AMF_RESULT          AMF_STD_CALL UnlockOpenGL() = 0;
        class AMFOpenGLLocker;

        // XV - Linux
        virtual AMF_RESULT          AMF_STD_CALL InitXV(void* pXVDevice) = 0;
        virtual void*               AMF_STD_CALL GetXVDevice() = 0;
        virtual AMF_RESULT          AMF_STD_CALL LockXV() = 0;
        virtual AMF_RESULT          AMF_STD_CALL UnlockXV() = 0;
        class AMFXVLocker;

        // Gralloc - Android
        virtual AMF_RESULT          AMF_STD_CALL InitGralloc(void* pGrallocDevice) = 0;
        virtual void*               AMF_STD_CALL GetGrallocDevice() = 0;
        virtual AMF_RESULT          AMF_STD_CALL LockGralloc() = 0;
        virtual AMF_RESULT          AMF_STD_CALL UnlockGralloc() = 0;
        class AMFGrallocLocker;

        // Allocation
        virtual AMF_RESULT          AMF_STD_CALL AllocBuffer(AMF_MEMORY_TYPE type, amf_size size, AMFBuffer** ppBuffer) = 0;
        virtual AMF_RESULT          AMF_STD_CALL AllocSurface(AMF_MEMORY_TYPE type, AMF_SURFACE_FORMAT format, amf_int32 width, amf_int32 height, AMFSurface** ppSurface) = 0;
        virtual AMF_RESULT          AMF_STD_CALL AllocAudioBuffer(AMF_MEMORY_TYPE type, AMF_AUDIO_FORMAT format, amf_int32 samples, amf_int32 sampleRate, amf_int32 channels, 
                                                    AMFAudioBuffer** ppAudioBuffer) = 0;

        // Wrap existing objects
        virtual AMF_RESULT          AMF_STD_CALL CreateBufferFromHostNative(void* pHostBuffer, amf_size size, AMFBuffer** ppBuffer, AMFBufferObserver* pObserver) = 0;
        virtual AMF_RESULT          AMF_STD_CALL CreateSurfaceFromHostNative(AMF_SURFACE_FORMAT format, amf_int32 width, amf_int32 height, amf_int32 hPitch, amf_int32 vPitch, void* pData, 
                                                     AMFSurface** ppSurface, AMFSurfaceObserver* pObserver) = 0;
        virtual AMF_RESULT          AMF_STD_CALL CreateSurfaceFromDX9Native(void* pDX9Surface, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver) = 0;
        virtual AMF_RESULT          AMF_STD_CALL CreateSurfaceFromDX11Native(void* pDX11Surface, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver) = 0;
        virtual AMF_RESULT          AMF_STD_CALL CreateSurfaceFromOpenGLNative(AMF_SURFACE_FORMAT format, amf_handle hGLTextureID, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver) = 0;
        virtual AMF_RESULT          AMF_STD_CALL CreateSurfaceFromGrallocNative(amf_handle hGrallocSurface, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver) = 0;
        virtual AMF_RESULT          AMF_STD_CALL CreateSurfaceFromOpenCLNative(AMF_SURFACE_FORMAT format, amf_int32 width, amf_int32 height, void** pClPlanes, 
                                                     AMFSurface** ppSurface, AMFSurfaceObserver* pObserver) = 0;
        virtual AMF_RESULT          AMF_STD_CALL CreateBufferFromOpenCLNative(void* pCLBuffer, amf_size size, AMFBuffer** ppBuffer) = 0;

        // Access to AMFCompute interface - AMF_MEMORY_OPENCL, AMF_MEMORY_COMPUTE_FOR_DX9, AMF_MEMORY_COMPUTE_FOR_DX11 are currently supported
        virtual AMF_RESULT          AMF_STD_CALL GetCompute(AMF_MEMORY_TYPE eMemType, AMFCompute** ppCompute) = 0;
    };

    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFContext> AMFContextPtr;

    //----------------------------------------------------------------------------------------------
    // AMFContext1 interface
    //----------------------------------------------------------------------------------------------

    class AMF_NO_VTABLE AMFContext1 : public AMFContext
    {
    public:
        AMF_DECLARE_IID(0xd9e9f868, 0x6220, 0x44c6, 0xa2, 0x2f, 0x7c, 0xd6, 0xda, 0xc6, 0x86, 0x46)

        virtual AMF_RESULT          AMF_STD_CALL CreateBufferFromDX11Native(void* pHostBuffer, AMFBuffer** ppBuffer, AMFBufferObserver* pObserver) = 0;

        virtual AMF_RESULT          AMF_STD_CALL AllocBufferEx(AMF_MEMORY_TYPE type, amf_size size, AMF_BUFFER_USAGE usage, AMF_MEMORY_CPU_ACCESS access, AMFBuffer** ppBuffer) = 0;
        virtual AMF_RESULT          AMF_STD_CALL AllocSurfaceEx(AMF_MEMORY_TYPE type, AMF_SURFACE_FORMAT format, amf_int32 width, amf_int32 height, AMF_SURFACE_USAGE usage, AMF_MEMORY_CPU_ACCESS access, AMFSurface** ppSurface) = 0;

        // Vulkan - Windows, Linux
        virtual AMF_RESULT          AMF_STD_CALL InitVulkan(void* pVulkanDevice) = 0;
        virtual void*               AMF_STD_CALL GetVulkanDevice() = 0;
        virtual AMF_RESULT          AMF_STD_CALL LockVulkan() = 0;
        virtual AMF_RESULT          AMF_STD_CALL UnlockVulkan() = 0;

        virtual AMF_RESULT          AMF_STD_CALL CreateSurfaceFromVulkanNative(void* pVulkanImage, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver) = 0;
        virtual AMF_RESULT          AMF_STD_CALL CreateBufferFromVulkanNative(void* pVulkanBuffer, AMFBuffer** ppBuffer, AMFBufferObserver* pObserver) = 0;
        virtual AMF_RESULT          AMF_STD_CALL GetVulkanDeviceExtensions(amf_size *pCount, const char **ppExtensions) = 0;

        
        class AMFVulkanLocker;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFContext1> AMFContext1Ptr;

    class AMF_NO_VTABLE AMFContext2 : public AMFContext1
    {
    public:
        AMF_DECLARE_IID(0x726241d3, 0xbd46, 0x4e90, 0x99, 0x68, 0x93, 0xe0, 0x7e, 0xa2, 0x98, 0x4d)

        // DX12
        virtual AMF_RESULT          AMF_STD_CALL InitDX12(void* pDX11Device, AMF_DX_VERSION dxVersionRequired = AMF_DX12) = 0;
        virtual void*               AMF_STD_CALL GetDX12Device(AMF_DX_VERSION dxVersionRequired = AMF_DX12) = 0;
        virtual AMF_RESULT          AMF_STD_CALL LockDX12() = 0;
        virtual AMF_RESULT          AMF_STD_CALL UnlockDX12() = 0;
        virtual AMF_RESULT          AMF_STD_CALL CreateSurfaceFromDX12Native(void* pResourceTexture, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver) = 0;
        virtual AMF_RESULT          AMF_STD_CALL CreateBufferFromDX12Native(void* pResourceBuffer, AMFBuffer** ppBuffer, AMFBufferObserver* pObserver) = 0;

        class AMFDX12Locker;
    };
    typedef AMFInterfacePtr_T<AMFContext2> AMFContext2Ptr;
#else
    typedef struct AMFContext AMFContext;
    AMF_DECLARE_IID(AMFContext, 0xa76a13f0, 0xd80e, 0x4fcc, 0xb5, 0x8, 0x65, 0xd0, 0xb5, 0x2e, 0xd9, 0xee)

    typedef struct AMFContextVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFContext* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFContext* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFContext* pThis, const struct AMFGuid *interfaceID, void** ppInterface);
        
        // AMFInterface AMFPropertyStorage

        AMF_RESULT          (AMF_STD_CALL *SetProperty)(AMFContext* pThis, const wchar_t* name, AMFVariantStruct value);
        AMF_RESULT          (AMF_STD_CALL *GetProperty)(AMFContext* pThis, const wchar_t* name, AMFVariantStruct* pValue);
        amf_bool            (AMF_STD_CALL *HasProperty)(AMFContext* pThis, const wchar_t* name);
        amf_size            (AMF_STD_CALL *GetPropertyCount)(AMFContext* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyAt)(AMFContext* pThis, amf_size index, wchar_t* name, amf_size nameSize, AMFVariantStruct* pValue);
        AMF_RESULT          (AMF_STD_CALL *Clear)(AMFContext* pThis);
        AMF_RESULT          (AMF_STD_CALL *AddTo)(AMFContext* pThis, AMFPropertyStorage* pDest, amf_bool overwrite, amf_bool deep);
        AMF_RESULT          (AMF_STD_CALL *CopyTo)(AMFContext* pThis, AMFPropertyStorage* pDest, amf_bool deep);
        void                (AMF_STD_CALL *AddObserver)(AMFContext* pThis, AMFPropertyStorageObserver* pObserver);
        void                (AMF_STD_CALL *RemoveObserver)(AMFContext* pThis, AMFPropertyStorageObserver* pObserver);

        // AMFContext interface
       
        // Cleanup
        AMF_RESULT          (AMF_STD_CALL *Terminate)(AMFContext* pThis);

        // DX9
        AMF_RESULT          (AMF_STD_CALL *InitDX9)(AMFContext* pThis, void* pDX9Device);
        void*               (AMF_STD_CALL *GetDX9Device)(AMFContext* pThis, AMF_DX_VERSION dxVersionRequired);
        AMF_RESULT          (AMF_STD_CALL *LockDX9)(AMFContext* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockDX9)(AMFContext* pThis);
        // DX11
        AMF_RESULT          (AMF_STD_CALL *InitDX11)(AMFContext* pThis, void* pDX11Device, AMF_DX_VERSION dxVersionRequired);
        void*               (AMF_STD_CALL *GetDX11Device)(AMFContext* pThis, AMF_DX_VERSION dxVersionRequired);
        AMF_RESULT          (AMF_STD_CALL *LockDX11)(AMFContext* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockDX11)(AMFContext* pThis);

        // OpenCL
        AMF_RESULT          (AMF_STD_CALL *InitOpenCL)(AMFContext* pThis, void* pCommandQueue);
        void*               (AMF_STD_CALL *GetOpenCLContext)(AMFContext* pThis);
        void*               (AMF_STD_CALL *GetOpenCLCommandQueue)(AMFContext* pThis);
        void*               (AMF_STD_CALL *GetOpenCLDeviceID)(AMFContext* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetOpenCLComputeFactory)(AMFContext* pThis, AMFComputeFactory **ppFactory); // advanced compute - multiple queries
        AMF_RESULT          (AMF_STD_CALL *InitOpenCLEx)(AMFContext* pThis, AMFComputeDevice *pDevice);
        AMF_RESULT          (AMF_STD_CALL *LockOpenCL)(AMFContext* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockOpenCL)(AMFContext* pThis);

        // OpenGL
        AMF_RESULT          (AMF_STD_CALL *InitOpenGL)(AMFContext* pThis, amf_handle hOpenGLContext, amf_handle hWindow, amf_handle hDC);
        amf_handle          (AMF_STD_CALL *GetOpenGLContext)(AMFContext* pThis);
        amf_handle          (AMF_STD_CALL *GetOpenGLDrawable)(AMFContext* pThis);
        AMF_RESULT          (AMF_STD_CALL *LockOpenGL)(AMFContext* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockOpenGL)(AMFContext* pThis);
        // XV - Linux
        AMF_RESULT          (AMF_STD_CALL *InitXV)(AMFContext* pThis, void* pXVDevice);
        void*               (AMF_STD_CALL *GetXVDevice)(AMFContext* pThis);
        AMF_RESULT          (AMF_STD_CALL *LockXV)(AMFContext* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockXV)(AMFContext* pThis);

        // Gralloc - Android
        AMF_RESULT          (AMF_STD_CALL *InitGralloc)(AMFContext* pThis, void* pGrallocDevice);
        void*               (AMF_STD_CALL *GetGrallocDevice)(AMFContext* pThis);
        AMF_RESULT          (AMF_STD_CALL *LockGralloc)(AMFContext* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockGralloc)(AMFContext* pThis);
        // Allocation
        AMF_RESULT          (AMF_STD_CALL *AllocBuffer)(AMFContext* pThis, AMF_MEMORY_TYPE type, amf_size size, AMFBuffer** ppBuffer);
        AMF_RESULT          (AMF_STD_CALL *AllocSurface)(AMFContext* pThis, AMF_MEMORY_TYPE type, AMF_SURFACE_FORMAT format, amf_int32 width, amf_int32 height, AMFSurface** ppSurface);
        AMF_RESULT          (AMF_STD_CALL *AllocAudioBuffer)(AMFContext* pThis, AMF_MEMORY_TYPE type, AMF_AUDIO_FORMAT format, amf_int32 samples, amf_int32 sampleRate, amf_int32 channels, 
                                                    AMFAudioBuffer** ppAudioBuffer);

        // Wrap existing objects
        AMF_RESULT          (AMF_STD_CALL *CreateBufferFromHostNative)(AMFContext* pThis, void* pHostBuffer, amf_size size, AMFBuffer** ppBuffer, AMFBufferObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromHostNative)(AMFContext* pThis, AMF_SURFACE_FORMAT format, amf_int32 width, amf_int32 height, amf_int32 hPitch, amf_int32 vPitch, void* pData, 
                                                     AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromDX9Native)(AMFContext* pThis, void* pDX9Surface, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromDX11Native)(AMFContext* pThis, void* pDX11Surface, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromOpenGLNative)(AMFContext* pThis, AMF_SURFACE_FORMAT format, amf_handle hGLTextureID, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromGrallocNative)(AMFContext* pThis, amf_handle hGrallocSurface, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromOpenCLNative)(AMFContext* pThis, AMF_SURFACE_FORMAT format, amf_int32 width, amf_int32 height, void** pClPlanes, 
                                                     AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateBufferFromOpenCLNative)(AMFContext* pThis, void* pCLBuffer, amf_size size, AMFBuffer** ppBuffer);

        // Access to AMFCompute interface - AMF_MEMORY_OPENCL, AMF_MEMORY_COMPUTE_FOR_DX9, AMF_MEMORY_COMPUTE_FOR_DX11 are currently supported
        AMF_RESULT          (AMF_STD_CALL *GetCompute)(AMFContext* pThis, AMF_MEMORY_TYPE eMemType, AMFCompute** ppCompute);

    } AMFContextVtbl;

    struct AMFContext
    {
        const AMFContextVtbl *pVtbl;
    };


    typedef struct AMFContext1 AMFContext1;
    AMF_DECLARE_IID(AMFContext1, 0xd9e9f868, 0x6220, 0x44c6, 0xa2, 0x2f, 0x7c, 0xd6, 0xda, 0xc6, 0x86, 0x46)

    typedef struct AMFContext1Vtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFContext1* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFContext1* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFContext1* pThis, const struct AMFGuid *interfaceID, void** ppInterface);
        
        // AMFInterface AMFPropertyStorage

        AMF_RESULT          (AMF_STD_CALL *SetProperty)(AMFContext1* pThis, const wchar_t* name, AMFVariantStruct value);
        AMF_RESULT          (AMF_STD_CALL *GetProperty)(AMFContext1* pThis, const wchar_t* name, AMFVariantStruct* pValue);
        amf_bool            (AMF_STD_CALL *HasProperty)(AMFContext1* pThis, const wchar_t* name);
        amf_size            (AMF_STD_CALL *GetPropertyCount)(AMFContext1* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyAt)(AMFContext1* pThis, amf_size index, wchar_t* name, amf_size nameSize, AMFVariantStruct* pValue);
        AMF_RESULT          (AMF_STD_CALL *Clear)(AMFContext1* pThis);
        AMF_RESULT          (AMF_STD_CALL *AddTo)(AMFContext1* pThis, AMFPropertyStorage* pDest, amf_bool overwrite, amf_bool deep);
        AMF_RESULT          (AMF_STD_CALL *CopyTo)(AMFContext1* pThis, AMFPropertyStorage* pDest, amf_bool deep);
        void                (AMF_STD_CALL *AddObserver)(AMFContext1* pThis, AMFPropertyStorageObserver* pObserver);
        void                (AMF_STD_CALL *RemoveObserver)(AMFContext1* pThis, AMFPropertyStorageObserver* pObserver);

        // AMFContext interface
       
        // Cleanup
        AMF_RESULT          (AMF_STD_CALL *Terminate)(AMFContext1* pThis);

        // DX9
        AMF_RESULT          (AMF_STD_CALL *InitDX9)(AMFContext1* pThis, void* pDX9Device);
        void*               (AMF_STD_CALL *GetDX9Device)(AMFContext1* pThis, AMF_DX_VERSION dxVersionRequired);
        AMF_RESULT          (AMF_STD_CALL *LockDX9)(AMFContext1* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockDX9)(AMFContext1* pThis);
        // DX11
        AMF_RESULT          (AMF_STD_CALL *InitDX11)(AMFContext1* pThis, void* pDX11Device, AMF_DX_VERSION dxVersionRequired);
        void*               (AMF_STD_CALL *GetDX11Device)(AMFContext1* pThis, AMF_DX_VERSION dxVersionRequired);
        AMF_RESULT          (AMF_STD_CALL *LockDX11)(AMFContext1* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockDX11)(AMFContext1* pThis);

        // OpenCL
        AMF_RESULT          (AMF_STD_CALL *InitOpenCL)(AMFContext1* pThis, void* pCommandQueue);
        void*               (AMF_STD_CALL *GetOpenCLContext)(AMFContext1* pThis);
        void*               (AMF_STD_CALL *GetOpenCLCommandQueue)(AMFContext1* pThis);
        void*               (AMF_STD_CALL *GetOpenCLDeviceID)(AMFContext1* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetOpenCLComputeFactory)(AMFContext1* pThis, AMFComputeFactory **ppFactory); // advanced compute - multiple queries
        AMF_RESULT          (AMF_STD_CALL *InitOpenCLEx)(AMFContext1* pThis, AMFComputeDevice *pDevice);
        AMF_RESULT          (AMF_STD_CALL *LockOpenCL)(AMFContext1* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockOpenCL)(AMFContext1* pThis);

        // OpenGL
        AMF_RESULT          (AMF_STD_CALL *InitOpenGL)(AMFContext1* pThis, amf_handle hOpenGLContext, amf_handle hWindow, amf_handle hDC);
        amf_handle          (AMF_STD_CALL *GetOpenGLContext)(AMFContext1* pThis);
        amf_handle          (AMF_STD_CALL *GetOpenGLDrawable)(AMFContext1* pThis);
        AMF_RESULT          (AMF_STD_CALL *LockOpenGL)(AMFContext1* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockOpenGL)(AMFContext1* pThis);
        // XV - Linux
        AMF_RESULT          (AMF_STD_CALL *InitXV)(AMFContext1* pThis, void* pXVDevice);
        void*               (AMF_STD_CALL *GetXVDevice)(AMFContext1* pThis);
        AMF_RESULT          (AMF_STD_CALL *LockXV)(AMFContext1* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockXV)(AMFContext1* pThis);

        // Gralloc - Android
        AMF_RESULT          (AMF_STD_CALL *InitGralloc)(AMFContext1* pThis, void* pGrallocDevice);
        void*               (AMF_STD_CALL *GetGrallocDevice)(AMFContext1* pThis);
        AMF_RESULT          (AMF_STD_CALL *LockGralloc)(AMFContext1* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockGralloc)(AMFContext1* pThis);
        // Allocation
        AMF_RESULT          (AMF_STD_CALL *AllocBuffer)(AMFContext1* pThis, AMF_MEMORY_TYPE type, amf_size size, AMFBuffer** ppBuffer);
        AMF_RESULT          (AMF_STD_CALL *AllocSurface)(AMFContext1* pThis, AMF_MEMORY_TYPE type, AMF_SURFACE_FORMAT format, amf_int32 width, amf_int32 height, AMFSurface** ppSurface);
        AMF_RESULT          (AMF_STD_CALL *AllocAudioBuffer)(AMFContext1* pThis, AMF_MEMORY_TYPE type, AMF_AUDIO_FORMAT format, amf_int32 samples, amf_int32 sampleRate, amf_int32 channels, 
                                                    AMFAudioBuffer** ppAudioBuffer);

        // Wrap existing objects
        AMF_RESULT          (AMF_STD_CALL *CreateBufferFromHostNative)(AMFContext1* pThis, void* pHostBuffer, amf_size size, AMFBuffer** ppBuffer, AMFBufferObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromHostNative)(AMFContext1* pThis, AMF_SURFACE_FORMAT format, amf_int32 width, amf_int32 height, amf_int32 hPitch, amf_int32 vPitch, void* pData, 
                                                     AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromDX9Native)(AMFContext1* pThis, void* pDX9Surface, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromDX11Native)(AMFContext1* pThis, void* pDX11Surface, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromOpenGLNative)(AMFContext1* pThis, AMF_SURFACE_FORMAT format, amf_handle hGLTextureID, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromGrallocNative)(AMFContext1* pThis, amf_handle hGrallocSurface, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromOpenCLNative)(AMFContext1* pThis, AMF_SURFACE_FORMAT format, amf_int32 width, amf_int32 height, void** pClPlanes, 
                                                     AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateBufferFromOpenCLNative)(AMFContext1* pThis, void* pCLBuffer, amf_size size, AMFBuffer** ppBuffer);

        // Access to AMFCompute interface - AMF_MEMORY_OPENCL, AMF_MEMORY_COMPUTE_FOR_DX9, AMF_MEMORY_COMPUTE_FOR_DX11 are currently supported
        AMF_RESULT          (AMF_STD_CALL *GetCompute)(AMFContext1* pThis, AMF_MEMORY_TYPE eMemType, AMFCompute** ppCompute);

        // AMFContext1 interface

        AMF_RESULT          (AMF_STD_CALL *CreateBufferFromDX11Native)(AMFContext1* pThis, void* pHostBuffer, AMFBuffer** ppBuffer, AMFBufferObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *AllocBufferEx)(AMFContext1* pThis, AMF_MEMORY_TYPE type, amf_size size, AMF_BUFFER_USAGE usage, AMF_MEMORY_CPU_ACCESS access, AMFBuffer** ppBuffer);
        AMF_RESULT          (AMF_STD_CALL *AllocSurfaceEx)(AMFContext1* pThis, AMF_MEMORY_TYPE type, AMF_SURFACE_FORMAT format, amf_int32 width, amf_int32 height, AMF_SURFACE_USAGE usage, AMF_MEMORY_CPU_ACCESS access, AMFSurface** ppSurface);

        // Vulkan - Windows, Linux
        AMF_RESULT          (AMF_STD_CALL *InitVulkan)(AMFContext1* pThis, void* pVulkanDevice);
        void*               (AMF_STD_CALL *GetVulkanDevice)(AMFContext1* pThis);
        AMF_RESULT          (AMF_STD_CALL *LockVulkan)(AMFContext1* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockVulkan)(AMFContext1* pThis);

        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromVulkanNative)(AMFContext1* pThis, void* pVulkanImage, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateBufferFromVulkanNative)(AMFContext1* pThis, void* pVulkanBuffer, AMFBuffer** ppBuffer, AMFBufferObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *GetVulkanDeviceExtensions)(AMFContext1* pThis, amf_size *pCount, const char **ppExtensions);

    } AMFContext1Vtbl;

    struct AMFContext1
    {
        const AMFContext1Vtbl *pVtbl;
    };

    typedef struct AMFContext2 AMFContext2;
    AMF_DECLARE_IID(AMFContext2, 0xd9e9f868, 0x6220, 0x44c6, 0xa2, 0x2f, 0x7c, 0xd6, 0xda, 0xc6, 0x86, 0x46)

        typedef struct AMFContext2Vtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFContext2* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFContext2* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFContext2* pThis, const struct AMFGuid *interfaceID, void** ppInterface);

        // AMFInterface AMFPropertyStorage

        AMF_RESULT          (AMF_STD_CALL *SetProperty)(AMFContext2* pThis, const wchar_t* name, AMFVariantStruct value);
        AMF_RESULT          (AMF_STD_CALL *GetProperty)(AMFContext2* pThis, const wchar_t* name, AMFVariantStruct* pValue);
        amf_bool            (AMF_STD_CALL *HasProperty)(AMFContext2* pThis, const wchar_t* name);
        amf_size            (AMF_STD_CALL *GetPropertyCount)(AMFContext2* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyAt)(AMFContext2* pThis, amf_size index, wchar_t* name, amf_size nameSize, AMFVariantStruct* pValue);
        AMF_RESULT          (AMF_STD_CALL *Clear)(AMFContext2* pThis);
        AMF_RESULT          (AMF_STD_CALL *AddTo)(AMFContext2* pThis, AMFPropertyStorage* pDest, amf_bool overwrite, amf_bool deep);
        AMF_RESULT          (AMF_STD_CALL *CopyTo)(AMFContext2* pThis, AMFPropertyStorage* pDest, amf_bool deep);
        void                (AMF_STD_CALL *AddObserver)(AMFContext2* pThis, AMFPropertyStorageObserver* pObserver);
        void                (AMF_STD_CALL *RemoveObserver)(AMFContext2* pThis, AMFPropertyStorageObserver* pObserver);

        // AMFContext interface

        // Cleanup
        AMF_RESULT          (AMF_STD_CALL *Terminate)(AMFContext2* pThis);

        // DX9
        AMF_RESULT          (AMF_STD_CALL *InitDX9)(AMFContext2* pThis, void* pDX9Device);
        void*                         (AMF_STD_CALL *GetDX9Device)(AMFContext2* pThis, AMF_DX_VERSION dxVersionRequired);
        AMF_RESULT          (AMF_STD_CALL *LockDX9)(AMFContext2* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockDX9)(AMFContext2* pThis);
        // DX11
        AMF_RESULT          (AMF_STD_CALL *InitDX11)(AMFContext2* pThis, void* pDX11Device, AMF_DX_VERSION dxVersionRequired);
        void*                         (AMF_STD_CALL *GetDX11Device)(AMFContext2* pThis, AMF_DX_VERSION dxVersionRequired);
        AMF_RESULT          (AMF_STD_CALL *LockDX11)(AMFContext2* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockDX11)(AMFContext2* pThis);

        // OpenCL
        AMF_RESULT          (AMF_STD_CALL *InitOpenCL)(AMFContext2* pThis, void* pCommandQueue);
        void*               (AMF_STD_CALL *GetOpenCLContext)(AMFContext2* pThis);
        void*               (AMF_STD_CALL *GetOpenCLCommandQueue)(AMFContext2* pThis);
        void*               (AMF_STD_CALL *GetOpenCLDeviceID)(AMFContext2* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetOpenCLComputeFactory)(AMFContext2* pThis, AMFComputeFactory **ppFactory); // advanced compute - multiple queries
        AMF_RESULT          (AMF_STD_CALL *InitOpenCLEx)(AMFContext2* pThis, AMFComputeDevice *pDevice);
        AMF_RESULT          (AMF_STD_CALL *LockOpenCL)(AMFContext2* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockOpenCL)(AMFContext2* pThis);

        // OpenGL
        AMF_RESULT          (AMF_STD_CALL *InitOpenGL)(AMFContext2* pThis, amf_handle hOpenGLContext, amf_handle hWindow, amf_handle hDC);
        amf_handle          (AMF_STD_CALL *GetOpenGLContext)(AMFContext2* pThis);
        amf_handle          (AMF_STD_CALL *GetOpenGLDrawable)(AMFContext2* pThis);
        AMF_RESULT          (AMF_STD_CALL *LockOpenGL)(AMFContext2* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockOpenGL)(AMFContext2* pThis);
        // XV - Linux
        AMF_RESULT          (AMF_STD_CALL *InitXV)(AMFContext2* pThis, void* pXVDevice);
        void*               (AMF_STD_CALL *GetXVDevice)(AMFContext2* pThis);
        AMF_RESULT          (AMF_STD_CALL *LockXV)(AMFContext2* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockXV)(AMFContext2* pThis);

        // Gralloc - Android
        AMF_RESULT          (AMF_STD_CALL *InitGralloc)(AMFContext2* pThis, void* pGrallocDevice);
        void*               (AMF_STD_CALL *GetGrallocDevice)(AMFContext2* pThis);
        AMF_RESULT          (AMF_STD_CALL *LockGralloc)(AMFContext2* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockGralloc)(AMFContext2* pThis);
        // Allocation
        AMF_RESULT          (AMF_STD_CALL *AllocBuffer)(AMFContext2* pThis, AMF_MEMORY_TYPE type, amf_size size, AMFBuffer** ppBuffer);
        AMF_RESULT          (AMF_STD_CALL *AllocSurface)(AMFContext2* pThis, AMF_MEMORY_TYPE type, AMF_SURFACE_FORMAT format, amf_int32 width, amf_int32 height, AMFSurface** ppSurface);
        AMF_RESULT          (AMF_STD_CALL *AllocAudioBuffer)(AMFContext2* pThis, AMF_MEMORY_TYPE type, AMF_AUDIO_FORMAT format, amf_int32 samples, amf_int32 sampleRate, amf_int32 channels, AMFAudioBuffer** ppAudioBuffer);

        // Wrap existing objects
        AMF_RESULT          (AMF_STD_CALL *CreateBufferFromHostNative)(AMFContext2* pThis, void* pHostBuffer, amf_size size, AMFBuffer** ppBuffer, AMFBufferObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromHostNative)(AMFContext2* pThis, AMF_SURFACE_FORMAT format, amf_int32 width, amf_int32 height, amf_int32 hPitch, amf_int32 vPitch, void* pData,AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromDX9Native)(AMFContext2* pThis, void* pDX9Surface, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromDX11Native)(AMFContext2* pThis, void* pDX11Surface, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromOpenGLNative)(AMFContext2* pThis, AMF_SURFACE_FORMAT format, amf_handle hGLTextureID, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromGrallocNative)(AMFContext2* pThis, amf_handle hGrallocSurface, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromOpenCLNative)(AMFContext2* pThis, AMF_SURFACE_FORMAT format, amf_int32 width, amf_int32 height, void** pClPlanes, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateBufferFromOpenCLNative)(AMFContext2* pThis, void* pCLBuffer, amf_size size, AMFBuffer** ppBuffer);

        // Access to AMFCompute interface - AMF_MEMORY_OPENCL, AMF_MEMORY_COMPUTE_FOR_DX9, AMF_MEMORY_COMPUTE_FOR_DX11 are currently supported
        AMF_RESULT          (AMF_STD_CALL *GetCompute)(AMFContext2* pThis, AMF_MEMORY_TYPE eMemType, AMFCompute** ppCompute);

        // AMFContext1 interface

        AMF_RESULT          (AMF_STD_CALL *CreateBufferFromDX11Native)(AMFContext2* pThis, void* pHostBuffer, AMFBuffer** ppBuffer, AMFBufferObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *AllocBufferEx)(AMFContext2* pThis, AMF_MEMORY_TYPE type, amf_size size, AMF_BUFFER_USAGE usage, AMF_MEMORY_CPU_ACCESS access, AMFBuffer** ppBuffer);
        AMF_RESULT          (AMF_STD_CALL *AllocSurfaceEx)(AMFContext2* pThis, AMF_MEMORY_TYPE type, AMF_SURFACE_FORMAT format, amf_int32 width, amf_int32 height, AMF_SURFACE_USAGE usage, AMF_MEMORY_CPU_ACCESS access, AMFSurface** ppSurface);

        // Vulkan - Windows, Linux
        AMF_RESULT          (AMF_STD_CALL *InitVulkan)(AMFContext2* pThis, void* pVulkanDevice);
        void*               (AMF_STD_CALL *GetVulkanDevice)(AMFContext2* pThis);
        AMF_RESULT          (AMF_STD_CALL *LockVulkan)(AMFContext2* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockVulkan)(AMFContext2* pThis);

        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromVulkanNative)(AMFContext2* pThis, void* pVulkanImage, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateBufferFromVulkanNative)(AMFContext2* pThis, void* pVulkanBuffer, AMFBuffer** ppBuffer, AMFBufferObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *GetVulkanDeviceExtensions)(AMFContext2* pThis, amf_size *pCount, const char **ppExtensions);

        // AMFContext2 interface
        AMF_RESULT          (AMF_STD_CALL *InitDX12)(AMFContext2* pThis, void* pDX11Device, AMF_DX_VERSION dxVersionRequired);
        void*               (AMF_STD_CALL *GetDX12Device)(AMFContext2* pThis, AMF_DX_VERSION dxVersionRequired);
        AMF_RESULT          (AMF_STD_CALL *LockDX12)(AMFContext2* pThis);
        AMF_RESULT          (AMF_STD_CALL *UnlockDX12)(AMFContext2* pThis);

        AMF_RESULT          (AMF_STD_CALL *CreateSurfaceFromDX12Native)(AMFContext2* pThis, void* pResourceTexture, AMFSurface** ppSurface, AMFSurfaceObserver* pObserver);
        AMF_RESULT          (AMF_STD_CALL *CreateBufferFromDX12Native)(AMFContext2* pThis, void* pResourceBuffer, AMFBuffer** ppBuffer, AMFBufferObserver* pObserver);


    } AMFContext2Vtbl;

    struct AMFContext2
    {
        const AMFContext2Vtbl *pVtbl;
    };
#endif

#if defined(__cplusplus)
    //----------------------------------------------------------------------------------------------
    // Lockers
    //----------------------------------------------------------------------------------------------
    class AMFContext::AMFDX9Locker
    {
    public:
        AMFDX9Locker() : m_Context(NULL)
        {}
        AMFDX9Locker(AMFContext* resources) : m_Context(NULL)
        {
            Lock(resources);
        }
        ~AMFDX9Locker()
        {
            if(m_Context != NULL)
            {
                m_Context->UnlockDX9();
            }
        }
        void Lock(AMFContext* resources)
        {
            if(m_Context != NULL)
            {
                m_Context->UnlockDX9();
            }
            m_Context = resources;
            if(m_Context != NULL)
            {
                m_Context->LockDX9();
            }
        }
    protected:
        AMFContext* m_Context;

    private:
        AMFDX9Locker(const AMFDX9Locker&);
        AMFDX9Locker& operator=(const AMFDX9Locker&);
    };
    //----------------------------------------------------------------------------------------------
    class AMFContext::AMFDX11Locker
    {
    public:
        AMFDX11Locker() : m_Context(NULL)
        {}
        AMFDX11Locker(AMFContext* resources) : m_Context(NULL)
        {
            Lock(resources);
        }
        ~AMFDX11Locker()
        {
            if(m_Context != NULL)
            {
                m_Context->UnlockDX11();
            }
        }
        void Lock(AMFContext* resources)
        {
            if(m_Context != NULL)
            {
                m_Context->UnlockDX11();
            }
            m_Context = resources;
            if(m_Context != NULL)
            {
                m_Context->LockDX11();
            }
        }
    protected:
        AMFContext* m_Context;

    private:
        AMFDX11Locker(const AMFDX11Locker&);
        AMFDX11Locker& operator=(const AMFDX11Locker&);
    };
    //----------------------------------------------------------------------------------------------
    class AMFContext::AMFOpenCLLocker
    {
    public:
        AMFOpenCLLocker() : m_Context(NULL)
        {}
        AMFOpenCLLocker(AMFContext* resources) : m_Context(NULL)
        {
            Lock(resources);
        }
        ~AMFOpenCLLocker()
        {
            if(m_Context != NULL)
            {
                m_Context->UnlockOpenCL();
            }
        }
        void Lock(AMFContext* resources)
        {
            if(m_Context != NULL)
            {
                m_Context->UnlockOpenCL();
            }
            m_Context = resources;
            if(m_Context != NULL)
            {
                m_Context->LockOpenCL();
            }
        }
    protected:
        AMFContext* m_Context;
    private:
        AMFOpenCLLocker(const AMFOpenCLLocker&);
        AMFOpenCLLocker& operator=(const AMFOpenCLLocker&);
    };
    //----------------------------------------------------------------------------------------------
    class AMFContext::AMFOpenGLLocker
    {
    public:
        AMFOpenGLLocker(AMFContext* pContext) : m_pContext(pContext),
            m_GLLocked(false)
        {
            if(m_pContext != NULL)
            {
                if(m_pContext->LockOpenGL() == AMF_OK)
                {
                    m_GLLocked = true;
                }
            }
        }
        ~AMFOpenGLLocker()
        {
            if(m_GLLocked)
            {
                m_pContext->UnlockOpenGL();
            }
        }
    private:
        AMFContext* m_pContext;
        amf_bool m_GLLocked; ///< AMFOpenGLLocker can be called when OpenGL is not initialized yet
                             ///< in this case don't call UnlockOpenGL
        AMFOpenGLLocker(const AMFOpenGLLocker&);
        AMFOpenGLLocker& operator=(const AMFOpenGLLocker&);
    };
    //----------------------------------------------------------------------------------------------
    class AMFContext::AMFXVLocker
    {
    public:
        AMFXVLocker() : m_pContext(NULL)
        {}
        AMFXVLocker(AMFContext* pContext) : m_pContext(NULL)
        {
            Lock(pContext);
        }
        ~AMFXVLocker()
        {
            if(m_pContext != NULL)
            {
                m_pContext->UnlockXV();
            }
        }
        void Lock(AMFContext* pContext)
        {
            if((pContext != NULL) && (pContext->GetXVDevice() != NULL))
            {
                m_pContext = pContext;
                m_pContext->LockXV();
            }
        }
    protected:
        AMFContext* m_pContext;
    private:
        AMFXVLocker(const AMFXVLocker&);
        AMFXVLocker& operator=(const AMFXVLocker&);
    };
    //----------------------------------------------------------------------------------------------
    class AMFContext::AMFGrallocLocker
    {
    public:
        AMFGrallocLocker() : m_pContext(NULL)
        {}
        AMFGrallocLocker(AMFContext* pContext) : m_pContext(NULL)
        {
            Lock(pContext);
        }
        ~AMFGrallocLocker()
        {
            if(m_pContext != NULL)
            {
                m_pContext->UnlockGralloc();
            }
        }
        void Lock(AMFContext* pContext)
        {
            if((pContext != NULL) && (pContext->GetGrallocDevice() != NULL))
            {
                m_pContext = pContext;
                m_pContext->LockGralloc();
            }
        }
    protected:
        AMFContext* m_pContext;
    private:
        AMFGrallocLocker(const AMFGrallocLocker&);
        AMFGrallocLocker& operator=(const AMFGrallocLocker&);
    };
    //----------------------------------------------------------------------------------------------
    class AMFContext1::AMFVulkanLocker
    {
    public:
        AMFVulkanLocker() : m_pContext(NULL)
        {}
        AMFVulkanLocker(AMFContext1* pContext) : m_pContext(NULL)
        {
            Lock(pContext);
        }
        ~AMFVulkanLocker()
        {
            if(m_pContext != NULL)
            {
                m_pContext->UnlockVulkan();
            }
        }
        void Lock(AMFContext1* pContext)
        {
            if((pContext != NULL) && (pContext->GetVulkanDevice() != NULL))
            {
                m_pContext = pContext;
                m_pContext->LockVulkan();
            }
        }
    protected:
        AMFContext1* m_pContext;
    private:
        AMFVulkanLocker(const AMFVulkanLocker&);
        AMFVulkanLocker& operator=(const AMFVulkanLocker&);
    };
    //----------------------------------------------------------------------------------------------
    class AMFContext2::AMFDX12Locker
    {
    public:
        AMFDX12Locker() : m_Context(NULL)
        {}
        AMFDX12Locker(AMFContext2* resources) : m_Context(NULL)
        {
            Lock(resources);
        }
        ~AMFDX12Locker()
        {
            if (m_Context != NULL)
            {
                m_Context->UnlockDX12();
            }
        }
        void Lock(AMFContext2* resources)
        {
            if (m_Context != NULL)
            {
                m_Context->UnlockDX12();
            }
            m_Context = resources;
            if (m_Context != NULL)
            {
                m_Context->LockDX12();
            }
        }
    protected:
        AMFContext2* m_Context;

    private:
        AMFDX12Locker(const AMFDX12Locker&);
        AMFDX12Locker& operator=(const AMFDX12Locker&);
    };
    //----------------------------------------------------------------------------------------------
    //----------------------------------------------------------------------------------------------
    //----------------------------------------------------------------------------------------------
#endif
#if defined(__cplusplus)
}
#endif
enum AMF_CONTEXT_DEVICETYPE_ENUM
{
    AMF_CONTEXT_DEVICE_TYPE_GPU = 0,
    AMF_CONTEXT_DEVICE_TYPE_CPU
};
#define AMF_CONTEXT_DEVICE_TYPE  L"AMF_Context_DeviceType" //Value type: amf_int64; Values : AMF_CONTEXT_DEVICE_TYPE_GPU for GPU (default) , AMF_CONTEXT_DEVICE_TYPE_CPU for CPU.
#endif //#ifndef AMF_Context_h
