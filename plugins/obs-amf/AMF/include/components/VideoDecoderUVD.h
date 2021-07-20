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
//  VideoDecoderUVD interface declaration
//-------------------------------------------------------------------------------------------------
#ifndef AMF_VideoDecoderUVD_h
#define AMF_VideoDecoderUVD_h
#pragma once

#include "Component.h"
#include "ColorSpace.h"

#define AMFVideoDecoderUVD_MPEG2                     L"AMFVideoDecoderUVD_MPEG2"
#define AMFVideoDecoderUVD_MPEG4                     L"AMFVideoDecoderUVD_MPEG4"
#define AMFVideoDecoderUVD_WMV3                      L"AMFVideoDecoderUVD_WMV3"
#define AMFVideoDecoderUVD_VC1                       L"AMFVideoDecoderUVD_VC1"
#define AMFVideoDecoderUVD_H264_AVC                  L"AMFVideoDecoderUVD_H264_AVC"
#define AMFVideoDecoderUVD_H264_MVC                  L"AMFVideoDecoderUVD_H264_MVC"
#define AMFVideoDecoderUVD_H264_SVC                  L"AMFVideoDecoderUVD_H264_SVC"
#define AMFVideoDecoderUVD_MJPEG                     L"AMFVideoDecoderUVD_MJPEG"
#define AMFVideoDecoderHW_H265_HEVC                  L"AMFVideoDecoderHW_H265_HEVC"
#define AMFVideoDecoderHW_H265_MAIN10                L"AMFVideoDecoderHW_H265_MAIN10"
#define AMFVideoDecoderHW_VP9                        L"AMFVideoDecoderHW_VP9"
#define AMFVideoDecoderHW_VP9_10BIT                  L"AMFVideoDecoderHW_VP9_10BIT"
#define AMFVideoDecoderHW_AV1                        L"AMFVideoDecoderHW_AV1"

enum AMF_VIDEO_DECODER_MODE_ENUM
{
    AMF_VIDEO_DECODER_MODE_REGULAR = 0,     // DPB delay is based on number of reference frames + 1 (from SPS)
    AMF_VIDEO_DECODER_MODE_COMPLIANT,       // DPB delay is based on profile - up to 16
    AMF_VIDEO_DECODER_MODE_LOW_LATENCY,     // DPB delay is 0. Expect stream with no reordering in P-Frames or B-Frames. B-frames can be present as long as they do not introduce any frame re-ordering 
};
enum AMF_TIMESTAMP_MODE_ENUM
{
    AMF_TS_PRESENTATION = 0, // default. decoder will preserve timestamps from input to output
       AMF_TS_SORT,             // decoder will resort PTS list 
    AMF_TS_DECODE            // timestamps reflect decode order - decoder will reuse them
};

#define AMF_VIDEO_DECODER_SURFACE_COPY                 L"SurfaceCopy"           // amf_bool; default = false; return output surfaces as a copy
#define AMF_VIDEO_DECODER_EXTRADATA                    L"ExtraData"             // AMFInterface* -> AMFBuffer* - AVCC - size length + SPS/PPS; or as Annex B. Optional if stream is Annex B
#define AMF_VIDEO_DECODER_FRAME_RATE                   L"FrameRate"             // amf_double; default = 0.0, optional property to restore duration in the output if needed
#define AMF_TIMESTAMP_MODE                             L"TimestampMode"         // amf_int64(AMF_TIMESTAMP_MODE_ENUM)  - default AMF_TS_PRESENTATION - how input timestamps are treated

// dynamic/adaptive resolution change
#define AMF_VIDEO_DECODER_ADAPTIVE_RESOLUTION_CHANGE   L"AdaptiveResolutionChange" // amf_bool; default = false; reuse allocated surfaces if new resolution is smaller
#define AMF_VIDEO_DECODER_ALLOC_SIZE                   L"AllocSize"             // AMFSize; default (1920,1088); size of allocated surface if AdaptiveResolutionChange is true
#define AMF_VIDEO_DECODER_CURRENT_SIZE                 L"CurrentSize"           // AMFSize; default = (0,0); current size of the video

