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
// AMFFVideoConverter interface declaration
//-------------------------------------------------------------------------------------------------
#ifndef AMF_VideoConverter_h
#define AMF_VideoConverter_h
#pragma once

#include "Component.h"
#include "ColorSpace.h"

#define AMFVideoConverter L"AMFVideoConverter"

enum AMF_VIDEO_CONVERTER_SCALE_ENUM
{
    AMF_VIDEO_CONVERTER_SCALE_INVALID          = -1,
    AMF_VIDEO_CONVERTER_SCALE_BILINEAR          = 0,
    AMF_VIDEO_CONVERTER_SCALE_BICUBIC           = 1
};


#define AMF_VIDEO_CONVERTER_OUTPUT_FORMAT                   L"OutputFormat"             // Values : AMF_SURFACE_NV12 or AMF_SURFACE_BGRA or AMF_SURFACE_YUV420P
#define AMF_VIDEO_CONVERTER_MEMORY_TYPE                     L"MemoryType"               // Values : AMF_MEMORY_DX11 or AMF_MEMORY_DX9 or AMF_MEMORY_UNKNOWN (get from input type)
#define AMF_VIDEO_CONVERTER_COMPUTE_DEVICE                  L"ComputeDevice"            // Values : AMF_MEMORY_COMPUTE_FOR_DX9 enumeration

#define AMF_VIDEO_CONVERTER_OUTPUT_SIZE                     L"OutputSize"               // AMFSize  (default=0,0) width in pixels. default means no scaling
#define AMF_VIDEO_CONVERTER_OUTPUT_RECT                     L"OutputRect"               // AMFRect  (default=0, 0, 0, 0) rectangle in pixels. default means no rect
#define AMF_VIDEO_CONVERTER_SCALE                           L"ScaleType"            // amf_int64(AMF_VIDEO_CONVERTER_SCALE_ENUM); default = AMF_VIDEO_CONVERTER_SCALE_BILINEAR
#define AMF_VIDEO_CONVERTER_FORCE_OUTPUT_SURFACE_SIZE       L"ForceOutputSurfaceSize" // bool (default=false) Force output size from output surface 

#define AMF_VIDEO_CONVERTER_KEEP_ASPECT_RATIO               L"KeepAspectRatio"          // bool (default=false) Keep aspect ratio if scaling. 
#define AMF_VIDEO_CONVERTER_FILL                            L"Fill"                     // bool (default=false) fill area out of ROI. 
#define AMF_VIDEO_CONVERTER_FILL_COLOR                      L"FillColor"                // AMFColor 

//-------------------------------------------------------------------------------------------------
// SDR color conversion
//-------------------------------------------------------------------------------------------------
#define AMF_VIDEO_CONVERTER_COLOR_PROFILE                   L"ColorProfile"         // amf_int64(AMF_VIDEO_CONVERTER_COLOR_PROFILE_ENUM); default = AMF_VIDEO_CONVERTER_COLOR_PROFILE_UNKNOWN - mean AUTO
#define AMF_VIDEO_CONVERTER_LINEAR_RGB                      L"LinearRGB"             // bool (default=false) Convert to/from linear RGB instead of sRGB using AMF_VIDEO_DECODER_COLOR_TRANSFER_CHARACTERISTIC or by default AMF_VIDEO_CONVERTER_TRANSFER_CHARACTERISTIC

//-------------------------------------------------------------------------------------------------
// HDR color conversion
//-------------------------------------------------------------------------------------------------
// AMF_VIDEO_CONVERTER_COLOR_PROFILE is used to define color space conversion

// HDR data - can be set on converter or respectively on input and output surfaces (output surface via custom allocator) 
// if present, HDR_METADATA primary color overwrites COLOR_PRIMARIES

