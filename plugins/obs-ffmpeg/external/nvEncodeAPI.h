/*
 * This copyright notice applies to this header file only:
 *
 * Copyright (c) 2010-2018 NVIDIA Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the software, and to permit persons to whom the
 * software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * \file nvEncodeAPI.h
 *   NVIDIA GPUs - beginning with the Kepler generation - contain a hardware-based encoder
 *   (referred to as NVENC) which provides fully-accelerated hardware-based video encoding.
 *   NvEncodeAPI provides the interface for NVIDIA video encoder (NVENC).
 * \date 2011-2018
 *  This file contains the interface constants, structure definitions and function prototypes.
 */

#ifndef _NV_ENCODEAPI_H_
#define _NV_ENCODEAPI_H_

#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _MSC_VER
#ifndef _STDINT
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
#endif
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup ENCODER_STRUCTURE NvEncodeAPI Data structures
 * @{
 */

#ifdef _WIN32
#define NVENCAPI     __stdcall
typedef RECT NVENC_RECT;
#else
#define NVENCAPI
// =========================================================================================
#ifndef GUID
/*!
 * \struct GUID
 * Abstracts the GUID structure for non-windows platforms.
 */
// =========================================================================================
typedef struct
{
    uint32_t Data1;                                      /**< [in]: Specifies the first 8 hexadecimal digits of the GUID.                                */
    uint16_t Data2;                                      /**< [in]: Specifies the first group of 4 hexadecimal digits.                                   */
    uint16_t Data3;                                      /**< [in]: Specifies the second group of 4 hexadecimal digits.                                  */
    uint8_t  Data4[8];                                   /**< [in]: Array of 8 bytes. The first 2 bytes contain the third group of 4 hexadecimal digits.
                                                                    The remaining 6 bytes contain the final 12 hexadecimal digits.                       */
} GUID;
#endif // GUID

/**
 * \struct _NVENC_RECT
 * Defines a Rectangle. Used in ::NV_ENC_PREPROCESS_FRAME.
 */
typedef struct _NVENC_RECT
{
    uint32_t left;                                        /**< [in]: X coordinate of the upper left corner of rectangular area to be specified.       */
    uint32_t top;                                         /**< [in]: Y coordinate of the upper left corner of the rectangular area to be specified.   */
    uint32_t right;                                       /**< [in]: X coordinate of the bottom right corner of the rectangular area to be specified. */
    uint32_t bottom;                                      /**< [in]: Y coordinate of the bottom right corner of the rectangular area to be specified. */
} NVENC_RECT;

#endif // _WIN32

/** @} */ /* End of GUID and NVENC_RECT structure grouping*/

typedef void* NV_ENC_INPUT_PTR;             /**< NVENCODE API input buffer                              */
typedef void* NV_ENC_OUTPUT_PTR;            /**< NVENCODE API output buffer*/
typedef void* NV_ENC_REGISTERED_PTR;        /**< A Resource that has been registered with NVENCODE API*/

#define NVENCAPI_MAJOR_VERSION 8
#define NVENCAPI_MINOR_VERSION 1

#define NVENCAPI_VERSION (NVENCAPI_MAJOR_VERSION | (NVENCAPI_MINOR_VERSION << 24))

/**
 * Macro to generate per-structure version for use with API.
 */
#define NVENCAPI_STRUCT_VERSION(ver) ((uint32_t)NVENCAPI_VERSION | ((ver)<<16) | (0x7 << 28))


#define NVENC_INFINITE_GOPLENGTH  0xffffffff

#define NV_MAX_SEQ_HDR_LEN  (512)

// =========================================================================================
// Encode Codec GUIDS supported by the NvEncodeAPI interface.
// =========================================================================================

// {6BC82762-4E63-4ca4-AA85-1E50F321F6BF}
static const GUID NV_ENC_CODEC_H264_GUID =
{ 0x6bc82762, 0x4e63, 0x4ca4, { 0xaa, 0x85, 0x1e, 0x50, 0xf3, 0x21, 0xf6, 0xbf } };

// {790CDC88-4522-4d7b-9425-BDA9975F7603}
static const GUID NV_ENC_CODEC_HEVC_GUID =
{ 0x790cdc88, 0x4522, 0x4d7b, { 0x94, 0x25, 0xbd, 0xa9, 0x97, 0x5f, 0x76, 0x3 } };



// =========================================================================================
// *   Encode Profile GUIDS supported by the NvEncodeAPI interface.
// =========================================================================================

// {BFD6F8E7-233C-4341-8B3E-4818523803F4}
static const GUID NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID =
{ 0xbfd6f8e7, 0x233c, 0x4341, { 0x8b, 0x3e, 0x48, 0x18, 0x52, 0x38, 0x3, 0xf4 } };

// {0727BCAA-78C4-4c83-8C2F-EF3DFF267C6A}
static const GUID  NV_ENC_H264_PROFILE_BASELINE_GUID =
{ 0x727bcaa, 0x78c4, 0x4c83, { 0x8c, 0x2f, 0xef, 0x3d, 0xff, 0x26, 0x7c, 0x6a } };

// {60B5C1D4-67FE-4790-94D5-C4726D7B6E6D}
static const GUID  NV_ENC_H264_PROFILE_MAIN_GUID =
{ 0x60b5c1d4, 0x67fe, 0x4790, { 0x94, 0xd5, 0xc4, 0x72, 0x6d, 0x7b, 0x6e, 0x6d } };

// {E7CBC309-4F7A-4b89-AF2A-D537C92BE310}
static const GUID NV_ENC_H264_PROFILE_HIGH_GUID =
{ 0xe7cbc309, 0x4f7a, 0x4b89, { 0xaf, 0x2a, 0xd5, 0x37, 0xc9, 0x2b, 0xe3, 0x10 } };

// {7AC663CB-A598-4960-B844-339B261A7D52}
static const GUID  NV_ENC_H264_PROFILE_HIGH_444_GUID =
{ 0x7ac663cb, 0xa598, 0x4960, { 0xb8, 0x44, 0x33, 0x9b, 0x26, 0x1a, 0x7d, 0x52 } };

// {40847BF5-33F7-4601-9084-E8FE3C1DB8B7}
static const GUID NV_ENC_H264_PROFILE_STEREO_GUID =
{ 0x40847bf5, 0x33f7, 0x4601, { 0x90, 0x84, 0xe8, 0xfe, 0x3c, 0x1d, 0xb8, 0xb7 } };

// {CE788D20-AAA9-4318-92BB-AC7E858C8D36}
static const GUID NV_ENC_H264_PROFILE_SVC_TEMPORAL_SCALABILTY =
{ 0xce788d20, 0xaaa9, 0x4318, { 0x92, 0xbb, 0xac, 0x7e, 0x85, 0x8c, 0x8d, 0x36 } };

// {B405AFAC-F32B-417B-89C4-9ABEED3E5978}
static const GUID NV_ENC_H264_PROFILE_PROGRESSIVE_HIGH_GUID =
{ 0xb405afac, 0xf32b, 0x417b, { 0x89, 0xc4, 0x9a, 0xbe, 0xed, 0x3e, 0x59, 0x78 } };

// {AEC1BD87-E85B-48f2-84C3-98BCA6285072}
static const GUID NV_ENC_H264_PROFILE_CONSTRAINED_HIGH_GUID =
{ 0xaec1bd87, 0xe85b, 0x48f2, { 0x84, 0xc3, 0x98, 0xbc, 0xa6, 0x28, 0x50, 0x72 } };

// {B514C39A-B55B-40fa-878F-F1253B4DFDEC}
static const GUID NV_ENC_HEVC_PROFILE_MAIN_GUID =
{ 0xb514c39a, 0xb55b, 0x40fa, { 0x87, 0x8f, 0xf1, 0x25, 0x3b, 0x4d, 0xfd, 0xec } };

// {fa4d2b6c-3a5b-411a-8018-0a3f5e3c9be5}
static const GUID NV_ENC_HEVC_PROFILE_MAIN10_GUID =
{ 0xfa4d2b6c, 0x3a5b, 0x411a, { 0x80, 0x18, 0x0a, 0x3f, 0x5e, 0x3c, 0x9b, 0xe5 } };

// For HEVC Main 444 8 bit and HEVC Main 444 10 bit profiles only
// {51ec32b5-1b4c-453c-9cbd-b616bd621341}
static const GUID NV_ENC_HEVC_PROFILE_FREXT_GUID =
{ 0x51ec32b5, 0x1b4c, 0x453c, { 0x9c, 0xbd, 0xb6, 0x16, 0xbd, 0x62, 0x13, 0x41 } };

// =========================================================================================
// *   Preset GUIDS supported by the NvEncodeAPI interface.
// =========================================================================================
// {B2DFB705-4EBD-4C49-9B5F-24A777D3E587}
static const GUID NV_ENC_PRESET_DEFAULT_GUID =
{ 0xb2dfb705, 0x4ebd, 0x4c49, { 0x9b, 0x5f, 0x24, 0xa7, 0x77, 0xd3, 0xe5, 0x87 } };

// {60E4C59F-E846-4484-A56D-CD45BE9FDDF6}
static const GUID NV_ENC_PRESET_HP_GUID =
{ 0x60e4c59f, 0xe846, 0x4484, { 0xa5, 0x6d, 0xcd, 0x45, 0xbe, 0x9f, 0xdd, 0xf6 } };

// {34DBA71D-A77B-4B8F-9C3E-B6D5DA24C012}
static const GUID NV_ENC_PRESET_HQ_GUID =
{ 0x34dba71d, 0xa77b, 0x4b8f, { 0x9c, 0x3e, 0xb6, 0xd5, 0xda, 0x24, 0xc0, 0x12 } };

// {82E3E450-BDBB-4e40-989C-82A90DF9EF32}
static const GUID NV_ENC_PRESET_BD_GUID  =
{ 0x82e3e450, 0xbdbb, 0x4e40, { 0x98, 0x9c, 0x82, 0xa9, 0xd, 0xf9, 0xef, 0x32 } };

// {49DF21C5-6DFA-4feb-9787-6ACC9EFFB726}
static const GUID NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID  =
{ 0x49df21c5, 0x6dfa, 0x4feb, { 0x97, 0x87, 0x6a, 0xcc, 0x9e, 0xff, 0xb7, 0x26 } };

// {C5F733B9-EA97-4cf9-BEC2-BF78A74FD105}
static const GUID NV_ENC_PRESET_LOW_LATENCY_HQ_GUID  =
{ 0xc5f733b9, 0xea97, 0x4cf9, { 0xbe, 0xc2, 0xbf, 0x78, 0xa7, 0x4f, 0xd1, 0x5 } };

// {67082A44-4BAD-48FA-98EA-93056D150A58}
static const GUID NV_ENC_PRESET_LOW_LATENCY_HP_GUID =
{ 0x67082a44, 0x4bad, 0x48fa, { 0x98, 0xea, 0x93, 0x5, 0x6d, 0x15, 0xa, 0x58 } };

// {D5BFB716-C604-44e7-9BB8-DEA5510FC3AC}
static const GUID NV_ENC_PRESET_LOSSLESS_DEFAULT_GUID =
{ 0xd5bfb716, 0xc604, 0x44e7, { 0x9b, 0xb8, 0xde, 0xa5, 0x51, 0xf, 0xc3, 0xac } };

// {149998E7-2364-411d-82EF-179888093409}
static const GUID NV_ENC_PRESET_LOSSLESS_HP_GUID =
{ 0x149998e7, 0x2364, 0x411d, { 0x82, 0xef, 0x17, 0x98, 0x88, 0x9, 0x34, 0x9 } };

/**
 * \addtogroup ENCODER_STRUCTURE NvEncodeAPI Data structures
 * @{
 */

/**
 * Input frame encode modes
 */
typedef enum _NV_ENC_PARAMS_FRAME_FIELD_MODE
{
    NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME = 0x01,  /**< Frame mode */
    NV_ENC_PARAMS_FRAME_FIELD_MODE_FIELD = 0x02,  /**< Field mode */
    NV_ENC_PARAMS_FRAME_FIELD_MODE_MBAFF = 0x03   /**< MB adaptive frame/field */
} NV_ENC_PARAMS_FRAME_FIELD_MODE;

/**
 * Rate Control Modes
 */
typedef enum _NV_ENC_PARAMS_RC_MODE
{
    NV_ENC_PARAMS_RC_CONSTQP                = 0x0,       /**< Constant QP mode */
    NV_ENC_PARAMS_RC_VBR                    = 0x1,       /**< Variable bitrate mode */
    NV_ENC_PARAMS_RC_CBR                    = 0x2,       /**< Constant bitrate mode */
    NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ        = 0x8,       /**< low-delay CBR, high quality */
    NV_ENC_PARAMS_RC_CBR_HQ                 = 0x10,      /**< CBR, high quality (slower) */
    NV_ENC_PARAMS_RC_VBR_HQ                 = 0x20       /**< VBR, high quality (slower) */
} NV_ENC_PARAMS_RC_MODE;

/**
 * Emphasis Levels
 */
typedef enum _NV_ENC_EMPHASIS_MAP_LEVEL
{
    NV_ENC_EMPHASIS_MAP_LEVEL_0               = 0x0,       /**< Emphasis Map Level 0, for zero Delta QP value */
    NV_ENC_EMPHASIS_MAP_LEVEL_1               = 0x1,       /**< Emphasis Map Level 1, for very low Delta QP value */
    NV_ENC_EMPHASIS_MAP_LEVEL_2               = 0x2,       /**< Emphasis Map Level 2, for low Delta QP value */
    NV_ENC_EMPHASIS_MAP_LEVEL_3               = 0x3,       /**< Emphasis Map Level 3, for medium Delta QP value */
    NV_ENC_EMPHASIS_MAP_LEVEL_4               = 0x4,       /**< Emphasis Map Level 4, for high Delta QP value */
    NV_ENC_EMPHASIS_MAP_LEVEL_5               = 0x5        /**< Emphasis Map Level 5, for very high Delta QP value */
} NV_ENC_EMPHASIS_MAP_LEVEL;

/**
 * QP MAP MODE
 */
typedef enum _NV_ENC_QP_MAP_MODE
{
    NV_ENC_QP_MAP_DISABLED               = 0x0,             /**< Value in NV_ENC_PIC_PARAMS::qpDeltaMap have no effect. */
    NV_ENC_QP_MAP_EMPHASIS               = 0x1,             /**< Value in NV_ENC_PIC_PARAMS::qpDeltaMap will be treated as Empasis level. Currently this is only supported for H264 */
    NV_ENC_QP_MAP_DELTA                  = 0x2,             /**< Value in NV_ENC_PIC_PARAMS::qpDeltaMap will be treated as QP delta map. */
    NV_ENC_QP_MAP                        = 0x3,             /**< Currently This is not supported. Value in NV_ENC_PIC_PARAMS::qpDeltaMap will be treated as QP value.   */
} NV_ENC_QP_MAP_MODE;

#define NV_ENC_PARAMS_RC_VBR_MINQP              (NV_ENC_PARAMS_RC_MODE)0x4          /**< Deprecated */
#define NV_ENC_PARAMS_RC_2_PASS_QUALITY         NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ    /**< Deprecated */
#define NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP   NV_ENC_PARAMS_RC_CBR_HQ             /**< Deprecated */
#define NV_ENC_PARAMS_RC_2_PASS_VBR             NV_ENC_PARAMS_RC_VBR_HQ             /**< Deprecated */
#define NV_ENC_PARAMS_RC_CBR2                   NV_ENC_PARAMS_RC_CBR                /**< Deprecated */

/**
 * Input picture structure
 */
typedef enum _NV_ENC_PIC_STRUCT
{
    NV_ENC_PIC_STRUCT_FRAME             = 0x01,                 /**< Progressive frame */
    NV_ENC_PIC_STRUCT_FIELD_TOP_BOTTOM  = 0x02,                 /**< Field encoding top field first */
    NV_ENC_PIC_STRUCT_FIELD_BOTTOM_TOP  = 0x03                  /**< Field encoding bottom field first */
} NV_ENC_PIC_STRUCT;

/**
 * Input picture type
 */
typedef enum _NV_ENC_PIC_TYPE
{
    NV_ENC_PIC_TYPE_P               = 0x0,     /**< Forward predicted */
    NV_ENC_PIC_TYPE_B               = 0x01,    /**< Bi-directionally predicted picture */
    NV_ENC_PIC_TYPE_I               = 0x02,    /**< Intra predicted picture */
    NV_ENC_PIC_TYPE_IDR             = 0x03,    /**< IDR picture */
    NV_ENC_PIC_TYPE_BI              = 0x04,    /**< Bi-directionally predicted with only Intra MBs */
    NV_ENC_PIC_TYPE_SKIPPED         = 0x05,    /**< Picture is skipped */
    NV_ENC_PIC_TYPE_INTRA_REFRESH   = 0x06,    /**< First picture in intra refresh cycle */
    NV_ENC_PIC_TYPE_UNKNOWN         = 0xFF     /**< Picture type unknown */
} NV_ENC_PIC_TYPE;

/**
 * Motion vector precisions
 */
typedef enum _NV_ENC_MV_PRECISION
{
    NV_ENC_MV_PRECISION_DEFAULT     = 0x0,       /**<Driver selects QuarterPel motion vector precision by default*/
    NV_ENC_MV_PRECISION_FULL_PEL    = 0x01,    /**< FullPel  motion vector precision */
    NV_ENC_MV_PRECISION_HALF_PEL    = 0x02,    /**< HalfPel motion vector precision */
    NV_ENC_MV_PRECISION_QUARTER_PEL = 0x03     /**< QuarterPel motion vector precision */
} NV_ENC_MV_PRECISION;


/**
 * Input buffer formats
 */
typedef enum _NV_ENC_BUFFER_FORMAT
{
    NV_ENC_BUFFER_FORMAT_UNDEFINED                       = 0x00000000,  /**< Undefined buffer format */

    NV_ENC_BUFFER_FORMAT_NV12                            = 0x00000001,  /**< Semi-Planar YUV [Y plane followed by interleaved UV plane] */
    NV_ENC_BUFFER_FORMAT_YV12                            = 0x00000010,  /**< Planar YUV [Y plane followed by V and U planes] */
    NV_ENC_BUFFER_FORMAT_IYUV                            = 0x00000100,  /**< Planar YUV [Y plane followed by U and V planes] */
    NV_ENC_BUFFER_FORMAT_YUV444                          = 0x00001000,  /**< Planar YUV [Y plane followed by U and V planes] */
    NV_ENC_BUFFER_FORMAT_YUV420_10BIT                    = 0x00010000,  /**< 10 bit Semi-Planar YUV [Y plane followed by interleaved UV plane]. Each pixel of size 2 bytes. Most Significant 10 bits contain pixel data. */
    NV_ENC_BUFFER_FORMAT_YUV444_10BIT                    = 0x00100000,  /**< 10 bit Planar YUV444 [Y plane followed by U and V planes]. Each pixel of size 2 bytes. Most Significant 10 bits contain pixel data.  */
    NV_ENC_BUFFER_FORMAT_ARGB                            = 0x01000000,  /**< 8 bit Packed A8R8G8B8. This is a word-ordered format
                                                                             where a pixel is represented by a 32-bit word with B
                                                                             in the lowest 8 bits, G in the next 8 bits, R in the
                                                                             8 bits after that and A in the highest 8 bits. */
    NV_ENC_BUFFER_FORMAT_ARGB10                          = 0x02000000,  /**< 10 bit Packed A2R10G10B10. This is a word-ordered format
                                                                             where a pixel is represented by a 32-bit word with B
                                                                             in the lowest 10 bits, G in the next 10 bits, R in the
                                                                             10 bits after that and A in the highest 2 bits. */
    NV_ENC_BUFFER_FORMAT_AYUV                            = 0x04000000,  /**< 8 bit Packed A8Y8U8V8. This is a word-ordered format
                                                                             where a pixel is represented by a 32-bit word with V
                                                                             in the lowest 8 bits, U in the next 8 bits, Y in the
                                                                             8 bits after that and A in the highest 8 bits. */
    NV_ENC_BUFFER_FORMAT_ABGR                            = 0x10000000,  /**< 8 bit Packed A8B8G8R8. This is a word-ordered format
                                                                             where a pixel is represented by a 32-bit word with R
                                                                             in the lowest 8 bits, G in the next 8 bits, B in the
                                                                             8 bits after that and A in the highest 8 bits. */
    NV_ENC_BUFFER_FORMAT_ABGR10                          = 0x20000000,  /**< 10 bit Packed A2B10G10R10. This is a word-ordered format
                                                                             where a pixel is represented by a 32-bit word with R
                                                                             in the lowest 10 bits, G in the next 10 bits, B in the
                                                                             10 bits after that and A in the highest 2 bits. */
} NV_ENC_BUFFER_FORMAT;

#define NV_ENC_BUFFER_FORMAT_NV12_PL NV_ENC_BUFFER_FORMAT_NV12
#define NV_ENC_BUFFER_FORMAT_YV12_PL NV_ENC_BUFFER_FORMAT_YV12
#define NV_ENC_BUFFER_FORMAT_IYUV_PL NV_ENC_BUFFER_FORMAT_IYUV
#define NV_ENC_BUFFER_FORMAT_YUV444_PL NV_ENC_BUFFER_FORMAT_YUV444

/**
 * Encoding levels
 */
typedef enum _NV_ENC_LEVEL
{
    NV_ENC_LEVEL_AUTOSELECT         = 0,

    NV_ENC_LEVEL_H264_1             = 10,
    NV_ENC_LEVEL_H264_1b            = 9,
    NV_ENC_LEVEL_H264_11            = 11,
    NV_ENC_LEVEL_H264_12            = 12,
    NV_ENC_LEVEL_H264_13            = 13,
    NV_ENC_LEVEL_H264_2             = 20,
    NV_ENC_LEVEL_H264_21            = 21,
    NV_ENC_LEVEL_H264_22            = 22,
    NV_ENC_LEVEL_H264_3             = 30,
    NV_ENC_LEVEL_H264_31            = 31,
    NV_ENC_LEVEL_H264_32            = 32,
    NV_ENC_LEVEL_H264_4             = 40,
    NV_ENC_LEVEL_H264_41            = 41,
    NV_ENC_LEVEL_H264_42            = 42,
    NV_ENC_LEVEL_H264_5             = 50,
    NV_ENC_LEVEL_H264_51            = 51,
    NV_ENC_LEVEL_H264_52            = 52,


    NV_ENC_LEVEL_HEVC_1             = 30,
    NV_ENC_LEVEL_HEVC_2             = 60,
    NV_ENC_LEVEL_HEVC_21            = 63,
    NV_ENC_LEVEL_HEVC_3             = 90,
    NV_ENC_LEVEL_HEVC_31            = 93,
    NV_ENC_LEVEL_HEVC_4             = 120,
    NV_ENC_LEVEL_HEVC_41            = 123,
    NV_ENC_LEVEL_HEVC_5             = 150,
    NV_ENC_LEVEL_HEVC_51            = 153,
    NV_ENC_LEVEL_HEVC_52            = 156,
    NV_ENC_LEVEL_HEVC_6             = 180,
    NV_ENC_LEVEL_HEVC_61            = 183,
    NV_ENC_LEVEL_HEVC_62            = 186,

    NV_ENC_TIER_HEVC_MAIN           = 0,
    NV_ENC_TIER_HEVC_HIGH           = 1
} NV_ENC_LEVEL;

/**
 * Error Codes
 */
typedef enum _NVENCSTATUS
{
    /**
     * This indicates that API call returned with no errors.
     */
    NV_ENC_SUCCESS,

    /**
     * This indicates that no encode capable devices were detected.
     */
    NV_ENC_ERR_NO_ENCODE_DEVICE,

    /**
     * This indicates that devices pass by the client is not supported.
     */
    NV_ENC_ERR_UNSUPPORTED_DEVICE,

    /**
     * This indicates that the encoder device supplied by the client is not
     * valid.
     */
    NV_ENC_ERR_INVALID_ENCODERDEVICE,

    /**
     * This indicates that device passed to the API call is invalid.
     */
    NV_ENC_ERR_INVALID_DEVICE,

    /**
     * This indicates that device passed to the API call is no longer available and
     * needs to be reinitialized. The clients need to destroy the current encoder
     * session by freeing the allocated input output buffers and destroying the device
     * and create a new encoding session.
     */
    NV_ENC_ERR_DEVICE_NOT_EXIST,

    /**
     * This indicates that one or more of the pointers passed to the API call
     * is invalid.
     */
    NV_ENC_ERR_INVALID_PTR,

    /**
     * This indicates that completion event passed in ::NvEncEncodePicture() call
     * is invalid.
     */
    NV_ENC_ERR_INVALID_EVENT,

    /**
     * This indicates that one or more of the parameter passed to the API call
     * is invalid.
     */
    NV_ENC_ERR_INVALID_PARAM,

    /**
     * This indicates that an API call was made in wrong sequence/order.
     */
    NV_ENC_ERR_INVALID_CALL,

    /**
     * This indicates that the API call failed because it was unable to allocate
     * enough memory to perform the requested operation.
     */
    NV_ENC_ERR_OUT_OF_MEMORY,

    /**
     * This indicates that the encoder has not been initialized with
     * ::NvEncInitializeEncoder() or that initialization has failed.
     * The client cannot allocate input or output buffers or do any encoding
     * related operation before successfully initializing the encoder.
     */
    NV_ENC_ERR_ENCODER_NOT_INITIALIZED,

    /**
     * This indicates that an unsupported parameter was passed by the client.
     */
    NV_ENC_ERR_UNSUPPORTED_PARAM,

    /**
     * This indicates that the ::NvEncLockBitstream() failed to lock the output
     * buffer. This happens when the client makes a non blocking lock call to
     * access the output bitstream by passing NV_ENC_LOCK_BITSTREAM::doNotWait flag.
     * This is not a fatal error and client should retry the same operation after
     * few milliseconds.
     */
    NV_ENC_ERR_LOCK_BUSY,

    /**
     * This indicates that the size of the user buffer passed by the client is
     * insufficient for the requested operation.
     */
    NV_ENC_ERR_NOT_ENOUGH_BUFFER,

    /**
     * This indicates that an invalid struct version was used by the client.
     */
    NV_ENC_ERR_INVALID_VERSION,

    /**
     * This indicates that ::NvEncMapInputResource() API failed to map the client
     * provided input resource.
     */
    NV_ENC_ERR_MAP_FAILED,

    /**
     * This indicates encode driver requires more input buffers to produce an output
     * bitstream. If this error is returned from ::NvEncEncodePicture() API, this
     * is not a fatal error. If the client is encoding with B frames then,
     * ::NvEncEncodePicture() API might be buffering the input frame for re-ordering.
     *
     * A client operating in synchronous mode cannot call ::NvEncLockBitstream()
     * API on the output bitstream buffer if ::NvEncEncodePicture() returned the
     * ::NV_ENC_ERR_NEED_MORE_INPUT error code.
     * The client must continue providing input frames until encode driver returns
     * ::NV_ENC_SUCCESS. After receiving ::NV_ENC_SUCCESS status the client can call
     * ::NvEncLockBitstream() API on the output buffers in the same order in which
     * it has called ::NvEncEncodePicture().
     */
    NV_ENC_ERR_NEED_MORE_INPUT,

    /**
     * This indicates that the HW encoder is busy encoding and is unable to encode
     * the input. The client should call ::NvEncEncodePicture() again after few
     * milliseconds.
     */
    NV_ENC_ERR_ENCODER_BUSY,

    /**
     * This indicates that the completion event passed in ::NvEncEncodePicture()
     * API has not been registered with encoder driver using ::NvEncRegisterAsyncEvent().
     */
    NV_ENC_ERR_EVENT_NOT_REGISTERD,

    /**
     * This indicates that an unknown internal error has occurred.
     */
    NV_ENC_ERR_GENERIC,

    /**
     * This indicates that the client is attempting to use a feature
     * that is not available for the license type for the current system.
     */
    NV_ENC_ERR_INCOMPATIBLE_CLIENT_KEY,

    /**
     * This indicates that the client is attempting to use a feature
     * that is not implemented for the current version.
     */
    NV_ENC_ERR_UNIMPLEMENTED,

    /**
     * This indicates that the ::NvEncRegisterResource API failed to register the resource.
     */
    NV_ENC_ERR_RESOURCE_REGISTER_FAILED,

    /**
     * This indicates that the client is attempting to unregister a resource
     * that has not been successfully registered.
     */
    NV_ENC_ERR_RESOURCE_NOT_REGISTERED,

    /**
     * This indicates that the client is attempting to unmap a resource
     * that has not been successfully mapped.
     */
    NV_ENC_ERR_RESOURCE_NOT_MAPPED,

} NVENCSTATUS;

/**
 * Encode Picture encode flags.
 */
typedef enum _NV_ENC_PIC_FLAGS
{
    NV_ENC_PIC_FLAG_FORCEINTRA         = 0x1,   /**< Encode the current picture as an Intra picture */
    NV_ENC_PIC_FLAG_FORCEIDR           = 0x2,   /**< Encode the current picture as an IDR picture.
                                                     This flag is only valid when Picture type decision is taken by the Encoder
                                                     [_NV_ENC_INITIALIZE_PARAMS::enablePTD == 1]. */
    NV_ENC_PIC_FLAG_OUTPUT_SPSPPS      = 0x4,   /**< Write the sequence and picture header in encoded bitstream of the current picture */
    NV_ENC_PIC_FLAG_EOS                = 0x8,   /**< Indicates end of the input stream */
} NV_ENC_PIC_FLAGS;

/**
 * Memory heap to allocate input and output buffers.
 */
typedef enum _NV_ENC_MEMORY_HEAP
{
    NV_ENC_MEMORY_HEAP_AUTOSELECT      = 0, /**< Memory heap to be decided by the encoder driver based on the usage */
    NV_ENC_MEMORY_HEAP_VID             = 1, /**< Memory heap is in local video memory */
    NV_ENC_MEMORY_HEAP_SYSMEM_CACHED   = 2, /**< Memory heap is in cached system memory */
    NV_ENC_MEMORY_HEAP_SYSMEM_UNCACHED = 3  /**< Memory heap is in uncached system memory */
} NV_ENC_MEMORY_HEAP;

/**
 * B-frame used as reference modes
 */
typedef enum _NV_ENC_BFRAME_REF_MODE
{
    NV_ENC_BFRAME_REF_MODE_DISABLED = 0x0,          /**< B frame is not used for reference */
    NV_ENC_BFRAME_REF_MODE_EACH     = 0x1,          /**< Each B-frame will be used for reference. currently not supported */
    NV_ENC_BFRAME_REF_MODE_MIDDLE   = 0x2,          /**< Only(Number of B-frame)/2 th B-frame will be used for reference */
} NV_ENC_BFRAME_REF_MODE;

/**
 * H.264 entropy coding modes.
 */
typedef enum _NV_ENC_H264_ENTROPY_CODING_MODE
{
    NV_ENC_H264_ENTROPY_CODING_MODE_AUTOSELECT = 0x0,   /**< Entropy coding mode is auto selected by the encoder driver */
    NV_ENC_H264_ENTROPY_CODING_MODE_CABAC      = 0x1,   /**< Entropy coding mode is CABAC */
    NV_ENC_H264_ENTROPY_CODING_MODE_CAVLC      = 0x2    /**< Entropy coding mode is CAVLC */
} NV_ENC_H264_ENTROPY_CODING_MODE;

/**
 * H.264 specific Bdirect modes
 */
typedef enum _NV_ENC_H264_BDIRECT_MODE
{
    NV_ENC_H264_BDIRECT_MODE_AUTOSELECT = 0x0,          /**< BDirect mode is auto selected by the encoder driver */
    NV_ENC_H264_BDIRECT_MODE_DISABLE    = 0x1,          /**< Disable BDirect mode */
    NV_ENC_H264_BDIRECT_MODE_TEMPORAL   = 0x2,          /**< Temporal BDirect mode */
    NV_ENC_H264_BDIRECT_MODE_SPATIAL    = 0x3           /**< Spatial BDirect mode */
} NV_ENC_H264_BDIRECT_MODE;

/**
 * H.264 specific FMO usage
 */
typedef enum _NV_ENC_H264_FMO_MODE
{
    NV_ENC_H264_FMO_AUTOSELECT          = 0x0,          /**< FMO usage is auto selected by the encoder driver */
    NV_ENC_H264_FMO_ENABLE              = 0x1,          /**< Enable FMO */
    NV_ENC_H264_FMO_DISABLE             = 0x2,          /**< Disble FMO */
} NV_ENC_H264_FMO_MODE;

/**
 * H.264 specific Adaptive Transform modes
 */
typedef enum _NV_ENC_H264_ADAPTIVE_TRANSFORM_MODE
{
    NV_ENC_H264_ADAPTIVE_TRANSFORM_AUTOSELECT = 0x0,   /**< Adaptive Transform 8x8 mode is auto selected by the encoder driver*/
    NV_ENC_H264_ADAPTIVE_TRANSFORM_DISABLE    = 0x1,   /**< Adaptive Transform 8x8 mode disabled */
    NV_ENC_H264_ADAPTIVE_TRANSFORM_ENABLE     = 0x2,   /**< Adaptive Transform 8x8 mode should be used */
} NV_ENC_H264_ADAPTIVE_TRANSFORM_MODE;

/**
 * Stereo frame packing modes.
 */
typedef enum _NV_ENC_STEREO_PACKING_MODE
{
    NV_ENC_STEREO_PACKING_MODE_NONE             = 0x0,  /**< No Stereo packing required */
    NV_ENC_STEREO_PACKING_MODE_CHECKERBOARD     = 0x1,  /**< Checkerboard mode for packing stereo frames */
    NV_ENC_STEREO_PACKING_MODE_COLINTERLEAVE    = 0x2,  /**< Column Interleave mode for packing stereo frames */
    NV_ENC_STEREO_PACKING_MODE_ROWINTERLEAVE    = 0x3,  /**< Row Interleave mode for packing stereo frames */
    NV_ENC_STEREO_PACKING_MODE_SIDEBYSIDE       = 0x4,  /**< Side-by-side mode for packing stereo frames */
    NV_ENC_STEREO_PACKING_MODE_TOPBOTTOM        = 0x5,  /**< Top-Bottom mode for packing stereo frames */
    NV_ENC_STEREO_PACKING_MODE_FRAMESEQ         = 0x6   /**< Frame Sequential mode for packing stereo frames */
} NV_ENC_STEREO_PACKING_MODE;

/**
 *  Input Resource type
 */
typedef enum _NV_ENC_INPUT_RESOURCE_TYPE
{
    NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX          = 0x0,   /**< input resource type is a directx9 surface*/
    NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR    = 0x1,   /**< input resource type is a cuda device pointer surface*/
    NV_ENC_INPUT_RESOURCE_TYPE_CUDAARRAY        = 0x2,   /**< input resource type is a cuda array surface */
    NV_ENC_INPUT_RESOURCE_TYPE_OPENGL_TEX       = 0x3    /**< input resource type is an OpenGL texture */
} NV_ENC_INPUT_RESOURCE_TYPE;

/**
 *  Encoder Device type
 */
typedef enum _NV_ENC_DEVICE_TYPE
{
    NV_ENC_DEVICE_TYPE_DIRECTX          = 0x0,   /**< encode device type is a directx9 device */
    NV_ENC_DEVICE_TYPE_CUDA             = 0x1,   /**< encode device type is a cuda device */
    NV_ENC_DEVICE_TYPE_OPENGL           = 0x2    /**< encode device type is an OpenGL device.
                                                      Use of this device type is supported only on Linux */
} NV_ENC_DEVICE_TYPE;

/**
 * Encoder capabilities enumeration.
 */
typedef enum _NV_ENC_CAPS
{
    /**
     * Maximum number of B-Frames supported.
     */
    NV_ENC_CAPS_NUM_MAX_BFRAMES,

    /**
     * Rate control modes supported.
     * \n The API return value is a bitmask of the values in NV_ENC_PARAMS_RC_MODE.
     */
    NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES,

    /**
     * Indicates HW support for field mode encoding.
     * \n 0 : Interlaced mode encoding is not supported.
     * \n 1 : Interlaced field mode encoding is supported.
     * \n 2 : Interlaced frame encoding and field mode encoding are both supported.
     */
     NV_ENC_CAPS_SUPPORT_FIELD_ENCODING,

    /**
     * Indicates HW support for monochrome mode encoding.
     * \n 0 : Monochrome mode not supported.
     * \n 1 : Monochrome mode supported.
     */
    NV_ENC_CAPS_SUPPORT_MONOCHROME,

    /**
     * Indicates HW support for FMO.
     * \n 0 : FMO not supported.
     * \n 1 : FMO supported.
     */
    NV_ENC_CAPS_SUPPORT_FMO,

    /**
     * Indicates HW capability for Quarter pel motion estimation.
     * \n 0 : QuarterPel Motion Estimation not supported.
     * \n 1 : QuarterPel Motion Estimation supported.
     */
    NV_ENC_CAPS_SUPPORT_QPELMV,

    /**
     * H.264 specific. Indicates HW support for BDirect modes.
     * \n 0 : BDirect mode encoding not supported.
     * \n 1 : BDirect mode encoding supported.
     */
    NV_ENC_CAPS_SUPPORT_BDIRECT_MODE,

    /**
     * H264 specific. Indicates HW support for CABAC entropy coding mode.
     * \n 0 : CABAC entropy coding not supported.
     * \n 1 : CABAC entropy coding supported.
     */
    NV_ENC_CAPS_SUPPORT_CABAC,

    /**
     * Indicates HW support for Adaptive Transform.
     * \n 0 : Adaptive Transform not supported.
     * \n 1 : Adaptive Transform supported.
     */
    NV_ENC_CAPS_SUPPORT_ADAPTIVE_TRANSFORM,

    /**
     * Reserved enum field.
     */
    NV_ENC_CAPS_SUPPORT_RESERVED,

    /**
     * Indicates HW support for encoding Temporal layers.
     * \n 0 : Encoding Temporal layers not supported.
     * \n 1 : Encoding Temporal layers supported.
     */
    NV_ENC_CAPS_NUM_MAX_TEMPORAL_LAYERS,

    /**
     * Indicates HW support for Hierarchical P frames.
     * \n 0 : Hierarchical P frames not supported.
     * \n 1 : Hierarchical P frames supported.
     */
    NV_ENC_CAPS_SUPPORT_HIERARCHICAL_PFRAMES,

    /**
     * Indicates HW support for Hierarchical B frames.
     * \n 0 : Hierarchical B frames not supported.
     * \n 1 : Hierarchical B frames supported.
     */
    NV_ENC_CAPS_SUPPORT_HIERARCHICAL_BFRAMES,

    /**
     * Maximum Encoding level supported (See ::NV_ENC_LEVEL for details).
     */
    NV_ENC_CAPS_LEVEL_MAX,

    /**
     * Minimum Encoding level supported (See ::NV_ENC_LEVEL for details).
     */
    NV_ENC_CAPS_LEVEL_MIN,

    /**
     * Indicates HW support for separate colour plane encoding.
     * \n 0 : Separate colour plane encoding not supported.
     * \n 1 : Separate colour plane encoding supported.
     */
    NV_ENC_CAPS_SEPARATE_COLOUR_PLANE,

    /**
     * Maximum output width supported.
     */
    NV_ENC_CAPS_WIDTH_MAX,

    /**
     * Maximum output height supported.
     */
    NV_ENC_CAPS_HEIGHT_MAX,

    /**
     * Indicates Temporal Scalability Support.
     * \n 0 : Temporal SVC encoding not supported.
     * \n 1 : Temporal SVC encoding supported.
     */
    NV_ENC_CAPS_SUPPORT_TEMPORAL_SVC,

    /**
     * Indicates Dynamic Encode Resolution Change Support.
     * Support added from NvEncodeAPI version 2.0.
     * \n 0 : Dynamic Encode Resolution Change not supported.
     * \n 1 : Dynamic Encode Resolution Change supported.
     */
    NV_ENC_CAPS_SUPPORT_DYN_RES_CHANGE,

    /**
     * Indicates Dynamic Encode Bitrate Change Support.
     * Support added from NvEncodeAPI version 2.0.
     * \n 0 : Dynamic Encode bitrate change not supported.
     * \n 1 : Dynamic Encode bitrate change supported.
     */
    NV_ENC_CAPS_SUPPORT_DYN_BITRATE_CHANGE,

    /**
     * Indicates Forcing Constant QP On The Fly Support.
     * Support added from NvEncodeAPI version 2.0.
     * \n 0 : Forcing constant QP on the fly not supported.
     * \n 1 : Forcing constant QP on the fly supported.
     */
    NV_ENC_CAPS_SUPPORT_DYN_FORCE_CONSTQP,

    /**
     * Indicates Dynamic rate control mode Change Support.
     * \n 0 : Dynamic rate control mode change not supported.
     * \n 1 : Dynamic rate control mode change supported.
     */
    NV_ENC_CAPS_SUPPORT_DYN_RCMODE_CHANGE,

    /**
     * Indicates Subframe readback support for slice-based encoding.
     * \n 0 : Subframe readback not supported.
     * \n 1 : Subframe readback supported.
     */
    NV_ENC_CAPS_SUPPORT_SUBFRAME_READBACK,

    /**
     * Indicates Constrained Encoding mode support.
     * Support added from NvEncodeAPI version 2.0.
     * \n 0 : Constrained encoding mode not supported.
     * \n 1 : Constarined encoding mode supported.
     * If this mode is supported client can enable this during initialisation.
     * Client can then force a picture to be coded as constrained picture where
     * each slice in a constrained picture will have constrained_intra_pred_flag set to 1
     * and disable_deblocking_filter_idc will be set to 2 and prediction vectors for inter
     * macroblocks in each slice will be restricted to the slice region.
     */
    NV_ENC_CAPS_SUPPORT_CONSTRAINED_ENCODING,

    /**
     * Indicates Intra Refresh Mode Support.
     * Support added from NvEncodeAPI version 2.0.
     * \n 0 : Intra Refresh Mode not supported.
     * \n 1 : Intra Refresh Mode supported.
     */
    NV_ENC_CAPS_SUPPORT_INTRA_REFRESH,

    /**
     * Indicates Custom VBV Bufer Size support. It can be used for capping frame size.
     * Support added from NvEncodeAPI version 2.0.
     * \n 0 : Custom VBV buffer size specification from client, not supported.
     * \n 1 : Custom VBV buffer size specification from client, supported.
     */
    NV_ENC_CAPS_SUPPORT_CUSTOM_VBV_BUF_SIZE,

    /**
     * Indicates Dynamic Slice Mode Support.
     * Support added from NvEncodeAPI version 2.0.
     * \n 0 : Dynamic Slice Mode not supported.
     * \n 1 : Dynamic Slice Mode supported.
     */
    NV_ENC_CAPS_SUPPORT_DYNAMIC_SLICE_MODE,

    /**
     * Indicates Reference Picture Invalidation Support.
     * Support added from NvEncodeAPI version 2.0.
     * \n 0 : Reference Picture Invalidation not supported.
     * \n 1 : Reference Picture Invalidation supported.
     */
    NV_ENC_CAPS_SUPPORT_REF_PIC_INVALIDATION,

    /**
     * Indicates support for PreProcessing.
     * The API return value is a bitmask of the values defined in ::NV_ENC_PREPROC_FLAGS
     */
    NV_ENC_CAPS_PREPROC_SUPPORT,

    /**
    * Indicates support Async mode.
    * \n 0 : Async Encode mode not supported.
    * \n 1 : Async Encode mode supported.
    */
    NV_ENC_CAPS_ASYNC_ENCODE_SUPPORT,

    /**
     * Maximum MBs per frame supported.
     */
    NV_ENC_CAPS_MB_NUM_MAX,

    /**
     * Maximum aggregate throughput in MBs per sec.
     */
    NV_ENC_CAPS_MB_PER_SEC_MAX,

    /**
     * Indicates HW support for YUV444 mode encoding.
     * \n 0 : YUV444 mode encoding not supported.
     * \n 1 : YUV444 mode encoding supported.
     */
    NV_ENC_CAPS_SUPPORT_YUV444_ENCODE,

    /**
     * Indicates HW support for lossless encoding.
     * \n 0 : lossless encoding not supported.
     * \n 1 : lossless encoding supported.
     */
    NV_ENC_CAPS_SUPPORT_LOSSLESS_ENCODE,

     /**
     * Indicates HW support for Sample Adaptive Offset.
     * \n 0 : SAO not supported.
     * \n 1 : SAO encoding supported.
     */
    NV_ENC_CAPS_SUPPORT_SAO,

    /**
     * Indicates HW support for MEOnly Mode.
     * \n 0 : MEOnly Mode not supported.
     * \n 1 : MEOnly Mode supported for I and P frames.
     * \n 2 : MEOnly Mode supported for I, P and B frames.
     */
    NV_ENC_CAPS_SUPPORT_MEONLY_MODE,

    /**
     * Indicates HW support for lookahead encoding (enableLookahead=1).
     * \n 0 : Lookahead not supported.
     * \n 1 : Lookahead supported.
     */
    NV_ENC_CAPS_SUPPORT_LOOKAHEAD,

    /**
     * Indicates HW support for temporal AQ encoding (enableTemporalAQ=1).
     * \n 0 : Temporal AQ not supported.
     * \n 1 : Temporal AQ supported.
     */
    NV_ENC_CAPS_SUPPORT_TEMPORAL_AQ,
    /**
     * Indicates HW support for 10 bit encoding.
     * \n 0 : 10 bit encoding not supported.
     * \n 1 : 10 bit encoding supported.
     */
    NV_ENC_CAPS_SUPPORT_10BIT_ENCODE,
    /**
     * Maximum number of Long Term Reference frames supported
     */
    NV_ENC_CAPS_NUM_MAX_LTR_FRAMES,

    /**
     * Indicates HW support for Weighted Predicition.
     * \n 0 : Weighted Predicition not supported.
     * \n 1 : Weighted Predicition supported.
     */
    NV_ENC_CAPS_SUPPORT_WEIGHTED_PREDICTION,


    /**
     * On managed (vGPU) platforms (Windows only), this API, in conjunction with other GRID Management APIs, can be used
     * to estimate the residual capacity of the hardware encoder on the GPU as a percentage of the total available encoder capacity.
     * This API can be called at any time; i.e. during the encode session or before opening the encode session.
     * If the available encoder capacity is returned as zero, applications may choose to switch to software encoding
     * and continue to call this API (e.g. polling once per second) until capacity becomes available.
     *
     * On baremetal (non-virtualized GPU) and linux platforms, this API always returns 100.
     */
    NV_ENC_CAPS_DYNAMIC_QUERY_ENCODER_CAPACITY,

     /**
     * Indicates B as refererence support.
     * \n 0 : B as reference is not supported.
     * \n 1 : each B-Frame as reference is supported.
     * \n 2 : only Middle B-frame as reference is supported.
     */
    NV_ENC_CAPS_SUPPORT_BFRAME_REF_MODE,

    /**
     * Indicates HW support for Emphasis Level Map based delta QP computation.
     * \n 0 : Emphasis Level Map based delta QP not supported.
     * \n 1 : Emphasis Level Map based delta QP is supported.
     */
    NV_ENC_CAPS_SUPPORT_EMPHASIS_LEVEL_MAP,

     /**
     * Reserved - Not to be used by clients.
     */
    NV_ENC_CAPS_EXPOSED_COUNT
} NV_ENC_CAPS;

/**
 *  HEVC CU SIZE
 */
typedef enum _NV_ENC_HEVC_CUSIZE
{
    NV_ENC_HEVC_CUSIZE_AUTOSELECT = 0,
    NV_ENC_HEVC_CUSIZE_8x8        = 1,
    NV_ENC_HEVC_CUSIZE_16x16      = 2,
    NV_ENC_HEVC_CUSIZE_32x32      = 3,
    NV_ENC_HEVC_CUSIZE_64x64      = 4,
}NV_ENC_HEVC_CUSIZE;

/**
 * Input struct for querying Encoding capabilities.
 */
typedef struct _NV_ENC_CAPS_PARAM
{
    uint32_t version;                                  /**< [in]: Struct version. Must be set to ::NV_ENC_CAPS_PARAM_VER */
    NV_ENC_CAPS  capsToQuery;                          /**< [in]: Specifies the encode capability to be queried. Client should pass a member for ::NV_ENC_CAPS enum. */
    uint32_t reserved[62];                             /**< [in]: Reserved and must be set to 0 */
} NV_ENC_CAPS_PARAM;

/** NV_ENC_CAPS_PARAM struct version. */
#define NV_ENC_CAPS_PARAM_VER NVENCAPI_STRUCT_VERSION(1)


/**
 * Creation parameters for input buffer.
 */
typedef struct _NV_ENC_CREATE_INPUT_BUFFER
{
    uint32_t                  version;                 /**< [in]: Struct version. Must be set to ::NV_ENC_CREATE_INPUT_BUFFER_VER */
    uint32_t                  width;                   /**< [in]: Input buffer width */
    uint32_t                  height;                  /**< [in]: Input buffer width */
    NV_ENC_MEMORY_HEAP        memoryHeap;              /**< [in]: Deprecated. Do not use */
    NV_ENC_BUFFER_FORMAT      bufferFmt;               /**< [in]: Input buffer format */
    uint32_t                  reserved;                /**< [in]: Reserved and must be set to 0 */
    NV_ENC_INPUT_PTR          inputBuffer;             /**< [out]: Pointer to input buffer */
    void*                     pSysMemBuffer;           /**< [in]: Pointer to existing sysmem buffer */
    uint32_t                  reserved1[57];           /**< [in]: Reserved and must be set to 0 */
    void*                     reserved2[63];           /**< [in]: Reserved and must be set to NULL */
} NV_ENC_CREATE_INPUT_BUFFER;

/** NV_ENC_CREATE_INPUT_BUFFER struct version. */
#define NV_ENC_CREATE_INPUT_BUFFER_VER NVENCAPI_STRUCT_VERSION(1)

/**
 * Creation parameters for output bitstream buffer.
 */
typedef struct _NV_ENC_CREATE_BITSTREAM_BUFFER
{
    uint32_t              version;                     /**< [in]: Struct version. Must be set to ::NV_ENC_CREATE_BITSTREAM_BUFFER_VER */
    uint32_t              size;                        /**< [in]: Deprecated. Do not use */
    NV_ENC_MEMORY_HEAP    memoryHeap;                  /**< [in]: Deprecated. Do not use */
    uint32_t              reserved;                    /**< [in]: Reserved and must be set to 0 */
    NV_ENC_OUTPUT_PTR     bitstreamBuffer;             /**< [out]: Pointer to the output bitstream buffer */
    void*                 bitstreamBufferPtr;          /**< [out]: Reserved and should not be used */
    uint32_t              reserved1[58];               /**< [in]: Reserved and should be set to 0 */
    void*                 reserved2[64];               /**< [in]: Reserved and should be set to NULL */
} NV_ENC_CREATE_BITSTREAM_BUFFER;

/** NV_ENC_CREATE_BITSTREAM_BUFFER struct version. */
#define NV_ENC_CREATE_BITSTREAM_BUFFER_VER NVENCAPI_STRUCT_VERSION(1)

/**
 * Structs needed for ME only mode.
 */
typedef struct _NV_ENC_MVECTOR
{
    int16_t             mvx;               /**< the x component of MV in qpel units */
    int16_t             mvy;               /**< the y component of MV in qpel units */
} NV_ENC_MVECTOR;

/**
 * Motion vector structure per macroblock for H264 motion estimation.
 */
typedef struct _NV_ENC_H264_MV_DATA
{
    NV_ENC_MVECTOR      mv[4];             /**< up to 4 vectors for 8x8 partition */
    uint8_t             mbType;            /**< 0 (I), 1 (P), 2 (IPCM), 3 (B) */
    uint8_t             partitionType;     /**< Specifies the block partition type. 0:16x16, 1:8x8, 2:16x8, 3:8x16 */
    uint16_t            reserved;          /**< reserved padding for alignment */
    uint32_t            mbCost;
} NV_ENC_H264_MV_DATA;

/**
 * Motion vector structure per CU for HEVC motion estimation.
 */
typedef struct _NV_ENC_HEVC_MV_DATA
{
    NV_ENC_MVECTOR    mv[4];               /**< up to 4 vectors within a CU */
    uint8_t           cuType;              /**< 0 (I), 1(P), 2 (Skip) */
    uint8_t           cuSize;              /**< 0: 8x8, 1: 16x16, 2: 32x32, 3: 64x64 */
    uint8_t           partitionMode;       /**< The CU partition mode
                                                0 (2Nx2N), 1 (2NxN), 2(Nx2N), 3 (NxN),
                                                4 (2NxnU), 5 (2NxnD), 6(nLx2N), 7 (nRx2N) */
    uint8_t           lastCUInCTB;         /**< Marker to separate CUs in the current CTB from CUs in the next CTB */
} NV_ENC_HEVC_MV_DATA;

/**
 * Creation parameters for output motion vector buffer for ME only mode.
 */
typedef struct _NV_ENC_CREATE_MV_BUFFER
{
    uint32_t            version;           /**< [in]: Struct version. Must be set to NV_ENC_CREATE_MV_BUFFER_VER */
    NV_ENC_OUTPUT_PTR   mvBuffer;          /**< [out]: Pointer to the output motion vector buffer */
    uint32_t            reserved1[255];    /**< [in]: Reserved and should be set to 0 */
    void*               reserved2[63];     /**< [in]: Reserved and should be set to NULL */
} NV_ENC_CREATE_MV_BUFFER;

/** NV_ENC_CREATE_MV_BUFFER struct version*/
#define NV_ENC_CREATE_MV_BUFFER_VER NVENCAPI_STRUCT_VERSION(1)

/**
 * QP value for frames
 */
typedef struct _NV_ENC_QP
{
    uint32_t        qpInterP;
    uint32_t        qpInterB;
    uint32_t        qpIntra;
} NV_ENC_QP;

/**
 * Rate Control Configuration Paramters
 */
 typedef struct _NV_ENC_RC_PARAMS
 {
    uint32_t                        version;
    NV_ENC_PARAMS_RC_MODE           rateControlMode;                             /**< [in]: Specifies the rate control mode. Check support for various rate control modes using ::NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES caps. */
    NV_ENC_QP                       constQP;                                     /**< [in]: Specifies the initial QP to be used for encoding, these values would be used for all frames if in Constant QP mode. */
    uint32_t                        averageBitRate;                              /**< [in]: Specifies the average bitrate(in bits/sec) used for encoding. */
    uint32_t                        maxBitRate;                                  /**< [in]: Specifies the maximum bitrate for the encoded output. This is used for VBR and ignored for CBR mode. */
    uint32_t                        vbvBufferSize;                               /**< [in]: Specifies the VBV(HRD) buffer size. in bits. Set 0 to use the default VBV  buffer size. */
    uint32_t                        vbvInitialDelay;                             /**< [in]: Specifies the VBV(HRD) initial delay in bits. Set 0 to use the default VBV  initial delay .*/
    uint32_t                        enableMinQP          :1;                     /**< [in]: Set this to 1 if minimum QP used for rate control. */
    uint32_t                        enableMaxQP          :1;                     /**< [in]: Set this to 1 if maximum QP used for rate control. */
    uint32_t                        enableInitialRCQP    :1;                     /**< [in]: Set this to 1 if user suppplied initial QP is used for rate control. */
    uint32_t                        enableAQ             :1;                     /**< [in]: Set this to 1 to enable adaptive quantization (Spatial). */
    uint32_t                        reservedBitField1    :1;                     /**< [in]: Reserved bitfields and must be set to 0. */
    uint32_t                        enableLookahead      :1;                     /**< [in]: Set this to 1 to enable lookahead with depth <lookaheadDepth> (if lookahead is enabled, input frames must remain available to the encoder until encode completion) */
    uint32_t                        disableIadapt        :1;                     /**< [in]: Set this to 1 to disable adaptive I-frame insertion at scene cuts (only has an effect when lookahead is enabled) */
    uint32_t                        disableBadapt        :1;                     /**< [in]: Set this to 1 to disable adaptive B-frame decision (only has an effect when lookahead is enabled) */
    uint32_t                        enableTemporalAQ     :1;                     /**< [in]: Set this to 1 to enable temporal AQ for H.264 */
    uint32_t                        zeroReorderDelay     :1;                     /**< [in]: Set this to 1 to indicate zero latency operation (no reordering delay, num_reorder_frames=0) */
    uint32_t                        enableNonRefP        :1;                     /**< [in]: Set this to 1 to enable automatic insertion of non-reference P-frames (no effect if enablePTD=0) */
    uint32_t                        strictGOPTarget      :1;                     /**< [in]: Set this to 1 to minimize GOP-to-GOP rate fluctuations */
    uint32_t                        aqStrength           :4;                     /**< [in]: When AQ (Spatial) is enabled (i.e. NV_ENC_RC_PARAMS::enableAQ is set), this field is used to specify AQ strength. AQ strength scale is from 1 (low) - 15 (aggressive). If not set, strength is autoselected by driver. */
    uint32_t                        reservedBitFields    :16;                    /**< [in]: Reserved bitfields and must be set to 0 */
    NV_ENC_QP                       minQP;                                       /**< [in]: Specifies the minimum QP used for rate control. Client must set NV_ENC_CONFIG::enableMinQP to 1. */
    NV_ENC_QP                       maxQP;                                       /**< [in]: Specifies the maximum QP used for rate control. Client must set NV_ENC_CONFIG::enableMaxQP to 1. */
    NV_ENC_QP                       initialRCQP;                                 /**< [in]: Specifies the initial QP used for rate control. Client must set NV_ENC_CONFIG::enableInitialRCQP to 1. */
    uint32_t                        temporallayerIdxMask;                        /**< [in]: Specifies the temporal layers (as a bitmask) whose QPs have changed. Valid max bitmask is [2^NV_ENC_CAPS_NUM_MAX_TEMPORAL_LAYERS - 1] */
    uint8_t                         temporalLayerQP[8];                          /**< [in]: Specifies the temporal layer QPs used for rate control. Temporal layer index is used as as the array index */
    uint8_t                         targetQuality;                               /**< [in]: Target CQ (Constant Quality) level for VBR mode (range 0-51 with 0-automatic)  */
    uint8_t                         targetQualityLSB;                            /**< [in]: Fractional part of target quality (as 8.8 fixed point format) */
    uint16_t                        lookaheadDepth;                              /**< [in]: Maximum depth of lookahead with range 0-32 (only used if enableLookahead=1) */
    uint32_t                        reserved1;
    NV_ENC_QP_MAP_MODE              qpMapMode;                                   /**< [in]: This flag is used to interpret values in array pecified by NV_ENC_PIC_PARAMS::qpDeltaMap.
                                                                                            Set this to NV_ENC_QP_MAP_EMPHASIS to treat values specified by NV_ENC_PIC_PARAMS::qpDeltaMap as Emphasis level Map.
                                                                                            Emphasis Level can be assigned any value specified in enum NV_ENC_EMPHASIS_MAP_LEVEL.
                                                                                            Emphasis Level Map is used to specify regions to be encoded at varying levels of quality.
                                                                                            The hardware encoder adjusts the quantization within the image as per the provided emphasis map,
                                                                                            by adjusting the quantization parameter (QP) assigned to each macroblock. This adjustment is commonly called “Delta QP”.
                                                                                            The adjustment depends on the absolute QP decided by the rate control algorithm, and is applied after the rate control has decided each macroblock’s QP.
                                                                                            Since the Delta QP overrides rate control, enabling emphasis level map may violate bitrate and VBV buffersize constraints.
                                                                                            Emphasis level map is useful in situations when client has a priori knowledge of the image complexity (e.g. via use of NVFBC's Classification feature) and encoding those high-complexity areas at higher quality (lower QP) is important, even at the possible cost of violating bitrate/VBV buffersize constraints
                                                                                            This feature is not supported when AQ( Spatial/Temporal) is enabled.
                                                                                            This feature is only supported for H264 codec currently.

                                                                                            Set this to NV_ENC_QP_MAP_DELTA to treat values specified by NV_ENC_PIC_PARAMS::qpDeltaMap as QPDelta. This specify QP modifier to be applied on top of the QP chosen by rate control

                                                                                            Set this to NV_ENC_QP_MAP_DISABLED to ignore NV_ENC_PIC_PARAMS::qpDeltaMap values. In this case, qpDeltaMap should be set to NULL.

                                                                                            Other values are reserved for future use.*/
    uint32_t                        reserved[7];
 } NV_ENC_RC_PARAMS;

/** macro for constructing the version field of ::_NV_ENC_RC_PARAMS */
#define NV_ENC_RC_PARAMS_VER NVENCAPI_STRUCT_VERSION(1)



/**
 * \struct _NV_ENC_CONFIG_H264_VUI_PARAMETERS
 * H264 Video Usability Info parameters
 */
typedef struct _NV_ENC_CONFIG_H264_VUI_PARAMETERS
{
    uint32_t    overscanInfoPresentFlag;              /**< [in]: if set to 1 , it specifies that the overscanInfo is present */
    uint32_t    overscanInfo;                         /**< [in]: Specifies the overscan info(as defined in Annex E of the ITU-T Specification). */
    uint32_t    videoSignalTypePresentFlag;           /**< [in]: If set to 1, it specifies  that the videoFormat, videoFullRangeFlag and colourDescriptionPresentFlag are present. */
    uint32_t    videoFormat;                          /**< [in]: Specifies the source video format(as defined in Annex E of the ITU-T Specification).*/
    uint32_t    videoFullRangeFlag;                   /**< [in]: Specifies the output range of the luma and chroma samples(as defined in Annex E of the ITU-T Specification). */
    uint32_t    colourDescriptionPresentFlag;         /**< [in]: If set to 1, it specifies that the colourPrimaries, transferCharacteristics and colourMatrix are present. */
    uint32_t    colourPrimaries;                      /**< [in]: Specifies color primaries for converting to RGB(as defined in Annex E of the ITU-T Specification) */
    uint32_t    transferCharacteristics;              /**< [in]: Specifies the opto-electronic transfer characteristics to use (as defined in Annex E of the ITU-T Specification) */
    uint32_t    colourMatrix;                         /**< [in]: Specifies the matrix coefficients used in deriving the luma and chroma from the RGB primaries (as defined in Annex E of the ITU-T Specification). */
    uint32_t    chromaSampleLocationFlag;             /**< [in]: if set to 1 , it specifies that the chromaSampleLocationTop and chromaSampleLocationBot are present.*/
    uint32_t    chromaSampleLocationTop;              /**< [in]: Specifies the chroma sample location for top field(as defined in Annex E of the ITU-T Specification) */
    uint32_t    chromaSampleLocationBot;              /**< [in]: Specifies the chroma sample location for bottom field(as defined in Annex E of the ITU-T Specification) */
    uint32_t    bitstreamRestrictionFlag;             /**< [in]: if set to 1, it specifies the bitstream restriction parameters are present in the bitstream.*/
    uint32_t    reserved[15];
}NV_ENC_CONFIG_H264_VUI_PARAMETERS;

typedef NV_ENC_CONFIG_H264_VUI_PARAMETERS NV_ENC_CONFIG_HEVC_VUI_PARAMETERS;

/**
 * \struct _NVENC_EXTERNAL_ME_HINT_COUNTS_PER_BLOCKTYPE
 * External motion vector hint counts per block type.
 * H264 supports multiple hint while HEVC supports one hint for each valid candidate.
 */
typedef struct _NVENC_EXTERNAL_ME_HINT_COUNTS_PER_BLOCKTYPE
{
    uint32_t   numCandsPerBlk16x16                   : 4;   /**< [in]: Supported for H264,HEVC.It Specifies the number of candidates per 16x16 block. */
    uint32_t   numCandsPerBlk16x8                    : 4;   /**< [in]: Supported for H264 only.Specifies the number of candidates per 16x8 block. */
    uint32_t   numCandsPerBlk8x16                    : 4;   /**< [in]: Supported for H264 only.Specifies the number of candidates per 8x16 block. */
    uint32_t   numCandsPerBlk8x8                     : 4;   /**< [in]: Supported for H264,HEVC.Specifies the number of candidates per 8x8 block. */
    uint32_t   reserved                              : 16;  /**< [in]: Reserved for padding. */
    uint32_t   reserved1[3];                                /**< [in]: Reserved for future use. */
} NVENC_EXTERNAL_ME_HINT_COUNTS_PER_BLOCKTYPE;


/**
 * \struct _NVENC_EXTERNAL_ME_HINT
 * External Motion Vector hint structure.
 */
typedef struct _NVENC_EXTERNAL_ME_HINT
{
    int32_t    mvx         : 12;                        /**< [in]: Specifies the x component of integer pixel MV (relative to current MB) S12.0. */
    int32_t    mvy         : 10;                        /**< [in]: Specifies the y component of integer pixel MV (relative to current MB) S10.0 .*/
    int32_t    refidx      : 5;                         /**< [in]: Specifies the reference index (31=invalid). Current we support only 1 reference frame per direction for external hints, so \p refidx must be 0. */
    int32_t    dir         : 1;                         /**< [in]: Specifies the direction of motion estimation . 0=L0 1=L1.*/
    int32_t    partType    : 2;                         /**< [in]: Specifies the block partition type.0=16x16 1=16x8 2=8x16 3=8x8 (blocks in partition must be consecutive).*/
    int32_t    lastofPart  : 1;                         /**< [in]: Set to 1 for the last MV of (sub) partition  */
    int32_t    lastOfMB    : 1;                         /**< [in]: Set to 1 for the last MV of macroblock. */
} NVENC_EXTERNAL_ME_HINT;


/**
 * \struct _NV_ENC_CONFIG_H264
 * H264 encoder configuration parameters
 */
typedef struct _NV_ENC_CONFIG_H264
{
    uint32_t enableTemporalSVC         :1;                          /**< [in]: Set to 1 to enable SVC temporal*/
    uint32_t enableStereoMVC           :1;                          /**< [in]: Set to 1 to enable stereo MVC*/
    uint32_t hierarchicalPFrames       :1;                          /**< [in]: Set to 1 to enable hierarchical PFrames */
    uint32_t hierarchicalBFrames       :1;                          /**< [in]: Set to 1 to enable hierarchical BFrames */
    uint32_t outputBufferingPeriodSEI  :1;                          /**< [in]: Set to 1 to write SEI buffering period syntax in the bitstream */
    uint32_t outputPictureTimingSEI    :1;                          /**< [in]: Set to 1 to write SEI picture timing syntax in the bitstream.  When set for following rateControlMode : NV_ENC_PARAMS_RC_CBR, NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ,
                                                                               NV_ENC_PARAMS_RC_CBR_HQ, filler data is inserted if needed to achieve hrd bitrate */
    uint32_t outputAUD                 :1;                          /**< [in]: Set to 1 to write access unit delimiter syntax in bitstream */
    uint32_t disableSPSPPS             :1;                          /**< [in]: Set to 1 to disable writing of Sequence and Picture parameter info in bitstream */
    uint32_t outputFramePackingSEI     :1;                          /**< [in]: Set to 1 to enable writing of frame packing arrangement SEI messages to bitstream */
    uint32_t outputRecoveryPointSEI    :1;                          /**< [in]: Set to 1 to enable writing of recovery point SEI message */
    uint32_t enableIntraRefresh        :1;                          /**< [in]: Set to 1 to enable gradual decoder refresh or intra refresh. If the GOP structure uses B frames this will be ignored */
    uint32_t enableConstrainedEncoding :1;                          /**< [in]: Set this to 1 to enable constrainedFrame encoding where each slice in the constarined picture is independent of other slices
                                                                               Check support for constrained encoding using ::NV_ENC_CAPS_SUPPORT_CONSTRAINED_ENCODING caps. */
    uint32_t repeatSPSPPS              :1;                          /**< [in]: Set to 1 to enable writing of Sequence and Picture parameter for every IDR frame */
    uint32_t enableVFR                 :1;                          /**< [in]: Set to 1 to enable variable frame rate. */
    uint32_t enableLTR                 :1;                          /**< [in]: Set to 1 to enable LTR (Long Term Reference) frame support. LTR can be used in two modes: "LTR Trust" mode and "LTR Per Picture" mode.
                                                                               LTR Trust mode: In this mode, ltrNumFrames pictures after IDR are automatically marked as LTR. This mode is enabled by setting ltrTrustMode = 1.
                                                                                               Use of LTR Trust mode is strongly discouraged as this mode may be deprecated in future.
                                                                               LTR Per Picture mode: In this mode, client can control whether the current picture should be marked as LTR. Enable this mode by setting
                                                                                                     ltrTrustMode = 0 and ltrMarkFrame = 1 for the picture to be marked as LTR. This is the preferred mode
                                                                                                     for using LTR.
                                                                               Note that LTRs are not supported if encoding session is configured with B-frames */
    uint32_t qpPrimeYZeroTransformBypassFlag :1;                    /**< [in]: To enable lossless encode set this to 1, set QP to 0 and RC_mode to NV_ENC_PARAMS_RC_CONSTQP and profile to HIGH_444_PREDICTIVE_PROFILE.
                                                                               Check support for lossless encoding using ::NV_ENC_CAPS_SUPPORT_LOSSLESS_ENCODE caps.  */
    uint32_t useConstrainedIntraPred   :1;                          /**< [in]: Set 1 to enable constrained intra prediction. */
    uint32_t reservedBitFields         :15;                         /**< [in]: Reserved bitfields and must be set to 0 */
    uint32_t level;                                                 /**< [in]: Specifies the encoding level. Client is recommended to set this to NV_ENC_LEVEL_AUTOSELECT in order to enable the NvEncodeAPI interface to select the correct level. */
    uint32_t idrPeriod;                                             /**< [in]: Specifies the IDR interval. If not set, this is made equal to gopLength in NV_ENC_CONFIG.Low latency application client can set IDR interval to NVENC_INFINITE_GOPLENGTH so that IDR frames are not inserted automatically. */
    uint32_t separateColourPlaneFlag;                               /**< [in]: Set to 1 to enable 4:4:4 separate colour planes */
    uint32_t disableDeblockingFilterIDC;                            /**< [in]: Specifies the deblocking filter mode. Permissible value range: [0,2] */
    uint32_t numTemporalLayers;                                     /**< [in]: Specifies max temporal layers to be used for hierarchical coding. Valid value range is [1,::NV_ENC_CAPS_NUM_MAX_TEMPORAL_LAYERS] */
    uint32_t spsId;                                                 /**< [in]: Specifies the SPS id of the sequence header */
    uint32_t ppsId;                                                 /**< [in]: Specifies the PPS id of the picture header */
    NV_ENC_H264_ADAPTIVE_TRANSFORM_MODE adaptiveTransformMode;      /**< [in]: Specifies the AdaptiveTransform Mode. Check support for AdaptiveTransform mode using ::NV_ENC_CAPS_SUPPORT_ADAPTIVE_TRANSFORM caps. */
    NV_ENC_H264_FMO_MODE                fmoMode;                    /**< [in]: Specified the FMO Mode. Check support for FMO using ::NV_ENC_CAPS_SUPPORT_FMO caps. */
    NV_ENC_H264_BDIRECT_MODE            bdirectMode;                /**< [in]: Specifies the BDirect mode. Check support for BDirect mode using ::NV_ENC_CAPS_SUPPORT_BDIRECT_MODE caps.*/
    NV_ENC_H264_ENTROPY_CODING_MODE     entropyCodingMode;          /**< [in]: Specifies the entropy coding mode. Check support for CABAC mode using ::NV_ENC_CAPS_SUPPORT_CABAC caps. */
    NV_ENC_STEREO_PACKING_MODE          stereoMode;                 /**< [in]: Specifies the stereo frame packing mode which is to be signalled in frame packing arrangement SEI */
    uint32_t                            intraRefreshPeriod;         /**< [in]: Specifies the interval between successive intra refresh if enableIntrarefresh is set. Requires enableIntraRefresh to be set.
                                                                               Will be disabled if NV_ENC_CONFIG::gopLength is not set to NVENC_INFINITE_GOPLENGTH. */
    uint32_t                            intraRefreshCnt;            /**< [in]: Specifies the length of intra refresh in number of frames for periodic intra refresh. This value should be smaller than intraRefreshPeriod */
    uint32_t                            maxNumRefFrames;            /**< [in]: Specifies the DPB size used for encoding. Setting it to 0 will let driver use the default dpb size.
                                                                               The low latency application which wants to invalidate reference frame as an error resilience tool
                                                                               is recommended to use a large DPB size so that the encoder can keep old reference frames which can be used if recent
                                                                               frames are invalidated. */
    uint32_t                            sliceMode;                  /**< [in]: This parameter in conjunction with sliceModeData specifies the way in which the picture is divided into slices
                                                                               sliceMode = 0 MB based slices, sliceMode = 1 Byte based slices, sliceMode = 2 MB row based slices, sliceMode = 3 numSlices in Picture.
                                                                               When forceIntraRefreshWithFrameCnt is set it will have priority over sliceMode setting
                                                                               When sliceMode == 0 and sliceModeData == 0 whole picture will be coded with one slice */
    uint32_t                            sliceModeData;              /**< [in]: Specifies the parameter needed for sliceMode. For:
                                                                               sliceMode = 0, sliceModeData specifies # of MBs in each slice (except last slice)
                                                                               sliceMode = 1, sliceModeData specifies maximum # of bytes in each slice (except last slice)
                                                                               sliceMode = 2, sliceModeData specifies # of MB rows in each slice (except last slice)
                                                                               sliceMode = 3, sliceModeData specifies number of slices in the picture. Driver will divide picture into slices optimally */
    NV_ENC_CONFIG_H264_VUI_PARAMETERS   h264VUIParameters;          /**< [in]: Specifies the H264 video usability info pamameters */
    uint32_t                            ltrNumFrames;               /**< [in]: Specifies the number of LTR frames. This parameter has different meaning in two LTR modes.
                                                                               In "LTR Trust" mode (ltrTrustMode = 1), encoder will mark the first ltrNumFrames base layer reference frames within each IDR interval as LTR.
                                                                               In "LTR Per Picture" mode (ltrTrustMode = 0 and ltrMarkFrame = 1), ltrNumFrames specifies maximum number of LTR frames in DPB. */
    uint32_t                            ltrTrustMode;               /**< [in]: Specifies the LTR operating mode. See comments near NV_ENC_CONFIG_H264::enableLTR for description of the two modes.
                                                                               Set to 1 to use "LTR Trust" mode of LTR operation. Clients are discouraged to use "LTR Trust" mode as this mode may
                                                                               be deprecated in future releases.
                                                                               Set to 0 when using "LTR Per Picture" mode of LTR operation. */
    uint32_t                            chromaFormatIDC;            /**< [in]: Specifies the chroma format. Should be set to 1 for yuv420 input, 3 for yuv444 input.
                                                                               Check support for YUV444 encoding using ::NV_ENC_CAPS_SUPPORT_YUV444_ENCODE caps.*/
    uint32_t                            maxTemporalLayers;          /**< [in]: Specifies the max temporal layer used for hierarchical coding. */
    NV_ENC_BFRAME_REF_MODE              useBFramesAsRef;            /**< [in]: Specifies the B-Frame as reference mode. Check support for useBFramesAsRef mode using ::NV_ENC_CAPS_SUPPORT_BFRAME_REF_MODE caps.*/
    uint32_t                            reserved1[269];             /**< [in]: Reserved and must be set to 0 */
    void*                               reserved2[64];              /**< [in]: Reserved and must be set to NULL */
} NV_ENC_CONFIG_H264;

/**
 * \struct _NV_ENC_CONFIG_HEVC
 * HEVC encoder configuration parameters to be set during initialization.
 */
typedef struct _NV_ENC_CONFIG_HEVC
{
    uint32_t level;                                                 /**< [in]: Specifies the level of the encoded bitstream.*/
    uint32_t tier;                                                  /**< [in]: Specifies the level tier of the encoded bitstream.*/
    NV_ENC_HEVC_CUSIZE minCUSize;                                   /**< [in]: Specifies the minimum size of luma coding unit.*/
    NV_ENC_HEVC_CUSIZE maxCUSize;                                   /**< [in]: Specifies the maximum size of luma coding unit. Currently NVENC SDK only supports maxCUSize equal to NV_ENC_HEVC_CUSIZE_32x32.*/
    uint32_t useConstrainedIntraPred               :1;              /**< [in]: Set 1 to enable constrained intra prediction. */
    uint32_t disableDeblockAcrossSliceBoundary     :1;              /**< [in]: Set 1 to disable in loop filtering across slice boundary.*/
    uint32_t outputBufferingPeriodSEI              :1;              /**< [in]: Set 1 to write SEI buffering period syntax in the bitstream */
    uint32_t outputPictureTimingSEI                :1;              /**< [in]: Set 1 to write SEI picture timing syntax in the bitstream */
    uint32_t outputAUD                             :1;              /**< [in]: Set 1 to write Access Unit Delimiter syntax. */
    uint32_t enableLTR                             :1;              /**< [in]: Set to 1 to enable LTR (Long Term Reference) frame support. LTR can be used in two modes: "LTR Trust" mode and "LTR Per Picture" mode.
                                                                               LTR Trust mode: In this mode, ltrNumFrames pictures after IDR are automatically marked as LTR. This mode is enabled by setting ltrTrustMode = 1.
                                                                                               Use of LTR Trust mode is strongly discouraged as this mode may be deprecated in future releases.
                                                                               LTR Per Picture mode: In this mode, client can control whether the current picture should be marked as LTR. Enable this mode by setting
                                                                                                     ltrTrustMode = 0 and ltrMarkFrame = 1 for the picture to be marked as LTR. This is the preferred mode
                                                                                                     for using LTR.
                                                                               Note that LTRs are not supported if encoding session is configured with B-frames */
    uint32_t disableSPSPPS                         :1;              /**< [in]: Set 1 to disable VPS,SPS and PPS signalling in the bitstream. */
    uint32_t repeatSPSPPS                          :1;              /**< [in]: Set 1 to output VPS,SPS and PPS for every IDR frame.*/
    uint32_t enableIntraRefresh                    :1;              /**< [in]: Set 1 to enable gradual decoder refresh or intra refresh. If the GOP structure uses B frames this will be ignored */
    uint32_t chromaFormatIDC                       :2;              /**< [in]: Specifies the chroma format. Should be set to 1 for yuv420 input, 3 for yuv444 input.*/
    uint32_t pixelBitDepthMinus8                   :3;              /**< [in]: Specifies pixel bit depth minus 8. Should be set to 0 for 8 bit input, 2 for 10 bit input.*/
    uint32_t reserved                              :18;             /**< [in]: Reserved bitfields.*/
    uint32_t idrPeriod;                                             /**< [in]: Specifies the IDR interval. If not set, this is made equal to gopLength in NV_ENC_CONFIG.Low latency application client can set IDR interval to NVENC_INFINITE_GOPLENGTH so that IDR frames are not inserted automatically. */
    uint32_t intraRefreshPeriod;                                    /**< [in]: Specifies the interval between successive intra refresh if enableIntrarefresh is set. Requires enableIntraRefresh to be set.
                                                                    Will be disabled if NV_ENC_CONFIG::gopLength is not set to NVENC_INFINITE_GOPLENGTH. */
    uint32_t intraRefreshCnt;                                       /**< [in]: Specifies the length of intra refresh in number of frames for periodic intra refresh. This value should be smaller than intraRefreshPeriod */
    uint32_t maxNumRefFramesInDPB;                                  /**< [in]: Specifies the maximum number of references frames in the DPB.*/
    uint32_t ltrNumFrames;                                          /**< [in]: This parameter has different meaning in two LTR modes.
                                                                               In "LTR Trust" mode (ltrTrustMode = 1), encoder will mark the first ltrNumFrames base layer reference frames within each IDR interval as LTR.
                                                                               In "LTR Per Picture" mode (ltrTrustMode = 0 and ltrMarkFrame = 1), ltrNumFrames specifies maximum number of LTR frames in DPB. */
    uint32_t vpsId;                                                 /**< [in]: Specifies the VPS id of the video parameter set */
    uint32_t spsId;                                                 /**< [in]: Specifies the SPS id of the sequence header */
    uint32_t ppsId;                                                 /**< [in]: Specifies the PPS id of the picture header */
    uint32_t sliceMode;                                             /**< [in]: This parameter in conjunction with sliceModeData specifies the way in which the picture is divided into slices
                                                                                sliceMode = 0 CTU based slices, sliceMode = 1 Byte based slices, sliceMode = 2 CTU row based slices, sliceMode = 3, numSlices in Picture
                                                                                When sliceMode == 0 and sliceModeData == 0 whole picture will be coded with one slice */
    uint32_t sliceModeData;                                         /**< [in]: Specifies the parameter needed for sliceMode. For:
                                                                                sliceMode = 0, sliceModeData specifies # of CTUs in each slice (except last slice)
                                                                                sliceMode = 1, sliceModeData specifies maximum # of bytes in each slice (except last slice)
                                                                                sliceMode = 2, sliceModeData specifies # of CTU rows in each slice (except last slice)
                                                                                sliceMode = 3, sliceModeData specifies number of slices in the picture. Driver will divide picture into slices optimally */
    uint32_t maxTemporalLayersMinus1;                               /**< [in]: Specifies the max temporal layer used for hierarchical coding. */
    NV_ENC_CONFIG_HEVC_VUI_PARAMETERS   hevcVUIParameters;          /**< [in]: Specifies the HEVC video usability info pamameters */
    uint32_t ltrTrustMode;                                          /**< [in]: Specifies the LTR operating mode. See comments near NV_ENC_CONFIG_HEVC::enableLTR for description of the two modes.
                                                                               Set to 1 to use "LTR Trust" mode of LTR operation. Clients are discouraged to use "LTR Trust" mode as this mode may
                                                                               be deprecated in future releases.
                                                                               Set to 0 when using "LTR Per Picture" mode of LTR operation. */
    uint32_t                            reserved1[217];             /**< [in]: Reserved and must be set to 0.*/
    void*    reserved2[64];                                         /**< [in]: Reserved and must be set to NULL */
} NV_ENC_CONFIG_HEVC;

/**
 * \struct _NV_ENC_CONFIG_H264_MEONLY
 * H264 encoder configuration parameters for ME only Mode
 *
 */
typedef struct _NV_ENC_CONFIG_H264_MEONLY
{
    uint32_t disablePartition16x16 :1;                          /**< [in]: Disable MotionEstimation on 16x16 blocks*/
    uint32_t disablePartition8x16  :1;                          /**< [in]: Disable MotionEstimation on 8x16 blocks*/
    uint32_t disablePartition16x8  :1;                          /**< [in]: Disable MotionEstimation on 16x8 blocks*/
    uint32_t disablePartition8x8   :1;                          /**< [in]: Disable MotionEstimation on 8x8 blocks*/
    uint32_t disableIntraSearch    :1;                          /**< [in]: Disable Intra search during MotionEstimation*/
    uint32_t bStereoEnable         :1;                          /**< [in]: Enable Stereo Mode for Motion Estimation where each view is independently executed*/
    uint32_t reserved              :26;                         /**< [in]: Reserved and must be set to 0 */
    uint32_t reserved1 [255];                                   /**< [in]: Reserved and must be set to 0 */
    void*    reserved2[64];                                     /**< [in]: Reserved and must be set to NULL */
} NV_ENC_CONFIG_H264_MEONLY;


/**
 * \struct _NV_ENC_CONFIG_HEVC_MEONLY
 * HEVC encoder configuration parameters for ME only Mode
 *
 */
typedef struct _NV_ENC_CONFIG_HEVC_MEONLY
{
    uint32_t reserved [256];                                   /**< [in]: Reserved and must be set to 0 */
    void*    reserved1[64];                                     /**< [in]: Reserved and must be set to NULL */
} NV_ENC_CONFIG_HEVC_MEONLY;

/**
 * \struct _NV_ENC_CODEC_CONFIG
 * Codec-specific encoder configuration parameters to be set during initialization.
 */
typedef union _NV_ENC_CODEC_CONFIG
{
    NV_ENC_CONFIG_H264        h264Config;                /**< [in]: Specifies the H.264-specific encoder configuration. */
    NV_ENC_CONFIG_HEVC        hevcConfig;                /**< [in]: Specifies the HEVC-specific encoder configuration. */
    NV_ENC_CONFIG_H264_MEONLY h264MeOnlyConfig;          /**< [in]: Specifies the H.264-specific ME only encoder configuration. */
    NV_ENC_CONFIG_HEVC_MEONLY hevcMeOnlyConfig;          /**< [in]: Specifies the HEVC-specific ME only encoder configuration. */
    uint32_t                reserved[320];               /**< [in]: Reserved and must be set to 0 */
} NV_ENC_CODEC_CONFIG;


/**
 * \struct _NV_ENC_CONFIG
 * Encoder configuration parameters to be set during initialization.
 */
typedef struct _NV_ENC_CONFIG
{
    uint32_t                        version;                                     /**< [in]: Struct version. Must be set to ::NV_ENC_CONFIG_VER. */
    GUID                            profileGUID;                                 /**< [in]: Specifies the codec profile guid. If client specifies \p NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID the NvEncodeAPI interface will select the appropriate codec profile. */
    uint32_t                        gopLength;                                   /**< [in]: Specifies the number of pictures in one GOP. Low latency application client can set goplength to NVENC_INFINITE_GOPLENGTH so that keyframes are not inserted automatically. */
    int32_t                         frameIntervalP;                              /**< [in]: Specifies the GOP pattern as follows: \p frameIntervalP = 0: I, 1: IPP, 2: IBP, 3: IBBP  If goplength is set to NVENC_INFINITE_GOPLENGTH \p frameIntervalP should be set to 1. */
    uint32_t                        monoChromeEncoding;                          /**< [in]: Set this to 1 to enable monochrome encoding for this session. */
    NV_ENC_PARAMS_FRAME_FIELD_MODE  frameFieldMode;                              /**< [in]: Specifies the frame/field mode.
                                                                                            Check support for field encoding using ::NV_ENC_CAPS_SUPPORT_FIELD_ENCODING caps.
                                                                                            Using a frameFieldMode other than NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME for RGB input is not supported. */
    NV_ENC_MV_PRECISION             mvPrecision;                                 /**< [in]: Specifies the desired motion vector prediction precision. */
    NV_ENC_RC_PARAMS                rcParams;                                    /**< [in]: Specifies the rate control parameters for the current encoding session. */
    NV_ENC_CODEC_CONFIG             encodeCodecConfig;                           /**< [in]: Specifies the codec specific config parameters through this union. */
    uint32_t                        reserved [278];                              /**< [in]: Reserved and must be set to 0 */
    void*                           reserved2[64];                               /**< [in]: Reserved and must be set to NULL */
} NV_ENC_CONFIG;

/** macro for constructing the version field of ::_NV_ENC_CONFIG */
#define NV_ENC_CONFIG_VER (NVENCAPI_STRUCT_VERSION(7) | ( 1<<31 ))


/**
 * \struct _NV_ENC_INITIALIZE_PARAMS
 * Encode Session Initialization parameters.
 */
typedef struct _NV_ENC_INITIALIZE_PARAMS
{
    uint32_t                                   version;                         /**< [in]: Struct version. Must be set to ::NV_ENC_INITIALIZE_PARAMS_VER. */
    GUID                                       encodeGUID;                      /**< [in]: Specifies the Encode GUID for which the encoder is being created. ::NvEncInitializeEncoder() API will fail if this is not set, or set to unsupported value. */
    GUID                                       presetGUID;                      /**< [in]: Specifies the preset for encoding. If the preset GUID is set then , the preset configuration will be applied before any other parameter. */
    uint32_t                                   encodeWidth;                     /**< [in]: Specifies the encode width. If not set ::NvEncInitializeEncoder() API will fail. */
    uint32_t                                   encodeHeight;                    /**< [in]: Specifies the encode height. If not set ::NvEncInitializeEncoder() API will fail. */
    uint32_t                                   darWidth;                        /**< [in]: Specifies the display aspect ratio Width. */
    uint32_t                                   darHeight;                       /**< [in]: Specifies the display aspect ratio height. */
    uint32_t                                   frameRateNum;                    /**< [in]: Specifies the numerator for frame rate used for encoding in frames per second ( Frame rate = frameRateNum / frameRateDen ). */
    uint32_t                                   frameRateDen;                    /**< [in]: Specifies the denominator for frame rate used for encoding in frames per second ( Frame rate = frameRateNum / frameRateDen ). */
    uint32_t                                   enableEncodeAsync;               /**< [in]: Set this to 1 to enable asynchronous mode and is expected to use events to get picture completion notification. */
    uint32_t                                   enablePTD;                       /**< [in]: Set this to 1 to enable the Picture Type Decision is be taken by the NvEncodeAPI interface. */
    uint32_t                                   reportSliceOffsets        :1;    /**< [in]: Set this to 1 to enable reporting slice offsets in ::_NV_ENC_LOCK_BITSTREAM. NV_ENC_INITIALIZE_PARAMS::enableEncodeAsync must be set to 0 to use this feature. Client must set this to 0 if NV_ENC_CONFIG_H264::sliceMode is 1 on Kepler GPUs */
    uint32_t                                   enableSubFrameWrite       :1;    /**< [in]: Set this to 1 to write out available bitstream to memory at subframe intervals */
    uint32_t                                   enableExternalMEHints     :1;    /**< [in]: Set to 1 to enable external ME hints for the current frame. For NV_ENC_INITIALIZE_PARAMS::enablePTD=1 with B frames, programming L1 hints is optional for B frames since Client doesn't know internal GOP structure.
                                                                                           NV_ENC_PIC_PARAMS::meHintRefPicDist should preferably be set with enablePTD=1. */
    uint32_t                                   enableMEOnlyMode          :1;    /**< [in]: Set to 1 to enable ME Only Mode .*/
    uint32_t                                   enableWeightedPrediction  :1;    /**< [in]: Set this to 1 to enable weighted prediction. Not supported if encode session is configured for B-Frames( 'frameIntervalP' in NV_ENC_CONFIG is greater than 1).*/
    uint32_t                                   reservedBitFields         :27;   /**< [in]: Reserved bitfields and must be set to 0 */
    uint32_t                                   privDataSize;                    /**< [in]: Reserved private data buffer size and must be set to 0 */
    void*                                      privData;                        /**< [in]: Reserved private data buffer and must be set to NULL */
    NV_ENC_CONFIG*                             encodeConfig;                    /**< [in]: Specifies the advanced codec specific structure. If client has sent a valid codec config structure, it will override parameters set by the NV_ENC_INITIALIZE_PARAMS::presetGUID parameter. If set to NULL the NvEncodeAPI interface will use the NV_ENC_INITIALIZE_PARAMS::presetGUID to set the codec specific parameters.
                                                                                           Client can also optionally query the NvEncodeAPI interface to get codec specific parameters for a presetGUID using ::NvEncGetEncodePresetConfig() API. It can then modify (if required) some of the codec config parameters and send down a custom config structure as part of ::_NV_ENC_INITIALIZE_PARAMS.
                                                                                           Even in this case client is recommended to pass the same preset guid it has used in ::NvEncGetEncodePresetConfig() API to query the config structure; as NV_ENC_INITIALIZE_PARAMS::presetGUID. This will not override the custom config structure but will be used to determine other Encoder HW specific parameters not exposed in the API. */
    uint32_t                                   maxEncodeWidth;                  /**< [in]: Maximum encode width to be used for current Encode session.
                                                                                           Client should allocate output buffers according to this dimension for dynamic resolution change. If set to 0, Encoder will not allow dynamic resolution change. */
    uint32_t                                   maxEncodeHeight;                 /**< [in]: Maximum encode height to be allowed for current Encode session.
                                                                                           Client should allocate output buffers according to this dimension for dynamic resolution change. If set to 0, Encode will not allow dynamic resolution change. */
    NVENC_EXTERNAL_ME_HINT_COUNTS_PER_BLOCKTYPE maxMEHintCountsPerBlock[2];      /**< [in]: If Client wants to pass external motion vectors in NV_ENC_PIC_PARAMS::meExternalHints buffer it must specify the maximum number of hint candidates per block per direction for the encode session.
                                                                                           The NV_ENC_INITIALIZE_PARAMS::maxMEHintCountsPerBlock[0] is for L0 predictors and NV_ENC_INITIALIZE_PARAMS::maxMEHintCountsPerBlock[1] is for L1 predictors.
                                                                                           This client must also set NV_ENC_INITIALIZE_PARAMS::enableExternalMEHints to 1. */
    uint32_t                                   reserved [289];                  /**< [in]: Reserved and must be set to 0 */
    void*                                      reserved2[64];                   /**< [in]: Reserved and must be set to NULL */
} NV_ENC_INITIALIZE_PARAMS;

/** macro for constructing the version field of ::_NV_ENC_INITIALIZE_PARAMS */
#define NV_ENC_INITIALIZE_PARAMS_VER (NVENCAPI_STRUCT_VERSION(5) | ( 1<<31 ))


/**
 * \struct _NV_ENC_RECONFIGURE_PARAMS
 * Encode Session Reconfigured parameters.
 */
typedef struct _NV_ENC_RECONFIGURE_PARAMS
{
    uint32_t                                    version;                        /**< [in]: Struct version. Must be set to ::NV_ENC_RECONFIGURE_PARAMS_VER. */
    NV_ENC_INITIALIZE_PARAMS                    reInitEncodeParams;             /**< [in]: Encoder session re-initialization parameters.
                                                                                           If reInitEncodeParams.encodeConfig is NULL and
                                                                                           reInitEncodeParams.presetGUID is the same as the preset
                                                                                           GUID specified on the call to NvEncInitializeEncoder(),
                                                                                           EncodeAPI will continue to use the existing encode
                                                                                           configuration.
                                                                                           If reInitEncodeParams.encodeConfig is NULL and
                                                                                           reInitEncodeParams.presetGUID is different from the preset
                                                                                           GUID specified on the call to NvEncInitializeEncoder(),
                                                                                           EncodeAPI will try to use the default configuration for
                                                                                           the preset specified by reInitEncodeParams.presetGUID.
                                                                                           In this case, reconfiguration may fail if the new
                                                                                           configuration is incompatible with the existing
                                                                                           configuration (e.g. the new configuration results in
                                                                                           a change in the GOP structure). */
    uint32_t                                    resetEncoder            :1;     /**< [in]: This resets the rate control states and other internal encoder states. This should be used only with an IDR frame.
                                                                                           If NV_ENC_INITIALIZE_PARAMS::enablePTD is set to 1, encoder will force the frame type to IDR */
    uint32_t                                    forceIDR                :1;     /**< [in]: Encode the current picture as an IDR picture. This flag is only valid when Picture type decision is taken by the Encoder
                                                                                           [_NV_ENC_INITIALIZE_PARAMS::enablePTD == 1]. */
    uint32_t                                    reserved                :30;

}NV_ENC_RECONFIGURE_PARAMS;

/** macro for constructing the version field of ::_NV_ENC_RECONFIGURE_PARAMS */
#define NV_ENC_RECONFIGURE_PARAMS_VER (NVENCAPI_STRUCT_VERSION(1) | ( 1<<31 ))

/**
 * \struct _NV_ENC_PRESET_CONFIG
 * Encoder preset config
 */
typedef struct _NV_ENC_PRESET_CONFIG
{
    uint32_t      version;                               /**< [in]:  Struct version. Must be set to ::NV_ENC_PRESET_CONFIG_VER. */
    NV_ENC_CONFIG presetCfg;                             /**< [out]: preset config returned by the Nvidia Video Encoder interface. */
    uint32_t      reserved1[255];                        /**< [in]: Reserved and must be set to 0 */
    void*         reserved2[64];                         /**< [in]: Reserved and must be set to NULL */
}NV_ENC_PRESET_CONFIG;

/** macro for constructing the version field of ::_NV_ENC_PRESET_CONFIG */
#define NV_ENC_PRESET_CONFIG_VER (NVENCAPI_STRUCT_VERSION(4) | ( 1<<31 ))


/**
 * \struct _NV_ENC_SEI_PAYLOAD
 *  User SEI message
 */
typedef struct _NV_ENC_SEI_PAYLOAD
{
    uint32_t payloadSize;            /**< [in] SEI payload size in bytes. SEI payload must be byte aligned, as described in Annex D */
    uint32_t payloadType;            /**< [in] SEI payload types and syntax can be found in Annex D of the H.264 Specification. */
    uint8_t *payload;                /**< [in] pointer to user data */
} NV_ENC_SEI_PAYLOAD;

#define NV_ENC_H264_SEI_PAYLOAD NV_ENC_SEI_PAYLOAD

/**
 * \struct _NV_ENC_PIC_PARAMS_H264
 * H264 specific enc pic params. sent on a per frame basis.
 */
typedef struct _NV_ENC_PIC_PARAMS_H264
{
    uint32_t displayPOCSyntax;                           /**< [in]: Specifies the display POC syntax This is required to be set if client is handling the picture type decision. */
    uint32_t reserved3;                                  /**< [in]: Reserved and must be set to 0 */
    uint32_t refPicFlag;                                 /**< [in]: Set to 1 for a reference picture. This is ignored if NV_ENC_INITIALIZE_PARAMS::enablePTD is set to 1. */
    uint32_t colourPlaneId;                              /**< [in]: Specifies the colour plane ID associated with the current input. */
    uint32_t forceIntraRefreshWithFrameCnt;              /**< [in]: Forces an intra refresh with duration equal to intraRefreshFrameCnt.
                                                                    When outputRecoveryPointSEI is set this is value is used for recovery_frame_cnt in recovery point SEI message
                                                                    forceIntraRefreshWithFrameCnt cannot be used if B frames are used in the GOP structure specified */
    uint32_t constrainedFrame           :1;              /**< [in]: Set to 1 if client wants to encode this frame with each slice completely independent of other slices in the frame.
                                                                    NV_ENC_INITIALIZE_PARAMS::enableConstrainedEncoding should be set to 1 */
    uint32_t sliceModeDataUpdate        :1;              /**< [in]: Set to 1 if client wants to change the sliceModeData field to specify new sliceSize Parameter
                                                                    When forceIntraRefreshWithFrameCnt is set it will have priority over sliceMode setting */
    uint32_t ltrMarkFrame               :1;              /**< [in]: Set to 1 if client wants to mark this frame as LTR */
    uint32_t ltrUseFrames               :1;              /**< [in]: Set to 1 if client allows encoding this frame using the LTR frames specified in ltrFrameBitmap */
    uint32_t reservedBitFields          :28;             /**< [in]: Reserved bit fields and must be set to 0 */
    uint8_t* sliceTypeData;                              /**< [in]: Deprecated. */
    uint32_t sliceTypeArrayCnt;                          /**< [in]: Deprecated. */
    uint32_t seiPayloadArrayCnt;                         /**< [in]: Specifies the number of elements allocated in  seiPayloadArray array. */
    NV_ENC_SEI_PAYLOAD* seiPayloadArray;                 /**< [in]: Array of SEI payloads which will be inserted for this frame. */
    uint32_t sliceMode;                                  /**< [in]: This parameter in conjunction with sliceModeData specifies the way in which the picture is divided into slices
                                                                    sliceMode = 0 MB based slices, sliceMode = 1 Byte based slices, sliceMode = 2 MB row based slices, sliceMode = 3, numSlices in Picture
                                                                    When forceIntraRefreshWithFrameCnt is set it will have priority over sliceMode setting
                                                                    When sliceMode == 0 and sliceModeData == 0 whole picture will be coded with one slice */
    uint32_t sliceModeData;                              /**< [in]: Specifies the parameter needed for sliceMode. For:
                                                                    sliceMode = 0, sliceModeData specifies # of MBs in each slice (except last slice)
                                                                    sliceMode = 1, sliceModeData specifies maximum # of bytes in each slice (except last slice)
                                                                    sliceMode = 2, sliceModeData specifies # of MB rows in each slice (except last slice)
                                                                    sliceMode = 3, sliceModeData specifies number of slices in the picture. Driver will divide picture into slices optimally */
    uint32_t ltrMarkFrameIdx;                            /**< [in]: Specifies the long term referenceframe index to use for marking this frame as LTR.*/
    uint32_t ltrUseFrameBitmap;                          /**< [in]: Specifies the the associated bitmap of LTR frame indices to use when encoding this frame. */
    uint32_t ltrUsageMode;                               /**< [in]: Not supported. Reserved for future use and must be set to 0. */
    uint32_t reserved [243];                             /**< [in]: Reserved and must be set to 0. */
    void*    reserved2[62];                              /**< [in]: Reserved and must be set to NULL. */
} NV_ENC_PIC_PARAMS_H264;

/**
 * \struct _NV_ENC_PIC_PARAMS_HEVC
 * HEVC specific enc pic params. sent on a per frame basis.
 */
typedef struct _NV_ENC_PIC_PARAMS_HEVC
{
    uint32_t displayPOCSyntax;                           /**< [in]: Specifies the display POC syntax This is required to be set if client is handling the picture type decision. */
    uint32_t refPicFlag;                                 /**< [in]: Set to 1 for a reference picture. This is ignored if NV_ENC_INITIALIZE_PARAMS::enablePTD is set to 1. */
    uint32_t temporalId;                                 /**< [in]: Specifies the temporal id of the picture */
    uint32_t forceIntraRefreshWithFrameCnt;              /**< [in]: Forces an intra refresh with duration equal to intraRefreshFrameCnt.
                                                                    When outputRecoveryPointSEI is set this is value is used for recovery_frame_cnt in recovery point SEI message
                                                                    forceIntraRefreshWithFrameCnt cannot be used if B frames are used in the GOP structure specified */
    uint32_t constrainedFrame           :1;              /**< [in]: Set to 1 if client wants to encode this frame with each slice completely independent of other slices in the frame.
                                                                    NV_ENC_INITIALIZE_PARAMS::enableConstrainedEncoding should be set to 1 */
    uint32_t sliceModeDataUpdate        :1;              /**< [in]: Set to 1 if client wants to change the sliceModeData field to specify new sliceSize Parameter
                                                                    When forceIntraRefreshWithFrameCnt is set it will have priority over sliceMode setting */
    uint32_t ltrMarkFrame               :1;              /**< [in]: Set to 1 if client wants to mark this frame as LTR */
    uint32_t ltrUseFrames               :1;              /**< [in]: Set to 1 if client allows encoding this frame using the LTR frames specified in ltrFrameBitmap */
    uint32_t reservedBitFields          :28;             /**< [in]: Reserved bit fields and must be set to 0 */
    uint8_t* sliceTypeData;                              /**< [in]: Array which specifies the slice type used to force intra slice for a particular slice. Currently supported only for NV_ENC_CONFIG_H264::sliceMode == 3.
                                                                    Client should allocate array of size sliceModeData where sliceModeData is specified in field of ::_NV_ENC_CONFIG_H264
                                                                    Array element with index n corresponds to nth slice. To force a particular slice to intra client should set corresponding array element to NV_ENC_SLICE_TYPE_I
                                                                    all other array elements should be set to NV_ENC_SLICE_TYPE_DEFAULT */
    uint32_t sliceTypeArrayCnt;                          /**< [in]: Client should set this to the number of elements allocated in sliceTypeData array. If sliceTypeData is NULL then this should be set to 0 */
    uint32_t sliceMode;                                  /**< [in]: This parameter in conjunction with sliceModeData specifies the way in which the picture is divided into slices
                                                                    sliceMode = 0 CTU based slices, sliceMode = 1 Byte based slices, sliceMode = 2 CTU row based slices, sliceMode = 3, numSlices in Picture
                                                                    When forceIntraRefreshWithFrameCnt is set it will have priority over sliceMode setting
                                                                    When sliceMode == 0 and sliceModeData == 0 whole picture will be coded with one slice */
    uint32_t sliceModeData;                              /**< [in]: Specifies the parameter needed for sliceMode. For:
                                                                    sliceMode = 0, sliceModeData specifies # of CTUs in each slice (except last slice)
                                                                    sliceMode = 1, sliceModeData specifies maximum # of bytes in each slice (except last slice)
                                                                    sliceMode = 2, sliceModeData specifies # of CTU rows in each slice (except last slice)
                                                                    sliceMode = 3, sliceModeData specifies number of slices in the picture. Driver will divide picture into slices optimally */
    uint32_t ltrMarkFrameIdx;                            /**< [in]: Specifies the long term reference frame index to use for marking this frame as LTR.*/
    uint32_t ltrUseFrameBitmap;                          /**< [in]: Specifies the associated bitmap of LTR frame indices to use when encoding this frame. */
    uint32_t ltrUsageMode;                               /**< [in]: Not supported. Reserved for future use and must be set to 0. */
    uint32_t seiPayloadArrayCnt;                         /**< [in]: Specifies the number of elements allocated in  seiPayloadArray array. */
    uint32_t reserved;                                   /**< [in]: Reserved and must be set to 0. */
    NV_ENC_SEI_PAYLOAD* seiPayloadArray;                 /**< [in]: Array of SEI payloads which will be inserted for this frame. */
    uint32_t reserved2 [244];                             /**< [in]: Reserved and must be set to 0. */
    void*    reserved3[61];                              /**< [in]: Reserved and must be set to NULL. */
} NV_ENC_PIC_PARAMS_HEVC;

/**
 * Codec specific per-picture encoding parameters.
 */
typedef union _NV_ENC_CODEC_PIC_PARAMS
{
    NV_ENC_PIC_PARAMS_H264 h264PicParams;                /**< [in]: H264 encode picture params. */
    NV_ENC_PIC_PARAMS_HEVC hevcPicParams;                /**< [in]: HEVC encode picture params. */
    uint32_t               reserved[256];                /**< [in]: Reserved and must be set to 0. */
} NV_ENC_CODEC_PIC_PARAMS;

/**
 * \struct _NV_ENC_PIC_PARAMS
 * Encoding parameters that need to be sent on a per frame basis.
 */
typedef struct _NV_ENC_PIC_PARAMS
{
    uint32_t                                    version;                        /**< [in]: Struct version. Must be set to ::NV_ENC_PIC_PARAMS_VER. */
    uint32_t                                    inputWidth;                     /**< [in]: Specifies the input buffer width */
    uint32_t                                    inputHeight;                    /**< [in]: Specifies the input buffer height */
    uint32_t                                    inputPitch;                     /**< [in]: Specifies the input buffer pitch. If pitch value is not known, set this to inputWidth. */
    uint32_t                                    encodePicFlags;                 /**< [in]: Specifies bit-wise OR`ed encode pic flags. See ::NV_ENC_PIC_FLAGS enum. */
    uint32_t                                    frameIdx;                       /**< [in]: Specifies the frame index associated with the input frame [optional]. */
    uint64_t                                    inputTimeStamp;                 /**< [in]: Specifies presentation timestamp associated with the input picture. */
    uint64_t                                    inputDuration;                  /**< [in]: Specifies duration of the input picture */
    NV_ENC_INPUT_PTR                            inputBuffer;                    /**< [in]: Specifies the input buffer pointer. Client must use a pointer obtained from ::NvEncCreateInputBuffer() or ::NvEncMapInputResource() APIs.*/
    NV_ENC_OUTPUT_PTR                           outputBitstream;                /**< [in]: Specifies the pointer to output buffer. Client should use a pointer obtained from ::NvEncCreateBitstreamBuffer() API. */
    void*                                       completionEvent;                /**< [in]: Specifies an event to be signalled on completion of encoding of this Frame [only if operating in Asynchronous mode]. Each output buffer should be associated with a distinct event pointer. */
    NV_ENC_BUFFER_FORMAT                        bufferFmt;                      /**< [in]: Specifies the input buffer format. */
    NV_ENC_PIC_STRUCT                           pictureStruct;                  /**< [in]: Specifies structure of the input picture. */
    NV_ENC_PIC_TYPE                             pictureType;                    /**< [in]: Specifies input picture type. Client required to be set explicitly by the client if the client has not set NV_ENC_INITALIZE_PARAMS::enablePTD to 1 while calling NvInitializeEncoder. */
    NV_ENC_CODEC_PIC_PARAMS                     codecPicParams;                 /**< [in]: Specifies the codec specific per-picture encoding parameters. */
    NVENC_EXTERNAL_ME_HINT_COUNTS_PER_BLOCKTYPE meHintCountsPerBlock[2];        /**< [in]: Specifies the number of hint candidates per block per direction for the current frame. meHintCountsPerBlock[0] is for L0 predictors and meHintCountsPerBlock[1] is for L1 predictors.
                                                                                           The candidate count in NV_ENC_PIC_PARAMS::meHintCountsPerBlock[lx] must never exceed NV_ENC_INITIALIZE_PARAMS::maxMEHintCountsPerBlock[lx] provided during encoder intialization. */
    NVENC_EXTERNAL_ME_HINT                     *meExternalHints;                /**< [in]: Specifies the pointer to ME external hints for the current frame. The size of ME hint buffer should be equal to number of macroblocks * the total number of candidates per macroblock.
                                                                                           The total number of candidates per MB per direction = 1*meHintCountsPerBlock[Lx].numCandsPerBlk16x16 + 2*meHintCountsPerBlock[Lx].numCandsPerBlk16x8 + 2*meHintCountsPerBlock[Lx].numCandsPerBlk8x8
                                                                                           + 4*meHintCountsPerBlock[Lx].numCandsPerBlk8x8. For frames using bidirectional ME , the total number of candidates for single macroblock is sum of total number of candidates per MB for each direction (L0 and L1) */
    uint32_t                                    reserved1[6];                    /**< [in]: Reserved and must be set to 0 */
    void*                                       reserved2[2];                    /**< [in]: Reserved and must be set to NULL */
    int8_t                                     *qpDeltaMap;                      /**< [in]: Specifies the pointer to signed byte array containing value per MB in raster scan order for the current picture, which will be Interperated depending on NV_ENC_RC_PARAMS::qpMapMode.
                                                                                            If NV_ENC_RC_PARAMS::qpMapMode is NV_ENC_QP_MAP_DELTA , This specify QP modifier to be applied on top of the QP chosen by rate control.
                                                                                            If NV_ENC_RC_PARAMS::qpMapMode is NV_ENC_QP_MAP_EMPHASIS, it specifies emphasis level map per MB. This level value along with QP chosen by rate control is used to compute the QP modifier,
                                                                                            which in turn is applied on top of QP chosen by rate control.
                                                                                            If NV_ENC_RC_PARAMS::qpMapMode is NV_ENC_QP_MAP_DISABLED value in qpDeltaMap will be ignored.*/
    uint32_t                                    qpDeltaMapSize;                  /**< [in]: Specifies the size in bytes of qpDeltaMap surface allocated by client and pointed to by NV_ENC_PIC_PARAMS::qpDeltaMap. Surface (array) should be picWidthInMbs * picHeightInMbs */
    uint32_t                                    reservedBitFields;               /**< [in]: Reserved bitfields and must be set to 0 */
    uint16_t                                    meHintRefPicDist[2];             /**< [in]: Specifies temporal distance for reference picture (NVENC_EXTERNAL_ME_HINT::refidx = 0) used during external ME with NV_ENC_INITALIZE_PARAMS::enablePTD = 1 . meHintRefPicDist[0] is for L0 hints and meHintRefPicDist[1] is for L1 hints.
                                                                                            If not set, will internally infer distance of 1. Ignored for NV_ENC_INITALIZE_PARAMS::enablePTD = 0 */
    uint32_t                                    reserved3[286];                  /**< [in]: Reserved and must be set to 0 */
    void*                                       reserved4[60];                   /**< [in]: Reserved and must be set to NULL */
} NV_ENC_PIC_PARAMS;

/** Macro for constructing the version field of ::_NV_ENC_PIC_PARAMS */
#define NV_ENC_PIC_PARAMS_VER (NVENCAPI_STRUCT_VERSION(4) | ( 1<<31 ))


/**
 * \struct _NV_ENC_MEONLY_PARAMS
 * MEOnly parameters that need to be sent on a per motion estimation basis.
 * NV_ENC_MEONLY_PARAMS::meExternalHints is supported for H264 only.
 */
typedef struct _NV_ENC_MEONLY_PARAMS
{
    uint32_t                version;                            /**< [in]: Struct version. Must be set to NV_ENC_MEONLY_PARAMS_VER.*/
    uint32_t                inputWidth;                         /**< [in]: Specifies the input buffer width */
    uint32_t                inputHeight;                        /**< [in]: Specifies the input buffer height */
    NV_ENC_INPUT_PTR        inputBuffer;                        /**< [in]: Specifies the input buffer pointer. Client must use a pointer obtained from NvEncCreateInputBuffer() or NvEncMapInputResource() APIs. */
    NV_ENC_INPUT_PTR        referenceFrame;                     /**< [in]: Specifies the reference frame pointer */
    NV_ENC_OUTPUT_PTR       mvBuffer;                           /**< [in]: Specifies the pointer to motion vector data buffer allocated by NvEncCreateMVBuffer. Client must lock mvBuffer using ::NvEncLockBitstream() API to get the motion vector data. */
    NV_ENC_BUFFER_FORMAT    bufferFmt;                          /**< [in]: Specifies the input buffer format. */
    void*                   completionEvent;                    /**< [in]: Specifies an event to be signalled on completion of motion estimation
                                                                           of this Frame [only if operating in Asynchronous mode].
                                                                           Each output buffer should be associated with a distinct event pointer. */
    uint32_t                viewID;                             /**< [in]: Specifies left,right viewID if NV_ENC_CONFIG_H264_MEONLY::bStereoEnable is set.
                                                                            viewID can be 0,1 if bStereoEnable is set, 0 otherwise. */
    NVENC_EXTERNAL_ME_HINT_COUNTS_PER_BLOCKTYPE
                            meHintCountsPerBlock[2];            /**< [in]: Specifies the number of hint candidates per block for the current frame. meHintCountsPerBlock[0] is for L0 predictors.
                                                                            The candidate count in NV_ENC_PIC_PARAMS::meHintCountsPerBlock[lx] must never exceed NV_ENC_INITIALIZE_PARAMS::maxMEHintCountsPerBlock[lx] provided during encoder intialization. */
    NVENC_EXTERNAL_ME_HINT  *meExternalHints;                   /**< [in]: Specifies the pointer to ME external hints for the current frame. The size of ME hint buffer should be equal to number of macroblocks * the total number of candidates per macroblock.
                                                                            The total number of candidates per MB per direction = 1*meHintCountsPerBlock[Lx].numCandsPerBlk16x16 + 2*meHintCountsPerBlock[Lx].numCandsPerBlk16x8 + 2*meHintCountsPerBlock[Lx].numCandsPerBlk8x8
                                                                            + 4*meHintCountsPerBlock[Lx].numCandsPerBlk8x8. For frames using bidirectional ME , the total number of candidates for single macroblock is sum of total number of candidates per MB for each direction (L0 and L1) */
    uint32_t                reserved1[243];                     /**< [in]: Reserved and must be set to 0 */
    void*                   reserved2[59];                      /**< [in]: Reserved and must be set to NULL */
} NV_ENC_MEONLY_PARAMS;

/** NV_ENC_MEONLY_PARAMS struct version*/
#define NV_ENC_MEONLY_PARAMS_VER NVENCAPI_STRUCT_VERSION(3)


/**
 * \struct _NV_ENC_LOCK_BITSTREAM
 * Bitstream buffer lock parameters.
 */
typedef struct _NV_ENC_LOCK_BITSTREAM
{
    uint32_t                version;                     /**< [in]: Struct version. Must be set to ::NV_ENC_LOCK_BITSTREAM_VER. */
    uint32_t                doNotWait         :1;        /**< [in]: If this flag is set, the NvEncodeAPI interface will return buffer pointer even if operation is not completed. If not set, the call will block until operation completes. */
    uint32_t                ltrFrame          :1;        /**< [out]: Flag indicating this frame is marked as LTR frame */
    uint32_t                reservedBitFields :30;       /**< [in]: Reserved bit fields and must be set to 0 */
    void*                   outputBitstream;             /**< [in]: Pointer to the bitstream buffer being locked. */
    uint32_t*               sliceOffsets;                /**< [in,out]: Array which receives the slice offsets. This is not supported if NV_ENC_CONFIG_H264::sliceMode is 1 on Kepler GPUs. Array size must be equal to size of frame in MBs. */
    uint32_t                frameIdx;                    /**< [out]: Frame no. for which the bitstream is being retrieved. */
    uint32_t                hwEncodeStatus;              /**< [out]: The NvEncodeAPI interface status for the locked picture. */
    uint32_t                numSlices;                   /**< [out]: Number of slices in the encoded picture. Will be reported only if NV_ENC_INITIALIZE_PARAMS::reportSliceOffsets set to 1. */
    uint32_t                bitstreamSizeInBytes;        /**< [out]: Actual number of bytes generated and copied to the memory pointed by bitstreamBufferPtr. */
    uint64_t                outputTimeStamp;             /**< [out]: Presentation timestamp associated with the encoded output. */
    uint64_t                outputDuration;              /**< [out]: Presentation duration associates with the encoded output. */
    void*                   bitstreamBufferPtr;          /**< [out]: Pointer to the generated output bitstream.
                                                                     For MEOnly mode _NV_ENC_LOCK_BITSTREAM::bitstreamBufferPtr should be typecast to
                                                                     NV_ENC_H264_MV_DATA/NV_ENC_HEVC_MV_DATA pointer respectively for H264/HEVC  */
    NV_ENC_PIC_TYPE         pictureType;                 /**< [out]: Picture type of the encoded picture. */
    NV_ENC_PIC_STRUCT       pictureStruct;               /**< [out]: Structure of the generated output picture. */
    uint32_t                frameAvgQP;                  /**< [out]: Average QP of the frame. */
    uint32_t                frameSatd;                   /**< [out]: Total SATD cost for whole frame. */
    uint32_t                ltrFrameIdx;                 /**< [out]: Frame index associated with this LTR frame. */
    uint32_t                ltrFrameBitmap;              /**< [out]: Bitmap of LTR frames indices which were used for encoding this frame. Value of 0 if no LTR frames were used. */
    uint32_t                reserved [236];              /**< [in]: Reserved and must be set to 0 */
    void*                   reserved2[64];               /**< [in]: Reserved and must be set to NULL */
} NV_ENC_LOCK_BITSTREAM;

/** Macro for constructing the version field of ::_NV_ENC_LOCK_BITSTREAM */
#define NV_ENC_LOCK_BITSTREAM_VER NVENCAPI_STRUCT_VERSION(1)


/**
 * \struct _NV_ENC_LOCK_INPUT_BUFFER
 * Uncompressed Input Buffer lock parameters.
 */
typedef struct _NV_ENC_LOCK_INPUT_BUFFER
{
    uint32_t                  version;                   /**< [in]:  Struct version. Must be set to ::NV_ENC_LOCK_INPUT_BUFFER_VER. */
    uint32_t                  doNotWait         :1;      /**< [in]:  Set to 1 to make ::NvEncLockInputBuffer() a unblocking call. If the encoding is not completed, driver will return ::NV_ENC_ERR_ENCODER_BUSY error code. */
    uint32_t                  reservedBitFields :31;     /**< [in]:  Reserved bitfields and must be set to 0 */
    NV_ENC_INPUT_PTR          inputBuffer;               /**< [in]:  Pointer to the input buffer to be locked, client should pass the pointer obtained from ::NvEncCreateInputBuffer() or ::NvEncMapInputResource API. */
    void*                     bufferDataPtr;             /**< [out]: Pointed to the locked input buffer data. Client can only access input buffer using the \p bufferDataPtr. */
    uint32_t                  pitch;                     /**< [out]: Pitch of the locked input buffer. */
    uint32_t                  reserved1[251];            /**< [in]:  Reserved and must be set to 0  */
    void*                     reserved2[64];             /**< [in]:  Reserved and must be set to NULL  */
} NV_ENC_LOCK_INPUT_BUFFER;

/** Macro for constructing the version field of ::_NV_ENC_LOCK_INPUT_BUFFER */
#define NV_ENC_LOCK_INPUT_BUFFER_VER NVENCAPI_STRUCT_VERSION(1)


/**
 * \struct _NV_ENC_MAP_INPUT_RESOURCE
 * Map an input resource to a Nvidia Encoder Input Buffer
 */
typedef struct _NV_ENC_MAP_INPUT_RESOURCE
{
    uint32_t                   version;                   /**< [in]:  Struct version. Must be set to ::NV_ENC_MAP_INPUT_RESOURCE_VER. */
    uint32_t                   subResourceIndex;          /**< [in]:  Deprecated. Do not use. */
    void*                      inputResource;             /**< [in]:  Deprecated. Do not use. */
    NV_ENC_REGISTERED_PTR      registeredResource;        /**< [in]:  The Registered resource handle obtained by calling NvEncRegisterInputResource. */
    NV_ENC_INPUT_PTR           mappedResource;            /**< [out]: Mapped pointer corresponding to the registeredResource. This pointer must be used in NV_ENC_PIC_PARAMS::inputBuffer parameter in ::NvEncEncodePicture() API. */
    NV_ENC_BUFFER_FORMAT       mappedBufferFmt;           /**< [out]: Buffer format of the outputResource. This buffer format must be used in NV_ENC_PIC_PARAMS::bufferFmt if client using the above mapped resource pointer. */
    uint32_t                   reserved1[251];            /**< [in]:  Reserved and must be set to 0. */
    void*                      reserved2[63];             /**< [in]:  Reserved and must be set to NULL */
} NV_ENC_MAP_INPUT_RESOURCE;

/** Macro for constructing the version field of ::_NV_ENC_MAP_INPUT_RESOURCE */
#define NV_ENC_MAP_INPUT_RESOURCE_VER NVENCAPI_STRUCT_VERSION(4)

/**
 * \struct _NV_ENC_INPUT_RESOURCE_OPENGL_TEX
 * NV_ENC_REGISTER_RESOURCE::resourceToRegister must be a pointer to a variable of this type,
 * when NV_ENC_REGISTER_RESOURCE::resourceType is NV_ENC_INPUT_RESOURCE_TYPE_OPENGL_TEX
 */
typedef struct _NV_ENC_INPUT_RESOURCE_OPENGL_TEX
{
    uint32_t texture;                                     /**< [in]: The name of the texture to be used. */
    uint32_t target;                                      /**< [in]: Accepted values are GL_TEXTURE_RECTANGLE and GL_TEXTURE_2D. */
} NV_ENC_INPUT_RESOURCE_OPENGL_TEX;

/**
 * \struct _NV_ENC_REGISTER_RESOURCE
 * Register a resource for future use with the Nvidia Video Encoder Interface.
 */
typedef struct _NV_ENC_REGISTER_RESOURCE
{
    uint32_t                    version;                        /**< [in]: Struct version. Must be set to ::NV_ENC_REGISTER_RESOURCE_VER. */
    NV_ENC_INPUT_RESOURCE_TYPE  resourceType;                   /**< [in]: Specifies the type of resource to be registered.
                                                                           Supported values are
                                                                           ::NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX,
                                                                           ::NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR,
                                                                           ::NV_ENC_INPUT_RESOURCE_TYPE_OPENGL_TEX */
    uint32_t                    width;                          /**< [in]: Input buffer Width. */
    uint32_t                    height;                         /**< [in]: Input buffer Height. */
    uint32_t                    pitch;                          /**< [in]: Input buffer Pitch.  */
    uint32_t                    subResourceIndex;               /**< [in]: Subresource Index of the DirectX resource to be registered. Should be set to 0 for other interfaces. */
    void*                       resourceToRegister;             /**< [in]: Handle to the resource that is being registered. */
    NV_ENC_REGISTERED_PTR       registeredResource;             /**< [out]: Registered resource handle. This should be used in future interactions with the Nvidia Video Encoder Interface. */
    NV_ENC_BUFFER_FORMAT        bufferFormat;                   /**< [in]: Buffer format of resource to be registered. */
    uint32_t                    reserved1[248];                 /**< [in]: Reserved and must be set to 0. */
    void*                       reserved2[62];                  /**< [in]: Reserved and must be set to NULL. */
} NV_ENC_REGISTER_RESOURCE;

/** Macro for constructing the version field of ::_NV_ENC_REGISTER_RESOURCE */
#define NV_ENC_REGISTER_RESOURCE_VER NVENCAPI_STRUCT_VERSION(3)

/**
 * \struct _NV_ENC_STAT
 * Encode Stats structure.
 */
typedef struct _NV_ENC_STAT
{
    uint32_t            version;                         /**< [in]:  Struct version. Must be set to ::NV_ENC_STAT_VER. */
    uint32_t            reserved;                        /**< [in]:  Reserved and must be set to 0 */
    NV_ENC_OUTPUT_PTR   outputBitStream;                 /**< [out]: Specifies the pointer to output bitstream. */
    uint32_t            bitStreamSize;                   /**< [out]: Size of generated bitstream in bytes. */
    uint32_t            picType;                         /**< [out]: Picture type of encoded picture. See ::NV_ENC_PIC_TYPE. */
    uint32_t            lastValidByteOffset;             /**< [out]: Offset of last valid bytes of completed bitstream */
    uint32_t            sliceOffsets[16];                /**< [out]: Offsets of each slice */
    uint32_t            picIdx;                          /**< [out]: Picture number */
    uint32_t            reserved1[233];                  /**< [in]:  Reserved and must be set to 0 */
    void*               reserved2[64];                   /**< [in]:  Reserved and must be set to NULL */
} NV_ENC_STAT;

/** Macro for constructing the version field of ::_NV_ENC_STAT */
#define NV_ENC_STAT_VER NVENCAPI_STRUCT_VERSION(1)


/**
 * \struct _NV_ENC_SEQUENCE_PARAM_PAYLOAD
 * Sequence and picture paramaters payload.
 */
typedef struct _NV_ENC_SEQUENCE_PARAM_PAYLOAD
{
    uint32_t            version;                         /**< [in]:  Struct version. Must be set to ::NV_ENC_INITIALIZE_PARAMS_VER. */
    uint32_t            inBufferSize;                    /**< [in]:  Specifies the size of the spsppsBuffer provied by the client */
    uint32_t            spsId;                           /**< [in]:  Specifies the SPS id to be used in sequence header. Default value is 0.  */
    uint32_t            ppsId;                           /**< [in]:  Specifies the PPS id to be used in picture header. Default value is 0.  */
    void*               spsppsBuffer;                    /**< [in]:  Specifies bitstream header pointer of size NV_ENC_SEQUENCE_PARAM_PAYLOAD::inBufferSize. It is the client's responsibility to manage this memory. */
    uint32_t*           outSPSPPSPayloadSize;            /**< [out]: Size of the sequence and picture header in  bytes written by the NvEncodeAPI interface to the SPSPPSBuffer. */
    uint32_t            reserved [250];                  /**< [in]:  Reserved and must be set to 0 */
    void*               reserved2[64];                   /**< [in]:  Reserved and must be set to NULL */
} NV_ENC_SEQUENCE_PARAM_PAYLOAD;

/** Macro for constructing the version field of ::_NV_ENC_SEQUENCE_PARAM_PAYLOAD */
#define NV_ENC_SEQUENCE_PARAM_PAYLOAD_VER NVENCAPI_STRUCT_VERSION(1)


/**
 * Event registration/unregistration parameters.
 */
typedef struct _NV_ENC_EVENT_PARAMS
{
    uint32_t            version;                          /**< [in]: Struct version. Must be set to ::NV_ENC_EVENT_PARAMS_VER. */
    uint32_t            reserved;                         /**< [in]: Reserved and must be set to 0 */
    void*               completionEvent;                  /**< [in]: Handle to event to be registered/unregistered with the NvEncodeAPI interface. */
    uint32_t            reserved1[253];                   /**< [in]: Reserved and must be set to 0    */
    void*               reserved2[64];                    /**< [in]: Reserved and must be set to NULL */
} NV_ENC_EVENT_PARAMS;

/** Macro for constructing the version field of ::_NV_ENC_EVENT_PARAMS */
#define NV_ENC_EVENT_PARAMS_VER NVENCAPI_STRUCT_VERSION(1)

/**
 * Encoder Session Creation parameters
 */
typedef struct _NV_ENC_OPEN_ENCODE_SESSIONEX_PARAMS
{
    uint32_t            version;                          /**< [in]: Struct version. Must be set to ::NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER. */
    NV_ENC_DEVICE_TYPE  deviceType;                       /**< [in]: Specified the device Type */
    void*               device;                           /**< [in]: Pointer to client device. */
    void*               reserved;                         /**< [in]: Reserved and must be set to 0. */
    uint32_t            apiVersion;                       /**< [in]: API version. Should be set to NVENCAPI_VERSION. */
    uint32_t            reserved1[253];                   /**< [in]: Reserved and must be set to 0    */
    void*               reserved2[64];                    /**< [in]: Reserved and must be set to NULL */
} NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS;
/** Macro for constructing the version field of ::_NV_ENC_OPEN_ENCODE_SESSIONEX_PARAMS */
#define NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER NVENCAPI_STRUCT_VERSION(1)

/** @} */ /* END ENCODER_STRUCTURE */


/**
 * \addtogroup ENCODE_FUNC NvEncodeAPI Functions
 * @{
 */

// NvEncOpenEncodeSession
/**
 * \brief Opens an encoding session.
 *
 * Deprecated.
 *
 * \return
 * ::NV_ENC_ERR_INVALID_CALL\n
 *
 */
NVENCSTATUS NVENCAPI NvEncOpenEncodeSession                     (void* device, uint32_t deviceType, void** encoder);

// NvEncGetEncodeGuidCount
/**
 * \brief Retrieves the number of supported encode GUIDs.
 *
 * The function returns the number of codec guids supported by the NvEncodeAPI
 * interface.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [out] encodeGUIDCount
 *   Number of supported encode GUIDs.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncGetEncodeGUIDCount                    (void* encoder, uint32_t* encodeGUIDCount);


// NvEncGetEncodeGUIDs
/**
 * \brief Retrieves an array of supported encoder codec GUIDs.
 *
 * The function returns an array of codec guids supported by the NvEncodeAPI interface.
 * The client must allocate an array where the NvEncodeAPI interface can
 * fill the supported guids and pass the pointer in \p *GUIDs parameter.
 * The size of the array can be determined by using ::NvEncGetEncodeGUIDCount() API.
 * The Nvidia Encoding interface returns the number of codec guids it has actually
 * filled in the guid array in the \p GUIDCount parameter.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] guidArraySize
 *   Number of GUIDs to retrieved. Should be set to the number retrieved using
 *   ::NvEncGetEncodeGUIDCount.
 * \param [out] GUIDs
 *   Array of supported Encode GUIDs.
 * \param [out] GUIDCount
 *   Number of supported Encode GUIDs.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncGetEncodeGUIDs                        (void* encoder, GUID* GUIDs, uint32_t guidArraySize, uint32_t* GUIDCount);


// NvEncGetEncodeProfileGuidCount
/**
 * \brief Retrieves the number of supported profile GUIDs.
 *
 * The function returns the number of profile GUIDs supported for a given codec.
 * The client must first enumerate the codec guids supported by the NvEncodeAPI
 * interface. After determining the codec guid, it can query the NvEncodeAPI
 * interface to determine the number of profile guids supported for a particular
 * codec guid.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] encodeGUID
 *   The codec guid for which the profile guids are being enumerated.
 * \param [out] encodeProfileGUIDCount
 *   Number of encode profiles supported for the given encodeGUID.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncGetEncodeProfileGUIDCount                    (void* encoder, GUID encodeGUID, uint32_t* encodeProfileGUIDCount);


// NvEncGetEncodeProfileGUIDs
/**
 * \brief Retrieves an array of supported encode profile GUIDs.
 *
 * The function returns an array of supported profile guids for a particular
 * codec guid. The client must allocate an array where the NvEncodeAPI interface
 * can populate the profile guids. The client can determine the array size using
 * ::NvEncGetEncodeProfileGUIDCount() API. The client must also validiate that the
 * NvEncodeAPI interface supports the GUID the client wants to pass as \p encodeGUID
 * parameter.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] encodeGUID
 *   The encode guid whose profile guids are being enumerated.
 * \param [in] guidArraySize
 *   Number of GUIDs to be retrieved. Should be set to the number retrieved using
 *   ::NvEncGetEncodeProfileGUIDCount.
 * \param [out] profileGUIDs
 *   Array of supported Encode Profile GUIDs
 * \param [out] GUIDCount
 *   Number of valid encode profile GUIDs in \p profileGUIDs array.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncGetEncodeProfileGUIDs                               (void* encoder, GUID encodeGUID, GUID* profileGUIDs, uint32_t guidArraySize, uint32_t* GUIDCount);

// NvEncGetInputFormatCount
/**
 * \brief Retrieve the number of supported Input formats.
 *
 * The function returns the number of supported input formats. The client must
 * query the NvEncodeAPI interface to determine the supported input formats
 * before creating the input surfaces.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] encodeGUID
 *   Encode GUID, corresponding to which the number of supported input formats
 *   is to be retrieved.
 * \param [out] inputFmtCount
 *   Number of input formats supported for specified Encode GUID.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_GENERIC \n
 */
NVENCSTATUS NVENCAPI NvEncGetInputFormatCount                   (void* encoder, GUID encodeGUID, uint32_t* inputFmtCount);


// NvEncGetInputFormats
/**
 * \brief Retrieves an array of supported Input formats
 *
 * Returns an array of supported input formats  The client must use the input
 * format to create input surface using ::NvEncCreateInputBuffer() API.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] encodeGUID
 *   Encode GUID, corresponding to which the number of supported input formats
 *   is to be retrieved.
 *\param [in] inputFmtArraySize
 *   Size input format count array passed in \p inputFmts.
 *\param [out] inputFmts
 *   Array of input formats supported for this Encode GUID.
 *\param [out] inputFmtCount
 *   The number of valid input format types returned by the NvEncodeAPI
 *   interface in \p inputFmts array.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncGetInputFormats                       (void* encoder, GUID encodeGUID, NV_ENC_BUFFER_FORMAT* inputFmts, uint32_t inputFmtArraySize, uint32_t* inputFmtCount);


// NvEncGetEncodeCaps
/**
 * \brief Retrieves the capability value for a specified encoder attribute.
 *
 * The function returns the capability value for a given encoder attribute. The
 * client must validate the encodeGUID using ::NvEncGetEncodeGUIDs() API before
 * calling this function. The encoder attribute being queried are enumerated in
 * ::NV_ENC_CAPS_PARAM enum.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] encodeGUID
 *   Encode GUID, corresponding to which the capability attribute is to be retrieved.
 * \param [in] capsParam
 *   Used to specify attribute being queried. Refer ::NV_ENC_CAPS_PARAM for  more
 * details.
 * \param [out] capsVal
 *   The value corresponding to the capability attribute being queried.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_GENERIC \n
 */
NVENCSTATUS NVENCAPI NvEncGetEncodeCaps                     (void* encoder, GUID encodeGUID, NV_ENC_CAPS_PARAM* capsParam, int* capsVal);


// NvEncGetEncodePresetCount
/**
 * \brief Retrieves the number of supported preset GUIDs.
 *
 * The function returns the number of preset GUIDs available for a given codec.
 * The client must validate the codec guid using ::NvEncGetEncodeGUIDs() API
 * before calling this function.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] encodeGUID
 *   Encode GUID, corresponding to which the number of supported presets is to
 *   be retrieved.
 * \param [out] encodePresetGUIDCount
 *   Receives the number of supported preset GUIDs.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncGetEncodePresetCount              (void* encoder, GUID encodeGUID, uint32_t* encodePresetGUIDCount);


// NvEncGetEncodePresetGUIDs
/**
 * \brief Receives an array of supported encoder preset GUIDs.
 *
 * The function returns an array of encode preset guids available for a given codec.
 * The client can directly use one of the preset guids based upon the use case
 * or target device. The preset guid chosen can be directly used in
 * NV_ENC_INITIALIZE_PARAMS::presetGUID parameter to ::NvEncEncodePicture() API.
 * Alternately client can  also use the preset guid to retrieve the encoding config
 * parameters being used by NvEncodeAPI interface for that given preset, using
 * ::NvEncGetEncodePresetConfig() API. It can then modify preset config parameters
 * as per its use case and send it to NvEncodeAPI interface as part of
 * NV_ENC_INITIALIZE_PARAMS::encodeConfig parameter for NvEncInitializeEncoder()
 * API.
 *
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] encodeGUID
 *   Encode GUID, corresponding to which the list of supported presets is to be
 *   retrieved.
 * \param [in] guidArraySize
 *   Size of array of preset guids passed in \p preset GUIDs
 * \param [out] presetGUIDs
 *   Array of supported Encode preset GUIDs from the NvEncodeAPI interface
 *   to client.
 * \param [out] encodePresetGUIDCount
 *   Receives the number of preset GUIDs returned by the NvEncodeAPI
 *   interface.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncGetEncodePresetGUIDs                  (void* encoder, GUID encodeGUID, GUID* presetGUIDs, uint32_t guidArraySize, uint32_t* encodePresetGUIDCount);


// NvEncGetEncodePresetConfig
/**
 * \brief Returns a preset config structure supported for given preset GUID.
 *
 * The function returns a preset config structure for a given preset guid. Before
 * using this function the client must enumerate the preset guids available for
 * a given codec. The preset config structure can be modified by the client depending
 * upon its use case and can be then used to initialize the encoder using
 * ::NvEncInitializeEncoder() API. The client can use this function only if it
 * wants to modify the NvEncodeAPI preset configuration, otherwise it can
 * directly use the preset guid.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] encodeGUID
 *   Encode GUID, corresponding to which the list of supported presets is to be
 *   retrieved.
 * \param [in] presetGUID
 *   Preset GUID, corresponding to which the Encoding configurations is to be
 *   retrieved.
 * \param [out] presetConfig
 *   The requested Preset Encoder Attribute set. Refer ::_NV_ENC_CONFIG for
*    more details.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncGetEncodePresetConfig               (void* encoder, GUID encodeGUID, GUID  presetGUID, NV_ENC_PRESET_CONFIG* presetConfig);

// NvEncInitializeEncoder
/**
 * \brief Initialize the encoder.
 *
 * This API must be used to initialize the encoder. The initialization parameter
 * is passed using \p *createEncodeParams  The client must send the following
 * fields of the _NV_ENC_INITIALIZE_PARAMS structure with a valid value.
 * - NV_ENC_INITIALIZE_PARAMS::encodeGUID
 * - NV_ENC_INITIALIZE_PARAMS::encodeWidth
 * - NV_ENC_INITIALIZE_PARAMS::encodeHeight
 *
 * The client can pass a preset guid directly to the NvEncodeAPI interface using
 * NV_ENC_INITIALIZE_PARAMS::presetGUID field. If the client doesn't pass
 * NV_ENC_INITIALIZE_PARAMS::encodeConfig structure, the codec specific parameters
 * will be selected based on the preset guid. The preset guid must have been
 * validated by the client using ::NvEncGetEncodePresetGUIDs() API.
 * If the client passes a custom ::_NV_ENC_CONFIG structure through
 * NV_ENC_INITIALIZE_PARAMS::encodeConfig , it will override the codec specific parameters
 * based on the preset guid. It is recommended that even if the client passes a custom config,
 * it should also send a preset guid. In this case, the preset guid passed by the client
 * will not override any of the custom config parameters programmed by the client,
 * it is only used as a hint by the NvEncodeAPI interface to determine certain encoder parameters
 * which are not exposed to the client.
 *
 * There are two modes of operation for the encoder namely:
 * - Asynchronous mode
 * - Synchronous mode
 *
 * The client can select asynchronous or synchronous mode by setting the \p
 * enableEncodeAsync field in ::_NV_ENC_INITIALIZE_PARAMS to 1 or 0 respectively.
 *\par Asynchronous mode of operation:
 * The Asynchronous mode can be enabled by setting NV_ENC_INITIALIZE_PARAMS::enableEncodeAsync to 1.
 * The client operating in asynchronous mode must allocate completion event object
 * for each output buffer and pass the completion event object in the
 * ::NvEncEncodePicture() API. The client can create another thread and wait on
 * the event object to be signalled by NvEncodeAPI interface on completion of the
 * encoding process for the output frame. This should unblock the main thread from
 * submitting work to the encoder. When the event is signalled the client can call
 * NvEncodeAPI interfaces to copy the bitstream data using ::NvEncLockBitstream()
 * API. This is the preferred mode of operation.
 *
 * NOTE: Asynchronous mode is not supported on Linux.
 *
 *\par Synchronous mode of operation:
 * The client can select synchronous mode by setting NV_ENC_INITIALIZE_PARAMS::enableEncodeAsync to 0.
 * The client working in synchronous mode can work in a single threaded or multi
 * threaded mode. The client need not allocate any event objects. The client can
 * only lock the bitstream data after NvEncodeAPI interface has returned
 * ::NV_ENC_SUCCESS from encode picture. The NvEncodeAPI interface can return
 * ::NV_ENC_ERR_NEED_MORE_INPUT error code from ::NvEncEncodePicture() API. The
 * client must not lock the output buffer in such case but should send the next
 * frame for encoding. The client must keep on calling ::NvEncEncodePicture() API
 * until it returns ::NV_ENC_SUCCESS. \n
 * The client must always lock the bitstream data in order in which it has submitted.
 * This is true for both asynchronous and synchronous mode.
 *
 *\par Picture type decision:
 * If the client is taking the picture type decision and it must disable the picture
 * type decision module in NvEncodeAPI by setting NV_ENC_INITIALIZE_PARAMS::enablePTD
 * to 0. In this case the client is  required to send the picture in encoding
 * order to NvEncodeAPI by doing the re-ordering for B frames. \n
 * If the client doesn't want to take the picture type decision it can enable
 * picture type decision module in the NvEncodeAPI interface by setting
 * NV_ENC_INITIALIZE_PARAMS::enablePTD to 1 and send the input pictures in display
 * order.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] createEncodeParams
 *   Refer ::_NV_ENC_INITIALIZE_PARAMS for details.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncInitializeEncoder                     (void* encoder, NV_ENC_INITIALIZE_PARAMS* createEncodeParams);


// NvEncCreateInputBuffer
/**
 * \brief Allocates Input buffer.
 *
 * This function is used to allocate an input buffer. The client must enumerate
 * the input buffer format before allocating the input buffer resources. The
 * NV_ENC_INPUT_PTR returned by the NvEncodeAPI interface in the
 * NV_ENC_CREATE_INPUT_BUFFER::inputBuffer field can be directly used in
 * ::NvEncEncodePicture() API. The number of input buffers to be allocated by the
 * client must be at least 4 more than the number of B frames being used for encoding.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in,out] createInputBufferParams
 *  Pointer to the ::NV_ENC_CREATE_INPUT_BUFFER structure.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncCreateInputBuffer                     (void* encoder, NV_ENC_CREATE_INPUT_BUFFER* createInputBufferParams);


// NvEncDestroyInputBuffer
/**
 * \brief Release an input buffers.
 *
 * This function is used to free an input buffer. If the client has allocated
 * any input buffer using ::NvEncCreateInputBuffer() API, it must free those
 * input buffers by calling this function. The client must release the input
 * buffers before destroying the encoder using ::NvEncDestroyEncoder() API.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] inputBuffer
 *   Pointer to the input buffer to be released.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncDestroyInputBuffer                    (void* encoder, NV_ENC_INPUT_PTR inputBuffer);


// NvEncCreateBitstreamBuffer
/**
 * \brief Allocates an output bitstream buffer
 *
 * This function is used to allocate an output bitstream buffer and returns a
 * NV_ENC_OUTPUT_PTR to bitstream  buffer to the client in the
 * NV_ENC_CREATE_BITSTREAM_BUFFER::bitstreamBuffer field.
 * The client can only call this function after the encoder session has been
 * initialized using ::NvEncInitializeEncoder() API. The minimum number of output
 * buffers allocated by the client must be at least 4 more than the number of B
 * B frames being used for encoding. The client can only access the output
 * bitsteam data by locking the \p bitstreamBuffer using the ::NvEncLockBitstream()
 * function.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in,out] createBitstreamBufferParams
 *   Pointer ::NV_ENC_CREATE_BITSTREAM_BUFFER for details.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_ENCODER_NOT_INITIALIZED \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncCreateBitstreamBuffer                 (void* encoder, NV_ENC_CREATE_BITSTREAM_BUFFER* createBitstreamBufferParams);


// NvEncDestroyBitstreamBuffer
/**
 * \brief Release a bitstream buffer.
 *
 * This function is used to release the output bitstream buffer allocated using
 * the ::NvEncCreateBitstreamBuffer() function. The client must release the output
 * bitstreamBuffer using this function before destroying the encoder session.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] bitstreamBuffer
 *   Pointer to the bitstream buffer being released.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_ENCODER_NOT_INITIALIZED \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncDestroyBitstreamBuffer                (void* encoder, NV_ENC_OUTPUT_PTR bitstreamBuffer);

// NvEncEncodePicture
/**
 * \brief Submit an input picture for encoding.
 *
 * This function is used to submit an input picture buffer for encoding. The
 * encoding parameters are passed using \p *encodePicParams which is a pointer
 * to the ::_NV_ENC_PIC_PARAMS structure.
 *
 * If the client has set NV_ENC_INITIALIZE_PARAMS::enablePTD to 0, then it must
 * send a valid value for the following fields.
 * - NV_ENC_PIC_PARAMS::pictureType
 * - NV_ENC_PIC_PARAMS_H264::displayPOCSyntax (H264 only)
 * - NV_ENC_PIC_PARAMS_H264::frameNumSyntax(H264 only)
 * - NV_ENC_PIC_PARAMS_H264::refPicFlag(H264 only)
 *
 *
 *\par Asynchronous Encoding
 * If the client has enabled asynchronous mode of encoding by setting
 * NV_ENC_INITIALIZE_PARAMS::enableEncodeAsync to 1 in the ::NvEncInitializeEncoder()
 * API ,then the client must send a valid NV_ENC_PIC_PARAMS::completionEvent.
 * Incase of asynchronous mode of operation, client can queue the ::NvEncEncodePicture()
 * API commands from the main thread and then queue output buffers to be processed
 * to a secondary worker thread. Before the locking the output buffers in the
 * secondary thread , the client must wait on NV_ENC_PIC_PARAMS::completionEvent
 * it has queued in ::NvEncEncodePicture() API call. The client must always process
 * completion event and the output buffer in the same order in which they have been
 * submitted for encoding. The NvEncodeAPI interface is responsible for any
 * re-ordering required for B frames and will always ensure that encoded bitstream
 * data is written in the same order in which output buffer is submitted.
 *\code
  The below example shows how  asynchronous encoding in case of 1 B frames
  ------------------------------------------------------------------------
  Suppose the client allocated 4 input buffers(I1,I2..), 4 output buffers(O1,O2..)
  and 4 completion events(E1, E2, ...). The NvEncodeAPI interface will need to
  keep a copy of the input buffers for re-ordering and it allocates following
  internal buffers (NvI1, NvI2...). These internal buffers are managed by NvEncodeAPI
  and the client is not responsible for the allocating or freeing the memory of
  the internal buffers.

  a) The client main thread will queue the following encode frame calls.
  Note the picture type is unknown to the client, the decision is being taken by
  NvEncodeAPI interface. The client should pass ::_NV_ENC_PIC_PARAMS parameter
  consisting of allocated input buffer, output buffer and output events in successive
  ::NvEncEncodePicture() API calls along with other required encode picture params.
  For example:
  1st EncodePicture parameters - (I1, O1, E1)
  2nd EncodePicture parameters - (I2, O2, E2)
  3rd EncodePicture parameters - (I3, O3, E3)

  b) NvEncodeAPI SW will receive the following encode Commands from the client.
  The left side shows input from client in the form (Input buffer, Output Buffer,
  Output Event). The right hand side shows a possible picture type decision take by
  the NvEncodeAPI interface.
  (I1, O1, E1)    ---P1 Frame
  (I2, O2, E2)    ---B2 Frame
  (I3, O3, E3)    ---P3 Frame

  c) NvEncodeAPI interface will make a copy of the input buffers to its internal
   buffersfor re-ordering. These copies are done as part of nvEncEncodePicture
   function call from the client and NvEncodeAPI interface is responsible for
   synchronization of copy operation with the actual encoding operation.
   I1 --> NvI1
   I2 --> NvI2
   I3 --> NvI3

  d) After returning from ::NvEncEncodePicture() call , the client must queue the output
   bitstream  processing work to the secondary thread. The output bitstream processing
   for asynchronous mode consist of first waiting on completion event(E1, E2..)
   and then locking the output bitstream buffer(O1, O2..) for reading the encoded
   data. The work queued to the secondary thread by the client is in the following order
   (I1, O1, E1)
   (I2, O2, E2)
   (I3, O3, E3)
   Note they are in the same order in which client calls ::NvEncEncodePicture() API
   in \p step a).

  e) NvEncodeAPI interface  will do the re-ordering such that Encoder HW will receive
  the following encode commands:
  (NvI1, O1, E1)   ---P1 Frame
  (NvI3, O2, E2)   ---P3 Frame
  (NvI2, O3, E3)   ---B2 frame

  f) After the encoding operations are completed, the events will be signalled
  by NvEncodeAPI interface in the following order :
  (O1, E1) ---P1 Frame ,output bitstream copied to O1 and event E1 signalled.
  (O2, E2) ---P3 Frame ,output bitstream copied to O2 and event E2 signalled.
  (O3, E3) ---B2 Frame ,output bitstream copied to O3 and event E3 signalled.

  g) The client must lock the bitstream data using ::NvEncLockBitstream() API in
   the order O1,O2,O3  to read the encoded data, after waiting for the events
   to be signalled in the same order i.e E1, E2 and E3.The output processing is
   done in the secondary thread in the following order:
   Waits on E1, copies encoded bitstream from O1
   Waits on E2, copies encoded bitstream from O2
   Waits on E3, copies encoded bitstream from O3

  -Note the client will receive the events signalling and output buffer in the
   same order in which they have submitted for encoding.
  -Note the LockBitstream will have picture type field which will notify the
   output picture type to the clients.
  -Note the input, output buffer and the output completion event are free to be
   reused once NvEncodeAPI interfaced has signalled the event and the client has
   copied the data from the output buffer.

 * \endcode
 *
 *\par Synchronous Encoding
 * The client can enable synchronous mode of encoding by setting
 * NV_ENC_INITIALIZE_PARAMS::enableEncodeAsync to 0 in ::NvEncInitializeEncoder() API.
 * The NvEncodeAPI interface may return ::NV_ENC_ERR_NEED_MORE_INPUT error code for
 * some ::NvEncEncodePicture() API calls when NV_ENC_INITIALIZE_PARAMS::enablePTD
 * is set to 1, but the client must not treat it as a fatal error. The NvEncodeAPI
 * interface might not be able to submit an input picture buffer for encoding
 * immediately due to re-ordering for B frames. The NvEncodeAPI interface cannot
 * submit the input picture which is decided to be encoded as B frame as it waits
 * for backward reference from  temporally subsequent frames. This input picture
 * is buffered internally and waits for more input picture to arrive. The client
 * must not call ::NvEncLockBitstream() API on the output buffers whose
 * ::NvEncEncodePicture() API returns ::NV_ENC_ERR_NEED_MORE_INPUT. The client must
 * wait for the NvEncodeAPI interface to return ::NV_ENC_SUCCESS before locking the
 * output bitstreams to read the encoded bitstream data. The following example
 * explains the scenario with synchronous encoding with 2 B frames.
 *\code
 The below example shows how  synchronous encoding works in case of 1 B frames
 -----------------------------------------------------------------------------
 Suppose the client allocated 4 input buffers(I1,I2..), 4 output buffers(O1,O2..)
 and 4 completion events(E1, E2, ...). The NvEncodeAPI interface will need to
 keep a copy of the input buffers for re-ordering and it allocates following
 internal buffers (NvI1, NvI2...). These internal buffers are managed by NvEncodeAPI
 and the client is not responsible for the allocating or freeing the memory of
 the internal buffers.

 The client calls ::NvEncEncodePicture() API with input buffer I1 and output buffer O1.
 The NvEncodeAPI decides to encode I1 as P frame and submits it to encoder
 HW and returns ::NV_ENC_SUCCESS.
 The client can now read the encoded data by locking the output O1 by calling
 NvEncLockBitstream API.

 The client calls ::NvEncEncodePicture() API with input buffer I2 and output buffer O2.
 The NvEncodeAPI decides to encode I2 as B frame and buffers I2 by copying it
 to internal buffer and returns ::NV_ENC_ERR_NEED_MORE_INPUT.
 The error is not fatal and it notifies client that it cannot read the encoded
 data by locking the output O2 by calling ::NvEncLockBitstream() API without submitting
 more work to the NvEncodeAPI interface.

 The client calls ::NvEncEncodePicture() with input buffer I3 and output buffer O3.
 The NvEncodeAPI decides to encode I3 as P frame and it first submits I3 for
 encoding which will be used as backward reference frame for I2.
 The NvEncodeAPI then submits I2 for encoding and returns ::NV_ENC_SUCESS. Both
 the submission are part of the same ::NvEncEncodePicture() function call.
 The client can now read the encoded data for both the frames by locking the output
 O2 followed by  O3 ,by calling ::NvEncLockBitstream() API.

 The client must always lock the output in the same order in which it has submitted
 to receive the encoded bitstream in correct encoding order.

 * \endcode
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in,out] encodePicParams
 *   Pointer to the ::_NV_ENC_PIC_PARAMS structure.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_ENCODER_BUSY \n
 * ::NV_ENC_ERR_NEED_MORE_INPUT \n
 * ::NV_ENC_ERR_ENCODER_NOT_INITIALIZED \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncEncodePicture                         (void* encoder, NV_ENC_PIC_PARAMS* encodePicParams);


// NvEncLockBitstream
/**
 * \brief Lock output bitstream buffer
 *
 * This function is used to lock the bitstream buffer to read the encoded data.
 * The client can only access the encoded data by calling this function.
 * The pointer to client accessible encoded data is returned in the
 * NV_ENC_LOCK_BITSTREAM::bitstreamBufferPtr field. The size of the encoded data
 * in the output buffer is returned in the NV_ENC_LOCK_BITSTREAM::bitstreamSizeInBytes
 * The NvEncodeAPI interface also returns the output picture type and picture structure
 * of the encoded frame in NV_ENC_LOCK_BITSTREAM::pictureType and
 * NV_ENC_LOCK_BITSTREAM::pictureStruct fields respectively. If the client has
 * set NV_ENC_LOCK_BITSTREAM::doNotWait to 1, the function might return
 * ::NV_ENC_ERR_LOCK_BUSY if client is operating in synchronous mode. This is not
 * a fatal failure if NV_ENC_LOCK_BITSTREAM::doNotWait is set to 1. In the above case the client can
 * retry the function after few milliseconds.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in,out] lockBitstreamBufferParams
 *   Pointer to the ::_NV_ENC_LOCK_BITSTREAM structure.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_LOCK_BUSY \n
 * ::NV_ENC_ERR_ENCODER_NOT_INITIALIZED \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncLockBitstream                         (void* encoder, NV_ENC_LOCK_BITSTREAM* lockBitstreamBufferParams);


// NvEncUnlockBitstream
/**
 * \brief Unlock the output bitstream buffer
 *
 * This function is used to unlock the output bitstream buffer after the client
 * has read the encoded data from output buffer. The client must call this function
 * to unlock the output buffer which it has previously locked using ::NvEncLockBitstream()
 * function. Using a locked bitstream buffer in ::NvEncEncodePicture() API will cause
 * the function to fail.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in,out] bitstreamBuffer
 *   bitstream buffer pointer being unlocked
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_ENCODER_NOT_INITIALIZED \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncUnlockBitstream                       (void* encoder, NV_ENC_OUTPUT_PTR bitstreamBuffer);


// NvLockInputBuffer
/**
 * \brief Locks an input buffer
 *
 * This function is used to lock the input buffer to load the uncompressed YUV
 * pixel data into input buffer memory. The client must pass the NV_ENC_INPUT_PTR
 * it had previously allocated using ::NvEncCreateInputBuffer()in the
 * NV_ENC_LOCK_INPUT_BUFFER::inputBuffer field.
 * The NvEncodeAPI interface returns pointer to client accessible input buffer
 * memory in NV_ENC_LOCK_INPUT_BUFFER::bufferDataPtr field.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in,out] lockInputBufferParams
 *   Pointer to the ::_NV_ENC_LOCK_INPUT_BUFFER structure
 *
 * \return
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_LOCK_BUSY \n
 * ::NV_ENC_ERR_ENCODER_NOT_INITIALIZED \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncLockInputBuffer                      (void* encoder, NV_ENC_LOCK_INPUT_BUFFER* lockInputBufferParams);


// NvUnlockInputBuffer
/**
 * \brief Unlocks the input buffer
 *
 * This function is used to unlock the input buffer memory previously locked for
 * uploading YUV pixel data. The input buffer must be unlocked before being used
 * again for encoding, otherwise NvEncodeAPI will fail the ::NvEncEncodePicture()
 *
  * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] inputBuffer
 *   Pointer to the input buffer that is being unlocked.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_ENCODER_NOT_INITIALIZED \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 *
 */
NVENCSTATUS NVENCAPI NvEncUnlockInputBuffer                     (void* encoder, NV_ENC_INPUT_PTR inputBuffer);


// NvEncGetEncodeStats
/**
 * \brief Get encoding statistics.
 *
 * This function is used to retrieve the encoding statistics.
 * This API is not supported when encode device type is CUDA.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in,out] encodeStats
 *   Pointer to the ::_NV_ENC_STAT structure.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_ENCODER_NOT_INITIALIZED \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncGetEncodeStats                        (void* encoder, NV_ENC_STAT* encodeStats);


// NvEncGetSequenceParams
/**
 * \brief Get encoded sequence and picture header.
 *
 * This function can be used to retrieve the sequence and picture header out of
 * band. The client must call this function only after the encoder has been
 * initialized using ::NvEncInitializeEncoder() function. The client must
 * allocate the memory where the NvEncodeAPI interface can copy the bitstream
 * header and pass the pointer to the memory in NV_ENC_SEQUENCE_PARAM_PAYLOAD::spsppsBuffer.
 * The size of buffer is passed in the field  NV_ENC_SEQUENCE_PARAM_PAYLOAD::inBufferSize.
 * The NvEncodeAPI interface will copy the bitstream header payload and returns
 * the actual size of the bitstream header in the field
 * NV_ENC_SEQUENCE_PARAM_PAYLOAD::outSPSPPSPayloadSize.
 * The client must call  ::NvEncGetSequenceParams() function from the same thread which is
 * being used to call ::NvEncEncodePicture() function.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in,out] sequenceParamPayload
 *   Pointer to the ::_NV_ENC_SEQUENCE_PARAM_PAYLOAD structure.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_ENCODER_NOT_INITIALIZED \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncGetSequenceParams                     (void* encoder, NV_ENC_SEQUENCE_PARAM_PAYLOAD* sequenceParamPayload);


// NvEncRegisterAsyncEvent
/**
 * \brief Register event for notification to encoding completion.
 *
 * This function is used to register the completion event with NvEncodeAPI
 * interface. The event is required when the client has configured the encoder to
 * work in asynchronous mode. In this mode the client needs to send a completion
 * event with every output buffer. The NvEncodeAPI interface will signal the
 * completion of the encoding process using this event. Only after the event is
 * signalled the client can get the encoded data using ::NvEncLockBitstream() function.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] eventParams
 *   Pointer to the ::_NV_ENC_EVENT_PARAMS structure.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_ENCODER_NOT_INITIALIZED \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncRegisterAsyncEvent                    (void* encoder, NV_ENC_EVENT_PARAMS* eventParams);


// NvEncUnregisterAsyncEvent
/**
 * \brief Unregister completion event.
 *
 * This function is used to unregister completion event which has been previously
 * registered using ::NvEncRegisterAsyncEvent() function. The client must unregister
 * all events before destroying the encoder using ::NvEncDestroyEncoder() function.
 *
  * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] eventParams
 *   Pointer to the ::_NV_ENC_EVENT_PARAMS structure.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_ENCODER_NOT_INITIALIZED \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncUnregisterAsyncEvent                  (void* encoder, NV_ENC_EVENT_PARAMS* eventParams);


// NvEncMapInputResource
/**
 * \brief Map an externally created input resource pointer for encoding.
 *
 * Maps an externally allocated input resource [using and returns a NV_ENC_INPUT_PTR
 * which can be used for encoding in the ::NvEncEncodePicture() function. The
 * mapped resource is returned in the field NV_ENC_MAP_INPUT_RESOURCE::outputResourcePtr.
 * The NvEncodeAPI interface also returns the buffer format of the mapped resource
 * in the field NV_ENC_MAP_INPUT_RESOURCE::outbufferFmt.
 * This function provides synchronization guarantee that any graphics or compute
 * work submitted on the input buffer is completed before the buffer is used for encoding.
 * The client should not access any input buffer while they are mapped by the encoder.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in,out] mapInputResParams
 *   Pointer to the ::_NV_ENC_MAP_INPUT_RESOURCE structure.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_ENCODER_NOT_INITIALIZED \n
 * ::NV_ENC_ERR_RESOURCE_NOT_REGISTERED \n
 * ::NV_ENC_ERR_MAP_FAILED \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncMapInputResource                         (void* encoder, NV_ENC_MAP_INPUT_RESOURCE* mapInputResParams);


// NvEncUnmapInputResource
/**
 * \brief  UnMaps a NV_ENC_INPUT_PTR  which was mapped for encoding
 *
 *
 * UnMaps an input buffer which was previously mapped using ::NvEncMapInputResource()
 * API. The mapping created using ::NvEncMapInputResource() should be invalidated
 * using this API before the external resource is destroyed by the client. The client
 * must unmap the buffer after ::NvEncLockBitstream() API returns succuessfully for encode
 * work submitted using the mapped input buffer.
 *
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] mappedInputBuffer
 *   Pointer to the NV_ENC_INPUT_PTR
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_ENCODER_NOT_INITIALIZED \n
 * ::NV_ENC_ERR_RESOURCE_NOT_REGISTERED \n
 * ::NV_ENC_ERR_RESOURCE_NOT_MAPPED \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncUnmapInputResource                         (void* encoder, NV_ENC_INPUT_PTR mappedInputBuffer);

// NvEncDestroyEncoder
/**
 * \brief Destroy Encoding Session
 *
 * Destroys the encoder session previously created using ::NvEncOpenEncodeSession()
 * function. The client must flush the encoder before freeing any resources. In order
 * to flush the encoder the client must pass a NULL encode picture packet and either
 * wait for the ::NvEncEncodePicture() function to return in synchronous mode or wait
 * for the flush event to be signaled by the encoder in asynchronous mode.
 * The client must free all the input and output resources created using the
 * NvEncodeAPI interface before destroying the encoder. If the client is operating
 * in asynchronous mode, it must also unregister the completion events previously
 * registered.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncDestroyEncoder                        (void* encoder);

// NvEncInvalidateRefFrames
/**
 * \brief Invalidate reference frames
 *
 * Invalidates reference frame based on the time stamp provided by the client.
 * The encoder marks any reference frames or any frames which have been reconstructed
 * using the corrupt frame as invalid for motion estimation and uses older reference
 * frames for motion estimation. The encoded forces the current frame to be encoded
 * as an intra frame if no reference frames are left after invalidation process.
 * This is useful for low latency application for error resiliency. The client
 * is recommended to set NV_ENC_CONFIG_H264::maxNumRefFrames to a large value so
 * that encoder can keep a backup of older reference frames in the DPB and can use them
 * for motion estimation when the newer reference frames have been invalidated.
 * This API can be called multiple times.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] invalidRefFrameTimeStamp
 *   Timestamp of the invalid reference frames which needs to be invalidated.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncInvalidateRefFrames(void* encoder, uint64_t invalidRefFrameTimeStamp);

// NvEncOpenEncodeSessionEx
/**
 * \brief Opens an encoding session.
 *
 * Opens an encoding session and returns a pointer to the encoder interface in
 * the \p **encoder parameter. The client should start encoding process by calling
 * this API first.
 * The client must pass a pointer to IDirect3DDevice9 device or CUDA context in the \p *device parameter.
 * For the OpenGL interface, \p device must be NULL. An OpenGL context must be current when
 * calling all NvEncodeAPI functions.
 * If the creation of encoder session fails, the client must call ::NvEncDestroyEncoder API
 * before exiting.
 *
 * \param [in] openSessionExParams
 *    Pointer to a ::NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS structure.
 * \param [out] encoder
 *    Encode Session pointer to the NvEncodeAPI interface.
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_NO_ENCODE_DEVICE \n
 * ::NV_ENC_ERR_UNSUPPORTED_DEVICE \n
 * ::NV_ENC_ERR_INVALID_DEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncOpenEncodeSessionEx                   (NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS *openSessionExParams, void** encoder);

// NvEncRegisterResource
/**
 * \brief Registers a resource with the Nvidia Video Encoder Interface.
 *
 * Registers a resource with the Nvidia Video Encoder Interface for book keeping.
 * The client is expected to pass the registered resource handle as well, while calling ::NvEncMapInputResource API.
 *
 * \param [in] encoder
 *   Pointer to the NVEncodeAPI interface.
 *
 * \param [in] registerResParams
 *   Pointer to a ::_NV_ENC_REGISTER_RESOURCE structure
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_ENCODER_NOT_INITIALIZED \n
 * ::NV_ENC_ERR_RESOURCE_REGISTER_FAILED \n
 * ::NV_ENC_ERR_GENERIC \n
 * ::NV_ENC_ERR_UNIMPLEMENTED \n
 *
 */
NVENCSTATUS NVENCAPI NvEncRegisterResource                      (void* encoder, NV_ENC_REGISTER_RESOURCE* registerResParams);

// NvEncUnregisterResource
/**
 * \brief Unregisters a resource previously registered with the Nvidia Video Encoder Interface.
 *
 * Unregisters a resource previously registered with the Nvidia Video Encoder Interface.
 * The client is expected to unregister any resource that it has registered with the
 * Nvidia Video Encoder Interface before destroying the resource.
 *
 * \param [in] encoder
 *   Pointer to the NVEncodeAPI interface.
 *
 * \param [in] registeredResource
 *   The registered resource pointer that was returned in ::NvEncRegisterResource.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_ENCODER_NOT_INITIALIZED \n
 * ::NV_ENC_ERR_RESOURCE_NOT_REGISTERED \n
 * ::NV_ENC_ERR_GENERIC \n
 * ::NV_ENC_ERR_UNIMPLEMENTED \n
 *
 */
NVENCSTATUS NVENCAPI NvEncUnregisterResource                    (void* encoder, NV_ENC_REGISTERED_PTR registeredResource);

// NvEncReconfigureEncoder
/**
 * \brief Reconfigure an existing encoding session.
 *
 * Reconfigure an existing encoding session.
 * The client should call this API to change/reconfigure the parameter passed during
 * NvEncInitializeEncoder API call.
 * Currently Reconfiguration of following are not supported.
 * Change in GOP structure.
 * Change in sync-Async mode.
 * Change in MaxWidth & MaxHeight.
 * Change in PTDmode.
 *
 * Resolution change is possible only if maxEncodeWidth & maxEncodeHeight of NV_ENC_INITIALIZE_PARAMS
 * is set while creating encoder session.
 *
 * \param [in] encoder
 *   Pointer to the NVEncodeAPI interface.
 *
 * \param [in] reInitEncodeParams
 *    Pointer to a ::NV_ENC_RECONFIGURE_PARAMS structure.
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_NO_ENCODE_DEVICE \n
 * ::NV_ENC_ERR_UNSUPPORTED_DEVICE \n
 * ::NV_ENC_ERR_INVALID_DEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_GENERIC \n
 *
 */
NVENCSTATUS NVENCAPI NvEncReconfigureEncoder                   (void *encoder, NV_ENC_RECONFIGURE_PARAMS* reInitEncodeParams);



// NvEncCreateMVBuffer
/**
 * \brief Allocates output MV buffer for ME only mode.
 *
 * This function is used to allocate an output MV buffer. The size of the mvBuffer is
 * dependent on the frame height and width of the last ::NvEncCreateInputBuffer() call.
 * The NV_ENC_OUTPUT_PTR returned by the NvEncodeAPI interface in the
 * ::NV_ENC_CREATE_MV_BUFFER::mvBuffer field should be used in
 * ::NvEncRunMotionEstimationOnly() API.
 * Client must lock ::NV_ENC_CREATE_MV_BUFFER::mvBuffer using ::NvEncLockBitstream() API to get the motion vector data.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in,out] createMVBufferParams
 *  Pointer to the ::NV_ENC_CREATE_MV_BUFFER structure.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_GENERIC \n
 */
NVENCSTATUS NVENCAPI NvEncCreateMVBuffer                        (void* encoder, NV_ENC_CREATE_MV_BUFFER* createMVBufferParams);


// NvEncDestroyMVBuffer
/**
 * \brief Release an output MV buffer for ME only mode.
 *
 * This function is used to release the output MV buffer allocated using
 * the ::NvEncCreateMVBuffer() function. The client must release the output
 * mvBuffer using this function before destroying the encoder session.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] mvBuffer
 *   Pointer to the mvBuffer being released.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_ENCODER_NOT_INITIALIZED \n
 * ::NV_ENC_ERR_GENERIC \n
 */
NVENCSTATUS NVENCAPI NvEncDestroyMVBuffer                       (void* encoder, NV_ENC_OUTPUT_PTR mvBuffer);


// NvEncRunMotionEstimationOnly
/**
 * \brief Submit an input picture and reference frame for motion estimation in ME only mode.
 *
 * This function is used to submit the input frame and reference frame for motion
 * estimation. The ME parameters are passed using *meOnlyParams which is a pointer
 * to ::_NV_ENC_MEONLY_PARAMS structure.
 * Client must lock ::NV_ENC_CREATE_MV_BUFFER::mvBuffer using ::NvEncLockBitstream() API to get the motion vector data.
 * to get motion vector data.
 *
 * \param [in] encoder
 *   Pointer to the NvEncodeAPI interface.
 * \param [in] meOnlyParams
 *   Pointer to the ::_NV_ENC_MEONLY_PARAMS structure.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 * ::NV_ENC_ERR_INVALID_ENCODERDEVICE \n
 * ::NV_ENC_ERR_DEVICE_NOT_EXIST \n
 * ::NV_ENC_ERR_UNSUPPORTED_PARAM \n
 * ::NV_ENC_ERR_OUT_OF_MEMORY \n
 * ::NV_ENC_ERR_INVALID_PARAM \n
 * ::NV_ENC_ERR_INVALID_VERSION \n
 * ::NV_ENC_ERR_NEED_MORE_INPUT \n
 * ::NV_ENC_ERR_ENCODER_NOT_INITIALIZED \n
 * ::NV_ENC_ERR_GENERIC \n
 */
NVENCSTATUS NVENCAPI NvEncRunMotionEstimationOnly               (void* encoder, NV_ENC_MEONLY_PARAMS* meOnlyParams);

// NvEncodeAPIGetMaxSupportedVersion
/**
 * \brief Get the largest NvEncodeAPI version supported by the driver.
 *
 * This function can be used by clients to determine if the driver supports
 * the NvEncodeAPI header the application was compiled with.
 *
 * \param [out] version
 *   Pointer to the requested value. The 4 least significant bits in the returned
 *   indicate the minor version and the rest of the bits indicate the major
 *   version of the largest supported version.
 *
 * \return
 * ::NV_ENC_SUCCESS \n
 * ::NV_ENC_ERR_INVALID_PTR \n
 */
NVENCSTATUS NVENCAPI NvEncodeAPIGetMaxSupportedVersion          (uint32_t* version);


/// \cond API PFN
/*
 *  Defines API function pointers
 */
typedef NVENCSTATUS (NVENCAPI* PNVENCOPENENCODESESSION)         (void* device, uint32_t deviceType, void** encoder);
typedef NVENCSTATUS (NVENCAPI* PNVENCGETENCODEGUIDCOUNT)        (void* encoder, uint32_t* encodeGUIDCount);
typedef NVENCSTATUS (NVENCAPI* PNVENCGETENCODEGUIDS)            (void* encoder, GUID* GUIDs, uint32_t guidArraySize, uint32_t* GUIDCount);
typedef NVENCSTATUS (NVENCAPI* PNVENCGETENCODEPROFILEGUIDCOUNT) (void* encoder, GUID encodeGUID, uint32_t* encodeProfileGUIDCount);
typedef NVENCSTATUS (NVENCAPI* PNVENCGETENCODEPROFILEGUIDS)     (void* encoder, GUID encodeGUID, GUID* profileGUIDs, uint32_t guidArraySize, uint32_t* GUIDCount);
typedef NVENCSTATUS (NVENCAPI* PNVENCGETINPUTFORMATCOUNT)       (void* encoder, GUID encodeGUID, uint32_t* inputFmtCount);
typedef NVENCSTATUS (NVENCAPI* PNVENCGETINPUTFORMATS)           (void* encoder, GUID encodeGUID, NV_ENC_BUFFER_FORMAT* inputFmts, uint32_t inputFmtArraySize, uint32_t* inputFmtCount);
typedef NVENCSTATUS (NVENCAPI* PNVENCGETENCODECAPS)             (void* encoder, GUID encodeGUID, NV_ENC_CAPS_PARAM* capsParam, int* capsVal);
typedef NVENCSTATUS (NVENCAPI* PNVENCGETENCODEPRESETCOUNT)      (void* encoder, GUID encodeGUID, uint32_t* encodePresetGUIDCount);
typedef NVENCSTATUS (NVENCAPI* PNVENCGETENCODEPRESETGUIDS)      (void* encoder, GUID encodeGUID, GUID* presetGUIDs, uint32_t guidArraySize, uint32_t* encodePresetGUIDCount);
typedef NVENCSTATUS (NVENCAPI* PNVENCGETENCODEPRESETCONFIG)     (void* encoder, GUID encodeGUID, GUID  presetGUID, NV_ENC_PRESET_CONFIG* presetConfig);
typedef NVENCSTATUS (NVENCAPI* PNVENCINITIALIZEENCODER)         (void* encoder, NV_ENC_INITIALIZE_PARAMS* createEncodeParams);
typedef NVENCSTATUS (NVENCAPI* PNVENCCREATEINPUTBUFFER)         (void* encoder, NV_ENC_CREATE_INPUT_BUFFER* createInputBufferParams);
typedef NVENCSTATUS (NVENCAPI* PNVENCDESTROYINPUTBUFFER)        (void* encoder, NV_ENC_INPUT_PTR inputBuffer);
typedef NVENCSTATUS (NVENCAPI* PNVENCCREATEBITSTREAMBUFFER)     (void* encoder, NV_ENC_CREATE_BITSTREAM_BUFFER* createBitstreamBufferParams);
typedef NVENCSTATUS (NVENCAPI* PNVENCDESTROYBITSTREAMBUFFER)    (void* encoder, NV_ENC_OUTPUT_PTR bitstreamBuffer);
typedef NVENCSTATUS (NVENCAPI* PNVENCENCODEPICTURE)             (void* encoder, NV_ENC_PIC_PARAMS* encodePicParams);
typedef NVENCSTATUS (NVENCAPI* PNVENCLOCKBITSTREAM)             (void* encoder, NV_ENC_LOCK_BITSTREAM* lockBitstreamBufferParams);
typedef NVENCSTATUS (NVENCAPI* PNVENCUNLOCKBITSTREAM)           (void* encoder, NV_ENC_OUTPUT_PTR bitstreamBuffer);
typedef NVENCSTATUS (NVENCAPI* PNVENCLOCKINPUTBUFFER)           (void* encoder, NV_ENC_LOCK_INPUT_BUFFER* lockInputBufferParams);
typedef NVENCSTATUS (NVENCAPI* PNVENCUNLOCKINPUTBUFFER)         (void* encoder, NV_ENC_INPUT_PTR inputBuffer);
typedef NVENCSTATUS (NVENCAPI* PNVENCGETENCODESTATS)            (void* encoder, NV_ENC_STAT* encodeStats);
typedef NVENCSTATUS (NVENCAPI* PNVENCGETSEQUENCEPARAMS)         (void* encoder, NV_ENC_SEQUENCE_PARAM_PAYLOAD* sequenceParamPayload);
typedef NVENCSTATUS (NVENCAPI* PNVENCREGISTERASYNCEVENT)        (void* encoder, NV_ENC_EVENT_PARAMS* eventParams);
typedef NVENCSTATUS (NVENCAPI* PNVENCUNREGISTERASYNCEVENT)      (void* encoder, NV_ENC_EVENT_PARAMS* eventParams);
typedef NVENCSTATUS (NVENCAPI* PNVENCMAPINPUTRESOURCE)          (void* encoder, NV_ENC_MAP_INPUT_RESOURCE* mapInputResParams);
typedef NVENCSTATUS (NVENCAPI* PNVENCUNMAPINPUTRESOURCE)        (void* encoder, NV_ENC_INPUT_PTR mappedInputBuffer);
typedef NVENCSTATUS (NVENCAPI* PNVENCDESTROYENCODER)            (void* encoder);
typedef NVENCSTATUS (NVENCAPI* PNVENCINVALIDATEREFFRAMES)       (void* encoder, uint64_t invalidRefFrameTimeStamp);
typedef NVENCSTATUS (NVENCAPI* PNVENCOPENENCODESESSIONEX)       (NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS *openSessionExParams, void** encoder);
typedef NVENCSTATUS (NVENCAPI* PNVENCREGISTERRESOURCE)          (void* encoder, NV_ENC_REGISTER_RESOURCE* registerResParams);
typedef NVENCSTATUS (NVENCAPI* PNVENCUNREGISTERRESOURCE)        (void* encoder, NV_ENC_REGISTERED_PTR registeredRes);
typedef NVENCSTATUS (NVENCAPI* PNVENCRECONFIGUREENCODER)        (void* encoder, NV_ENC_RECONFIGURE_PARAMS* reInitEncodeParams);

typedef NVENCSTATUS (NVENCAPI* PNVENCCREATEMVBUFFER)            (void* encoder, NV_ENC_CREATE_MV_BUFFER* createMVBufferParams);
typedef NVENCSTATUS (NVENCAPI* PNVENCDESTROYMVBUFFER)           (void* encoder, NV_ENC_OUTPUT_PTR mvBuffer);
typedef NVENCSTATUS (NVENCAPI* PNVENCRUNMOTIONESTIMATIONONLY)   (void* encoder, NV_ENC_MEONLY_PARAMS* meOnlyParams);


/// \endcond


/** @} */ /* END ENCODE_FUNC */

/**
 * \ingroup ENCODER_STRUCTURE
 * NV_ENCODE_API_FUNCTION_LIST
 */
typedef struct _NV_ENCODE_API_FUNCTION_LIST
{
    uint32_t                        version;                           /**< [in]: Client should pass NV_ENCODE_API_FUNCTION_LIST_VER.                               */
    uint32_t                        reserved;                          /**< [in]: Reserved and should be set to 0.                                                  */
    PNVENCOPENENCODESESSION         nvEncOpenEncodeSession;            /**< [out]: Client should access ::NvEncOpenEncodeSession() API through this pointer.        */
    PNVENCGETENCODEGUIDCOUNT        nvEncGetEncodeGUIDCount;           /**< [out]: Client should access ::NvEncGetEncodeGUIDCount() API through this pointer.       */
    PNVENCGETENCODEPRESETCOUNT      nvEncGetEncodeProfileGUIDCount;    /**< [out]: Client should access ::NvEncGetEncodeProfileGUIDCount() API through this pointer.*/
    PNVENCGETENCODEPRESETGUIDS      nvEncGetEncodeProfileGUIDs;        /**< [out]: Client should access ::NvEncGetEncodeProfileGUIDs() API through this pointer.    */
    PNVENCGETENCODEGUIDS            nvEncGetEncodeGUIDs;               /**< [out]: Client should access ::NvEncGetEncodeGUIDs() API through this pointer.           */
    PNVENCGETINPUTFORMATCOUNT       nvEncGetInputFormatCount;          /**< [out]: Client should access ::NvEncGetInputFormatCount() API through this pointer.      */
    PNVENCGETINPUTFORMATS           nvEncGetInputFormats;              /**< [out]: Client should access ::NvEncGetInputFormats() API through this pointer.          */
    PNVENCGETENCODECAPS             nvEncGetEncodeCaps;                /**< [out]: Client should access ::NvEncGetEncodeCaps() API through this pointer.            */
    PNVENCGETENCODEPRESETCOUNT      nvEncGetEncodePresetCount;         /**< [out]: Client should access ::NvEncGetEncodePresetCount() API through this pointer.     */
    PNVENCGETENCODEPRESETGUIDS      nvEncGetEncodePresetGUIDs;         /**< [out]: Client should access ::NvEncGetEncodePresetGUIDs() API through this pointer.     */
    PNVENCGETENCODEPRESETCONFIG     nvEncGetEncodePresetConfig;        /**< [out]: Client should access ::NvEncGetEncodePresetConfig() API through this pointer.    */
    PNVENCINITIALIZEENCODER         nvEncInitializeEncoder;            /**< [out]: Client should access ::NvEncInitializeEncoder() API through this pointer.        */
    PNVENCCREATEINPUTBUFFER         nvEncCreateInputBuffer;            /**< [out]: Client should access ::NvEncCreateInputBuffer() API through this pointer.        */
    PNVENCDESTROYINPUTBUFFER        nvEncDestroyInputBuffer;           /**< [out]: Client should access ::NvEncDestroyInputBuffer() API through this pointer.       */
    PNVENCCREATEBITSTREAMBUFFER     nvEncCreateBitstreamBuffer;        /**< [out]: Client should access ::NvEncCreateBitstreamBuffer() API through this pointer.    */
    PNVENCDESTROYBITSTREAMBUFFER    nvEncDestroyBitstreamBuffer;       /**< [out]: Client should access ::NvEncDestroyBitstreamBuffer() API through this pointer.   */
    PNVENCENCODEPICTURE             nvEncEncodePicture;                /**< [out]: Client should access ::NvEncEncodePicture() API through this pointer.            */
    PNVENCLOCKBITSTREAM             nvEncLockBitstream;                /**< [out]: Client should access ::NvEncLockBitstream() API through this pointer.            */
    PNVENCUNLOCKBITSTREAM           nvEncUnlockBitstream;              /**< [out]: Client should access ::NvEncUnlockBitstream() API through this pointer.          */
    PNVENCLOCKINPUTBUFFER           nvEncLockInputBuffer;              /**< [out]: Client should access ::NvEncLockInputBuffer() API through this pointer.          */
    PNVENCUNLOCKINPUTBUFFER         nvEncUnlockInputBuffer;            /**< [out]: Client should access ::NvEncUnlockInputBuffer() API through this pointer.        */
    PNVENCGETENCODESTATS            nvEncGetEncodeStats;               /**< [out]: Client should access ::NvEncGetEncodeStats() API through this pointer.           */
    PNVENCGETSEQUENCEPARAMS         nvEncGetSequenceParams;            /**< [out]: Client should access ::NvEncGetSequenceParams() API through this pointer.        */
    PNVENCREGISTERASYNCEVENT        nvEncRegisterAsyncEvent;           /**< [out]: Client should access ::NvEncRegisterAsyncEvent() API through this pointer.       */
    PNVENCUNREGISTERASYNCEVENT      nvEncUnregisterAsyncEvent;         /**< [out]: Client should access ::NvEncUnregisterAsyncEvent() API through this pointer.     */
    PNVENCMAPINPUTRESOURCE          nvEncMapInputResource;             /**< [out]: Client should access ::NvEncMapInputResource() API through this pointer.         */
    PNVENCUNMAPINPUTRESOURCE        nvEncUnmapInputResource;           /**< [out]: Client should access ::NvEncUnmapInputResource() API through this pointer.       */
    PNVENCDESTROYENCODER            nvEncDestroyEncoder;               /**< [out]: Client should access ::NvEncDestroyEncoder() API through this pointer.           */
    PNVENCINVALIDATEREFFRAMES       nvEncInvalidateRefFrames;          /**< [out]: Client should access ::NvEncInvalidateRefFrames() API through this pointer.      */
    PNVENCOPENENCODESESSIONEX       nvEncOpenEncodeSessionEx;          /**< [out]: Client should access ::NvEncOpenEncodeSession() API through this pointer.        */
    PNVENCREGISTERRESOURCE          nvEncRegisterResource;             /**< [out]: Client should access ::NvEncRegisterResource() API through this pointer.         */
    PNVENCUNREGISTERRESOURCE        nvEncUnregisterResource;           /**< [out]: Client should access ::NvEncUnregisterResource() API through this pointer.       */
    PNVENCRECONFIGUREENCODER        nvEncReconfigureEncoder;           /**< [out]: Client should access ::NvEncReconfigureEncoder() API through this pointer.       */
    void*                           reserved1;
    PNVENCCREATEMVBUFFER            nvEncCreateMVBuffer;               /**< [out]: Client should access ::NvEncCreateMVBuffer API through this pointer.             */
    PNVENCDESTROYMVBUFFER           nvEncDestroyMVBuffer;              /**< [out]: Client should access ::NvEncDestroyMVBuffer API through this pointer.            */
    PNVENCRUNMOTIONESTIMATIONONLY   nvEncRunMotionEstimationOnly;      /**< [out]: Client should access ::NvEncRunMotionEstimationOnly API through this pointer.    */
    void*                           reserved2[281];                    /**< [in]:  Reserved and must be set to NULL                                                 */
} NV_ENCODE_API_FUNCTION_LIST;

/** Macro for constructing the version field of ::_NV_ENCODEAPI_FUNCTION_LIST. */
#define NV_ENCODE_API_FUNCTION_LIST_VER NVENCAPI_STRUCT_VERSION(2)

// NvEncodeAPICreateInstance
/**
 * \ingroup ENCODE_FUNC
 * Entry Point to the NvEncodeAPI interface.
 *
 * Creates an instance of the NvEncodeAPI interface, and populates the
 * pFunctionList with function pointers to the API routines implemented by the
 * NvEncodeAPI interface.
 *
 * \param [out] functionList
 *
 * \return
 * ::NV_ENC_SUCCESS
 * ::NV_ENC_ERR_INVALID_PTR
 */
NVENCSTATUS NVENCAPI NvEncodeAPICreateInstance(NV_ENCODE_API_FUNCTION_LIST *functionList);

#ifdef __cplusplus
}
#endif


#endif

