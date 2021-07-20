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
// Copyright (c) 2020 Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef AMFPreProcessing_h
#define AMFPreProcessing_h

#pragma once

#define AMFPreProcessing L"AMFPreProcessing"


// PA object properties
#define AMF_PP_ENGINE_TYPE                   L"PPEngineType"                   // AMF_MEMORY_TYPE (Host, DX11, OPENCL default : OPENCL)"        - determines how the object is initialized and what kernels to use
                                                                               //                                                                 by default it is OpenCL (Host, DX11 and OpenCL are currently available)

#define AMF_PP_ADAPTIVE_FILTER_STRENGTH      L"PPAdaptiveFilterStrength"       // int       (default : 4)                                       - strength:    0 - 10: the higher the value, the stronger the filtering
#define AMF_PP_ADAPTIVE_FILTER_SENSITIVITY   L"PPAdaptiveFilterSensitivity"	   // int       (default : 4)                                       - sensitivity: 0 - 10: the lower the value, the more sensitive to edge (preserve more details)



#endif //#ifndef AMFPreProcessing_h
