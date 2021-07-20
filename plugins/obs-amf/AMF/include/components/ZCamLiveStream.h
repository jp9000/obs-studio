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
#ifndef AMF_ZCamLiveStream_h
#define AMF_ZCamLiveStream_h

#pragma once
#define ZCAMLIVE_STREAMCOUNT             L"StreamCount"          // amf_int64 (default = 4),  number of streams
#define ZCAMLIVE_VIDEO_FRAMESIZE         L"FrameSize"            // AMFSize   (default = AMFConstructSize(2704, 1520)), frame size
#define ZCAMLIVE_VIDEO_FRAMERATE         L"FrameRate"            // AMFRate   (default = 30.0), video frame rate
#define ZCAMLIVE_VIDEO_BIT_RATE          L"BitRate"              // amf_int64 (default = 3000000), video bitrate
#define ZCAMLIVE_STREAM_ACTIVE_CAMERA    L"ActiveCamera"         // amf_int64 (default = -1, all the cameras), the index of the camera to capture
#define ZCAMLIVE_STREAM_FRAMECOUNT       L"FrameCount"           // amf_int64 (default = 0), number of frames captured
#define ZCAMLIVE_CODEC_ID                L"CodecID"              // WString (default = "AMFVideoDecoderUVD_H264_AVC"), UVD codec ID
#define ZCAMLIVE_VIDEO_MODE              L"VideoMode"            // Enum (default = 0, 2K7P30), ZCam mode
#define ZCAMLIVE_AUDIO_MODE              L"AudioMode"            // Enum (default = 0, Silent) - Audio mode
#define ZCAMLIVE_LOWLATENCY              L"LowLatency"           // amf_int64 (default = 1, LowLatency), low latency flag
#define ZCAMLIVE_IP_0                    L"ZCamIP_00"               // WString, IP address of the #1 stream, default "10.98.32.1"
#define ZCAMLIVE_IP_1                    L"ZCamIP_01"             // WString, IP address of the #2 stream, default "10.98.32.2"
#define ZCAMLIVE_IP_2                    L"ZCamIP_02"             // WString, IP address of the #3 stream, default "10.98.32.3"
#define ZCAMLIVE_IP_3                    L"ZCamIP_03"             // WString, IP address of the #4 stream, default "10.98.32.4"

//Camera live capture Mode
enum CAMLIVE_MODE_ENUM
{
    CAMLIVE_MODE_ZCAM_1080P24 = 0,  //1920x1080, 24FPS
    CAMLIVE_MODE_ZCAM_1080P30,      //1920x1080, 30FPS
    CAMLIVE_MODE_ZCAM_1080P60,      //1920x1080, 60FPS
    CAMLIVE_MODE_ZCAM_2K7P24,       //2704x1520, 24FPS
    CAMLIVE_MODE_ZCAM_2K7P30,       //2704x1520, 24FPS
    CAMLIVE_MODE_ZCAM_2K7P60,       //2704x1520, 24FPS
    CAMLIVE_MODE_ZCAM_2544P24,      //3392x2544, 24FPS
    CAMLIVE_MODE_ZCAM_2544P30,      //3392x2544, 24FPS
    CAMLIVE_MODE_ZCAM_2544P60,      //3392x2544, 24FPS
    CAMLIVE_MODE_THETAS,            //Ricoh TheataS
    CAMLIVE_MODE_THETAV,            //Ricoh TheataV
    CAMLIVE_MODE_INVALID = -1,
};
 
enum CAM_AUDIO_MODE_ENUM
{
    CAM_AUDIO_MODE_NONE = 0,   //None
    CAM_AUDIO_MODE_SILENT,     //Silent audio
    CAM_AUDIO_MODE_CAMERA      //Capture from camera, not supported yet
};

extern "C"
{
    AMF_RESULT AMF_CDECL_CALL AMFCreateComponentZCamLiveStream(amf::AMFContext* pContext, amf::AMFComponentEx** ppComponent);
}
#endif // AMF_ZCamLiveStream_h