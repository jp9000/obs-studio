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
// AudioEncoderFFMPEG  interface declaration
//-------------------------------------------------------------------------------------------------
#ifndef AMF_AudioEncoderFFMPEG_h
#define AMF_AudioEncoderFFMPEG_h

#pragma once


#define FFMPEG_AUDIO_ENCODER    L"AudioEncoderFFMPEG"


#define AUDIO_ENCODER_ENABLE_DEBUGGING              L"EnableDebug"                  // bool (default = false) - trace some debug information if set to true
#define AUDIO_ENCODER_ENABLE_ENCODING               L"EnableEncoding"               // bool (default = true) - if false, component will not encode anything
#define AUDIO_ENCODER_AUDIO_CODEC_ID                L"CodecID"                      // amf_int64 (default = AV_CODEC_ID_NONE) - FFMPEG codec ID

#define AUDIO_ENCODER_IN_AUDIO_SAMPLE_RATE          L"In_SampleRate"                // amf_int64 (default = 44100)
#define AUDIO_ENCODER_IN_AUDIO_CHANNELS             L"In_Channels"                  // amf_int64 (default = 2)
#define AUDIO_ENCODER_IN_AUDIO_SAMPLE_FORMAT        L"In_SampleFormat"              // amf_int64 (default = AMFAF_S16)  (AMF_AUDIO_FORMAT)
#define AUDIO_ENCODER_IN_AUDIO_CHANNEL_LAYOUT       L"In_ChannelLayout"             // amf_int64 (default = 3)
#define AUDIO_ENCODER_IN_AUDIO_BLOCK_ALIGN          L"In_BlockAlign"                // amf_int64 (default = 0)

#define AUDIO_ENCODER_OUT_AUDIO_BIT_RATE            L"Out_BitRate"                  // amf_int64 (default = 128000)
#define AUDIO_ENCODER_OUT_AUDIO_EXTRA_DATA          L"Out_ExtraData"                // interface to AMFBuffer
#define AUDIO_ENCODER_OUT_AUDIO_SAMPLE_RATE         L"Out_SampleRate"               // amf_int64 (default = 44100)
#define AUDIO_ENCODER_OUT_AUDIO_CHANNELS            L"Out_Channels"                 // amf_int64 (default = 2)
#define AUDIO_ENCODER_OUT_AUDIO_SAMPLE_FORMAT       L"Out_SampleFormat"             // amf_int64 (default = AMFAF_S16)  (AMF_AUDIO_FORMAT)
#define AUDIO_ENCODER_OUT_AUDIO_CHANNEL_LAYOUT      L"Out_ChannelLayout"            // amf_int64 (default = 0)
#define AUDIO_ENCODER_OUT_AUDIO_BLOCK_ALIGN         L"Out_BlockAlign"               // amf_int64 (default = 0)
#define AUDIO_ENCODER_OUT_AUDIO_FRAME_SIZE          L"Out_FrameSize"                // amf_int64 (default = 0)



#endif //#ifndef AMF_AudioEncoderFFMPEG_h
