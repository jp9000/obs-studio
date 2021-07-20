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
// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
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
// Desktop duplication interface declaration
//-------------------------------------------------------------------------------------------------

#ifndef AMF_DisplayCapture_h
#define AMF_DisplayCapture_h
#pragma once

#include "Component.h"

extern "C"
{
    // To create capture component with Desktop Duplication API use this function
    AMF_RESULT AMF_CDECL_CALL AMFCreateComponentDisplayCapture(amf::AMFContext* pContext, void* reserved, amf::AMFComponent** ppComponent);
}

// Static properties
//
// 
#define AMF_DISPLAYCAPTURE_MONITOR_INDEX			L"MonitorIndex"             // amf_int64, default = 0, Index of the display monitor; is determined by using EnumAdapters() in DXGI.
#define AMF_DISPLAYCAPTURE_FRAMERATE				L"FrameRate"                // AMFRate,  default = (0, 1) Capture frame rate, if 0 - capture rate will be driven by flip off the app or dwm
#define AMF_DISPLAYCAPTURE_CURRENT_TIME_INTERFACE	L"CurrentTimeInterface"     // Optional interface object for getting current time.
#define AMF_DISPLAYCAPTURE_FORMAT					L"CurrentFormat"            // amf_int64(AMF_SURFACE_FORMAT) Capture format - read-only
#define AMF_DISPLAYCAPTURE_RESOLUTION			    L"Resolution"               // AMFSize - screen resolution - read-only
#define AMF_DISPLAYCAPTURE_DUPLICATEOUTPUT		    L"DuplicateOutput"          // amf_bool, default = false, output AMF surface is a copy of captured 
#define AMF_DISPLAYCAPTURE_DESKTOP_RECT             L"DesktopRect"              // AMFRect - rect of the capture desktop - read-only
#define AMF_DISPLAYCAPTURE_ENABLE_DIRTY_RECTS       L"EnableDirtyRects"         // amf_bool, default = false, enable dirty rectangles attached to output as AMF_DISPLAYCAPTURE_DIRTY_RECTS
#define AMF_DISPLAYCAPTURE_DRAW_DIRTY_RECTS         L"DrawDirtyRects"           // amf_bool, default = false, copies capture output and draws dirty rectangles with red - for debugging only

// can be set on output AMFSurface
#define AMF_DISPLAYCAPTURE_DIRTY_RECTS              L"DirtyRects"               // AMFInterface*(AMFBuffer*) - array of AMFRect

#endif // #ifndef AMF_DisplayCapture_h
