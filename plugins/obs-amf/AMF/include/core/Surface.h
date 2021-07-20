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

#ifndef AMF_Surface_h
#define AMF_Surface_h
#pragma once

#include "Data.h"
#include "Plane.h"

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
    typedef enum AMF_SURFACE_FORMAT
    {
        AMF_SURFACE_UNKNOWN     = 0,
        AMF_SURFACE_NV12,               ///< 1  - planar 4:2:0 Y width x height + packed UV width/2 x height/2 - 8 bit per component
        AMF_SURFACE_YV12,               ///< 2  - planar 4:2:0 Y width x height + V width/2 x height/2 + U width/2 x height/2 - 8 bit per component
        AMF_SURFACE_BGRA,               ///< 3  - packed 4:4:4 - 8 bit per component
        AMF_SURFACE_ARGB,               ///< 4  - packed 4:4:4 - 8 bit per component
        AMF_SURFACE_RGBA,               ///< 5  - packed 4:4:4 - 8 bit per component
        AMF_SURFACE_GRAY8,              ///< 6  - single component - 8 bit
        AMF_SURFACE_YUV420P,            ///< 7  - planar 4:2:0 Y width x height + U width/2 x height/2 + V width/2 x height/2 - 8 bit per component
        AMF_SURFACE_U8V8,               ///< 8  - packed double component - 8 bit per component
        AMF_SURFACE_YUY2,               ///< 9  - packed 4:2:2 Byte 0=8-bit Y'0; Byte 1=8-bit Cb; Byte 2=8-bit Y'1; Byte 3=8-bit Cr
        AMF_SURFACE_P010,               ///< 10 - planar 4:2:0 Y width x height + packed UV width/2 x height/2 - 10 bit per component (16 allocated, upper 10 bits are used)
        AMF_SURFACE_RGBA_F16,           ///< 11 - packed 4:4:4 - 16 bit per component float
        AMF_SURFACE_UYVY,               ///< 12 - packed 4:2:2 the similar to YUY2 but Y and UV swapped: Byte 0=8-bit Cb; Byte 1=8-bit Y'0; Byte 2=8-bit Cr Byte 3=8-bit Y'1; (used the same DX/CL/Vulkan storage as YUY2)
        AMF_SURFACE_R10G10B10A2,        ///< 13 - packed 4:4:4 to 4 bytes, 10 bit per RGB component, 2 bits per A 
        AMF_SURFACE_Y210,               ///< 14 - packed 4:2:2 - Word 0=10-bit Y'0; Word 1=10-bit Cb; Word 2=10-bit Y'1; Word 3=10-bit Cr
        AMF_SURFACE_AYUV,               ///< 15 - packed 4:4:4 - 8 bit per component YUVA
        AMF_SURFACE_Y410,               ///< 16 - packed 4:4:4 - 10 bit per YUV component, 2 bits per A, AVYU 
        AMF_SURFACE_Y416,               ///< 16 - packed 4:4:4 - 16 bit per component 4 bytes, AVYU
        AMF_SURFACE_GRAY32,             ///< 17 - single component - 32 bit

        AMF_SURFACE_FIRST = AMF_SURFACE_NV12,
        AMF_SURFACE_LAST = AMF_SURFACE_GRAY32
    } AMF_SURFACE_FORMAT;
    //----------------------------------------------------------------------------------------------
    // AMF_SURFACE_USAGE translates to D3D11_BIND_FLAG or VkImageUsageFlags
    // bit mask
    //----------------------------------------------------------------------------------------------
    typedef enum AMF_SURFACE_USAGE_BITS
    {                                                       // D3D11                        D3D12                                       Vulkan 
        AMF_SURFACE_USAGE_DEFAULT           = 0x80000000,   // will apply default           D3D12_RESOURCE_FLAG_NONE                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT| VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
        AMF_SURFACE_USAGE_NONE              = 0x00000000,   // 0,                           D3D12_RESOURCE_FLAG_NONE,                   0
        AMF_SURFACE_USAGE_SHADER_RESOURCE   = 0x00000001,   // D3D11_BIND_SHADER_RESOURCE,	D3D12_RESOURCE_FLAG_NONE					VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
        AMF_SURFACE_USAGE_RENDER_TARGET     = 0x00000002,   // D3D11_BIND_RENDER_TARGET,    D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        AMF_SURFACE_USAGE_UNORDERED_ACCESS  = 0x00000004,   // D3D11_BIND_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
        AMF_SURFACE_USAGE_TRANSFER_SRC      = 0x00000008,   //								D3D12_RESOURCE_FLAG_NONE    	            VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        AMF_SURFACE_USAGE_TRANSFER_DST      = 0x00000010,   //								D3D12_RESOURCE_FLAG_NONE		            VK_IMAGE_USAGE_TRANSFER_DST_BIT
        AMF_SURFACE_USAGE_LINEAR            = 0x00000020    
    } AMF_SURFACE_USAGE_BITS;
    typedef amf_flags AMF_SURFACE_USAGE;
    //----------------------------------------------------------------------------------------------

