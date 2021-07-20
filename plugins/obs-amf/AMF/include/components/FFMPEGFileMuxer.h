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
// MuxerFFMPEG  interface declaration
//-------------------------------------------------------------------------------------------------
#ifndef AMF_FileMuxerFFMPEG_h
#define AMF_FileMuxerFFMPEG_h

#pragma once

#define FFMPEG_MUXER L"MuxerFFMPEG"


// component properties
#define FFMPEG_MUXER_PATH                     L"Path"                     // string - the file to open
#define FFMPEG_MUXER_URL                      L"Url"                      // string - the stream url to open
#define FFMPEG_MUXER_LISTEN                   L"Listen"                   // bool (default = false)
#define FFMPEG_MUXER_ENABLE_VIDEO             L"EnableVideo"              // bool (default = true)
#define FFMPEG_MUXER_ENABLE_AUDIO             L"EnableAudio"              // bool (default = false)
#define FFMPEG_MUXER_CURRENT_TIME_INTERFACE   L"CurrentTimeInterface"
#define FFMPEG_MUXER_VIDEO_ROTATION           L"VideoRotation"            // amf_int64 (0, 90, 180, 270, default = 0)

#endif //#ifndef AMF_FileMuxerFFMPEG_h
