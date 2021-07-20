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
// AMFVideoEncoderHW_AVC interface declaration
//-------------------------------------------------------------------------------------------------

#ifndef AMF_VideoEncoderVCE_h
#define AMF_VideoEncoderVCE_h
#pragma once

#include "Component.h"
#include "ColorSpace.h"
#include "PreAnalysis.h"

#define AMFVideoEncoderVCE_AVC L"AMFVideoEncoderVCE_AVC"
#define AMFVideoEncoderVCE_SVC L"AMFVideoEncoderVCE_SVC"

enum AMF_VIDEO_ENCODER_USAGE_ENUM
{
    AMF_VIDEO_ENCODER_USAGE_TRANSCONDING = 0,
    AMF_VIDEO_ENCODER_USAGE_ULTRA_LOW_LATENCY,
    AMF_VIDEO_ENCODER_USAGE_LOW_LATENCY,
    AMF_VIDEO_ENCODER_USAGE_WEBCAM
};

enum AMF_VIDEO_ENCODER_PROFILE_ENUM
{
    AMF_VIDEO_ENCODER_PROFILE_UNKNOWN = 0,
    AMF_VIDEO_ENCODER_PROFILE_BASELINE = 66,
    AMF_VIDEO_ENCODER_PROFILE_MAIN = 77,
    AMF_VIDEO_ENCODER_PROFILE_HIGH = 100,
    AMF_VIDEO_ENCODER_PROFILE_CONSTRAINED_BASELINE = 256,
    AMF_VIDEO_ENCODER_PROFILE_CONSTRAINED_HIGH = 257
};

enum AMF_VIDEO_ENCODER_SCANTYPE_ENUM
{
    AMF_VIDEO_ENCODER_SCANTYPE_PROGRESSIVE = 0,
    AMF_VIDEO_ENCODER_SCANTYPE_INTERLACED
};

enum AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_ENUM
{
    AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_UNKNOWN = -1,
    AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CONSTANT_QP = 0,
    AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CBR,
    AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_PEAK_CONSTRAINED_VBR,
    AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_LATENCY_CONSTRAINED_VBR,
    AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_QUALITY_VBR
};

enum AMF_VIDEO_ENCODER_QUALITY_PRESET_ENUM
{
    AMF_VIDEO_ENCODER_QUALITY_PRESET_BALANCED = 0,
    AMF_VIDEO_ENCODER_QUALITY_PRESET_SPEED,
    AMF_VIDEO_ENCODER_QUALITY_PRESET_QUALITY
};

enum AMF_VIDEO_ENCODER_PICTURE_STRUCTURE_ENUM
{
    AMF_VIDEO_ENCODER_PICTURE_STRUCTURE_NONE = 0,
    AMF_VIDEO_ENCODER_PICTURE_STRUCTURE_FRAME,
    AMF_VIDEO_ENCODER_PICTURE_STRUCTURE_TOP_FIELD,
    AMF_VIDEO_ENCODER_PICTURE_STRUCTURE_BOTTOM_FIELD
};

enum AMF_VIDEO_ENCODER_PICTURE_TYPE_ENUM
{
    AMF_VIDEO_ENCODER_PICTURE_TYPE_NONE = 0,
    AMF_VIDEO_ENCODER_PICTURE_TYPE_SKIP,
    AMF_VIDEO_ENCODER_PICTURE_TYPE_IDR,
    AMF_VIDEO_ENCODER_PICTURE_TYPE_I,
    AMF_VIDEO_ENCODER_PICTURE_TYPE_P,
    AMF_VIDEO_ENCODER_PICTURE_TYPE_B
};

enum AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_ENUM
{
    AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_IDR,
    AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_I,
    AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_P,
    AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_B
};

enum AMF_VIDEO_ENCODER_PREENCODE_MODE_ENUM
{
    AMF_VIDEO_ENCODER_PREENCODE_DISABLED = 0,
    AMF_VIDEO_ENCODER_PREENCODE_ENABLED  = 1,
};

