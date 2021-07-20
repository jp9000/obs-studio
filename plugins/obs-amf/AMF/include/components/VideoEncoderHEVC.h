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
//  VideoEncoderHW_HEVC interface declaration
//-------------------------------------------------------------------------------------------------

#ifndef AMF_VideoEncoderHEVC_h
#define AMF_VideoEncoderHEVC_h
#pragma once

#include "Component.h"
#include "ColorSpace.h"
#include "PreAnalysis.h"

#define AMFVideoEncoder_HEVC L"AMFVideoEncoderHW_HEVC"

enum AMF_VIDEO_ENCODER_HEVC_USAGE_ENUM
{
    AMF_VIDEO_ENCODER_HEVC_USAGE_TRANSCONDING = 0,
    AMF_VIDEO_ENCODER_HEVC_USAGE_ULTRA_LOW_LATENCY,
    AMF_VIDEO_ENCODER_HEVC_USAGE_LOW_LATENCY,
    AMF_VIDEO_ENCODER_HEVC_USAGE_WEBCAM
};

enum AMF_VIDEO_ENCODER_HEVC_PROFILE_ENUM
{
    AMF_VIDEO_ENCODER_HEVC_PROFILE_MAIN = 1
};

enum AMF_VIDEO_ENCODER_HEVC_TIER_ENUM
{
    AMF_VIDEO_ENCODER_HEVC_TIER_MAIN    = 0,
    AMF_VIDEO_ENCODER_HEVC_TIER_HIGH    = 1
};

enum AMF_VIDEO_ENCODER_LEVEL_ENUM
{
    AMF_LEVEL_1     = 30,
    AMF_LEVEL_2     = 60,
    AMF_LEVEL_2_1   = 63,
    AMF_LEVEL_3     = 90,
    AMF_LEVEL_3_1   = 93,
    AMF_LEVEL_4     = 120,
    AMF_LEVEL_4_1   = 123,
    AMF_LEVEL_5     = 150,
    AMF_LEVEL_5_1   = 153,
    AMF_LEVEL_5_2   = 156,
    AMF_LEVEL_6     = 180,
    AMF_LEVEL_6_1   = 183,
    AMF_LEVEL_6_2   = 186
};

enum AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_METHOD_ENUM
{
    AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_METHOD_UNKNOWN = -1,
    AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_METHOD_CONSTANT_QP = 0,
    AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_METHOD_LATENCY_CONSTRAINED_VBR,
    AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_METHOD_PEAK_CONSTRAINED_VBR,
    AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_METHOD_CBR
};

enum AMF_VIDEO_ENCODER_HEVC_PICTURE_TYPE_ENUM
{
    AMF_VIDEO_ENCODER_HEVC_PICTURE_TYPE_NONE = 0,
    AMF_VIDEO_ENCODER_HEVC_PICTURE_TYPE_SKIP,
    AMF_VIDEO_ENCODER_HEVC_PICTURE_TYPE_IDR,
    AMF_VIDEO_ENCODER_HEVC_PICTURE_TYPE_I,
    AMF_VIDEO_ENCODER_HEVC_PICTURE_TYPE_P
};

enum AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE_ENUM
{
    AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE_IDR,
    AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE_I,
    AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE_P
};

enum AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_ENUM
{
    AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_QUALITY    = 0,    
    AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_BALANCED   = 5,
    AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_SPEED      = 10
};

enum AMF_VIDEO_ENCODER_HEVC_HEADER_INSERTION_MODE_ENUM
{
    AMF_VIDEO_ENCODER_HEVC_HEADER_INSERTION_MODE_NONE = 0,
    AMF_VIDEO_ENCODER_HEVC_HEADER_INSERTION_MODE_GOP_ALIGNED,
    AMF_VIDEO_ENCODER_HEVC_HEADER_INSERTION_MODE_IDR_ALIGNED
};

enum AMF_VIDEO_ENCODER_HEVC_PICTURE_TRANSFER_MODE_ENUM
{
    AMF_VIDEO_ENCODER_HEVC_PICTURE_TRANSFER_MODE_OFF = 0,
    AMF_VIDEO_ENCODER_HEVC_PICTURE_TRANSFER_MODE_ON
};