#if defined(_WIN32)
    AMF_WEAK GUID  AMFFormatGUID = { 0x8cd592d0, 0x8063, 0x4af8, 0xa7, 0xd0, 0x32, 0x5b, 0xc5, 0xf7, 0x48, 0xab}; // UINT(AMF_SURFACE_FORMAT), default - AMF_SURFACE_UNKNOWN; to be set on ID3D11Texture2D objects when used natively (i.e. force UYVY on DXGI_FORMAT_YUY2 texture)
#endif

    //----------------------------------------------------------------------------------------------
    // frame type
    //----------------------------------------------------------------------------------------------
    typedef enum AMF_FRAME_TYPE
    {
        // flags
        AMF_FRAME_STEREO_FLAG                           = 0x10000000,
        AMF_FRAME_LEFT_FLAG                             = AMF_FRAME_STEREO_FLAG | 0x20000000,
        AMF_FRAME_RIGHT_FLAG                            = AMF_FRAME_STEREO_FLAG | 0x40000000,
        AMF_FRAME_BOTH_FLAG                             = AMF_FRAME_LEFT_FLAG | AMF_FRAME_RIGHT_FLAG,
        AMF_FRAME_INTERLEAVED_FLAG                      = 0x01000000,
        AMF_FRAME_FIELD_FLAG                            = 0x02000000,
        AMF_FRAME_EVEN_FLAG                             = 0x04000000,
        AMF_FRAME_ODD_FLAG                              = 0x08000000,

        // values
        AMF_FRAME_UNKNOWN                               =-1,
        AMF_FRAME_PROGRESSIVE                           = 0,

        AMF_FRAME_INTERLEAVED_EVEN_FIRST                = AMF_FRAME_INTERLEAVED_FLAG | AMF_FRAME_EVEN_FLAG,
        AMF_FRAME_INTERLEAVED_ODD_FIRST                 = AMF_FRAME_INTERLEAVED_FLAG | AMF_FRAME_ODD_FLAG,
        AMF_FRAME_FIELD_SINGLE_EVEN                     = AMF_FRAME_FIELD_FLAG | AMF_FRAME_EVEN_FLAG,
        AMF_FRAME_FIELD_SINGLE_ODD                      = AMF_FRAME_FIELD_FLAG | AMF_FRAME_ODD_FLAG,

        AMF_FRAME_STEREO_LEFT                           = AMF_FRAME_LEFT_FLAG,
        AMF_FRAME_STEREO_RIGHT                          = AMF_FRAME_RIGHT_FLAG,
        AMF_FRAME_STEREO_BOTH                           = AMF_FRAME_BOTH_FLAG,

        AMF_FRAME_INTERLEAVED_EVEN_FIRST_STEREO_LEFT    = AMF_FRAME_INTERLEAVED_FLAG | AMF_FRAME_EVEN_FLAG | AMF_FRAME_LEFT_FLAG,
        AMF_FRAME_INTERLEAVED_EVEN_FIRST_STEREO_RIGHT   = AMF_FRAME_INTERLEAVED_FLAG | AMF_FRAME_EVEN_FLAG | AMF_FRAME_RIGHT_FLAG,
        AMF_FRAME_INTERLEAVED_EVEN_FIRST_STEREO_BOTH    = AMF_FRAME_INTERLEAVED_FLAG | AMF_FRAME_EVEN_FLAG | AMF_FRAME_BOTH_FLAG,

        AMF_FRAME_INTERLEAVED_ODD_FIRST_STEREO_LEFT     = AMF_FRAME_INTERLEAVED_FLAG | AMF_FRAME_ODD_FLAG | AMF_FRAME_LEFT_FLAG,
        AMF_FRAME_INTERLEAVED_ODD_FIRST_STEREO_RIGHT    = AMF_FRAME_INTERLEAVED_FLAG | AMF_FRAME_ODD_FLAG | AMF_FRAME_RIGHT_FLAG,
        AMF_FRAME_INTERLEAVED_ODD_FIRST_STEREO_BOTH     = AMF_FRAME_INTERLEAVED_FLAG | AMF_FRAME_ODD_FLAG | AMF_FRAME_BOTH_FLAG,
    } AMF_FRAME_TYPE;

    //----------------------------------------------------------------------------------------------
    // AMFSurfaceObserver interface - callback; is called before internal release resources.
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMFSurface;
    class AMF_NO_VTABLE AMFSurfaceObserver
    {
    public:
        virtual void AMF_STD_CALL OnSurfaceDataRelease(AMFSurface* pSurface) = 0;
    };
