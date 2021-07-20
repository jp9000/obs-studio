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
// Color Spacedeclaration
//-------------------------------------------------------------------------------------------------
#ifndef AMF_ColorSpace_h
#define AMF_ColorSpace_h
#pragma once

// YUV <--> RGB conversion matrix with range
enum AMF_VIDEO_CONVERTER_COLOR_PROFILE_ENUM
{
    AMF_VIDEO_CONVERTER_COLOR_PROFILE_UNKNOWN   =-1,
    AMF_VIDEO_CONVERTER_COLOR_PROFILE_601       = 0,    // studio range 
    AMF_VIDEO_CONVERTER_COLOR_PROFILE_709       = 1,    // studio range 
    AMF_VIDEO_CONVERTER_COLOR_PROFILE_2020      = 2,    // studio range 
    AMF_VIDEO_CONVERTER_COLOR_PROFILE_JPEG      = 3,    // full range 601
//    AMF_VIDEO_CONVERTER_COLOR_PROFILE_G22_BT709 = AMF_VIDEO_CONVERTER_COLOR_PROFILE_709,
//    AMF_VIDEO_CONVERTER_COLOR_PROFILE_G10_SCRGB = 4,
//    AMF_VIDEO_CONVERTER_COLOR_PROFILE_G10_BT709 = 5,
//    AMF_VIDEO_CONVERTER_COLOR_PROFILE_G10_BT2020 = AMF_VIDEO_CONVERTER_COLOR_PROFILE_2020,
//    AMF_VIDEO_CONVERTER_COLOR_PROFILE_G2084_BT2020 = 6,
    AMF_VIDEO_CONVERTER_COLOR_PROFILE_FULL_601  = AMF_VIDEO_CONVERTER_COLOR_PROFILE_JPEG,    // full range
    AMF_VIDEO_CONVERTER_COLOR_PROFILE_FULL_709  = 7,                                         // full range
    AMF_VIDEO_CONVERTER_COLOR_PROFILE_FULL_2020 = 8,                                        // full range
    AMF_VIDEO_CONVERTER_COLOR_PROFILE_COUNT
};

enum AMF_COLOR_PRIMARIES_ENUM // as in VUI color_primaries AVC and HEVC
{
    AMF_COLOR_PRIMARIES_UNDEFINED   = 0,
    AMF_COLOR_PRIMARIES_BT709       = 1,
    AMF_COLOR_PRIMARIES_UNSPECIFIED = 2,
    AMF_COLOR_PRIMARIES_RESERVED    = 3,
    AMF_COLOR_PRIMARIES_BT470M      = 4,
    AMF_COLOR_PRIMARIES_BT470BG     = 5,
    AMF_COLOR_PRIMARIES_SMPTE170M   = 6,
    AMF_COLOR_PRIMARIES_SMPTE240M   = 7,
    AMF_COLOR_PRIMARIES_FILM        = 8,
    AMF_COLOR_PRIMARIES_BT2020      = 9,
    AMF_COLOR_PRIMARIES_SMPTE428    = 10,
    AMF_COLOR_PRIMARIES_SMPTE431    = 11,
    AMF_COLOR_PRIMARIES_SMPTE432    = 12,
    AMF_COLOR_PRIMARIES_JEDEC_P22   = 22,
};
enum AMF_COLOR_TRANSFER_CHARACTERISTIC_ENUM    // as in VUI transfer_characteristic AVC and HEVC
{
    AMF_COLOR_TRANSFER_CHARACTERISTIC_UNDEFINED     = 0,
    AMF_COLOR_TRANSFER_CHARACTERISTIC_BT709         = 1,
    AMF_COLOR_TRANSFER_CHARACTERISTIC_UNSPECIFIED   = 2,
    AMF_COLOR_TRANSFER_CHARACTERISTIC_RESERVED      = 3,
    AMF_COLOR_TRANSFER_CHARACTERISTIC_GAMMA22       = 4,
    AMF_COLOR_TRANSFER_CHARACTERISTIC_GAMMA28       = 5,
    AMF_COLOR_TRANSFER_CHARACTERISTIC_SMPTE170M     = 6,
    AMF_COLOR_TRANSFER_CHARACTERISTIC_SMPTE240M     = 7,
    AMF_COLOR_TRANSFER_CHARACTERISTIC_LINEAR        = 8,
    AMF_COLOR_TRANSFER_CHARACTERISTIC_LOG           = 9,
    AMF_COLOR_TRANSFER_CHARACTERISTIC_LOG_SQRT      = 10,
    AMF_COLOR_TRANSFER_CHARACTERISTIC_IEC61966_2_4  = 11,
    AMF_COLOR_TRANSFER_CHARACTERISTIC_BT1361_ECG    = 12,
    AMF_COLOR_TRANSFER_CHARACTERISTIC_IEC61966_2_1  = 13,
    AMF_COLOR_TRANSFER_CHARACTERISTIC_BT2020_10     = 14, //BT709 curve
    AMF_COLOR_TRANSFER_CHARACTERISTIC_BT2020_12     = 15,
    AMF_COLOR_TRANSFER_CHARACTERISTIC_SMPTE2084     = 16, //PQ curve
    AMF_COLOR_TRANSFER_CHARACTERISTIC_SMPTE428      = 17,
    AMF_COLOR_TRANSFER_CHARACTERISTIC_ARIB_STD_B67  = 18,
};