enum AMF_VIDEO_ENCODER_CODING_ENUM
{
    AMF_VIDEO_ENCODER_UNDEFINED = 0, // BASELINE = CALV; MAIN, HIGH = CABAC
    AMF_VIDEO_ENCODER_CABAC,
    AMF_VIDEO_ENCODER_CALV,

};

enum AMF_VIDEO_ENCODER_PICTURE_TRANSFER_MODE_ENUM
{
    AMF_VIDEO_ENCODER_PICTURE_TRANSFER_MODE_OFF = 0,
    AMF_VIDEO_ENCODER_PICTURE_TRANSFER_MODE_ON
};


// Static properties - can be set before Init()

#define AMF_VIDEO_ENCODER_FRAMESIZE                             L"FrameSize"                // AMFSize; default = 0,0; Frame size
#define AMF_VIDEO_ENCODER_FRAMERATE                             L"FrameRate"                // AMFRate; default = depends on usage; Frame Rate 

#define AMF_VIDEO_ENCODER_EXTRADATA                             L"ExtraData"                // AMFInterface* - > AMFBuffer*; SPS/PPS buffer in Annex B format - read-only
#define AMF_VIDEO_ENCODER_USAGE                                 L"Usage"                    // amf_int64(AMF_VIDEO_ENCODER_USAGE_ENUM); default = N/A; Encoder usage type. fully configures parameter set. 
#define AMF_VIDEO_ENCODER_PROFILE                               L"Profile"                  // amf_int64(AMF_VIDEO_ENCODER_PROFILE_ENUM) ; default = AMF_VIDEO_ENCODER_PROFILE_MAIN;  H264 profile
#define AMF_VIDEO_ENCODER_PROFILE_LEVEL                         L"ProfileLevel"             // amf_int64; default = 42; H264 profile level
#define AMF_VIDEO_ENCODER_MAX_LTR_FRAMES                        L"MaxOfLTRFrames"           // amf_int64; default = 0; Max number of LTR frames
#define AMF_VIDEO_ENCODER_SCANTYPE                              L"ScanType"                 // amf_int64(AMF_VIDEO_ENCODER_SCANTYPE_ENUM); default = AMF_VIDEO_ENCODER_SCANTYPE_PROGRESSIVE; indicates input stream type
#define AMF_VIDEO_ENCODER_MAX_NUM_REFRAMES                      L"MaxNumRefFrames"          // amf_int64; Maximum number of reference frames
#define AMF_VIDEO_ENCODER_ASPECT_RATIO                          L"AspectRatio"              // AMFRatio; default = 1, 1
#define AMF_VIDEO_ENCODER_FULL_RANGE_COLOR                      L"FullRangeColor"           // bool; default = false; inidicates that YUV input is (0,255) 
#define AMF_VIDEO_ENCODER_LOWLATENCY_MODE                       L"LowLatencyInternal"       // bool; default = false, enables low latency mode and POC mode 2 in the encoder
#define AMF_VIDEO_ENCODER_PRE_ANALYSIS_ENABLE                   L"EnablePreAnalysis"        // bool; default = false; enables the pre-analysis module. Currently only works in AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_PEAK_CONSTRAINED_VBR mode. Refer to AMF Video PreAnalysis API reference for more details.
#define AMF_VIDEO_ENCODER_PREENCODE_ENABLE                      L"RateControlPreanalysisEnable"     // amf_int64(AMF_VIDEO_ENCODER_PREENCODE_MODE_ENUM); default =  AMF_VIDEO_ENCODER_PREENCODE_DISABLED; enables pre-encode assisted rate control 
#define AMF_VIDEO_ENCODER_RATE_CONTROL_PREANALYSIS_ENABLE       L"RateControlPreanalysisEnable"     // amf_int64(AMF_VIDEO_ENCODER_PREENCODE_MODE_ENUM); default =  AMF_VIDEO_ENCODER_PREENCODE_DISABLED; enables pre-encode assisted rate control. Deprecated, please use AMF_VIDEO_ENCODER_PREENCODE_ENABLE instead.
#ifdef _MSC_VER
    #pragma deprecated("AMF_VIDEO_ENCODER_RATE_CONTROL_PREANALYSIS_ENABLE")
