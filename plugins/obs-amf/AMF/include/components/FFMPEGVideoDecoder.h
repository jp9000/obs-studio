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
// VideoDecoderFFMPEG  interface declaration
//-------------------------------------------------------------------------------------------------
#ifndef AMF_VideoDecoderFFMPEG_h
#define AMF_VideoDecoderFFMPEG_h

#pragma once

#define FFMPEG_VIDEO_DECODER    L"VideoDecoderFFMPEG"

#define VIDEO_DECODER_ENABLE_DECODING      L"EnableDecoding"   // bool (default = true) - if false, component will not decode anything
#define VIDEO_DECODER_CODEC_ID             L"CodecID"          // amf_int64 (AMF_STREAM_CODEC_ID_ENUM) codec ID
#define VIDEO_DECODER_EXTRA_DATA           L"ExtraData"        // interface to AMFBuffer
#define VIDEO_DECODER_RESOLUTION           L"Resolution"       // AMFSize 
#define VIDEO_DECODER_BITRATE              L"BitRate"          // amf_int64 (default = 0)
#define VIDEO_DECODER_FRAMERATE            L"FrameRate"        // AMFRate
#define VIDEO_DECODER_SEEK_POSITION        L"SeekPosition"     // amf_int64 (default = 0)

#define VIDEO_DECODER_COLOR_TRANSFER_CHARACTERISTIC L"ColorTransferChar"    // amf_int64(AMF_COLOR_TRANSFER_CHARACTERISTIC_ENUM); default = AMF_COLOR_TRANSFER_CHARACTERISTIC_UNDEFINED, ISO/IEC 23001-8_2013 § 7.2

#endif //#ifndef AMF_VideoDecoderFFMPEG_h