enum AMF_COLOR_BIT_DEPTH_ENUM
{
    AMF_COLOR_BIT_DEPTH_UNDEFINED   = 0,
    AMF_COLOR_BIT_DEPTH_8           = 8,
    AMF_COLOR_BIT_DEPTH_10          = 10,
};

typedef struct AMFHDRMetadata
{
    amf_uint16  redPrimary[2];              // normalized to 50000
    amf_uint16  greenPrimary[2];            // normalized to 50000
    amf_uint16  bluePrimary[2];             // normalized to 50000
    amf_uint16  whitePoint[2];              // normalized to 50000
    amf_uint32  maxMasteringLuminance;      // normalized to 10000
    amf_uint32  minMasteringLuminance;      // normalized to 10000
    amf_uint16  maxContentLightLevel;       // nit value 
    amf_uint16  maxFrameAverageLightLevel;  // nit value 
} AMFHDRMetadata;


enum AMF_COLOR_RANGE_ENUM
{
    AMF_COLOR_RANGE_UNDEFINED   = 0,
    AMF_COLOR_RANGE_STUDIO      = 1,
    AMF_COLOR_RANGE_FULL        = 2,
};
// these properties can be set on input or outout surface
// IDs are the same as in decoder properties
// can be used to dynamically pass color data between components:
// Decoder, Capture, Encoder. Presenter etc.
#define AMF_VIDEO_COLOR_TRANSFER_CHARACTERISTIC         L"ColorTransferChar"    // amf_int64(AMF_COLOR_TRANSFER_CHARACTERISTIC_ENUM); default = AMF_COLOR_TRANSFER_CHARACTERISTIC_UNDEFINED, ISO/IEC 23001-8_2013 § 7.2 See ColorSpace.h for enum 
#define AMF_VIDEO_COLOR_PRIMARIES                       L"ColorPrimaries"       // amf_int64(AMF_COLOR_PRIMARIES_ENUM); default = AMF_COLOR_PRIMARIES_UNDEFINED, ISO/IEC 23001-8_2013 § 7.1 See ColorSpace.h for enum 
#define AMF_VIDEO_COLOR_RANGE                           L"ColorRange"           // amf_int64(AMF_COLOR_RANGE_ENUM) default = AMF_COLOR_RANGE_UNDEFINED
#define AMF_VIDEO_COLOR_HDR_METADATA                    L"HdrMetadata"          // AMFBuffer containing AMFHDRMetadata; default NULL

#endif //#ifndef AMF_ColorSpace_h
