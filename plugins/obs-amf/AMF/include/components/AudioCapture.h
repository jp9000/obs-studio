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
// Audio session  interface declaration
//-------------------------------------------------------------------------------------------------
#ifndef AMF_AudioCapture_h
#define AMF_AudioCapture_h

#pragma once

#include "public/include/components/Component.h"

// Set to capture from either a microphone or desktop
#define AUDIOCAPTURE_SOURCE                L"AudioCaptureSource"           // amf_bool true for microphone, false for desktop;

// In the case of capturing a microphone, the AUDIOCAPTURE_DEVICE_ACTIVE property
// can be set to -1 so that the active input devices are looked up. If the initialization
// is successful then the AUDIOCAPTURE_DEVICE_NAME and AUDIOCAPTURE_DEVICE_COUNT
// properties will be set.
#define AUDIOCAPTURE_DEVICE_ACTIVE          L"AudioCaptureDeviceActive"   // amf_int64
#define AUDIOCAPTURE_DEVICE_COUNT           L"AudioCaptureDeviceCount"    // amf_int64
#define AUDIOCAPTURE_DEVICE_NAME            L"AudioCaptureDeviceName"     // String

// Codec used for audio capture
#define AUDIOCAPTURE_CODEC                  L"AudioCaptureCodec"           // amf_int64, AV_CODEC_ID_PCM_F32LE
// Sample rate used for audio capture
#define AUDIOCAPTURE_SAMPLERATE             L"AudioCaptureSampleRate"      // amf_int64, 44100 in samples
// Sample count used for audio capture
#define AUDIOCAPTURE_SAMPLES                L"AudioCaptureSampleCount"     // amf_int64, 1024
// Bitrate used for audio capture
#define AUDIOCAPTURE_BITRATE                L"AudioCaptureBitRate"         // amf_int64, in bits
// Channel count used for audio capture
#define AUDIOCAPTURE_CHANNELS               L"AudioCaptureChannelCount"    // amf_int64, 2
// Format used for audio capture
#define AUDIOCAPTURE_FORMAT                 L"AudioCaptureFormat"          // amf_int64, AMFAF_U8
// Block alignment
#define AUDIOCAPTURE_BLOCKALIGN             L"AudioCaptureBlockAlign"      // amf_int64, bytes
// Audio frame size
#define AUDIOCAPTURE_FRAMESIZE              L"AudioCaptureFrameSize"       // amf_int64, bytes
// Audio low latency state
#define AUDIOCAPTURE_LOWLATENCY             L"AudioCaptureLowLatency"      // amf_int64;

// Optional interface that provides current time
#define AUDIOCAPTURE_CURRENT_TIME_INTERFACE	L"CurrentTimeInterface"        // interface to current time object

extern "C"
{
	// Component that allows the recording of inputs such as microphones or the audio that is being
	// rendered.  The direction that is captured is controlled by the AUDIOCAPTURE_CAPTURE property
	//
	AMF_RESULT AMF_CDECL_CALL AMFCreateComponentAudioCapture(amf::AMFContext* pContext, amf::AMFComponent** ppComponent);
}

#endif // #ifndef AMF_AudioCapture_h
