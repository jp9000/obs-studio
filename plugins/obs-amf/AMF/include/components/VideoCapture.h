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
// ZCamLive  interface declaration
//-------------------------------------------------------------------------------------------------
#ifndef AMF_VideoCapture_h
#define AMF_VideoCapture_h

#pragma once

#define VIDEOCAP_DEVICE_COUNT           L"VideoCapDeviceCount"    // amf_int64, (default=2), number of video capture devices
#define VIDEOCAP_DEVICE_NAME            L"VideoCapDeviceName"     // WString, (default=""), name of the video capture device
#define VIDEOCAP_DEVICE_ACTIVE          L"VideoCapDeviceActive"   // WString, (default=""), name of the selected video capture device

#define VIDEOCAP_CODEC                   L"CodecID"               // WString (default = "AMFVideoDecoderUVD_H264_AVC"), UVD codec ID
#define VIDEOCAP_FRAMESIZE               L"FrameSize"             // AMFSize, (default=AMFConstructSize(1920, 1080)), frame size in pixels

extern "C"
{
    AMF_RESULT AMF_CDECL_CALL AMFCreateComponentVideoCapture(amf::AMFContext* pContext, amf::AMFComponentEx** ppComponent);
}
#endif // AMF_VideoCapture_h