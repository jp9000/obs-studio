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

#ifndef AMF_Plane_h
#define AMF_Plane_h
#pragma once

#include "Interface.h"

#if defined(__cplusplus)
namespace amf
{
#endif
    //---------------------------------------------------------------------------------------------
    typedef enum AMF_PLANE_TYPE
    {
        AMF_PLANE_UNKNOWN       = 0,
        AMF_PLANE_PACKED        = 1,             // for all packed formats: BGRA, YUY2, etc
        AMF_PLANE_Y             = 2,
        AMF_PLANE_UV            = 3,
        AMF_PLANE_U             = 4,
        AMF_PLANE_V             = 5,
    } AMF_PLANE_TYPE;
    //---------------------------------------------------------------------------------------------
    // AMFPlane interface
    //---------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFPlane : public AMFInterface
    {
    public:
        AMF_DECLARE_IID(0xbede1aa6, 0xd8fa, 0x4625, 0x94, 0x65, 0x6c, 0x82, 0xc4, 0x37, 0x71, 0x2e)

        virtual AMF_PLANE_TYPE      AMF_STD_CALL GetType() = 0;
        virtual void*               AMF_STD_CALL GetNative() = 0;
        virtual amf_int32           AMF_STD_CALL GetPixelSizeInBytes() = 0;
        virtual amf_int32           AMF_STD_CALL GetOffsetX() = 0;
        virtual amf_int32           AMF_STD_CALL GetOffsetY() = 0;
        virtual amf_int32           AMF_STD_CALL GetWidth() = 0;
        virtual amf_int32           AMF_STD_CALL GetHeight() = 0;
        virtual amf_int32           AMF_STD_CALL GetHPitch() = 0;
        virtual amf_int32           AMF_STD_CALL GetVPitch() = 0;
        virtual bool                AMF_STD_CALL IsTiled() = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFPlane> AMFPlanePtr;
    //----------------------------------------------------------------------------------------------
#else // #if defined(__cplusplus)
    AMF_DECLARE_IID(AMFPlane, 0xbede1aa6, 0xd8fa, 0x4625, 0x94, 0x65, 0x6c, 0x82, 0xc4, 0x37, 0x71, 0x2e)
    typedef struct AMFPlane AMFPlane;
    typedef struct AMFPlaneVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFPlane* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFPlane* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFPlane* pThis, const struct AMFGuid *interfaceID, void** ppInterface);

        // AMFPlane interface
        AMF_PLANE_TYPE      (AMF_STD_CALL *GetType)(AMFPlane* pThis);
        void*               (AMF_STD_CALL *GetNative)(AMFPlane* pThis);
        amf_int32           (AMF_STD_CALL *GetPixelSizeInBytes)(AMFPlane* pThis);
        amf_int32           (AMF_STD_CALL *GetOffsetX)(AMFPlane* pThis);
        amf_int32           (AMF_STD_CALL *GetOffsetY)(AMFPlane* pThis);
        amf_int32           (AMF_STD_CALL *GetWidth)(AMFPlane* pThis);
        amf_int32           (AMF_STD_CALL *GetHeight)(AMFPlane* pThis);
        amf_int32           (AMF_STD_CALL *GetHPitch)(AMFPlane* pThis);
        amf_int32           (AMF_STD_CALL *GetVPitch)(AMFPlane* pThis);
        amf_bool            (AMF_STD_CALL *IsTiled)(AMFPlane* pThis);

    } AMFPlaneVtbl;

    struct AMFPlane
    {
        const AMFPlaneVtbl *pVtbl;
    };
#endif // #if defined(__cplusplus)

#if defined(__cplusplus)
} // namespace amf
#endif

#endif //#ifndef AMF_Plane_h