// Static properties - can be set before Init()
#define AMF_VIDEO_ENCODER_HEVC_FRAMESIZE                            L"HevcFrameSize"                // AMFSize; default = 0,0; Frame size

#define AMF_VIDEO_ENCODER_HEVC_USAGE                                L"HevcUsage"                    // amf_int64(AMF_VIDEO_ENCODER_HEVC_USAGE_ENUM); default = N/A; Encoder usage type. fully configures parameter set. 
#define AMF_VIDEO_ENCODER_HEVC_PROFILE                              L"HevcProfile"                  // amf_int64(AMF_VIDEO_ENCODER_HEVC_PROFILE_ENUM) ; default = AMF_VIDEO_ENCODER_HEVC_PROFILE_MAIN;
#define AMF_VIDEO_ENCODER_HEVC_TIER                                 L"HevcTier"                     // amf_int64(AMF_VIDEO_ENCODER_HEVC_TIER_ENUM) ; default = AMF_VIDEO_ENCODER_HEVC_TIER_MAIN;
#define AMF_VIDEO_ENCODER_HEVC_PROFILE_LEVEL                        L"HevcProfileLevel"             // amf_int64 (AMF_VIDEO_ENCODER_LEVEL_ENUM, default depends on HW capabilities); 
#define AMF_VIDEO_ENCODER_HEVC_MAX_LTR_FRAMES                       L"HevcMaxOfLTRFrames"           // amf_int64; default = 0; Max number of LTR frames
#define AMF_VIDEO_ENCODER_HEVC_MAX_NUM_REFRAMES                     L"HevcMaxNumRefFrames"          // amf_int64; default = 1; Maximum number of reference frames
#define AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET                       L"HevcQualityPreset"            // amf_int64(AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_ENUM); default = depends on USAGE; Quality Preset 
#define AMF_VIDEO_ENCODER_HEVC_EXTRADATA                            L"HevcExtraData"                // AMFInterface* - > AMFBuffer*; SPS/PPS buffer - read-only
#define AMF_VIDEO_ENCODER_HEVC_ASPECT_RATIO                         L"HevcAspectRatio"              // AMFRatio; default = 1, 1
#define AMF_VIDEO_ENCODER_HEVC_LOWLATENCY_MODE					    L"LowLatencyInternal"           // bool; default = false, enables low latency mode
#define AMF_VIDEO_ENCODER_HEVC_PRE_ANALYSIS_ENABLE                  L"HevcEnablePreAnalysis"        // bool; default = false; enables the pre-analysis module. Currently only works in AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_METHOD_PEAK_CONSTRAINED_VBR mode. Refer to AMF Video PreAnalysis API reference for more details.

// Picture control properties
#define AMF_VIDEO_ENCODER_HEVC_NUM_GOPS_PER_IDR                     L"HevcGOPSPerIDR"               // amf_int64; default = 1; The frequency to insert IDR as start of a GOP. 0 means no IDR will be inserted.
#define AMF_VIDEO_ENCODER_HEVC_GOP_SIZE                             L"HevcGOPSize"                  // amf_int64; default = 60; GOP Size, in frames
#define AMF_VIDEO_ENCODER_HEVC_DE_BLOCKING_FILTER_DISABLE           L"HevcDeBlockingFilter"         // bool; default = depends on USAGE; De-blocking Filter
#define AMF_VIDEO_ENCODER_HEVC_SLICES_PER_FRAME                     L"HevcSlicesPerFrame"           // amf_int64; default = 1; Number of slices Per Frame 
#define AMF_VIDEO_ENCODER_HEVC_HEADER_INSERTION_MODE                L"HevcHeaderInsertionMode"      // amf_int64(AMF_VIDEO_ENCODER_HEVC_HEADER_INSERTION_MODE_ENUM); default = NONE

