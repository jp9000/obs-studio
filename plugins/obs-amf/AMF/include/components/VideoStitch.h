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

/**
 ***************************************************************************************************
 * @file  VideoStitch.h
 * @brief AMFVideoStitch interface declaration
 ***************************************************************************************************
 */
#ifndef AMF_VideoStitch_h
#define AMF_VideoStitch_h
#pragma once

#include "public/include/components/Component.h"

#define AMFVideoStitch       L"AMFVideoStitch"                  //Component name

// static properties 
#define AMF_VIDEO_STITCH_OUTPUT_FORMAT       L"OutputFormat"    // Values, AMF_SURFACE_BGRA or AMF_SURFACE_RGBA
#define AMF_VIDEO_STITCH_MEMORY_TYPE         L"MemoryType"      // Values, only AMF_MEMORY_DX11 is supported for now.
#define AMF_VIDEO_STITCH_OUTPUT_SIZE         L"OutputSize"      // AMFSize, (width, height) in pixels. default= (0,0), will be the same size as input.
#define AMF_VIDEO_STITCH_INPUTCOUNT          L"InputCount"      // amf_uint64, number of camera inputs.

// individual camera direction and location
#define AMF_VIDEO_CAMERA_ANGLE_PITCH        L"CameraPitch"      // double, in radians, default = 0, camera pitch orientation 
#define AMF_VIDEO_CAMERA_ANGLE_YAW          L"CameraYaw"        // double, in radians, default = 0, camera yaw orientation 
#define AMF_VIDEO_CAMERA_ANGLE_ROLL         L"CameraRoll"       // double, in radians, default = 0, camera roll orientation 

#define AMF_VIDEO_CAMERA_OFFSET_X           L"CameraOffsetX"    // double, in pixels, default = 0, X offset of camera center of the lens from the center of the rig.
#define AMF_VIDEO_CAMERA_OFFSET_Y           L"CameraOffsetY"    // double, in pixels, default = 0, Y offset of camera center of the lens from the center of the rig.
#define AMF_VIDEO_CAMERA_OFFSET_Z           L"CameraOffsetZ"    // double, in pixels, default = 0, Z offset of camera center of the lens from the center of the rig.
#define AMF_VIDEO_CAMERA_HFOV               L"CameraHFOV"       // double, in radians, default = PI, - horizontal field of view
#define AMF_VIDEO_CAMERA_SCALE              L"CameraScale"      // double, default = 1, scale coeff

// lens correction parameters
#define AMF_VIDEO_STITCH_LENS_CORR_K1       L"LensK1"           // double, default = 0.
#define AMF_VIDEO_STITCH_LENS_CORR_K2       L"LensK2"           // double, default = 0.
#define AMF_VIDEO_STITCH_LENS_CORR_K3       L"LensK3"           // double, default = 0.
#define AMF_VIDEO_STITCH_LENS_CORR_OFFX     L"LensOffX"         // double, default = 0.
#define AMF_VIDEO_STITCH_LENS_CORR_OFFY     L"LensOffY"         // double, default = 0.
#define AMF_VIDEO_STITCH_CROP               L"Crop"             //AMFRect, in pixels default = (0,0,0,0).

#define AMF_VIDEO_STITCH_LENS_MODE          L"LensMode"         // Values, AMF_VIDEO_STITCH_LENS_CORR_MODE_ENUM, (default = AMF_VIDEO_STITCH_LENS_CORR_MODE_RADIAL)

#define AMF_VIDEO_STITCH_OUTPUT_MODE        L"OutputMode"       // AMF_VIDEO_STITCH_OUTPUT_MODE_ENUM (default=AMF_VIDEO_STITCH_OUTPUT_MODE_PREVIEW) 
#define AMF_VIDEO_STITCH_COMBINED_SOURCE    L"CombinedSource"   // bool, (default=false) video sources are combined in one stream

#define AMF_VIDEO_STITCH_COMPUTE_DEVICE     L"ComputeDevice"    // amf_int64(AMF_MEMORY_TYPE) Values, AMF_MEMORY_DX11, AMF_MEMORY_COMPUTE_FOR_DX11, AMF_MEMORY_OPENCL

//for debug
#define AMF_VIDEO_STITCH_WIRE_RENDER        L"Wire"             // bool (default=false) reder wireframe

//view angle 
#define AMF_VIDEO_STITCH_VIEW_ROTATE_X      L"AngleX"           // double, in radians, default = 0 - delta from current position / automatilcally reset to 0 inside SetProperty() call
#define AMF_VIDEO_STITCH_VIEW_ROTATE_Y      L"AngleY"           // double, in radians, default = 0 - delta from current position / automatilcally reset to 0 inside SetProperty() call
#define AMF_VIDEO_STITCH_VIEW_ROTATE_Z      L"AngleZ"           // double, in radians, default = 0 - delta from current position / automatilcally reset to 0 inside SetProperty() call

#define AMF_VIDEO_STITCH_COLOR_BALANCE      L"ColorBalance"     // bool (default=true) enables color balance

//lens mode
enum AMF_VIDEO_STITCH_LENS_ENUM
{
    AMF_VIDEO_STITCH_LENS_RECTILINEAR        = 0,   //rect linear lens
    AMF_VIDEO_STITCH_LENS_FISHEYE_FULLFRAME  = 1,   //fisheye full frame
    AMF_VIDEO_STITCH_LENS_FISHEYE_CIRCULAR   = 2,   //fisheye, circular
};

//Output Mode
enum AMF_VIDEO_STITCH_OUTPUT_MODE_ENUM
{
    AMF_VIDEO_STITCH_OUTPUT_MODE_PREVIEW          = 0,    //preview mode
    AMF_VIDEO_STITCH_OUTPUT_MODE_EQUIRECTANGULAR  = 1,    //equirectangular mode
    AMF_VIDEO_STITCH_OUTPUT_MODE_CUBEMAP          = 2,    //cubemap mode
    AMF_VIDEO_STITCH_OUTPUT_MODE_LAST = AMF_VIDEO_STITCH_OUTPUT_MODE_CUBEMAP,
};

//audio mode
enum AMF_VIDEO_STITCH_AUDIO_MODE_ENUM
{
    AMF_VIDEO_STITCH_AUDIO_MODE_NONE        = 0,    //no audio
    AMF_VIDEO_STITCH_AUDIO_MODE_VIDEO       = 1,    //using audio from video stream
    AMF_VIDEO_STITCH_AUDIO_MODE_FILE        = 2,    //using audio from file
    AMF_VIDEO_STITCH_AUDIO_MODE_CAPTURE     = 3,    //using audio from capture device
    AMF_VIDEO_STITCH_AUDIO_MODE_INVALID = -1,       //invalid
};


#if defined(_M_AMD64)
    #define STITCH_DLL_NAME    L"amf-stitch-64.dll"
#else
    #define STITCH_DLL_NAME    L"amf-stitch-32.dll"
#endif

#endif //#ifndef AMF_VideoStitch_h