#endif


    // Quality preset property
#define AMF_VIDEO_ENCODER_QUALITY_PRESET                        L"QualityPreset"            // amf_int64(AMF_VIDEO_ENCODER_QUALITY_PRESET_ENUM); default = depends on USAGE; Quality Preset 

    // color conversion
#define AMF_VIDEO_ENCODER_COLOR_BIT_DEPTH                       L"ColorBitDepth"            // amf_int64(AMF_COLOR_BIT_DEPTH_ENUM); default = AMF_COLOR_BIT_DEPTH_8

#define AMF_VIDEO_ENCODER_INPUT_COLOR_PROFILE                   L"InColorProfile"           // amf_int64(AMF_VIDEO_CONVERTER_COLOR_PROFILE_ENUM); default = AMF_VIDEO_CONVERTER_COLOR_PROFILE_UNKNOWN - mean AUTO by size
#define AMF_VIDEO_ENCODER_INPUT_TRANSFER_CHARACTERISTIC         L"InColorTransferChar"      // amf_int64(AMF_COLOR_TRANSFER_CHARACTERISTIC_ENUM); default = AMF_COLOR_TRANSFER_CHARACTERISTIC_UNDEFINED, ISO/IEC 23001-8_2013 § 7.2 See VideoDecoderUVD.h for enum 
#define AMF_VIDEO_ENCODER_INPUT_COLOR_PRIMARIES                 L"InColorPrimaries"         // amf_int64(AMF_COLOR_PRIMARIES_ENUM); default = AMF_COLOR_PRIMARIES_UNDEFINED, ISO/IEC 23001-8_2013 § 7.1 See ColorSpace.h for enum 
#define AMF_VIDEO_ENCODER_INPUT_HDR_METADATA                    L"InHDRMetadata"            // AMFBuffer containing AMFHDRMetadata; default NULL

#define AMF_VIDEO_ENCODER_OUTPUT_COLOR_PROFILE                  L"OutColorProfile"          // amf_int64(AMF_VIDEO_CONVERTER_COLOR_PROFILE_ENUM); default = AMF_VIDEO_CONVERTER_COLOR_PROFILE_UNKNOWN - mean AUTO by size
#define AMF_VIDEO_ENCODER_OUTPUT_TRANSFER_CHARACTERISTIC        L"OutColorTransferChar"     // amf_int64(AMF_COLOR_TRANSFER_CHARACTERISTIC_ENUM); default = AMF_COLOR_TRANSFER_CHARACTERISTIC_UNDEFINED, ISO/IEC 23001-8_2013 § 7.2 See VideoDecoderUVD.h for enum 
#define AMF_VIDEO_ENCODER_OUTPUT_COLOR_PRIMARIES                L"OutColorPrimaries"        // amf_int64(AMF_COLOR_PRIMARIES_ENUM); default = AMF_COLOR_PRIMARIES_UNDEFINED, ISO/IEC 23001-8_2013 § 7.1 See ColorSpace.h for enum 
#define AMF_VIDEO_ENCODER_OUTPUT_HDR_METADATA                   L"OutHDRMetadata"           // AMFBuffer containing AMFHDRMetadata; default NULL


// Dynamic properties - can be set at any time

    // Rate control properties
#define AMF_VIDEO_ENCODER_B_PIC_DELTA_QP                        L"BPicturesDeltaQP"         // amf_int64; default = depends on USAGE; B-picture Delta
#define AMF_VIDEO_ENCODER_REF_B_PIC_DELTA_QP                    L"ReferenceBPicturesDeltaQP"// amf_int64; default = depends on USAGE; Reference B-picture Delta

