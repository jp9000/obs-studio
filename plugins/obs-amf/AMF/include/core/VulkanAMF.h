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

#ifndef __VulkanAMF_h__
#define __VulkanAMF_h__
#pragma once 
#include "Platform.h"
#ifdef _WIN32
    #define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__linux)
    #define VK_USE_PLATFORM_XLIB_KHR
#endif

#include "vulkan/vulkan.h"

#if defined(__cplusplus)
namespace amf
{
#endif
    typedef struct AMFVulkanDevice
    {
        amf_size            cbSizeof;           // sizeof(AMFVulkanDevice)
        void*               pNext;              // reserved for extensions 
        VkInstance          hInstance;
        VkPhysicalDevice    hPhysicalDevice;    
        VkDevice            hDevice;
    } AMFVulkanDevice;

    typedef struct AMFVulkanSync
    {
        amf_size            cbSizeof;           // sizeof(AMFVulkanSemaphore)
        void*               pNext;              // reserved for extensions 
        VkSemaphore         hSemaphore; 
        bool                bSubmitted;         // if true - wait for hSemaphore. re-submit hSemaphore if not synced by other ways and set to true
        VkFence             hFence;             // To sync on CPU; can be nullptr. Submitted in vkQueueSubmit. If waited for hFence, null it, do not delete or reset.
    } AMFVulkanSync;

    typedef struct AMFVulkanBuffer
    {
        amf_size            cbSizeof;           // sizeof(AMFVulkanBuffer)
        void*               pNext;              // reserved for extensions 
        VkBuffer            hBuffer;
        VkDeviceMemory      hMemory;
        amf_int64           iSize;
        amf_int64           iAllocatedSize;     // for reuse
        amf_uint32          eAccessFlags;       // VkAccessFlagBits
        amf_uint32          eUsage;             // AMF_BUFFER_USAGE
        amf_uint32          eAccess;            // AMF_MEMORY_CPU_ACCESS 
        AMFVulkanSync       Sync;
    } AMFVulkanBuffer;

    typedef struct AMFVulkanSurface
    {
        amf_size            cbSizeof;           // sizeof(AMFVulkanSurface)
        void*               pNext;              // reserved for extensions 
        // surface properties
        VkImage             hImage;
        VkDeviceMemory      hMemory;
        amf_int64           iSize;              // memory size
        amf_uint32          eFormat;            // VkFormat
        amf_int32           iWidth;
        amf_int32           iHeight;
        amf_uint32          eCurrentLayout;     // VkImageLayout
        amf_uint32          eUsage;             // AMF_SURFACE_USAGE
        amf_uint32          eAccess;            // AMF_MEMORY_CPU_ACCESS
        AMFVulkanSync       Sync;               // To sync on GPU
    } AMFVulkanSurface;

    typedef struct AMFVulkanView
    {
        amf_size            cbSizeof;           // sizeof(AMFVulkanSurface)
        void*               pNext;              // reserved for extensions 
        // surface properties
        AMFVulkanSurface    *pSurface;
        VkImageView         hView;
        amf_int32           iPlaneWidth;
        amf_int32           iPlaneHeight;
        amf_int32           iPlaneWidthPitch;
        amf_int32           iPlaneHeightPitch;
    } AMFVulkanView;
#if defined(__cplusplus)
} // namespace amf
#endif
#endif // __VulkanAMF_h__