// Rate control properties
#define AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_METHOD                  L"HevcRateControlMethod"        // amf_int64(AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_MODE_ENUM); default = depends on USAGE; Rate Control Method 
#define AMF_VIDEO_ENCODER_HEVC_FRAMERATE                            L"HevcFrameRate"                // AMFRate; default = depends on usage; Frame Rate 
#define AMF_VIDEO_ENCODER_HEVC_VBV_BUFFER_SIZE                      L"HevcVBVBufferSize"            // amf_int64; default = depends on USAGE; VBV Buffer Size in bits
#define AMF_VIDEO_ENCODER_HEVC_INITIAL_VBV_BUFFER_FULLNESS          L"HevcInitialVBVBufferFullness" // amf_int64; default =  64; Initial VBV Buffer Fullness 0=0% 64=100%
#define AMF_VIDEO_ENCODER_HEVC_ENABLE_VBAQ                          L"HevcEnableVBAQ"               // // bool; default = depends on USAGE; Enable auto VBAQ
#define AMF_VIDEO_ENCODER_HEVC_HIGH_MOTION_QUALITY_BOOST_ENABLE     L"HevcHighMotionQualityBoostEnable"// bool; default = depends on USAGE; Enable High motion quality boost mode

#define AMF_VIDEO_ENCODER_HEVC_PREENCODE_ENABLE                     L"HevcRateControlPreAnalysisEnable"  // bool; default =  depends on USAGE; enables pre-encode assisted rate control 
#define AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_PREANALYSIS_ENABLE      L"HevcRateControlPreAnalysisEnable"  // bool; default =  depends on USAGE; enables pre-encode assisted rate control. Deprecated, please use AMF_VIDEO_ENCODER_PREENCODE_ENABLE instead. 
#ifdef _MSC_VER
    #pragma deprecated("AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_PREANALYSIS_ENABLE")
#endif

// Motion estimation
#define AMF_VIDEO_ENCODER_HEVC_MOTION_HALF_PIXEL                    L"HevcHalfPixel"                // bool; default= true; Half Pixel 
#define AMF_VIDEO_ENCODER_HEVC_MOTION_QUARTERPIXEL                  L"HevcQuarterPixel"             // bool; default= true; Quarter Pixel

// color conversion
#define AMF_VIDEO_ENCODER_HEVC_COLOR_BIT_DEPTH                      L"HevcColorBitDepth"            // amf_int64(AMF_COLOR_BIT_DEPTH_ENUM); default = AMF_COLOR_BIT_DEPTH_8

#define AMF_VIDEO_ENCODER_HEVC_INPUT_COLOR_PROFILE                  L"HevcInColorProfile"           // amf_int64(AMF_VIDEO_CONVERTER_COLOR_PROFILE_ENUM); default = AMF_VIDEO_CONVERTER_COLOR_PROFILE_UNKNOWN - mean AUTO by size
#define AMF_VIDEO_ENCODER_HEVC_INPUT_TRANSFER_CHARACTERISTIC        L"HevcInColorTransferChar"      // amf_int64(AMF_COLOR_TRANSFER_CHARACTERISTIC_ENUM); default = AMF_COLOR_TRANSFER_CHARACTERISTIC_UNDEFINED, ISO/IEC 23001-8_2013 § 7.2 See VideoDecoderUVD.h for enum 
#define AMF_VIDEO_ENCODER_HEVC_INPUT_COLOR_PRIMARIES                L"HevcInColorPrimaries"         // amf_int64(AMF_COLOR_PRIMARIES_ENUM); default = AMF_COLOR_PRIMARIES_UNDEFINED, ISO/IEC 23001-8_2013 § 7.1 See ColorSpace.h for enum 

#define AMF_VIDEO_ENCODER_HEVC_OUTPUT_COLOR_PROFILE                 L"HevcOutColorProfile"          // amf_int64(AMF_VIDEO_CONVERTER_COLOR_PROFILE_ENUM); default = AMF_VIDEO_CONVERTER_COLOR_PROFILE_UNKNOWN - mean AUTO by size
#define AMF_VIDEO_ENCODER_HEVC_OUTPUT_TRANSFER_CHARACTERISTIC       L"HevcOutColorTransferChar"     // amf_int64(AMF_COLOR_TRANSFER_CHARACTERISTIC_ENUM); default = AMF_COLOR_TRANSFER_CHARACTERISTIC_UNDEFINED, ISO/IEC 23001-8_2013 § 7.2 See VideoDecoderUVD.h for enum 
#define AMF_VIDEO_ENCODER_HEVC_OUTPUT_COLOR_PRIMARIES               L"HevcOutColorPrimaries"        // amf_int64(AMF_COLOR_PRIMARIES_ENUM); default = AMF_COLOR_PRIMARIES_UNDEFINED, ISO/IEC 23001-8_2013 § 7.1 See ColorSpace.h for enum 