#define AMF_VIDEO_ENCODER_ENFORCE_HRD                           L"EnforceHRD"               // bool; default = depends on USAGE; Enforce HRD
#define AMF_VIDEO_ENCODER_FILLER_DATA_ENABLE                    L"FillerDataEnable"         // bool; default = false; Filler Data Enable
#define AMF_VIDEO_ENCODER_ENABLE_VBAQ                           L"EnableVBAQ"               // bool; default = depends on USAGE; Enable VBAQ
#define AMF_VIDEO_ENCODER_HIGH_MOTION_QUALITY_BOOST_ENABLE      L"HighMotionQualityBoostEnable"// bool; default = depends on USAGE; Enable High motion quality boost mode


#define AMF_VIDEO_ENCODER_VBV_BUFFER_SIZE                       L"VBVBufferSize"            // amf_int64; default = depends on USAGE; VBV Buffer Size in bits
#define AMF_VIDEO_ENCODER_INITIAL_VBV_BUFFER_FULLNESS           L"InitialVBVBufferFullness" // amf_int64; default =  64; Initial VBV Buffer Fullness 0=0% 64=100%

#define AMF_VIDEO_ENCODER_MAX_AU_SIZE                           L"MaxAUSize"                // amf_int64; default = 0; Max AU Size in bits

#define AMF_VIDEO_ENCODER_MIN_QP                                L"MinQP"                    // amf_int64; default = depends on USAGE; Min QP; range = 0-51
#define AMF_VIDEO_ENCODER_MAX_QP                                L"MaxQP"                    // amf_int64; default = depends on USAGE; Max QP; range = 0-51
#define AMF_VIDEO_ENCODER_QP_I                                  L"QPI"                      // amf_int64; default = 22; I-frame QP; range = 0-51
#define AMF_VIDEO_ENCODER_QP_P                                  L"QPP"                      // amf_int64; default = 22; P-frame QP; range = 0-51
#define AMF_VIDEO_ENCODER_QP_B                                  L"QPB"                      // amf_int64; default = 22; B-frame QP; range = 0-51
#define AMF_VIDEO_ENCODER_TARGET_BITRATE                        L"TargetBitrate"            // amf_int64; default = depends on USAGE; Target bit rate in bits
#define AMF_VIDEO_ENCODER_PEAK_BITRATE                          L"PeakBitrate"              // amf_int64; default = depends on USAGE; Peak bit rate in bits
#define AMF_VIDEO_ENCODER_RATE_CONTROL_SKIP_FRAME_ENABLE        L"RateControlSkipFrameEnable"   // bool; default =  depends on USAGE; Rate Control Based Frame Skip 
#define AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD                   L"RateControlMethod"        // amf_int64(AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_ENUM); default = depends on USAGE; Rate Control Method 

    // Picture control properties
#define AMF_VIDEO_ENCODER_HEADER_INSERTION_SPACING              L"HeaderInsertionSpacing"   // amf_int64; default = depends on USAGE; Header Insertion Spacing; range 0-1000
#define AMF_VIDEO_ENCODER_B_PIC_PATTERN                         L"BPicturesPattern"         // amf_int64; default = 3; B-picture Pattern (number of B-Frames)
#define AMF_VIDEO_ENCODER_DE_BLOCKING_FILTER                    L"DeBlockingFilter"         // bool; default = depends on USAGE; De-blocking Filter
#define AMF_VIDEO_ENCODER_B_REFERENCE_ENABLE                    L"BReferenceEnable"         // bool; default = true; Enable Refrence to B-frames
#define AMF_VIDEO_ENCODER_IDR_PERIOD                            L"IDRPeriod"                // amf_int64; default = depends on USAGE; IDR Period in frames
#define AMF_VIDEO_ENCODER_INTRA_REFRESH_NUM_MBS_PER_SLOT        L"IntraRefreshMBsNumberPerSlot" // amf_int64; default = depends on USAGE; Intra Refresh MBs Number Per Slot in Macroblocks
#define AMF_VIDEO_ENCODER_SLICES_PER_FRAME                      L"SlicesPerFrame"           // amf_int64; default = 1; Number of slices Per Frame 
#define AMF_VIDEO_ENCODER_CABAC_ENABLE                          L"CABACEnable"              // amf_int64(AMF_VIDEO_ENCODER_CODING_ENUM) default = AMF_VIDEO_ENCODER_UNDEFINED

    // Motion estimation
