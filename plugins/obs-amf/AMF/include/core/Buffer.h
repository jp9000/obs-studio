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

#ifndef AMF_Buffer_h
#define AMF_Buffer_h
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

    //----------------------------------------------------------------------------------------------
    // AMF_BUFFER_USAGE translates to D3D11_BIND_FLAG or VkBufferUsageFlagBits
    // bit mask
    //----------------------------------------------------------------------------------------------
    typedef enum AMF_BUFFER_USAGE_BITS
    {                                                      // D3D11                         D3D12                                       Vulkan 
        AMF_BUFFER_USAGE_DEFAULT           = 0x80000000,   // D3D11_USAGE_STAGING,                                                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT 
        AMF_BUFFER_USAGE_NONE              = 0x00000000,   // 0                  ,          D3D12_RESOURCE_FLAG_NONE,                   0
        AMF_BUFFER_USAGE_CONSTANT          = 0x00000001,   // D3D11_BIND_CONSTANT_BUFFER,   											VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT 
        AMF_BUFFER_USAGE_SHADER_RESOURCE   = 0x00000002,   // D3D11_BIND_SHADER_RESOURCE,   D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
        AMF_BUFFER_USAGE_UNORDERED_ACCESS  = 0x00000004,   // D3D11_BIND_UNORDERED_ACCESS,  D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        AMF_BUFFER_USAGE_TRANSFER_SRC      = 0x00000008,   //                               								            VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        AMF_BUFFER_USAGE_TRANSFER_DST      = 0x00000010,   //                               								            VK_BUFFER_USAGE_TRANSFER_DST_BIT
    } AMF_BUFFER_USAGE_BITS;
    typedef amf_flags AMF_BUFFER_USAGE;
    //----------------------------------------------------------------------------------------------


    //----------------------------------------------------------------------------------------------
    // AMFBufferObserver interface - callback
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMFBuffer;
    class AMF_NO_VTABLE AMFBufferObserver
    {
    public:
        virtual void                AMF_STD_CALL OnBufferDataRelease(AMFBuffer* pBuffer) = 0;
    };
#else // #if defined(__cplusplus)
    typedef struct AMFBuffer AMFBuffer;
    typedef struct AMFBufferObserver AMFBufferObserver;

    typedef struct AMFBufferObserverVtbl
    {
        void                (AMF_STD_CALL *OnBufferDataRelease)(AMFBufferObserver* pThis, AMFBuffer* pBuffer);
    } AMFBufferObserverVtbl;

    struct AMFBufferObserver
    {
        const AMFBufferObserverVtbl *pVtbl;
    };

#endif // #if defined(__cplusplus)

    //----------------------------------------------------------------------------------------------
    // AMFBuffer interface
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFBuffer : public AMFData
    {
    public:
        AMF_DECLARE_IID(0xb04b7248, 0xb6f0, 0x4321, 0xb6, 0x91, 0xba, 0xa4, 0x74, 0xf, 0x9f, 0xcb)

        virtual AMF_RESULT          AMF_STD_CALL SetSize(amf_size newSize) = 0;
        virtual amf_size            AMF_STD_CALL GetSize() = 0;
        virtual void*               AMF_STD_CALL GetNative() = 0;

        // Observer management
        virtual void                AMF_STD_CALL AddObserver(AMFBufferObserver* pObserver) = 0;
        virtual void                AMF_STD_CALL RemoveObserver(AMFBufferObserver* pObserver) = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFBuffer> AMFBufferPtr;
    //----------------------------------------------------------------------------------------------

#else // #if defined(__cplusplus)
        AMF_DECLARE_IID(AMFBuffer, 0xb04b7248, 0xb6f0, 0x4321, 0xb6, 0x91, 0xba, 0xa4, 0x74, 0xf, 0x9f, 0xcb)

    typedef struct AMFBufferVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFBuffer* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFBuffer* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFBuffer* pThis, const struct AMFGuid *interfaceID, void** ppInterface);

        // AMFPropertyStorage interface
        AMF_RESULT          (AMF_STD_CALL *SetProperty)(AMFBuffer* pThis, const wchar_t* name, AMFVariantStruct value);
        AMF_RESULT          (AMF_STD_CALL *GetProperty)(AMFBuffer* pThis, const wchar_t* name, AMFVariantStruct* pValue);
        amf_bool            (AMF_STD_CALL *HasProperty)(AMFBuffer* pThis, const wchar_t* name);
        amf_size            (AMF_STD_CALL *GetPropertyCount)(AMFBuffer* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyAt)(AMFBuffer* pThis, amf_size index, wchar_t* name, amf_size nameSize, AMFVariantStruct* pValue);
        AMF_RESULT          (AMF_STD_CALL *Clear)(AMFBuffer* pThis);
        AMF_RESULT          (AMF_STD_CALL *AddTo)(AMFBuffer* pThis, AMFPropertyStorage* pDest, amf_bool overwrite, amf_bool deep);
        AMF_RESULT          (AMF_STD_CALL *CopyTo)(AMFBuffer* pThis, AMFPropertyStorage* pDest, amf_bool deep);
        void                (AMF_STD_CALL *AddObserver)(AMFBuffer* pThis, AMFPropertyStorageObserver* pObserver);
        void                (AMF_STD_CALL *RemoveObserver)(AMFBuffer* pThis, AMFPropertyStorageObserver* pObserver);

        // AMFData interface

        AMF_MEMORY_TYPE     (AMF_STD_CALL *GetMemoryType)(AMFBuffer* pThis);

        AMF_RESULT          (AMF_STD_CALL *Duplicate)(AMFBuffer* pThis, AMF_MEMORY_TYPE type, AMFData** ppData);
        AMF_RESULT          (AMF_STD_CALL *Convert)(AMFBuffer* pThis, AMF_MEMORY_TYPE type); // optimal interop if possilble. Copy through host memory if needed
        AMF_RESULT          (AMF_STD_CALL *Interop)(AMFBuffer* pThis, AMF_MEMORY_TYPE type); // only optimal interop if possilble. No copy through host memory for GPU objects

        AMF_DATA_TYPE       (AMF_STD_CALL *GetDataType)(AMFBuffer* pThis);

        amf_bool            (AMF_STD_CALL *IsReusable)(AMFBuffer* pThis);

        void                (AMF_STD_CALL *SetPts)(AMFBuffer* pThis, amf_pts pts);
        amf_pts             (AMF_STD_CALL *GetPts)(AMFBuffer* pThis);
        void                (AMF_STD_CALL *SetDuration)(AMFBuffer* pThis, amf_pts duration);
        amf_pts             (AMF_STD_CALL *GetDuration)(AMFBuffer* pThis);

        // AMFBuffer interface

        AMF_RESULT          (AMF_STD_CALL *SetSize)(AMFBuffer* pThis, amf_size newSize);
        amf_size            (AMF_STD_CALL *GetSize)(AMFBuffer* pThis);
        void*               (AMF_STD_CALL *GetNative)(AMFBuffer* pThis);

        // Observer management
        void                (AMF_STD_CALL *AddObserver_Buffer)(AMFBuffer* pThis, AMFBufferObserver* pObserver);
        void                (AMF_STD_CALL *RemoveObserver_Buffer)(AMFBuffer* pThis, AMFBufferObserver* pObserver);

    } AMFBufferVtbl;

    struct AMFBuffer
    {
        const AMFBufferVtbl *pVtbl;
    };

#endif // #if defined(__cplusplus)
#if defined(__cplusplus)
} // namespace
#endif
#if defined(_MSC_VER)
    #pragma warning( pop )
#endif
#endif //#ifndef AMF_Buffer_h
