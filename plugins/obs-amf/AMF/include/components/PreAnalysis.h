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
// Copyright (c) 2019 Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef AMFPreAnalysis_h
#define AMFPreAnalysis_h

#pragma once

#define AMFPreAnalysis L"AMFPreAnalysis"



enum  AMF_PA_SCENE_CHANGE_DETECTION_SENSITIVITY_ENUM
{
    AMF_PA_SCENE_CHANGE_DETECTION_SENSITIVITY_LOW    = 0,
    AMF_PA_SCENE_CHANGE_DETECTION_SENSITIVITY_MEDIUM = 1,
    AMF_PA_SCENE_CHANGE_DETECTION_SENSITIVITY_HIGH   = 2
};


enum  AMF_PA_STATIC_SCENE_DETECTION_SENSITIVITY_ENUM
{	
	AMF_PA_STATIC_SCENE_DETECTION_SENSITIVITY_LOW    = 0,
	AMF_PA_STATIC_SCENE_DETECTION_SENSITIVITY_MEDIUM = 1,
	AMF_PA_STATIC_SCENE_DETECTION_SENSITIVITY_HIGH   = 2
};


enum  AMF_PA_ACTIVITY_TYPE_ENUM
{
    AMF_PA_ACTIVITY_Y   = 0,
    AMF_PA_ACTIVITY_YUV = 1
};


enum  AMF_PA_CAQ_STRENGTH_ENUM
{
    AMF_PA_CAQ_STRENGTH_LOW    = 0,
    AMF_PA_CAQ_STRENGTH_MEDIUM = 1,
    AMF_PA_CAQ_STRENGTH_HIGH   = 2
};



// PA object properties
#define AMF_PA_ENGINE_TYPE                          L"PAEngineType"                         // AMF_MEMORY_TYPE (Host, DX11, OPENCL, Auto default : UNKNOWN (Auto))" - determines how the object is initialized and what kernels to use
                                                                                            //                                                                        by default it is Auto (DX11 and OpenCL are currently available)

#define AMF_PA_SCENE_CHANGE_DETECTION_ENABLE        L"PASceneChangeDetectionEnable"         // bool       (default : True)                                          - Enable Scene Change Detection GPU algorithm
#define AMF_PA_SCENE_CHANGE_DETECTION_SENSITIVITY   L"PASceneChangeDetectionSensitivity"	// AMF_PA_SCENE_CHANGE_DETECTION_SENSITIVITY_ENUM (default : Medium)    - Scene Change Detection Sensitivity
#define AMF_PA_STATIC_SCENE_DETECTION_ENABLE        L"PAStaticSceneDetectionEnable"         // bool       (default : True)                                          - Enable Skip Detection GPU algorithm
#define AMF_PA_STATIC_SCENE_DETECTION_SENSITIVITY   L"PAStaticSceneDetectionSensitivity"	// AMF_PA_STATIC_SCENE_DETECTION_SENSITIVITY_ENUM (default : High)      - Allowable absolute difference between pixels (sample counts)
#define AMF_PA_ACTIVITY_TYPE                        L"PAActivityType"                       // AMF_PA_ACTIVITY_TYPE_ENUM (default : Calculate on Y)                 - Block activity calculation mode



///////////////////////////////////////////
// the following properties are available 
// only through the Encoder - trying to 
// access/set them when PA is standalone
// will fail


#define AMF_PA_INITIAL_QP_AFTER_SCENE_CHANGE        L"PAInitialQPAfterSceneChange"          // amf_uint64 (default : 0)           Values: [0, 51]                   - Base QP to be used immediately after scene change. If this value is not set, PA will choose a proper QP value
#define AMF_PA_MAX_QP_BEFORE_FORCE_SKIP             L"PAMaxQPBeforeForceSkip"               // amf_uint64 (default : 35)          Values: [0, 51]                   - When a static scene is detected, a skip frame is inserted only if the previous encoded frame average QP <= this value


#define AMF_PA_CAQ_STRENGTH                         L"PACAQStrength"                        // AMF_PA_CAQ_STRENGTH_ENUM (default : Medium)                          - Content Adaptive Quantization (CAQ) strength




//////////////////////////////////////////////////
// properties set by PA on output buffer interface
#define AMF_PA_ACTIVITY_MAP                         L"PAActivityMap"                        // AMFInterface* -> AMFSurface*;       Values: int32                    - When PA is standalone, there will be a 2D Activity map generated for each frame
#define AMF_PA_SCENE_CHANGE_DETECT                  L"PASceneChangeDetect"                  // bool                                                                 - True/False - available if AMF_PA_SCENE_CHANGE_DETECTION_ENABLE was set to True
#define AMF_PA_STATIC_SCENE_DETECT                  L"PAStaticSceneDetect"                  // bool                                                                 - True/False - available if AMF_PA_STATIC_SCENE_DETECTION_ENABLE was set to True

#endif //#ifndef AMFPreAnalysis_h