// Dynamic properties - can be set at any time

// Rate control properties
#define AMF_VIDEO_ENCODER_HEVC_ENFORCE_HRD                          L"HevcEnforceHRD"               // bool; default = depends on USAGE; Enforce HRD
#define AMF_VIDEO_ENCODER_HEVC_FILLER_DATA_ENABLE                   L"HevcFillerDataEnable"         // bool; default = depends on USAGE; Enforce HRD
#define AMF_VIDEO_ENCODER_HEVC_TARGET_BITRATE                       L"HevcTargetBitrate"            // amf_int64; default = depends on USAGE; Target bit rate in bits
#define AMF_VIDEO_ENCODER_HEVC_PEAK_BITRATE                         L"HevcPeakBitrate"              // amf_int64; default = depends on USAGE; Peak bit rate in bits

#define AMF_VIDEO_ENCODER_HEVC_MAX_AU_SIZE                          L"HevcMaxAUSize"                // amf_int64; default = 60; Max AU Size in bits

#define AMF_VIDEO_ENCODER_HEVC_MIN_QP_I                             L"HevcMinQP_I"                  // amf_int64; default = depends on USAGE; Min QP; range = 
#define AMF_VIDEO_ENCODER_HEVC_MAX_QP_I                             L"HevcMaxQP_I"                  // amf_int64; default = depends on USAGE; Max QP; range = 
#define AMF_VIDEO_ENCODER_HEVC_MIN_QP_P                             L"HevcMinQP_P"                  // amf_int64; default = depends on USAGE; Min QP; range = 
#define AMF_VIDEO_ENCODER_HEVC_MAX_QP_P                             L"HevcMaxQP_P"                  // amf_int64; default = depends on USAGE; Max QP; range = 

#define AMF_VIDEO_ENCODER_HEVC_QP_I                                 L"HevcQP_I"                     // amf_int64; default = 26; P-frame QP; range = 0-51
#define AMF_VIDEO_ENCODER_HEVC_QP_P                                 L"HevcQP_P"                     // amf_int64; default = 26; P-frame QP; range = 0-51

#define AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_SKIP_FRAME_ENABLE       L"HevcRateControlSkipFrameEnable" // bool; default =  depends on USAGE; Rate Control Based Frame Skip 

// color conversion
#define AMF_VIDEO_ENCODER_HEVC_INPUT_HDR_METADATA                   L"HevcInHDRMetadata"             // AMFBuffer containing AMFHDRMetadata; default NULL
#define AMF_VIDEO_ENCODER_HEVC_OUTPUT_HDR_METADATA                  L"HevcOutHDRMetadata"            // AMFBuffer containing AMFHDRMetadata; default NULL

// DPB management
#define AMF_VIDEO_ENCODER_HEVC_PICTURE_TRANSFER_MODE                L"HevcPicTransferMode"           // amf_int64(AMF_VIDEO_ENCODER_HEVC_PICTURE_TRANSFER_MODE_ENUM); default = AMF_VIDEO_ENCODER_HEVC_PICTURE_TRANSFER_MODE_OFF - whether to exchange reference/reconstructed pic between encoder and application

// misc
#define AMF_VIDEO_ENCODER_HEVC_QUERY_TIMEOUT                       L"HevcQueryTimeout"            // amf_int64; default = 0 (no wait); timeout for QueryOutput call in ms.

// Per-submittion properties - can be set on input surface interface
#define AMF_VIDEO_ENCODER_HEVC_END_OF_SEQUENCE                      L"HevcEndOfSequence"            // bool; default = false; generate end of sequence
#define AMF_VIDEO_ENCODER_HEVC_FORCE_PICTURE_TYPE                   L"HevcForcePictureType"         // amf_int64(AMF_VIDEO_ENCODER_HEVC_PICTURE_TYPE_ENUM); default = AMF_VIDEO_ENCODER_HEVC_PICTURE_TYPE_NONE; generate particular picture type
#define AMF_VIDEO_ENCODER_HEVC_INSERT_AUD                           L"HevcInsertAUD"                // bool; default = false; insert AUD
#define AMF_VIDEO_ENCODER_HEVC_INSERT_HEADER                        L"HevcInsertHeader"             // bool; default = false; insert header(SPS, PPS, VPS)