// these properties can be set on converter component to configure input and output 
// these properties overwrite properties set on surface - see below
#define AMF_VIDEO_CONVERTER_INPUT_TRANSFER_CHARACTERISTIC   L"InputTransferChar"    // amf_int64(AMF_COLOR_TRANSFER_CHARACTERISTIC_ENUM); default = AMF_COLOR_TRANSFER_CHARACTERISTIC_UNDEFINED, ISO/IEC 23001-8_2013 § 7.2 See ColorSpace.h for enum 
#define AMF_VIDEO_CONVERTER_INPUT_COLOR_PRIMARIES           L"InputColorPrimaries"  // amf_int64(AMF_COLOR_PRIMARIES_ENUM); default = AMF_COLOR_PRIMARIES_UNDEFINED, ISO/IEC 23001-8_2013 § 7.1 See ColorSpace.h for enum 
#define AMF_VIDEO_CONVERTER_INPUT_COLOR_RANGE               L"InputColorRange"      // amf_int64(AMF_COLOR_RANGE_ENUM) default = AMF_COLOR_RANGE_UNDEFINED
#define AMF_VIDEO_CONVERTER_INPUT_HDR_METADATA              L"InputHdrMetadata"     // AMFBuffer containing AMFHDRMetadata; default NULL

#define AMF_VIDEO_CONVERTER_OUTPUT_TRANSFER_CHARACTERISTIC  L"OutputTransferChar"   // amf_int64(AMF_COLOR_TRANSFER_CHARACTERISTIC_ENUM); default = AMF_COLOR_TRANSFER_CHARACTERISTIC_UNDEFINED, ISO/IEC 23001-8_2013 § 7.2 See ColorSpace.h for enum 
#define AMF_VIDEO_CONVERTER_OUTPUT_COLOR_PRIMARIES          L"OutputColorPrimaries" // amf_int64(AMF_COLOR_PRIMARIES_ENUM); default = AMF_COLOR_PRIMARIES_UNDEFINED, ISO/IEC 23001-8_2013 § 7.1 See ColorSpace.h for enum 
#define AMF_VIDEO_CONVERTER_OUTPUT_COLOR_RANGE              L"OutputColorRange"     // amf_int64(AMF_COLOR_RANGE_ENUM) default = AMF_COLOR_RANGE_UNDEFINED
#define AMF_VIDEO_CONVERTER_OUTPUT_HDR_METADATA             L"OutputHdrMetadata"    // AMFBuffer containing AMFHDRMetadata; default NULL

// these properties can be set on input or outout surface See ColorSpace.h
// the same as decoder properties set on input surface - see below
//#define AMF_VIDEO_COLOR_TRANSFER_CHARACTERISTIC         L"ColorTransferChar"      // amf_int64(AMF_COLOR_TRANSFER_CHARACTERISTIC_ENUM); default = AMF_COLOR_TRANSFER_CHARACTERISTIC_UNDEFINED, ISO/IEC 23001-8_2013 § 7.2 See ColorSpace.h for enum 
//#define AMF_VIDEO_COLOR_PRIMARIES                       L"ColorPrimaries"         // amf_int64(AMF_COLOR_PRIMARIES_ENUM); default = AMF_COLOR_PRIMARIES_UNDEFINED, ISO/IEC 23001-8_2013 § 7.1 See ColorSpace.h for enum 
//#define AMF_VIDEO_COLOR_RANGE                           L"ColorRange"           // amf_int64(AMF_COLOR_RANGE_ENUM) default = AMF_COLOR_RANGE_UNDEFINED
//#define AMF_VIDEO_COLOR_HDR_METADATA                    L"HdrMetadata"            // AMFBuffer containing AMFHDRMetadata; default NULL

// If decoder properties can be set on input see VideoDecoder.h
// AMF_VIDEO_DECODER_COLOR_TRANSFER_CHARACTERISTIC
// AMF_VIDEO_DECODER_COLOR_PRIMARIES
// AMF_VIDEO_DECODER_COLOR_RANGE
// AMF_VIDEO_DECODER_HDR_METADATA

#define AMF_VIDEO_CONVERTER_USE_DECODER_HDR_METADATA    L"UseDecoderHDRMetadata" // bool (default=true) enables use of decoder / surface input color properties above


#endif //#ifndef AMF_VideoConverter_h