#define AMF_VIDEO_ENCODER_MOTION_HALF_PIXEL                     L"HalfPixel"                // bool; default= true; Half Pixel 
#define AMF_VIDEO_ENCODER_MOTION_QUARTERPIXEL                   L"QuarterPixel"             // bool; default= true; Quarter Pixel

    // SVC
#define AMF_VIDEO_ENCODER_NUM_TEMPORAL_ENHANCMENT_LAYERS        L"NumOfTemporalEnhancmentLayers" // amf_int64; default = 0; range = 0, min(2, caps->GetMaxNumOfTemporalLayers()) number of temporal enhancment Layers (SVC)


    // DPB management
#define AMF_VIDEO_ENCODER_PICTURE_TRANSFER_MODE                 L"PicTransferMode"               // amf_int64(AMF_VIDEO_ENCODER_PICTURE_TRANSFER_MODE_ENUM); default = AMF_VIDEO_ENCODER_PICTURE_TRANSFER_MODE_OFF - whether to exchange reference/reconstructed pic between encoder and application
    // misc
#define AMF_VIDEO_ENCODER_QUERY_TIMEOUT                       L"QueryTimeout"            // amf_int64; default = 0 (no wait); timeout for QueryOutput call in ms.

// Per-submittion properties - can be set on input surface interface
#define AMF_VIDEO_ENCODER_END_OF_SEQUENCE                       L"EndOfSequence"            // bool; default = false; generate end of sequence
#define AMF_VIDEO_ENCODER_END_OF_STREAM                         L"EndOfStream"              // bool; default = false; generate end of stream
#define AMF_VIDEO_ENCODER_FORCE_PICTURE_TYPE                    L"ForcePictureType"         // amf_int64(AMF_VIDEO_ENCODER_PICTURE_TYPE_ENUM); default = AMF_VIDEO_ENCODER_PICTURE_TYPE_NONE; generate particular picture type
#define AMF_VIDEO_ENCODER_INSERT_AUD                            L"InsertAUD"                // bool; default = false; insert AUD
#define AMF_VIDEO_ENCODER_INSERT_SPS                            L"InsertSPS"                // bool; default = false; insert SPS
#define AMF_VIDEO_ENCODER_INSERT_PPS                            L"InsertPPS"                // bool; default = false; insert PPS
#define AMF_VIDEO_ENCODER_PICTURE_STRUCTURE                     L"PictureStructure"         // amf_int64(AMF_VIDEO_ENCODER_PICTURE_STRUCTURE_ENUM); default = AMF_VIDEO_ENCODER_PICTURE_STRUCTURE_FRAME; indicate picture type
#define AMF_VIDEO_ENCODER_MARK_CURRENT_WITH_LTR_INDEX           L"MarkCurrentWithLTRIndex"  // //amf_int64; default = N/A; Mark current frame with LTR index
#define AMF_VIDEO_ENCODER_FORCE_LTR_REFERENCE_BITFIELD          L"ForceLTRReferenceBitfield"// amf_int64; default = 0; force LTR bit-field 
#define AMF_VIDEO_ENCODER_ROI_DATA                              L"ROIData"                  // 2D AMFSurface, surface format: AMF_SURFACE_GRAY32
#define AMF_VIDEO_ENCODER_REFERENCE_PICTURE                     L"ReferencePicture"         // AMFInterface(AMFSurface); surface used for frame injection