#define AMF_VIDEO_ENCODER_HEVC_MARK_CURRENT_WITH_LTR_INDEX          L"HevcMarkCurrentWithLTRIndex"  // amf_int64; default = N/A; Mark current frame with LTR index
#define AMF_VIDEO_ENCODER_HEVC_FORCE_LTR_REFERENCE_BITFIELD         L"HevcForceLTRReferenceBitfield"// amf_int64; default = 0; force LTR bit-field 
#define AMF_VIDEO_ENCODER_HEVC_ROI_DATA                             L"HevcROIData"					// 2D AMFSurface, surface format: AMF_SURFACE_GRAY32
#define AMF_VIDEO_ENCODER_HEVC_REFERENCE_PICTURE                    L"HevcReferencePicture"         // AMFInterface(AMFSurface); surface used for frame injection

// Properties set by encoder on output buffer interface
#define AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE                     L"HevcOutputDataType"           // amf_int64(AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE_ENUM); default = N/A
#define AMF_VIDEO_ENCODER_HEVC_OUTPUT_MARKED_LTR_INDEX              L"HevcMarkedLTRIndex"           // amf_int64; default = -1; Marked LTR index
#define AMF_VIDEO_ENCODER_HEVC_OUTPUT_REFERENCED_LTR_INDEX_BITFIELD L"HevcReferencedLTRIndexBitfield"// amf_int64; default = 0; referenced LTR bit-field 
#define AMF_VIDEO_ENCODER_HEVC_RECONSTRUCTED_PICTURE                L"HevcReconstructedPicture"     // AMFInterface(AMFSurface); returns reconstructed picture as an AMFSurface attached to the output buffer as property AMF_VIDEO_ENCODER_RECONSTRUCTED_PICTURE of AMFInterface type

// HEVC Encoder capabilities - exposed in AMFCaps interface
#define AMF_VIDEO_ENCODER_HEVC_CAP_MAX_BITRATE                      L"HevcMaxBitrate"               // amf_int64; Maximum bit rate in bits
#define AMF_VIDEO_ENCODER_HEVC_CAP_NUM_OF_STREAMS                   L"HevcNumOfStreams"             // amf_int64; maximum number of encode streams supported 
#define AMF_VIDEO_ENCODER_HEVC_CAP_MAX_PROFILE                      L"HevcMaxProfile"               // amf_int64(AMF_VIDEO_ENCODER_HEVC_PROFILE_ENUM)
#define AMF_VIDEO_ENCODER_HEVC_CAP_MAX_TIER                         L"HevcMaxTier"                  // amf_int64(AMF_VIDEO_ENCODER_HEVC_TIER_ENUM) maximum profile tier 
#define AMF_VIDEO_ENCODER_HEVC_CAP_MAX_LEVEL                        L"HevcMaxLevel"                 // amf_int64 maximum profile level
#define AMF_VIDEO_ENCODER_HEVC_CAP_MIN_REFERENCE_FRAMES             L"HevcMinReferenceFrames"       // amf_int64 minimum number of reference frames
#define AMF_VIDEO_ENCODER_HEVC_CAP_MAX_REFERENCE_FRAMES             L"HevcMaxReferenceFrames"       // amf_int64 maximum number of reference frames
#define AMF_VIDEO_ENCODER_HEVC_CAP_COLOR_CONVERSION                 L"HevcColorConversion"          // amf_int64(AMF_ACCELERATION_TYPE) - type of supported color conversion. default AMF_ACCEL_GPU
#define AMF_VIDEO_ENCODER_HEVC_CAP_PRE_ANALYSIS                     L"HevcPreAnalysis"              // amf_bool - pre analysis module is available for HEVC UVE encoder, n/a for the other encoders
#define AMF_VIDEO_ENCODER_HEVC_CAP_ROI                              L"HevcROIMap"                   // amf_bool - ROI map support is available for HEVC UVE encoder, n/a for the other encoders
#define AMF_VIDEO_ENCODER_HEVC_CAP_MAX_THROUGHPUT                   L"HevcMaxThroughput"            // amf_int64 - MAX throughput for HEVC encoder in MB (16 x 16 pixel)
#define AMF_VIDEO_ENCODER_HEVC_CAP_REQUESTED_THROUGHPUT             L"HevcRequestedThroughput"      // amf_int64 - Currently total requested throughput for HEVC encode in MB (16 x 16 pixel)
#define AMF_VIDEO_ENCODER_CAPS_HEVC_QUERY_TIMEOUT_SUPPORT           L"HevcQueryTimeoutSupport"      // amf_bool - Timeout supported for QueryOutout call