// reference frame management
#define AMF_VIDEO_DECODER_REORDER_MODE                 L"ReorderMode"           // amf_int64(AMF_VIDEO_DECODER_MODE_ENUM); default = AMF_VIDEO_DECODER_MODE_REGULAR;  defines number of surfaces in DPB list.
#define AMF_VIDEO_DECODER_SURFACE_POOL_SIZE            L"SurfacePoolSize"       // amf_int64; number of surfaces in the decode pool = DPB list size + number of surfaces for presentation
#define AMF_VIDEO_DECODER_DPB_SIZE                     L"DPBSize"               // amf_int64; minimum number of surfaces for reordering

#define AMF_VIDEO_DECODER_DEFAULT_SURFACES_FOR_TRANSIT  5                      // if AMF_VIDEO_DECODER_SURFACE_POOL_SIZE is 0 , AMF_VIDEO_DECODER_SURFACE_POOL_SIZE=AMF_VIDEO_DECODER_DEFAULT_SURFACES_FOR_TRANSIT+AMF_VIDEO_DECODER_DPB_SIZE

// Decoder capabilities - exposed in AMFCaps interface
#define AMF_VIDEO_DECODER_CAP_NUM_OF_STREAMS            L"NumOfStreams"               // amf_int64; maximum number of decode streams supported 


// metadata information: can be set on output surface

// Properties could be set on surface based on HDR SEI or VUI header
#define AMF_VIDEO_DECODER_COLOR_TRANSFER_CHARACTERISTIC L"ColorTransferChar"    // amf_int64(AMF_COLOR_TRANSFER_CHARACTERISTIC_ENUM); default = AMF_COLOR_TRANSFER_CHARACTERISTIC_UNDEFINED, ISO/IEC 23001-8_2013 § 7.2
#define AMF_VIDEO_DECODER_COLOR_PRIMARIES               L"ColorPrimaries"       // amf_int64(AMF_COLOR_PRIMARIES_ENUM); default = AMF_COLOR_PRIMARIES_UNDEFINED, ISO/IEC 23001-8_2013 § 7.1
#define AMF_VIDEO_DECODER_HDR_METADATA                  L"HdrMetadata"          // AMFBuffer containing AMFHDRMetadata; default NULL

/////// AMF_VIDEO_DECODER_FULL_RANGE_COLOR deprecated, use AMF_VIDEO_DECODER_COLOR_RANGE
#define AMF_VIDEO_DECODER_FULL_RANGE_COLOR              L"FullRangeColor"       // bool; default = false; false =  studio range, true = full range 
///////
#define AMF_VIDEO_DECODER_COLOR_RANGE                   L"ColorRange"           // amf_int64(AMF_COLOR_RANGE_ENUM) default = AMF_COLOR_RANGE_UNDEFINED

// can be set on output surface if YUV outout or on component to overwrite VUI
#define AMF_VIDEO_DECODER_COLOR_PROFILE                 L"ColorProfile"         // amf_int64(AMF_VIDEO_CONVERTER_COLOR_PROFILE_ENUM); default = AMF_VIDEO_CONVERTER_COLOR_PROFILE_UNKNOWN - mean AUTO

// properties to be set on decoder if internal converter is used
#define AMF_VIDEO_DECODER_OUTPUT_TRANSFER_CHARACTERISTIC        L"OutColorTransferChar"     // amf_int64(AMF_COLOR_TRANSFER_CHARACTERISTIC_ENUM); default = AMF_COLOR_TRANSFER_CHARACTERISTIC_UNDEFINED, ISO/IEC 23001-8_2013 § 7.2 See VideoDecoderUVD.h for enum 
#define AMF_VIDEO_DECODER_OUTPUT_COLOR_PRIMARIES                L"OutputColorPrimaries"       // amf_int64(AMF_COLOR_PRIMARIES_ENUM); default = AMF_COLOR_PRIMARIES_UNDEFINED, ISO/IEC 23001-8_2013 § 7.1 See ColorSpace.h for enum 
#define AMF_VIDEO_DECODER_OUTPUT_HDR_METADATA                   L"OutHDRMetadata"           // AMFBuffer containing AMFHDRMetadata; default NULL


#if defined(__ANDROID__)
#define AMF_VIDEO_DECODER_NATIVEWINDOW                  L"AndroidNativeWindow"  // amf_int64; default = 0; pointer to native window
#endif //__ANDROID__


#if defined(__APPLE__)
#define AMF_VIDEO_DECODER_NATIVEWINDOW                  L"AppleNativeWindow"  // amf_int64; default = 0; pointer to native window
#endif //__APPLE__


#endif //#ifndef AMF_VideoDecoderUVD_h