// properties set by encoder on output buffer interface
#define AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE                      L"OutputDataType"           // amf_int64(AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_ENUM); default = N/A
#define AMF_VIDEO_ENCODER_OUTPUT_MARKED_LTR_INDEX               L"MarkedLTRIndex"           //amf_int64; default = -1; Marked LTR index
#define AMF_VIDEO_ENCODER_OUTPUT_REFERENCED_LTR_INDEX_BITFIELD  L"ReferencedLTRIndexBitfield" // amf_int64; default = 0; referenced LTR bit-field 
#define AMF_VIDEO_ENCODER_RECONSTRUCTED_PICTURE                 L"ReconstructedPicture"     // AMFInterface(AMFSurface); returns reconstructed picture as an AMFSurface attached to the output buffer as property AMF_VIDEO_ENCODER_RECONSTRUCTED_PICTURE of AMFInterface type


#define AMF_VIDEO_ENCODER_HDCP_COUNTER                          L"HDCPCounter"              //  const void*

// Properties for multi-instance cloud gaming
#define AMF_VIDEO_ENCODER_MAX_INSTANCES                         L"EncoderMaxInstances"      //  amf_uint32; default = 1; max number of encoder instances
#define AMF_VIDEO_ENCODER_MULTI_INSTANCE_MODE                   L"MultiInstanceMode"        //  bool; default = false;
#define AMF_VIDEO_ENCODER_CURRENT_QUEUE                         L"MultiInstanceCurrentQueue"//  amf_uin32; default = 0; 


// VCE Encoder capabilities - exposed in AMFCaps interface
#define AMF_VIDEO_ENCODER_CAP_MAX_BITRATE                       L"MaxBitrate"               // amf_int64; Maximum bit rate in bits
#define AMF_VIDEO_ENCODER_CAP_NUM_OF_STREAMS                    L"NumOfStreams"             // amf_int64; maximum number of encode streams supported 
#define AMF_VIDEO_ENCODER_CAP_MAX_PROFILE                       L"MaxProfile"               // AMF_VIDEO_ENCODER_PROFILE_ENUM
#define AMF_VIDEO_ENCODER_CAP_MAX_LEVEL                         L"MaxLevel"                 // amf_int64 maximum profile level
#define AMF_VIDEO_ENCODER_CAP_BFRAMES                           L"BFrames"                  // bool  is B-Frames supported
#define AMF_VIDEO_ENCODER_CAP_MIN_REFERENCE_FRAMES              L"MinReferenceFrames"       // amf_int64 minimum number of reference frames
#define AMF_VIDEO_ENCODER_CAP_MAX_REFERENCE_FRAMES              L"MaxReferenceFrames"       // amf_int64 maximum number of reference frames
#define AMF_VIDEO_ENCODER_CAP_MAX_TEMPORAL_LAYERS               L"MaxTemporalLayers"        // amf_int64 maximum number of temporal layers
#define AMF_VIDEO_ENCODER_CAP_FIXED_SLICE_MODE                  L"FixedSliceMode"           // bool  is fixed slice mode supported
#define AMF_VIDEO_ENCODER_CAP_NUM_OF_HW_INSTANCES               L"NumOfHwInstances"         // amf_int64 number of HW encoder instances
#define AMF_VIDEO_ENCODER_CAP_COLOR_CONVERSION                  L"ColorConversion"          // amf_int64(AMF_ACCELERATION_TYPE) - type of supported color conversion. default AMF_ACCEL_GPU
#define AMF_VIDEO_ENCODER_CAP_PRE_ANALYSIS                      L"PreAnalysis"              // amf_bool - pre analysis module is available for H264 UVE encoder, n/a for the other encoders
#define AMF_VIDEO_ENCODER_CAP_ROI                               L"ROIMap"                   // amf_bool - ROI map support is available for H264 UVE encoder, n/a for the other encoders
#define AMF_VIDEO_ENCODER_CAP_MAX_THROUGHPUT                    L"MaxThroughput"            // amf_int64 - MAX throughput for H264 encoder in MB (16 x 16 pixel)
#define AMF_VIDEO_ENCODER_CAP_REQUESTED_THROUGHPUT              L"RequestedThroughput"      // amf_int64 - Currently total requested throughput for H264 encoder in MB (16 x 16 pixel)
#define AMF_VIDEO_ENCODER_CAPS_QUERY_TIMEOUT_SUPPORT            L"QueryTimeoutSupport"      // amf_bool - Timeout supported for QueryOutout call