#else // #if defined(__cplusplus)
    typedef struct AMFSurface AMFSurface;
    typedef struct AMFSurfaceObserver AMFSurfaceObserver;

    typedef struct AMFSurfaceObserverVtbl
    {
        void                (AMF_STD_CALL *OnSurfaceDataRelease)(AMFSurfaceObserver* pThis, AMFSurface* pSurface);
    } AMFSurfaceObserverVtbl;

    struct AMFSurfaceObserver
    {
        const AMFSurfaceObserverVtbl *pVtbl;
    };
#endif // #if defined(__cplusplus)

    //----------------------------------------------------------------------------------------------
    // AMFSurface interface
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFSurface : public AMFData
    {
    public:
        AMF_DECLARE_IID(0x3075dbe3, 0x8718, 0x4cfa, 0x86, 0xfb, 0x21, 0x14, 0xc0, 0xa5, 0xa4, 0x51)

        virtual AMF_SURFACE_FORMAT  AMF_STD_CALL GetFormat() = 0;

        // do not store planes outside. should be used together with Surface
        virtual amf_size            AMF_STD_CALL GetPlanesCount() = 0;
        virtual AMFPlane*           AMF_STD_CALL GetPlaneAt(amf_size index) = 0;
        virtual AMFPlane*           AMF_STD_CALL GetPlane(AMF_PLANE_TYPE type) = 0;

        virtual AMF_FRAME_TYPE      AMF_STD_CALL GetFrameType() = 0;
        virtual void                AMF_STD_CALL SetFrameType(AMF_FRAME_TYPE type) = 0;

        virtual AMF_RESULT          AMF_STD_CALL SetCrop(amf_int32 x,amf_int32 y, amf_int32 width, amf_int32 height) = 0;
        virtual AMF_RESULT          AMF_STD_CALL CopySurfaceRegion(AMFSurface* pDest, amf_int32 dstX, amf_int32 dstY, amf_int32 srcX, amf_int32 srcY, amf_int32 width, amf_int32 height) = 0;

        // Observer management
        virtual void                AMF_STD_CALL AddObserver(AMFSurfaceObserver* pObserver) = 0;
        virtual void                AMF_STD_CALL RemoveObserver(AMFSurfaceObserver* pObserver) = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFSurface> AMFSurfacePtr;
    //----------------------------------------------------------------------------------------------
#else // #if defined(__cplusplus)
        AMF_DECLARE_IID(AMFSurface, 0x3075dbe3, 0x8718, 0x4cfa, 0x86, 0xfb, 0x21, 0x14, 0xc0, 0xa5, 0xa4, 0x51)
    typedef struct AMFSurfaceVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFSurface* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFSurface* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFSurface* pThis, const struct AMFGuid *interfaceID, void** ppInterface);

        // AMFPropertyStorage interface
        AMF_RESULT          (AMF_STD_CALL *SetProperty)(AMFSurface* pThis, const wchar_t* name, AMFVariantStruct value);
        AMF_RESULT          (AMF_STD_CALL *GetProperty)(AMFSurface* pThis, const wchar_t* name, AMFVariantStruct* pValue);
        amf_bool            (AMF_STD_CALL *HasProperty)(AMFSurface* pThis, const wchar_t* name);
        amf_size            (AMF_STD_CALL *GetPropertyCount)(AMFSurface* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyAt)(AMFSurface* pThis, amf_size index, wchar_t* name, amf_size nameSize, AMFVariantStruct* pValue);
        AMF_RESULT          (AMF_STD_CALL *Clear)(AMFSurface* pThis);
        AMF_RESULT          (AMF_STD_CALL *AddTo)(AMFSurface* pThis, AMFPropertyStorage* pDest, amf_bool overwrite, amf_bool deep);
        AMF_RESULT          (AMF_STD_CALL *CopyTo)(AMFSurface* pThis, AMFPropertyStorage* pDest, amf_bool deep);
        void                (AMF_STD_CALL *AddObserver)(AMFSurface* pThis, AMFPropertyStorageObserver* pObserver);
        void                (AMF_STD_CALL *RemoveObserver)(AMFSurface* pThis, AMFPropertyStorageObserver* pObserver);

        // AMFData interface

        AMF_MEMORY_TYPE     (AMF_STD_CALL *GetMemoryType)(AMFSurface* pThis);

        AMF_RESULT          (AMF_STD_CALL *Duplicate)(AMFSurface* pThis, AMF_MEMORY_TYPE type, AMFData** ppData);
        AMF_RESULT          (AMF_STD_CALL *Convert)(AMFSurface* pThis, AMF_MEMORY_TYPE type); // optimal interop if possilble. Copy through host memory if needed
        AMF_RESULT          (AMF_STD_CALL *Interop)(AMFSurface* pThis, AMF_MEMORY_TYPE type); // only optimal interop if possilble. No copy through host memory for GPU objects

        AMF_DATA_TYPE       (AMF_STD_CALL *GetDataType)(AMFSurface* pThis);

        amf_bool            (AMF_STD_CALL *IsReusable)(AMFSurface* pThis);

        void                (AMF_STD_CALL *SetPts)(AMFSurface* pThis, amf_pts pts);
        amf_pts             (AMF_STD_CALL *GetPts)(AMFSurface* pThis);
        void                (AMF_STD_CALL *SetDuration)(AMFSurface* pThis, amf_pts duration);
        amf_pts             (AMF_STD_CALL *GetDuration)(AMFSurface* pThis);

        // AMFSurface interface

        AMF_SURFACE_FORMAT  (AMF_STD_CALL *GetFormat)(AMFSurface* pThis);

        // do not store planes outside. should be used together with Surface
        amf_size            (AMF_STD_CALL *GetPlanesCount)(AMFSurface* pThis);
        AMFPlane*           (AMF_STD_CALL *GetPlaneAt)(AMFSurface* pThis, amf_size index);
        AMFPlane*           (AMF_STD_CALL *GetPlane)(AMFSurface* pThis, AMF_PLANE_TYPE type);

        AMF_FRAME_TYPE      (AMF_STD_CALL *GetFrameType)(AMFSurface* pThis);
        void                (AMF_STD_CALL *SetFrameType)(AMFSurface* pThis, AMF_FRAME_TYPE type);

        AMF_RESULT          (AMF_STD_CALL *SetCrop)(AMFSurface* pThis, amf_int32 x,amf_int32 y, amf_int32 width, amf_int32 height);
        AMF_RESULT          (AMF_STD_CALL *CopySurfaceRegion)(AMFSurface* pThis, AMFSurface* pDest, amf_int32 dstX, amf_int32 dstY, amf_int32 srcX, amf_int32 srcY, amf_int32 width, amf_int32 height);


        // Observer management
        void                (AMF_STD_CALL *AddObserver_Surface)(AMFSurface* pThis, AMFSurfaceObserver* pObserver);
        void                (AMF_STD_CALL *RemoveObserver_Surface)(AMFSurface* pThis, AMFSurfaceObserver* pObserver);

    } AMFSurfaceVtbl;

    struct AMFSurface
    {
        const AMFSurfaceVtbl *pVtbl;
    };
#endif // #if defined(__cplusplus)
#if defined(__cplusplus)
}
#endif
#if defined(_MSC_VER)
    #pragma warning( pop )
#endif
#endif //#ifndef AMF_Surface_h
