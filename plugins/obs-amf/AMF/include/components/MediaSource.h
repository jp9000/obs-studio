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

#ifndef AMF_MediaSource_h
#define AMF_MediaSource_h

#pragma once

#include "public/include/core/Interface.h"

namespace amf
{
    enum AMF_SEEK_TYPE
    {
        AMF_SEEK_PREV = 0,           // nearest packet before pts
        AMF_SEEK_NEXT = 1,           // nearest packet after pts
        AMF_SEEK_PREV_KEYFRAME = 2,  // nearest keyframe packet before pts
        AMF_SEEK_NEXT_KEYFRAME = 3,  // nearest keyframe packet after pts
    };

    //----------------------------------------------------------------------------------------------
    // media source interface.  
    //----------------------------------------------------------------------------------------------
    class AMFMediaSource : public AMFInterface
    {
    public:
        AMF_DECLARE_IID(0xb367695a, 0xdbd0, 0x4430, 0x95, 0x3b, 0xbc, 0x7d, 0xbd, 0x2a, 0xa7, 0x66)

        // interface
        virtual AMF_RESULT  AMF_STD_CALL Seek(amf_pts pos, AMF_SEEK_TYPE seekType, amf_int32 whichStream) = 0;
        virtual amf_pts     AMF_STD_CALL GetPosition() = 0;
        virtual amf_pts     AMF_STD_CALL GetDuration() = 0;

        virtual void        AMF_STD_CALL SetMinPosition(amf_pts pts) = 0;
        virtual amf_pts     AMF_STD_CALL GetMinPosition() = 0;
        virtual void        AMF_STD_CALL SetMaxPosition(amf_pts pts) = 0;
        virtual amf_pts     AMF_STD_CALL GetMaxPosition() = 0;

        virtual amf_uint64  AMF_STD_CALL GetFrameFromPts(amf_pts pts) = 0;
        virtual amf_pts     AMF_STD_CALL GetPtsFromFrame(amf_uint64 frame) = 0;

        virtual bool        AMF_STD_CALL SupportFramesAccess() = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFMediaSource> AMFMediaSourcePtr;
} //namespace amf

#endif //#ifndef AMF_MediaSource_h