// properties set on AMFComponent to control component creation
#define AMF_VIDEO_ENCODER_MEMORY_TYPE                           L"EncoderMemoryType"        // amf_int64(AMF_MEMORY_TYPE) , default is AMF_MEMORY_UNKNOWN, Values : AMF_MEMORY_DX11, AMF_MEMORY_DX9, AMF_MEMORY_VULKAN or AMF_MEMORY_UNKNOWN (auto)

// properties set on a frame to retrieve encoder statistics
#define AMF_VIDEO_ENCODER_STATISTICS_FEEDBACK                   L"StatisticsFeedback"                       // amf_bool; default = false; Signal encoder to collect and feedback statistics

#define AMF_VIDEO_ENCODER_STATISTIC_FRAME_QP                    L"StatisticsFeedbackFrameQP"                // amf_uin32; Rate control base frame QP
#define AMF_VIDEO_ENCODER_STATISTIC_AVERAGE_QP                  L"StatisticsFeedbackAvgQP"                  // amf_uin32; Average QP of all encoded CTBs in a picture
#define AMF_VIDEO_ENCODER_STATISTIC_MAX_QP                      L"StatisticsFeedbackMaxQP"                  // amf_uin32; Max QP among all encoded CTBs in a picture
#define AMF_VIDEO_ENCODER_STATISTIC_MIN_QP                      L"StatisticsFeedbackMinQP"                  // amf_uin32; Min QP among all encoded CTBs in a picture
#define AMF_VIDEO_ENCODER_STATISTIC_PIX_NUM_INTRA               L"StatisticsFeedbackPixNumIntra"            // amf_uin32; Number of the intra encoded pixels
#define AMF_VIDEO_ENCODER_STATISTIC_PIX_NUM_INTER               L"StatisticsFeedbackPixNumInter"            // amf_uin32; Number of the inter encoded pixels
#define AMF_VIDEO_ENCODER_STATISTIC_PIX_NUM_SKIP                L"StatisticsFeedbackPixNumSkip"             // amf_uin32; Number of the skip mode pixels
#define AMF_VIDEO_ENCODER_STATISTIC_BITCOUNT_RESIDUAL           L"StatisticsFeedbackBitcountResidual"       // amf_uin32; The bit count that corresponds to residual data
#define AMF_VIDEO_ENCODER_STATISTIC_BITCOUNT_MOTION             L"StatisticsFeedbackBitcountMotion"         // amf_uin32; The bit count that corresponds to motion vectors
#define AMF_VIDEO_ENCODER_STATISTIC_BITCOUNT_INTER              L"StatisticsFeedbackBitcountInter"          // amf_uin32; The bit count that are assigned to inter CTBs
#define AMF_VIDEO_ENCODER_STATISTIC_BITCOUNT_INTRA              L"StatisticsFeedbackBitcountIntra"          // amf_uin32; The bit count that are assigned to intra CTBs
#define AMF_VIDEO_ENCODER_STATISTIC_BITCOUNT_ALL_MINUS_HEADER   L"StatisticsFeedbackBitcountAllMinusHeader" // amf_uin32; The bit count of the bitstream excluding header
#define AMF_VIDEO_ENCODER_STATISTIC_MV_X                        L"StatisticsFeedbackMvX"                    // amf_uin32; Accumulated absolute values of horizontal MV’s
#define AMF_VIDEO_ENCODER_STATISTIC_MV_Y                        L"StatisticsFeedbackMvY"                    // amf_uin32; Accumulated absolute values of vertical MV’s

#endif //#ifndef AMF_VideoEncoderVCE_h
