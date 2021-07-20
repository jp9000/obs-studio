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
// interface declaration;  Ambisonic to Stereo Renderer
//-------------------------------------------------------------------------------------------------

#ifndef AMF_Ambisonic2SRenderer_h
#define AMF_Ambisonic2SRenderer_h
#pragma once

#include "public/include/components/Component.h"

#define AMFAmbisonic2SRendererHW L"AMFAmbisonic2SRenderer"

enum AMF_AMBISONIC2SRENDERER_MODE_ENUM 
{
    AMF_AMBISONIC2SRENDERER_MODE_SIMPLE            = 0,
    AMF_AMBISONIC2SRENDERER_MODE_HRTF_AMD0         = 1,
    AMF_AMBISONIC2SRENDERER_MODE_HRTF_MIT1         = 2,
};


// static properties 
#define AMF_AMBISONIC2SRENDERER_IN_AUDIO_SAMPLE_RATE        L"InSampleRate"         // amf_int64 (default = 0)
#define AMF_AMBISONIC2SRENDERER_IN_AUDIO_CHANNELS           L"InChannels"           // amf_int64 (only = 4)
#define AMF_AMBISONIC2SRENDERER_IN_AUDIO_SAMPLE_FORMAT      L"InSampleFormat"       // amf_int64(AMF_AUDIO_FORMAT) (default = AMFAF_FLTP)

#define AMF_AMBISONIC2SRENDERER_OUT_AUDIO_CHANNELS          L"OutChannels"          // amf_int64 (only = 2 - stereo)
#define AMF_AMBISONIC2SRENDERER_OUT_AUDIO_SAMPLE_FORMAT     L"OutSampleFormat"      // amf_int64(AMF_AUDIO_FORMAT) (only = AMFAF_FLTP)
#define AMF_AMBISONIC2SRENDERER_OUT_AUDIO_CHANNEL_LAYOUT    L"OutChannelLayout"     // amf_int64 (only = 3 - defalut stereo L R)

#define AMF_AMBISONIC2SRENDERER_MODE                        L"StereoMode"               //TODO: AMF_AMBISONIC2SRENDERER_MODE_ENUM(default=AMF_AMBISONIC2SRENDERER_MODE_HRTF)


// dynamic properties
#define AMF_AMBISONIC2SRENDERER_W                           L"w"                        //amf_int64 (default=0)
#define AMF_AMBISONIC2SRENDERER_X                           L"x"                        //amf_int64 (default=1)
#define AMF_AMBISONIC2SRENDERER_Y                           L"y"                        //amf_int64 (default=2)
#define AMF_AMBISONIC2SRENDERER_Z                           L"z"                        //amf_int64 (default=3)

#define AMF_AMBISONIC2SRENDERER_THETA                       L"Theta"                    //double (default=0.0)
#define AMF_AMBISONIC2SRENDERER_PHI                         L"Phi"                      //double (default=0.0)
#define AMF_AMBISONIC2SRENDERER_RHO                         L"Rho"                      //double (default=0.0)

extern "C"
{
    AMF_RESULT AMF_CDECL_CALL AMFCreateComponentAmbisonic(amf::AMFContext* pContext, void* reserved, amf::AMFComponent** ppComponent);
}
#endif //#ifndef AMF_Ambisonic2SRenderer_h