// properties set on AMFComponent to control component creation
#define AMF_VIDEO_ENCODER_HEVC_MEMORY_TYPE                          L"HevcEncoderMemoryType"        // amf_int64(AMF_MEMORY_TYPE) , default is AMF_MEMORY_UNKNOWN, Values : AMF_MEMORY_DX11, AMF_MEMORY_DX9, AMF_MEMORY_UNKNOWN (auto)

// properties set on a frame to retrieve encoder statistics
#define AMF_VIDEO_ENCODER_HEVC_STATISTICS_FEEDBACK                  L"HevcStatisticsFeedback"                       // amf_bool; default = false; Signal encoder to collect and feedback statistics

#define AMF_VIDEO_ENCODER_HEVC_STATISTIC_FRAME_QP                   L"HevcStatisticsFeedbackFrameQP"                // amf_uin32; Rate control base frame QP
#define AMF_VIDEO_ENCODER_HEVC_STATISTIC_AVERAGE_QP                 L"HevcStatisticsFeedbackAvgQP"                  // amf_uin32; Average QP of all encoded CTBs in a picture
#define AMF_VIDEO_ENCODER_HEVC_STATISTIC_MAX_QP                     L"HevcStatisticsFeedbackMaxQP"                  // amf_uin32; Max QP among all encoded CTBs in a picture
#define AMF_VIDEO_ENCODER_HEVC_STATISTIC_MIN_QP                     L"HevcStatisticsFeedbackMinQP"                  // amf_uin32; Min QP among all encoded CTBs in a picture
#define AMF_VIDEO_ENCODER_HEVC_STATISTIC_PIX_NUM_INTRA              L"HevcStatisticsFeedbackPixNumIntra"            // amf_uin32; Number of the intra encoded pixels
#define AMF_VIDEO_ENCODER_HEVC_STATISTIC_PIX_NUM_INTER              L"HevcStatisticsFeedbackPixNumInter"            // amf_uin32; Number of the inter encoded pixels
#define AMF_VIDEO_ENCODER_HEVC_STATISTIC_PIX_NUM_SKIP               L"HevcStatisticsFeedbackPixNumSkip"             // amf_uin32; Number of the skip mode pixels
#define AMF_VIDEO_ENCODER_HEVC_STATISTIC_BITCOUNT_RESIDUAL          L"HevcStatisticsFeedbackBitcountResidual"       // amf_uin32; The bit count that corresponds to residual data
#define AMF_VIDEO_ENCODER_HEVC_STATISTIC_BITCOUNT_MOTION            L"HevcStatisticsFeedbackBitcountMotion"         // amf_uin32; The bit count that corresponds to motion vectors
#define AMF_VIDEO_ENCODER_HEVC_STATISTIC_BITCOUNT_INTER             L"HevcStatisticsFeedbackBitcountInter"          // amf_uin32; The bit count that are assigned to inter CTBs
#define AMF_VIDEO_ENCODER_HEVC_STATISTIC_BITCOUNT_INTRA             L"HevcStatisticsFeedbackBitcountIntra"          // amf_uin32; The bit count that are assigned to intra CTBs
#define AMF_VIDEO_ENCODER_HEVC_STATISTIC_BITCOUNT_ALL_MINUS_HEADER  L"HevcStatisticsFeedbackBitcountAllMinusHeader" // amf_uin32; The bit count of the bitstream excluding header
#define AMF_VIDEO_ENCODER_HEVC_STATISTIC_MV_X                       L"HevcStatisticsFeedbackMvX"                    // amf_uin32; Accumulated absolute values of horizontal MV’s
#define AMF_VIDEO_ENCODER_HEVC_STATISTIC_MV_Y                       L"HevcStatisticsFeedbackMvY"                    // amf_uin32; Accumulated absolute values of vertical MV’s

#endif //#ifndef AMF_VideoEncoderHEVC_h
