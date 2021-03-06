/******************************************************************************* *\

Copyright (C) 2007-2018 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfxstructures.h

*******************************************************************************/
#ifndef __MFXSTRUCTURES_H__
#define __MFXSTRUCTURES_H__
#include "mfxcommon.h"

#if !defined (__GNUC__)
#pragma warning(disable: 4201)
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* Frame ID for SVC and MVC */
typedef struct {
    mfxU16      TemporalId;
    mfxU16      PriorityId;
    union {
        struct {
            mfxU16  DependencyId;
            mfxU16  QualityId;
        };
        struct {
            mfxU16  ViewId;
        };
    };
} mfxFrameId;

#pragma pack(push, 4)
/* Frame Info */
typedef struct {
    mfxU32  reserved[4];
    mfxU16  reserved4;
    mfxU16  BitDepthLuma;
    mfxU16  BitDepthChroma;
    mfxU16  Shift;

    mfxFrameId FrameId;

    mfxU32  FourCC;
    union {
        struct { /* Frame parameters */
            mfxU16  Width;
            mfxU16  Height;

            mfxU16  CropX;
            mfxU16  CropY;
            mfxU16  CropW;
            mfxU16  CropH;
        };
        struct { /* Buffer parameters (for plain formats like P8) */
            mfxU64 BufferSize;
            mfxU32 reserved5;
        };
    };

    mfxU32  FrameRateExtN;
    mfxU32  FrameRateExtD;
    mfxU16  reserved3;

    mfxU16  AspectRatioW;
    mfxU16  AspectRatioH;

    mfxU16  PicStruct;
    mfxU16  ChromaFormat;
    mfxU16  reserved2;
} mfxFrameInfo;
#pragma pack(pop)

/* FourCC */
enum {
    MFX_FOURCC_NV12         = MFX_MAKEFOURCC('N','V','1','2'),   /* Native Format */
    MFX_FOURCC_YV12         = MFX_MAKEFOURCC('Y','V','1','2'),
    MFX_FOURCC_NV16         = MFX_MAKEFOURCC('N','V','1','6'),
    MFX_FOURCC_YUY2         = MFX_MAKEFOURCC('Y','U','Y','2'),
    MFX_FOURCC_RGB3         = MFX_MAKEFOURCC('R','G','B','3'),   /* deprecated */
    MFX_FOURCC_RGB4         = MFX_MAKEFOURCC('R','G','B','4'),   /* ARGB in that order, A channel is 8 MSBs */
    MFX_FOURCC_P8           = 41,                                /*  D3DFMT_P8   */
    MFX_FOURCC_P8_TEXTURE   = MFX_MAKEFOURCC('P','8','M','B'),
    MFX_FOURCC_P010         = MFX_MAKEFOURCC('P','0','1','0'),
    MFX_FOURCC_P210         = MFX_MAKEFOURCC('P','2','1','0'),
    MFX_FOURCC_BGR4         = MFX_MAKEFOURCC('B','G','R','4'),   /* ABGR in that order, A channel is 8 MSBs */
    MFX_FOURCC_A2RGB10      = MFX_MAKEFOURCC('R','G','1','0'),   /* ARGB in that order, A channel is two MSBs */
    MFX_FOURCC_ARGB16       = MFX_MAKEFOURCC('R','G','1','6'),   /* ARGB in that order, 64 bits, A channel is 16 MSBs */
    MFX_FOURCC_ABGR16       = MFX_MAKEFOURCC('B','G','1','6'),   /* ABGR in that order, 64 bits, A channel is 16 MSBs */
    MFX_FOURCC_R16          = MFX_MAKEFOURCC('R','1','6','U'),
    MFX_FOURCC_AYUV         = MFX_MAKEFOURCC('A','Y','U','V'),   /* YUV 4:4:4, AYUV in that order, A channel is 8 MSBs */
    MFX_FOURCC_AYUV_RGB4    = MFX_MAKEFOURCC('A','V','U','Y'),   /* ARGB in that order, A channel is 8 MSBs stored in AYUV surface*/
    MFX_FOURCC_UYVY         = MFX_MAKEFOURCC('U','Y','V','Y'),
    MFX_FOURCC_Y210         = MFX_MAKEFOURCC('Y','2','1','0'),
    MFX_FOURCC_Y410         = MFX_MAKEFOURCC('Y','4','1','0'),
};

/* PicStruct */
enum {
    MFX_PICSTRUCT_UNKNOWN       =0x00,
    MFX_PICSTRUCT_PROGRESSIVE   =0x01,
    MFX_PICSTRUCT_FIELD_TFF     =0x02,
    MFX_PICSTRUCT_FIELD_BFF     =0x04,

    MFX_PICSTRUCT_FIELD_REPEATED=0x10,  /* first field repeated, pic_struct=5 or 6 in H.264 */
    MFX_PICSTRUCT_FRAME_DOUBLING=0x20,  /* pic_struct=7 in H.264 */
    MFX_PICSTRUCT_FRAME_TRIPLING=0x40,  /* pic_struct=8 in H.264 */

    MFX_PICSTRUCT_FIELD_SINGLE      =0x100,
    MFX_PICSTRUCT_FIELD_TOP         =MFX_PICSTRUCT_FIELD_SINGLE | MFX_PICSTRUCT_FIELD_TFF,
    MFX_PICSTRUCT_FIELD_BOTTOM      =MFX_PICSTRUCT_FIELD_SINGLE | MFX_PICSTRUCT_FIELD_BFF,
    MFX_PICSTRUCT_FIELD_PAIRED_PREV =0x200,
    MFX_PICSTRUCT_FIELD_PAIRED_NEXT =0x400,
};

/* ColorFormat */
enum {
    MFX_CHROMAFORMAT_MONOCHROME =0,
    MFX_CHROMAFORMAT_YUV420     =1,
    MFX_CHROMAFORMAT_YUV422     =2,
    MFX_CHROMAFORMAT_YUV444     =3,
    MFX_CHROMAFORMAT_YUV400     = MFX_CHROMAFORMAT_MONOCHROME,
    MFX_CHROMAFORMAT_YUV411     = 4,
    MFX_CHROMAFORMAT_YUV422H    = MFX_CHROMAFORMAT_YUV422,
    MFX_CHROMAFORMAT_YUV422V    = 5,
    MFX_CHROMAFORMAT_RESERVED1  = 6
};

enum {
    MFX_TIMESTAMP_UNKNOWN = -1
};

enum {
    MFX_FRAMEORDER_UNKNOWN = -1
};

/* DataFlag in mfxFrameData */
enum {
    MFX_FRAMEDATA_ORIGINAL_TIMESTAMP = 0x0001
};

/* Corrupted in mfxFrameData */
enum {
    MFX_CORRUPTION_MINOR           = 0x0001,
    MFX_CORRUPTION_MAJOR           = 0x0002,
    MFX_CORRUPTION_ABSENT_TOP_FIELD           = 0x0004,
    MFX_CORRUPTION_ABSENT_BOTTOM_FIELD           = 0x0008,
    MFX_CORRUPTION_REFERENCE_FRAME = 0x0010,
    MFX_CORRUPTION_REFERENCE_LIST  = 0x0020
};

#pragma pack(push, 4)
typedef struct
{
    mfxU32 U : 10;
    mfxU32 Y : 10;
    mfxU32 V : 10;
    mfxU32 A :  2;
} mfxY410;
#pragma pack(pop)

#pragma pack(push, 4)
typedef struct
{
    mfxU32 B : 10;
    mfxU32 G : 10;
    mfxU32 R : 10;
    mfxU32 A :  2;
} mfxA2RGB10;
#pragma pack(pop)


/* Frame Data Info */
typedef struct {
    union {
        mfxExtBuffer **ExtParam;
        mfxU64       reserved2;
    };
    mfxU16  NumExtParam;

    mfxU16      reserved[9];
    mfxU16      MemType;
    mfxU16      PitchHigh;

    mfxU64      TimeStamp;
    mfxU32      FrameOrder;
    mfxU16      Locked;
    union{
        mfxU16  Pitch;
        mfxU16  PitchLow;
    };

    /* color planes */
    union {
        mfxU8   *Y;
        mfxU16  *Y16;
        mfxU8   *R;
    };
    union {
        mfxU8   *UV;            /* for UV merged formats */
        mfxU8   *VU;            /* for VU merged formats */
        mfxU8   *CbCr;          /* for CbCr merged formats */
        mfxU8   *CrCb;          /* for CrCb merged formats */
        mfxU8   *Cb;
        mfxU8   *U;
        mfxU16  *U16;
        mfxU8   *G;
        mfxY410 *Y410;          /* for Y410 format (merged AVYU) */
    };
    union {
        mfxU8   *Cr;
        mfxU8   *V;
        mfxU16  *V16;
        mfxU8   *B;
        mfxA2RGB10 *A2RGB10;    /* for A2RGB10 format (merged ARGB) */
    };
    mfxU8       *A;
    mfxMemId    MemId;

    /* Additional Flags */
    mfxU16  Corrupted;
    mfxU16  DataFlag;
} mfxFrameData;

/* Frame Surface */
typedef struct {
    mfxU32  reserved[4];
    mfxFrameInfo    Info;
    mfxFrameData    Data;
} mfxFrameSurface1;

enum {
    MFX_TIMESTAMPCALC_UNKNOWN = 0,
    MFX_TIMESTAMPCALC_TELECINE = 1,
};

/* Transcoding Info */
typedef struct {
    mfxU32  reserved[7];

    mfxU16  LowPower;
    mfxU16  BRCParamMultiplier;

    mfxFrameInfo    FrameInfo;
    mfxU32  CodecId;
    mfxU16  CodecProfile;
    mfxU16  CodecLevel;
    mfxU16  NumThread;

    union {
        struct {   /* Encoding Options */
            mfxU16  TargetUsage;

            mfxU16  GopPicSize;
            mfxU16  GopRefDist;
            mfxU16  GopOptFlag;
            mfxU16  IdrInterval;

            mfxU16  RateControlMethod;
            union {
                mfxU16  InitialDelayInKB;
                mfxU16  QPI;
                mfxU16  Accuracy;
            };
            mfxU16  BufferSizeInKB;
            union {
                mfxU16  TargetKbps;
                mfxU16  QPP;
                mfxU16  ICQQuality;
            };
            union {
                mfxU16  MaxKbps;
                mfxU16  QPB;
                mfxU16  Convergence;
            };

            mfxU16  NumSlice;
            mfxU16  NumRefFrame;
            mfxU16  EncodedOrder;
        };
        struct {   /* Decoding Options */
            mfxU16  DecodedOrder;
            mfxU16  ExtendedPicStruct;
            mfxU16  TimeStampCalc;
            mfxU16  SliceGroupsPresent;
            mfxU16  MaxDecFrameBuffering;
            mfxU16  EnableReallocRequest;
            mfxU16  reserved2[7];
        };
        struct {   /* JPEG Decoding Options */
            mfxU16  JPEGChromaFormat;
            mfxU16  Rotation;
            mfxU16  JPEGColorFormat;
            mfxU16  InterleavedDec;
            mfxU8   SamplingFactorH[4];
            mfxU8   SamplingFactorV[4];
            mfxU16  reserved3[5];
        };
        struct {   /* JPEG Encoding Options */
            mfxU16  Interleaved;
            mfxU16  Quality;
            mfxU16  RestartInterval;
            mfxU16  reserved5[10];
        };
    };
} mfxInfoMFX;

typedef struct {
    mfxU32  reserved[8];
    mfxFrameInfo    In;
    mfxFrameInfo    Out;
} mfxInfoVPP;

typedef struct {
    mfxU32  AllocId;
    mfxU32  reserved[2];
    mfxU16  reserved3;
    mfxU16  AsyncDepth;

    union {
        mfxInfoMFX  mfx;
        mfxInfoVPP  vpp;
    };
    mfxU16  Protected;
    mfxU16  IOPattern;
    mfxExtBuffer** ExtParam;
    mfxU16  NumExtParam;
    mfxU16  reserved2;
} mfxVideoParam;

/* IOPattern */
enum {
    MFX_IOPATTERN_IN_VIDEO_MEMORY   = 0x01,
    MFX_IOPATTERN_IN_SYSTEM_MEMORY  = 0x02,
    MFX_IOPATTERN_IN_OPAQUE_MEMORY  = 0x04,
    MFX_IOPATTERN_OUT_VIDEO_MEMORY  = 0x10,
    MFX_IOPATTERN_OUT_SYSTEM_MEMORY = 0x20,
    MFX_IOPATTERN_OUT_OPAQUE_MEMORY = 0x40
};

/* CodecId */
enum {
    MFX_CODEC_AVC         =MFX_MAKEFOURCC('A','V','C',' '),
    MFX_CODEC_HEVC        =MFX_MAKEFOURCC('H','E','V','C'),
    MFX_CODEC_MPEG2       =MFX_MAKEFOURCC('M','P','G','2'),
    MFX_CODEC_VC1         =MFX_MAKEFOURCC('V','C','1',' '),
    MFX_CODEC_CAPTURE     =MFX_MAKEFOURCC('C','A','P','T'),
    MFX_CODEC_VP9         =MFX_MAKEFOURCC('V','P','9',' '),
    MFX_CODEC_AV1         =MFX_MAKEFOURCC('A','V','1',' ')
};

/* CodecProfile, CodecLevel */
enum {
    MFX_PROFILE_UNKNOWN                     =0,
    MFX_LEVEL_UNKNOWN                       =0,

    /* AVC Profiles & Levels */
    MFX_PROFILE_AVC_CONSTRAINT_SET0     = (0x100 << 0),
    MFX_PROFILE_AVC_CONSTRAINT_SET1     = (0x100 << 1),
    MFX_PROFILE_AVC_CONSTRAINT_SET2     = (0x100 << 2),
    MFX_PROFILE_AVC_CONSTRAINT_SET3     = (0x100 << 3),
    MFX_PROFILE_AVC_CONSTRAINT_SET4     = (0x100 << 4),
    MFX_PROFILE_AVC_CONSTRAINT_SET5     = (0x100 << 5),

    MFX_PROFILE_AVC_BASELINE                =66,
    MFX_PROFILE_AVC_MAIN                    =77,
    MFX_PROFILE_AVC_EXTENDED                =88,
    MFX_PROFILE_AVC_HIGH                    =100,
    MFX_PROFILE_AVC_HIGH_422                =122,
    MFX_PROFILE_AVC_CONSTRAINED_BASELINE    =MFX_PROFILE_AVC_BASELINE + MFX_PROFILE_AVC_CONSTRAINT_SET1,
    MFX_PROFILE_AVC_CONSTRAINED_HIGH        =MFX_PROFILE_AVC_HIGH     + MFX_PROFILE_AVC_CONSTRAINT_SET4
                                                                      + MFX_PROFILE_AVC_CONSTRAINT_SET5,
    MFX_PROFILE_AVC_PROGRESSIVE_HIGH        =MFX_PROFILE_AVC_HIGH     + MFX_PROFILE_AVC_CONSTRAINT_SET4,

    MFX_LEVEL_AVC_1                         =10,
    MFX_LEVEL_AVC_1b                        =9,
    MFX_LEVEL_AVC_11                        =11,
    MFX_LEVEL_AVC_12                        =12,
    MFX_LEVEL_AVC_13                        =13,
    MFX_LEVEL_AVC_2                         =20,
    MFX_LEVEL_AVC_21                        =21,
    MFX_LEVEL_AVC_22                        =22,
    MFX_LEVEL_AVC_3                         =30,
    MFX_LEVEL_AVC_31                        =31,
    MFX_LEVEL_AVC_32                        =32,
    MFX_LEVEL_AVC_4                         =40,
    MFX_LEVEL_AVC_41                        =41,
    MFX_LEVEL_AVC_42                        =42,
    MFX_LEVEL_AVC_5                         =50,
    MFX_LEVEL_AVC_51                        =51,
    MFX_LEVEL_AVC_52                        =52,

    /* MPEG-2 Profiles & Levels */
    MFX_PROFILE_MPEG2_SIMPLE                =0x50,
    MFX_PROFILE_MPEG2_MAIN                  =0x40,
    MFX_PROFILE_MPEG2_HIGH                  =0x10,

    MFX_LEVEL_MPEG2_LOW                     =0xA,
    MFX_LEVEL_MPEG2_MAIN                    =0x8,
    MFX_LEVEL_MPEG2_HIGH                    =0x4,
    MFX_LEVEL_MPEG2_HIGH1440                =0x6,

    /* VC1 Profiles & Levels */
    MFX_PROFILE_VC1_SIMPLE                  =(0+1),
    MFX_PROFILE_VC1_MAIN                    =(4+1),
    MFX_PROFILE_VC1_ADVANCED                =(12+1),

    /* VC1 levels for simple & main profiles */
    MFX_LEVEL_VC1_LOW                       =(0+1),
    MFX_LEVEL_VC1_MEDIAN                    =(2+1),
    MFX_LEVEL_VC1_HIGH                      =(4+1),

    /* VC1 levels for the advanced profile */
    MFX_LEVEL_VC1_0                         =(0x00+1),
    MFX_LEVEL_VC1_1                         =(0x01+1),
    MFX_LEVEL_VC1_2                         =(0x02+1),
    MFX_LEVEL_VC1_3                         =(0x03+1),
    MFX_LEVEL_VC1_4                         =(0x04+1),

    /* HEVC Profiles & Levels & Tiers */
    MFX_PROFILE_HEVC_MAIN             =1,
    MFX_PROFILE_HEVC_MAIN10           =2,
    MFX_PROFILE_HEVC_MAINSP           =3,
    MFX_PROFILE_HEVC_REXT             =4,

    MFX_LEVEL_HEVC_1   = 10,
    MFX_LEVEL_HEVC_2   = 20,
    MFX_LEVEL_HEVC_21  = 21,
    MFX_LEVEL_HEVC_3   = 30,
    MFX_LEVEL_HEVC_31  = 31,
    MFX_LEVEL_HEVC_4   = 40,
    MFX_LEVEL_HEVC_41  = 41,
    MFX_LEVEL_HEVC_5   = 50,
    MFX_LEVEL_HEVC_51  = 51,
    MFX_LEVEL_HEVC_52  = 52,
    MFX_LEVEL_HEVC_6   = 60,
    MFX_LEVEL_HEVC_61  = 61,
    MFX_LEVEL_HEVC_62  = 62,

    MFX_TIER_HEVC_MAIN  = 0,
    MFX_TIER_HEVC_HIGH  = 0x100,

    /* VP9 Profiles */
    MFX_PROFILE_VP9_0                       = 1,
    MFX_PROFILE_VP9_1                       = 2,
    MFX_PROFILE_VP9_2                       = 3,
    MFX_PROFILE_VP9_3                       = 4,

};

/* GopOptFlag */
enum {
    MFX_GOP_CLOSED          =1,
    MFX_GOP_STRICT          =2
};

/* TargetUsages: from 1 to 7 inclusive */
enum {
    MFX_TARGETUSAGE_1    =1,
    MFX_TARGETUSAGE_2    =2,
    MFX_TARGETUSAGE_3    =3,
    MFX_TARGETUSAGE_4    =4,
    MFX_TARGETUSAGE_5    =5,
    MFX_TARGETUSAGE_6    =6,
    MFX_TARGETUSAGE_7    =7,

    MFX_TARGETUSAGE_UNKNOWN         =0,
    MFX_TARGETUSAGE_BEST_QUALITY    =MFX_TARGETUSAGE_1,
    MFX_TARGETUSAGE_BALANCED        =MFX_TARGETUSAGE_4,
    MFX_TARGETUSAGE_BEST_SPEED      =MFX_TARGETUSAGE_7
};

/* RateControlMethod */
enum {
    MFX_RATECONTROL_CBR       =1,
    MFX_RATECONTROL_VBR       =2,
    MFX_RATECONTROL_CQP       =3,
    MFX_RATECONTROL_AVBR      =4,
    MFX_RATECONTROL_RESERVED1 =5,
    MFX_RATECONTROL_RESERVED2 =6,
    MFX_RATECONTROL_RESERVED3 =100,
    MFX_RATECONTROL_RESERVED4 =7,
    MFX_RATECONTROL_LA        =8,
    MFX_RATECONTROL_ICQ       =9,
    MFX_RATECONTROL_VCM       =10,
    MFX_RATECONTROL_LA_ICQ    =11,
    MFX_RATECONTROL_LA_EXT    =12,
    MFX_RATECONTROL_LA_HRD    =13,
    MFX_RATECONTROL_QVBR      =14,
};

/* Trellis control*/
enum {
    MFX_TRELLIS_UNKNOWN =0,
    MFX_TRELLIS_OFF     =0x01,
    MFX_TRELLIS_I       =0x02,
    MFX_TRELLIS_P       =0x04,
    MFX_TRELLIS_B       =0x08
};

typedef struct {
    mfxExtBuffer Header;

    mfxU16      reserved1;
    mfxU16      RateDistortionOpt;      /* tri-state option */
    mfxU16      MECostType;
    mfxU16      MESearchType;
    mfxI16Pair  MVSearchWindow;
    mfxU16      EndOfSequence;          /* tri-state option */
    mfxU16      FramePicture;           /* tri-state option */

    mfxU16      CAVLC;                  /* tri-state option */
    mfxU16      reserved2[2];
    mfxU16      RecoveryPointSEI;       /* tri-state option */
    mfxU16      ViewOutput;             /* tri-state option */
    mfxU16      NalHrdConformance;      /* tri-state option */
    mfxU16      SingleSeiNalUnit;       /* tri-state option */
    mfxU16      VuiVclHrdParameters;    /* tri-state option */

    mfxU16      RefPicListReordering;   /* tri-state option */
    mfxU16      ResetRefList;           /* tri-state option */
    mfxU16      RefPicMarkRep;          /* tri-state option */
    mfxU16      FieldOutput;            /* tri-state option */

    mfxU16      IntraPredBlockSize;
    mfxU16      InterPredBlockSize;
    mfxU16      MVPrecision;
    mfxU16      MaxDecFrameBuffering;

    mfxU16      AUDelimiter;            /* tri-state option */
    mfxU16      EndOfStream;            /* tri-state option */
    mfxU16      PicTimingSEI;           /* tri-state option */
    mfxU16      VuiNalHrdParameters;    /* tri-state option */
} mfxExtCodingOption;

enum {
    MFX_B_REF_UNKNOWN = 0,
    MFX_B_REF_OFF     = 1,
    MFX_B_REF_PYRAMID = 2
};

enum {
    MFX_LOOKAHEAD_DS_UNKNOWN = 0,
    MFX_LOOKAHEAD_DS_OFF     = 1,
    MFX_LOOKAHEAD_DS_2x      = 2,
    MFX_LOOKAHEAD_DS_4x      = 3
};

enum {
    MFX_BPSEI_DEFAULT = 0x00,
    MFX_BPSEI_IFRAME  = 0x01
};

enum {
    MFX_SKIPFRAME_NO_SKIP         = 0,
    MFX_SKIPFRAME_INSERT_DUMMY    = 1,
    MFX_SKIPFRAME_INSERT_NOTHING  = 2,
    MFX_SKIPFRAME_BRC_ONLY        = 3,
};

/* Intra refresh types */
enum {
        MFX_REFRESH_NO             = 0,
        MFX_REFRESH_VERTICAL       = 1,
        MFX_REFRESH_HORIZONTAL     = 2,
        MFX_REFRESH_SLICE          = 3
};

typedef struct {
    mfxExtBuffer Header;

    mfxU16      IntRefType;
    mfxU16      IntRefCycleSize;
    mfxI16      IntRefQPDelta;

    mfxU32      MaxFrameSize;
    mfxU32      MaxSliceSize;

    mfxU16      BitrateLimit;           /* tri-state option */
    mfxU16      MBBRC;                  /* tri-state option */
    mfxU16      ExtBRC;                 /* tri-state option */
    mfxU16      LookAheadDepth;
    mfxU16      Trellis;
    mfxU16      RepeatPPS;              /* tri-state option */
    mfxU16      BRefType;
    mfxU16      AdaptiveI;              /* tri-state option */
    mfxU16      AdaptiveB;              /* tri-state option */
    mfxU16      LookAheadDS;
    mfxU16      NumMbPerSlice;
    mfxU16      SkipFrame;
    mfxU8       MinQPI;                 /* 1..51, 0 = default */
    mfxU8       MaxQPI;                 /* 1..51, 0 = default */
    mfxU8       MinQPP;                 /* 1..51, 0 = default */
    mfxU8       MaxQPP;                 /* 1..51, 0 = default */
    mfxU8       MinQPB;                 /* 1..51, 0 = default */
    mfxU8       MaxQPB;                 /* 1..51, 0 = default */
    mfxU16      FixedFrameRate;         /* tri-state option */
    mfxU16      DisableDeblockingIdc;
    mfxU16      DisableVUI;
    mfxU16      BufferingPeriodSEI;
    mfxU16      EnableMAD;              /* tri-state option */
    mfxU16      UseRawRef;              /* tri-state option */
} mfxExtCodingOption2;

/* WeightedPred */
enum {
    MFX_WEIGHTED_PRED_UNKNOWN  = 0,
    MFX_WEIGHTED_PRED_DEFAULT  = 1,
    MFX_WEIGHTED_PRED_EXPLICIT = 2,
    MFX_WEIGHTED_PRED_IMPLICIT = 3
};

/* ScenarioInfo */
enum {
    MFX_SCENARIO_UNKNOWN             = 0,
    MFX_SCENARIO_DISPLAY_REMOTING    = 1,
    MFX_SCENARIO_VIDEO_CONFERENCE    = 2,
    MFX_SCENARIO_ARCHIVE             = 3,
    MFX_SCENARIO_LIVE_STREAMING      = 4,
    MFX_SCENARIO_CAMERA_CAPTURE      = 5,
    MFX_SCENARIO_VIDEO_SURVEILLANCE  = 6,
    MFX_SCENARIO_GAME_STREAMING      = 7,
    MFX_SCENARIO_REMOTE_GAMING       = 8
};

/* ContentInfo */
enum {
    MFX_CONTENT_UNKNOWN              = 0,
    MFX_CONTENT_FULL_SCREEN_VIDEO    = 1,
    MFX_CONTENT_NON_VIDEO_SCREEN     = 2
};

/* PRefType */
enum {
    MFX_P_REF_DEFAULT = 0,
    MFX_P_REF_SIMPLE  = 1,
    MFX_P_REF_PYRAMID = 2
};


typedef struct {
    mfxExtBuffer Header;

    mfxU16      NumSliceI;
    mfxU16      NumSliceP;
    mfxU16      NumSliceB;

    mfxU16      WinBRCMaxAvgKbps;
    mfxU16      WinBRCSize;

    mfxU16      QVBRQuality;
    mfxU16      EnableMBQP;
    mfxU16      IntRefCycleDist;
    mfxU16      DirectBiasAdjustment;          /* tri-state option */
    mfxU16      GlobalMotionBiasAdjustment;    /* tri-state option */
    mfxU16      MVCostScalingFactor;
    mfxU16      MBDisableSkipMap;              /* tri-state option */

    mfxU16      WeightedPred;
    mfxU16      WeightedBiPred;

    mfxU16      AspectRatioInfoPresent;         /* tri-state option */
    mfxU16      OverscanInfoPresent;            /* tri-state option */
    mfxU16      OverscanAppropriate;            /* tri-state option */
    mfxU16      TimingInfoPresent;              /* tri-state option */
    mfxU16      BitstreamRestriction;           /* tri-state option */
    mfxU16      LowDelayHrd;                    /* tri-state option */
    mfxU16      MotionVectorsOverPicBoundaries; /* tri-state option */
    mfxU16      reserved1[2];

    mfxU16      ScenarioInfo;
    mfxU16      ContentInfo;

    mfxU16      PRefType;
    mfxU16      FadeDetection;            /* tri-state option */
    mfxU16      reserved2[2];
    mfxU16      GPB;                      /* tri-state option */

    mfxU32      MaxFrameSizeI;
    mfxU32      MaxFrameSizeP;
    mfxU32      reserved3[3];

    mfxU16      EnableQPOffset;           /* tri-state option */
    mfxI16      QPOffset[8];              /* FrameQP = QPX + QPOffset[pyramid_layer]; QPX = QPB for B-pyramid, QPP for P-pyramid */

    mfxU16      NumRefActiveP[8];
    mfxU16      NumRefActiveBL0[8];
    mfxU16      NumRefActiveBL1[8];

    mfxU16      reserved6;
    mfxU16      TransformSkip;  /* tri-state option; HEVC transform_skip_enabled_flag */
    mfxU16      TargetChromaFormatPlus1;   /* Minus 1 specifies target encoding chroma format (see ColorFormat enum). May differ from input one. */
    mfxU16      TargetBitDepthLuma;        /* Target encoding bit depth for luma samples. May differ from input one. */
    mfxU16      TargetBitDepthChroma;      /* Target encoding bit depth for chroma samples. May differ from input one. */
    mfxU16      BRCPanicMode;              /* tri-state option */

    mfxU16      LowDelayBRC;               /* tri-state option */
    mfxU16      EnableMBForceIntra;        /* tri-state option */
    mfxU16      AdaptiveMaxFrameSize;      /* tri-state option */

    mfxU16      RepartitionCheckEnable;    /* tri-state option */
    mfxU16      reserved5[3];
    mfxU16      EncodedUnitsInfo;          /* tri-state option */
    mfxU16      EnableNalUnitType;         /* tri-state option */
    mfxU16      ExtBrcAdaptiveLTR;         /* tri-state option for ExtBRC */
    mfxU16      reserved[163];
} mfxExtCodingOption3;

/* IntraPredBlockSize/InterPredBlockSize */
enum {
    MFX_BLOCKSIZE_UNKNOWN   = 0,
    MFX_BLOCKSIZE_MIN_16X16 = 1, /* 16x16              */
    MFX_BLOCKSIZE_MIN_8X8   = 2, /* 16x16, 8x8         */
    MFX_BLOCKSIZE_MIN_4X4   = 3  /* 16x16, 8x8, 4x4    */
};

/* MVPrecision */
enum {
    MFX_MVPRECISION_UNKNOWN    = 0,
    MFX_MVPRECISION_INTEGER    = (1 << 0),
    MFX_MVPRECISION_HALFPEL    = (1 << 1),
    MFX_MVPRECISION_QUARTERPEL = (1 << 2)
};

enum {
    MFX_CODINGOPTION_UNKNOWN    =0,
    MFX_CODINGOPTION_ON         =0x10,
    MFX_CODINGOPTION_OFF        =0x20,
    MFX_CODINGOPTION_ADAPTIVE   =0x30
};

/* Data Flag for mfxBitstream*/
enum {
    MFX_BITSTREAM_COMPLETE_FRAME    = 0x0001,        /* the bitstream contains a complete frame or field pair of data */
    MFX_BITSTREAM_EOS               = 0x0002
};
/* Extended Buffer Ids */
enum {
    MFX_EXTBUFF_CODING_OPTION                   = MFX_MAKEFOURCC('C','D','O','P'),
    MFX_EXTBUFF_CODING_OPTION_SPSPPS            = MFX_MAKEFOURCC('C','O','S','P'),
    MFX_EXTBUFF_VPP_DONOTUSE                    = MFX_MAKEFOURCC('N','U','S','E'),
    MFX_EXTBUFF_VPP_AUXDATA                     = MFX_MAKEFOURCC('A','U','X','D'),
    MFX_EXTBUFF_VPP_DENOISE                     = MFX_MAKEFOURCC('D','N','I','S'),
    MFX_EXTBUFF_VPP_SCENE_ANALYSIS              = MFX_MAKEFOURCC('S','C','L','Y'),
    MFX_EXTBUFF_VPP_SCENE_CHANGE                = MFX_EXTBUFF_VPP_SCENE_ANALYSIS,
    MFX_EXTBUFF_VPP_PROCAMP                     = MFX_MAKEFOURCC('P','A','M','P'),
    MFX_EXTBUFF_VPP_DETAIL                      = MFX_MAKEFOURCC('D','E','T',' '),
    MFX_EXTBUFF_VIDEO_SIGNAL_INFO               = MFX_MAKEFOURCC('V','S','I','N'),
    MFX_EXTBUFF_VPP_DOUSE                       = MFX_MAKEFOURCC('D','U','S','E'),
    MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION       = MFX_MAKEFOURCC('O','P','Q','S'),
    MFX_EXTBUFF_AVC_REFLIST_CTRL                = MFX_MAKEFOURCC('R','L','S','T'),
    MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION       = MFX_MAKEFOURCC('F','R','C',' '),
    MFX_EXTBUFF_PICTURE_TIMING_SEI              = MFX_MAKEFOURCC('P','T','S','E'),
    MFX_EXTBUFF_AVC_TEMPORAL_LAYERS             = MFX_MAKEFOURCC('A','T','M','L'),
    MFX_EXTBUFF_CODING_OPTION2                  = MFX_MAKEFOURCC('C','D','O','2'),
    MFX_EXTBUFF_VPP_IMAGE_STABILIZATION         = MFX_MAKEFOURCC('I','S','T','B'),
    MFX_EXTBUFF_VPP_PICSTRUCT_DETECTION         = MFX_MAKEFOURCC('I','D','E','T'),
    MFX_EXTBUFF_ENCODER_CAPABILITY              = MFX_MAKEFOURCC('E','N','C','P'),
    MFX_EXTBUFF_ENCODER_RESET_OPTION            = MFX_MAKEFOURCC('E','N','R','O'),
    MFX_EXTBUFF_ENCODED_FRAME_INFO              = MFX_MAKEFOURCC('E','N','F','I'),
    MFX_EXTBUFF_VPP_COMPOSITE                   = MFX_MAKEFOURCC('V','C','M','P'),
    MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO           = MFX_MAKEFOURCC('V','V','S','I'),
    MFX_EXTBUFF_ENCODER_ROI                     = MFX_MAKEFOURCC('E','R','O','I'),
    MFX_EXTBUFF_VPP_DEINTERLACING               = MFX_MAKEFOURCC('V','P','D','I'),
    MFX_EXTBUFF_AVC_REFLISTS                    = MFX_MAKEFOURCC('R','L','T','S'),
    MFX_EXTBUFF_DEC_VIDEO_PROCESSING            = MFX_MAKEFOURCC('D','E','C','V'),
    MFX_EXTBUFF_VPP_FIELD_PROCESSING            = MFX_MAKEFOURCC('F','P','R','O'),
    MFX_EXTBUFF_CODING_OPTION3                  = MFX_MAKEFOURCC('C','D','O','3'),
    MFX_EXTBUFF_CHROMA_LOC_INFO                 = MFX_MAKEFOURCC('C','L','I','N'),
    MFX_EXTBUFF_MBQP                            = MFX_MAKEFOURCC('M','B','Q','P'),
    MFX_EXTBUFF_MB_FORCE_INTRA                  = MFX_MAKEFOURCC('M','B','F','I'),
    MFX_EXTBUFF_HEVC_TILES                      = MFX_MAKEFOURCC('2','6','5','T'),
    MFX_EXTBUFF_MB_DISABLE_SKIP_MAP             = MFX_MAKEFOURCC('M','D','S','M'),
    MFX_EXTBUFF_HEVC_PARAM                      = MFX_MAKEFOURCC('2','6','5','P'),
    MFX_EXTBUFF_DECODED_FRAME_INFO              = MFX_MAKEFOURCC('D','E','F','I'),
    MFX_EXTBUFF_TIME_CODE                       = MFX_MAKEFOURCC('T','M','C','D'),
    MFX_EXTBUFF_HEVC_REGION                     = MFX_MAKEFOURCC('2','6','5','R'),
    MFX_EXTBUFF_PRED_WEIGHT_TABLE               = MFX_MAKEFOURCC('E','P','W','T'),
    MFX_EXTBUFF_DIRTY_RECTANGLES                = MFX_MAKEFOURCC('D','R','O','I'),
    MFX_EXTBUFF_MOVING_RECTANGLES               = MFX_MAKEFOURCC('M','R','O','I'),
    MFX_EXTBUFF_CODING_OPTION_VPS               = MFX_MAKEFOURCC('C','O','V','P'),
    MFX_EXTBUFF_VPP_ROTATION                    = MFX_MAKEFOURCC('R','O','T',' '),
    MFX_EXTBUFF_ENCODED_SLICES_INFO             = MFX_MAKEFOURCC('E','N','S','I'),
    MFX_EXTBUFF_VPP_SCALING                     = MFX_MAKEFOURCC('V','S','C','L'),
    MFX_EXTBUFF_HEVC_REFLIST_CTRL               = MFX_EXTBUFF_AVC_REFLIST_CTRL,
    MFX_EXTBUFF_HEVC_REFLISTS                   = MFX_EXTBUFF_AVC_REFLISTS,
    MFX_EXTBUFF_HEVC_TEMPORAL_LAYERS            = MFX_EXTBUFF_AVC_TEMPORAL_LAYERS,
    MFX_EXTBUFF_VPP_MIRRORING                   = MFX_MAKEFOURCC('M','I','R','R'),
    MFX_EXTBUFF_MV_OVER_PIC_BOUNDARIES          = MFX_MAKEFOURCC('M','V','P','B'),
    MFX_EXTBUFF_VPP_COLORFILL                   = MFX_MAKEFOURCC('V','C','L','F'),
    MFX_EXTBUFF_DECODE_ERROR_REPORT             = MFX_MAKEFOURCC('D', 'E', 'R', 'R'),
    MFX_EXTBUFF_VPP_COLOR_CONVERSION            = MFX_MAKEFOURCC('V', 'C', 'S', 'C'),
    MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO        = MFX_MAKEFOURCC('L', 'L', 'I', 'S'),
    MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME = MFX_MAKEFOURCC('D', 'C', 'V', 'S'),
    MFX_EXTBUFF_MULTI_FRAME_PARAM               = MFX_MAKEFOURCC('M', 'F', 'R', 'P'),
    MFX_EXTBUFF_MULTI_FRAME_CONTROL             = MFX_MAKEFOURCC('M', 'F', 'R', 'C'),
    MFX_EXTBUFF_ENCODED_UNITS_INFO              = MFX_MAKEFOURCC('E', 'N', 'U', 'I'),
    MFX_EXTBUFF_VPP_MCTF                        = MFX_MAKEFOURCC('M', 'C', 'T', 'F'),
    MFX_EXTBUFF_VP9_SEGMENTATION                = MFX_MAKEFOURCC('9', 'S', 'E', 'G'),
    MFX_EXTBUFF_VP9_TEMPORAL_LAYERS             = MFX_MAKEFOURCC('9', 'T', 'M', 'L'),
    MFX_EXTBUFF_VP9_PARAM                       = MFX_MAKEFOURCC('9', 'P', 'A', 'R'),
    MFX_EXTBUFF_AVC_ROUNDING_OFFSET             = MFX_MAKEFOURCC('R','N','D','O'),
};

/* VPP Conf: Do not use certain algorithms  */
typedef struct {
    mfxExtBuffer    Header;
    mfxU32          NumAlg;
    mfxU32*         AlgList;
} mfxExtVPPDoNotUse;

typedef struct {
    mfxExtBuffer    Header;
    mfxU16  DenoiseFactor;
} mfxExtVPPDenoise;

typedef struct {
    mfxExtBuffer    Header;
    mfxU16  DetailFactor;
} mfxExtVPPDetail;

typedef struct {
    mfxExtBuffer    Header;
    mfxF64   Brightness;
    mfxF64   Contrast;
    mfxF64   Hue;
    mfxF64   Saturation;
} mfxExtVPPProcAmp;

/* statistics collected for decode, encode and vpp */
typedef struct {
    mfxU32  reserved[16];
    mfxU32  NumFrame;
    mfxU64  NumBit;
    mfxU32  NumCachedFrame;
} mfxEncodeStat;

typedef struct {
    mfxU32  reserved[16];
    mfxU32  NumFrame;
    mfxU32  NumSkippedFrame;
    mfxU32  NumError;
    mfxU32  NumCachedFrame;
} mfxDecodeStat;

typedef struct {
    mfxU32  reserved[16];
    mfxU32  NumFrame;
    mfxU32  NumCachedFrame;
} mfxVPPStat;

typedef struct {
    mfxExtBuffer    Header;

    union{
        struct{
            mfxU32  SpatialComplexity;
            mfxU32  TemporalComplexity;
        };
        struct{
            mfxU16  PicStruct;
            mfxU16  reserved[3];
        };
    };
    mfxU16          SceneChangeRate;
    mfxU16          RepeatedFrame;
} mfxExtVppAuxData;

/* CtrlFlags */
enum {
    MFX_PAYLOAD_CTRL_SUFFIX = 0x00000001 /* HEVC suffix SEI */
};

typedef struct {
    mfxU32      CtrlFlags;
    mfxU32      reserved[3];
    mfxU8       *Data;      /* buffer pointer */
    mfxU32      NumBit;     /* number of bits */
    mfxU16      Type;       /* SEI message type in H.264 or user data start_code in MPEG-2 */
    mfxU16      BufSize;    /* payload buffer size in bytes */
} mfxPayload;

typedef struct {
    mfxExtBuffer    Header;
    mfxU32  reserved[4];
    mfxU16  reserved1;
    mfxU16  MfxNalUnitType;
    mfxU16  SkipFrame;

    mfxU16  QP; /* per frame QP */

    mfxU16  FrameType;
    mfxU16  NumExtParam;
    mfxU16  NumPayload;     /* MPEG-2 user data or H.264 SEI message(s) */
    mfxU16  reserved2;

    mfxExtBuffer    **ExtParam;
    mfxPayload      **Payload;      /* for field pair, first field uses even payloads and second field uses odd payloads */
} mfxEncodeCtrl;

/* Buffer Memory Types */
enum {
    /* Buffer types */
    MFX_MEMTYPE_PERSISTENT_MEMORY   =0x0002
};

/* Frame Memory Types */
#define MFX_MEMTYPE_BASE(x) (0x90ff & (x))

enum {
    MFX_MEMTYPE_DXVA2_DECODER_TARGET       =0x0010,
    MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET     =0x0020,
    MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET   = MFX_MEMTYPE_DXVA2_DECODER_TARGET,
    MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET,
    MFX_MEMTYPE_SYSTEM_MEMORY              =0x0040,
    MFX_MEMTYPE_RESERVED1                  =0x0080,

    MFX_MEMTYPE_FROM_ENCODE     = 0x0100,
    MFX_MEMTYPE_FROM_DECODE     = 0x0200,
    MFX_MEMTYPE_FROM_VPPIN      = 0x0400,
    MFX_MEMTYPE_FROM_VPPOUT     = 0x0800,
    MFX_MEMTYPE_FROM_ENC        = 0x2000,
    MFX_MEMTYPE_FROM_PAK        = 0x4000, //reserved

    MFX_MEMTYPE_INTERNAL_FRAME  = 0x0001,
    MFX_MEMTYPE_EXTERNAL_FRAME  = 0x0002,
    MFX_MEMTYPE_OPAQUE_FRAME    = 0x0004,
    MFX_MEMTYPE_EXPORT_FRAME    = 0x0008,
    MFX_MEMTYPE_SHARED_RESOURCE = MFX_MEMTYPE_EXPORT_FRAME,
    MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET = 0x1000
};

typedef struct {
    union {
        mfxU32  AllocId;
        mfxU32  reserved[1];
    };
    mfxU32  reserved3[3];
    mfxFrameInfo    Info;
    mfxU16  Type;   /* decoder or processor render targets */
    mfxU16  NumFrameMin;
    mfxU16  NumFrameSuggested;
    mfxU16  reserved2;
} mfxFrameAllocRequest;

typedef struct {
    mfxU32      AllocId;
    mfxU32      reserved[3];
    mfxMemId    *mids;      /* the array allocated by application */
    mfxU16      NumFrameActual;
    mfxU16      reserved2;
} mfxFrameAllocResponse;

/* FrameType */
enum {
    MFX_FRAMETYPE_UNKNOWN       =0x0000,

    MFX_FRAMETYPE_I             =0x0001,
    MFX_FRAMETYPE_P             =0x0002,
    MFX_FRAMETYPE_B             =0x0004,
    MFX_FRAMETYPE_S             =0x0008,

    MFX_FRAMETYPE_REF           =0x0040,
    MFX_FRAMETYPE_IDR           =0x0080,

    MFX_FRAMETYPE_xI            =0x0100,
    MFX_FRAMETYPE_xP            =0x0200,
    MFX_FRAMETYPE_xB            =0x0400,
    MFX_FRAMETYPE_xS            =0x0800,

    MFX_FRAMETYPE_xREF          =0x4000,
    MFX_FRAMETYPE_xIDR          =0x8000
};

enum {
    MFX_HEVC_NALU_TYPE_UNKNOWN    =      0,
    MFX_HEVC_NALU_TYPE_TRAIL_N    = ( 0+1),
    MFX_HEVC_NALU_TYPE_TRAIL_R    = ( 1+1),
    MFX_HEVC_NALU_TYPE_RADL_N     = ( 6+1),
    MFX_HEVC_NALU_TYPE_RADL_R     = ( 7+1),
    MFX_HEVC_NALU_TYPE_RASL_N     = ( 8+1),
    MFX_HEVC_NALU_TYPE_RASL_R     = ( 9+1),
    MFX_HEVC_NALU_TYPE_IDR_W_RADL = (19+1),
    MFX_HEVC_NALU_TYPE_IDR_N_LP   = (20+1),
    MFX_HEVC_NALU_TYPE_CRA_NUT    = (21+1)
};

typedef enum {
    MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9         =1,      /* IDirect3DDeviceManager9      */
    MFX_HANDLE_D3D9_DEVICE_MANAGER              = MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9,
    MFX_HANDLE_RESERVED1                        = 2,
    MFX_HANDLE_D3D11_DEVICE                     = 3,
    MFX_HANDLE_VA_DISPLAY                       = 4,
    MFX_HANDLE_RESERVED3                        = 5,
} mfxHandleType;

typedef enum {
    MFX_SKIPMODE_NOSKIP=0,
    MFX_SKIPMODE_MORE=1,
    MFX_SKIPMODE_LESS=2
} mfxSkipMode;

typedef struct {
    mfxExtBuffer    Header;
    mfxU8           *SPSBuffer;
    mfxU8           *PPSBuffer;
    mfxU16          SPSBufSize;
    mfxU16          PPSBufSize;
    mfxU16          SPSId;
    mfxU16          PPSId;
} mfxExtCodingOptionSPSPPS;

typedef struct {
    mfxExtBuffer    Header;

    union {
        mfxU8       *VPSBuffer;
        mfxU64      reserved1;
    };
    mfxU16          VPSBufSize;
    mfxU16          VPSId;

    mfxU16          reserved[6];
} mfxExtCodingOptionVPS;

typedef struct {
    mfxExtBuffer    Header;
    mfxU16          VideoFormat;
    mfxU16          VideoFullRange;
    mfxU16          ColourDescriptionPresent;
    mfxU16          ColourPrimaries;
    mfxU16          TransferCharacteristics;
    mfxU16          MatrixCoefficients;
} mfxExtVideoSignalInfo;

typedef struct {
    mfxExtBuffer    Header;
    mfxU32          NumAlg;
    mfxU32          *AlgList;
} mfxExtVPPDoUse;

typedef struct {
    mfxExtBuffer    Header;
    mfxU32      reserved1[2];
    struct {
        mfxFrameSurface1 **Surfaces;
        mfxU32  reserved2[5];
        mfxU16  Type;
        mfxU16  NumSurface;
    } In, Out;
} mfxExtOpaqueSurfaceAlloc;

typedef struct {
    mfxExtBuffer    Header;
    mfxU16          NumRefIdxL0Active;
    mfxU16          NumRefIdxL1Active;

    struct {
        mfxU32      FrameOrder;
        mfxU16      PicStruct;
        mfxU16      ViewId;
        mfxU16      LongTermIdx;
        mfxU16      reserved[3];
    } PreferredRefList[32], RejectedRefList[16], LongTermRefList[16];

    mfxU16      ApplyLongTermIdx;
    mfxU16      reserved[15];
} mfxExtAVCRefListCtrl;

enum {
    MFX_FRCALGM_PRESERVE_TIMESTAMP    = 0x0001,
    MFX_FRCALGM_DISTRIBUTED_TIMESTAMP = 0x0002,
    MFX_FRCALGM_FRAME_INTERPOLATION   = 0x0004
};

typedef struct {
    mfxExtBuffer    Header;
    mfxU16      Algorithm;
    mfxU16      reserved;
    mfxU32      reserved2[15];
} mfxExtVPPFrameRateConversion;

enum {
    MFX_IMAGESTAB_MODE_UPSCALE = 0x0001,
    MFX_IMAGESTAB_MODE_BOXING  = 0x0002
};

typedef struct {
    mfxExtBuffer    Header;
    mfxU16  Mode;
    mfxU16  reserved[11];
} mfxExtVPPImageStab;


enum {
    MFX_PAYLOAD_OFF = 0,
    MFX_PAYLOAD_IDR = 1
};

typedef struct {
    mfxExtBuffer    Header;
    mfxU16      reserved[15];

    mfxU16 InsertPayloadToggle;
    mfxU16 DisplayPrimariesX[3];
    mfxU16 DisplayPrimariesY[3];
    mfxU16 WhitePointX;
    mfxU16 WhitePointY;
    mfxU32 MaxDisplayMasteringLuminance;
    mfxU32 MinDisplayMasteringLuminance;
} mfxExtMasteringDisplayColourVolume;


typedef struct {
    mfxExtBuffer    Header;
    mfxU16      reserved[9];

    mfxU16 InsertPayloadToggle;
    mfxU16 MaxContentLightLevel;
    mfxU16 MaxPicAverageLightLevel;
} mfxExtContentLightLevelInfo;


typedef struct {
  mfxExtBuffer    Header;
  mfxU32      reserved[14];

  struct {
      mfxU16    ClockTimestampFlag;
      mfxU16    CtType;
      mfxU16    NuitFieldBasedFlag;
      mfxU16    CountingType;
      mfxU16    FullTimestampFlag;
      mfxU16    DiscontinuityFlag;
      mfxU16    CntDroppedFlag;
      mfxU16    NFrames;
      mfxU16    SecondsFlag;
      mfxU16    MinutesFlag;
      mfxU16    HoursFlag;
      mfxU16    SecondsValue;
      mfxU16    MinutesValue;
      mfxU16    HoursValue;
      mfxU32    TimeOffset;
  } TimeStamp[3];
} mfxExtPictureTimingSEI;

typedef struct {
    mfxExtBuffer    Header;
    mfxU32          reserved1[4];
    mfxU16          reserved2;
    mfxU16          BaseLayerPID;

    struct {
        mfxU16 Scale;
        mfxU16 reserved[3];
    }Layer[8];
} mfxExtAvcTemporalLayers;


typedef struct {
    mfxExtBuffer Header;

    mfxU32      MBPerSec;
    mfxU16      reserved[58];
} mfxExtEncoderCapability;

typedef struct {
    mfxExtBuffer Header;

    mfxU16      StartNewSequence;
    mfxU16      reserved[11];
} mfxExtEncoderResetOption;

/*LongTermIdx*/
enum {
    MFX_LONGTERM_IDX_NO_IDX = 0xFFFF
};

typedef struct {
    mfxExtBuffer    Header;

    mfxU32          FrameOrder;
    mfxU16          PicStruct;
    mfxU16          LongTermIdx;
    mfxU32          MAD;
    mfxU16          BRCPanicMode;
    mfxU16          QP;
    mfxU32          SecondFieldOffset;
    mfxU16          reserved[2];

    struct {
        mfxU32      FrameOrder;
        mfxU16      PicStruct;
        mfxU16      LongTermIdx;
        mfxU16      reserved[4];
    } UsedRefListL0[32], UsedRefListL1[32];
} mfxExtAVCEncodedFrameInfo;

typedef struct mfxVPPCompInputStream {
    mfxU32  DstX;
    mfxU32  DstY;
    mfxU32  DstW;
    mfxU32  DstH;

    mfxU16  LumaKeyEnable;
    mfxU16  LumaKeyMin;
    mfxU16  LumaKeyMax;

    mfxU16  GlobalAlphaEnable;
    mfxU16  GlobalAlpha;
    mfxU16  PixelAlphaEnable;

    mfxU16  TileId;

    mfxU16  reserved2[17];
} mfxVPPCompInputStream;

typedef struct {
    mfxExtBuffer    Header;

    /* background color*/
    union {
        mfxU16   Y;
        mfxU16   R;
    };
    union {
        mfxU16   U;
        mfxU16   G;
    };
    union {
        mfxU16   V;
        mfxU16   B;
    };
    mfxU16       NumTiles;
    mfxU16       reserved1[23];

    mfxU16       NumInputStream;
    mfxVPPCompInputStream *InputStream;
} mfxExtVPPComposite;

/* TransferMatrix */
enum {
    MFX_TRANSFERMATRIX_UNKNOWN = 0,
    MFX_TRANSFERMATRIX_BT709   = 1,
    MFX_TRANSFERMATRIX_BT601   = 2
};

/* NominalRange */
enum {
    MFX_NOMINALRANGE_UNKNOWN   = 0,
    MFX_NOMINALRANGE_0_255     = 1,
    MFX_NOMINALRANGE_16_235    = 2
};

typedef struct {
    mfxExtBuffer    Header;
    mfxU16          reserved1[4];

    union {
        struct { // Init
            struct  {
                mfxU16  TransferMatrix;
                mfxU16  NominalRange;
                mfxU16  reserved2[6];
            } In, Out;
        };
        struct { // Runtime
            mfxU16  TransferMatrix;
            mfxU16  NominalRange;
            mfxU16  reserved3[14];
        };
    };
} mfxExtVPPVideoSignalInfo;

/* ROI encoding mode */
enum {
    MFX_ROI_MODE_PRIORITY =  0,
    MFX_ROI_MODE_QP_DELTA =  1
};

typedef struct {
    mfxExtBuffer    Header;

    mfxU16  NumROI;
    mfxU16  ROIMode;
    mfxU16  reserved1[10];

    struct  {
        mfxU32  Left;
        mfxU32  Top;
        mfxU32  Right;
        mfxU32  Bottom;
        union {
            mfxI16  Priority;
            mfxI16  DeltaQP;
        };
        mfxU16  reserved2[7];
    } ROI[256];
} mfxExtEncoderROI;

/*Deinterlacing Mode*/
enum {
    MFX_DEINTERLACING_BOB                    =  1,
    MFX_DEINTERLACING_ADVANCED               =  2,
    MFX_DEINTERLACING_AUTO_DOUBLE            =  3,
    MFX_DEINTERLACING_AUTO_SINGLE            =  4,
    MFX_DEINTERLACING_FULL_FR_OUT            =  5,
    MFX_DEINTERLACING_HALF_FR_OUT            =  6,
    MFX_DEINTERLACING_24FPS_OUT              =  7,
    MFX_DEINTERLACING_FIXED_TELECINE_PATTERN =  8,
    MFX_DEINTERLACING_30FPS_OUT              =  9,
    MFX_DEINTERLACING_DETECT_INTERLACE       = 10,
    MFX_DEINTERLACING_ADVANCED_NOREF         = 11,
    MFX_DEINTERLACING_ADVANCED_SCD           = 12,
    MFX_DEINTERLACING_FIELD_WEAVING          = 13
};

/*TelecinePattern*/
enum {
    MFX_TELECINE_PATTERN_32           = 0,
    MFX_TELECINE_PATTERN_2332         = 1,
    MFX_TELECINE_PATTERN_FRAME_REPEAT = 2,
    MFX_TELECINE_PATTERN_41           = 3,
    MFX_TELECINE_POSITION_PROVIDED    = 4
};

typedef struct {
    mfxExtBuffer    Header;
    mfxU16  Mode;
    mfxU16  TelecinePattern;
    mfxU16  TelecineLocation;
    mfxU16  reserved[9];
} mfxExtVPPDeinterlacing;

typedef struct {
    mfxExtBuffer    Header;
    mfxU16          NumRefIdxL0Active;
    mfxU16          NumRefIdxL1Active;
    mfxU16          reserved[2];

    struct mfxRefPic{
        mfxU32      FrameOrder;
        mfxU16      PicStruct;
        mfxU16      reserved[5];
    } RefPicList0[32], RefPicList1[32];

}mfxExtAVCRefLists;

enum {
    MFX_VPP_COPY_FRAME      =0x01,
    MFX_VPP_COPY_FIELD      =0x02,
    MFX_VPP_SWAP_FIELDS     =0x03
};

/*PicType*/
enum {
    MFX_PICTYPE_UNKNOWN     =0x00,
    MFX_PICTYPE_FRAME       =0x01,
    MFX_PICTYPE_TOPFIELD    =0x02,
    MFX_PICTYPE_BOTTOMFIELD =0x04
};

typedef struct {
    mfxExtBuffer    Header;

    mfxU16          Mode;
    mfxU16          InField;
    mfxU16          OutField;
    mfxU16          reserved[25];
} mfxExtVPPFieldProcessing;

typedef struct {
    mfxExtBuffer    Header;

    struct mfxIn{
        mfxU16  CropX;
        mfxU16  CropY;
        mfxU16  CropW;
        mfxU16  CropH;
        mfxU16  reserved[12];
    }In;

    struct mfxOut{
        mfxU32  FourCC;
        mfxU16  ChromaFormat;
        mfxU16  reserved1;

        mfxU16  Width;
        mfxU16  Height;

        mfxU16  CropX;
        mfxU16  CropY;
        mfxU16  CropW;
        mfxU16  CropH;
        mfxU16  reserved[22];
    }Out;

    mfxU16  reserved[13];
} mfxExtDecVideoProcessing;

typedef struct {
    mfxExtBuffer Header;

    mfxU16       ChromaLocInfoPresentFlag;
    mfxU16       ChromaSampleLocTypeTopField;
    mfxU16       ChromaSampleLocTypeBottomField;
    mfxU16       reserved[9];
} mfxExtChromaLocInfo;

/* MBQPMode */
enum {
    MFX_MBQP_MODE_QP_VALUE = 0, // supported in CQP mode only
    MFX_MBQP_MODE_QP_DELTA = 1
};

typedef struct {
    mfxExtBuffer    Header;

    mfxU32 reserved[10];
    mfxU16 Mode;        // see MBQPMode enum
    mfxU16 BlockSize;   // QP block size, valid for HEVC only during Init and Runtime
    mfxU32 NumQPAlloc;  // Size of allocated by application QP or DeltaQP array
    union {
        mfxU8  *QP;         // Block QP value. Valid when Mode = MFX_MBQP_MODE_QP_VALUE
        mfxI8  *DeltaQP;    // For block i: QP[i] = BrcQP[i] + DeltaQP[i]. Valid when Mode = MFX_MBQP_MODE_QP_DELTA
        mfxU64 reserved2;
    };
} mfxExtMBQP;

typedef struct {
    mfxExtBuffer Header;

    mfxU32 reserved[11];
    mfxU32 MapSize;
    union {
        mfxU8  *Map;
        mfxU64  reserved2;
    };
} mfxExtMBForceIntra;

typedef struct {
    mfxExtBuffer Header;

    mfxU16 NumTileRows;
    mfxU16 NumTileColumns;
    mfxU16 reserved[74];
}mfxExtHEVCTiles;

typedef struct {
    mfxExtBuffer Header;

    mfxU32 reserved[11];
    mfxU32 MapSize;
    union {
        mfxU8  *Map;
        mfxU64  reserved2;
    };
} mfxExtMBDisableSkipMap;


/*GeneralConstraintFlags*/
enum {
    /* REXT Profile constraint flags*/
    MFX_HEVC_CONSTR_REXT_MAX_12BIT          = (1 << 0),
    MFX_HEVC_CONSTR_REXT_MAX_10BIT          = (1 << 1),
    MFX_HEVC_CONSTR_REXT_MAX_8BIT           = (1 << 2),
    MFX_HEVC_CONSTR_REXT_MAX_422CHROMA      = (1 << 3),
    MFX_HEVC_CONSTR_REXT_MAX_420CHROMA      = (1 << 4),
    MFX_HEVC_CONSTR_REXT_MAX_MONOCHROME     = (1 << 5),
    MFX_HEVC_CONSTR_REXT_INTRA              = (1 << 6),
    MFX_HEVC_CONSTR_REXT_ONE_PICTURE_ONLY   = (1 << 7),
    MFX_HEVC_CONSTR_REXT_LOWER_BIT_RATE     = (1 << 8)
};


/* SampleAdaptiveOffset */
enum {
    MFX_SAO_UNKNOWN       = 0x00,
    MFX_SAO_DISABLE       = 0x01,
    MFX_SAO_ENABLE_LUMA   = 0x02,
    MFX_SAO_ENABLE_CHROMA = 0x04
};


#pragma pack(push, 4)
typedef struct {
    mfxExtBuffer    Header;

    mfxU16          PicWidthInLumaSamples;
    mfxU16          PicHeightInLumaSamples;
    mfxU64          GeneralConstraintFlags;
    mfxU16          SampleAdaptiveOffset;   /* see enum SampleAdaptiveOffset, valid during Init and Runtime */
    mfxU16          LCUSize;
    mfxU16          reserved[116];
} mfxExtHEVCParam;
#pragma pack(pop)

/*ErrorTypes in mfxExtDecodeErrorReport*/
enum {
    MFX_ERROR_PPS           = (1 << 0),
    MFX_ERROR_SPS           = (1 << 1),
    MFX_ERROR_SLICEHEADER   = (1 << 2),
    MFX_ERROR_SLICEDATA     = (1 << 3),
    MFX_ERROR_FRAME_GAP     = (1 << 4),
};

typedef struct {
    mfxExtBuffer    Header;

    mfxU32          ErrorTypes;
    mfxU16          reserved[10];
} mfxExtDecodeErrorReport;


typedef struct {
    mfxExtBuffer Header;

    mfxU16       FrameType;
    mfxU16       reserved[59];
} mfxExtDecodedFrameInfo;

typedef struct {
    mfxExtBuffer Header;

    mfxU16       DropFrameFlag;
    mfxU16       TimeCodeHours;
    mfxU16       TimeCodeMinutes;
    mfxU16       TimeCodeSeconds;
    mfxU16       TimeCodePictures;
    mfxU16       reserved[7];
} mfxExtTimeCode;

/*RegionType*/
enum {
    MFX_HEVC_REGION_SLICE = 0
};

/*RegionEncoding*/
enum {
    MFX_HEVC_REGION_ENCODING_ON  = 0,
    MFX_HEVC_REGION_ENCODING_OFF = 1
};

typedef struct {
    mfxExtBuffer Header;

    mfxU32       RegionId;
    mfxU16       RegionType;
    mfxU16       RegionEncoding;
    mfxU16       reserved[24];
} mfxExtHEVCRegion;

typedef struct {
    mfxExtBuffer Header;

    mfxU16       LumaLog2WeightDenom;       // 0..7
    mfxU16       ChromaLog2WeightDenom;     // 0..7
    mfxU16       LumaWeightFlag[2][32];     // [list] 0,1
    mfxU16       ChromaWeightFlag[2][32];   // [list] 0,1
    mfxI16       Weights[2][32][3][2];      // [list][list entry][Y, Cb, Cr][weight, offset]
    mfxU16       reserved[58];
} mfxExtPredWeightTable;

typedef struct {
    mfxExtBuffer Header;

    mfxU16       EnableRoundingIntra;       // tri-state option
    mfxU16       RoundingOffsetIntra;       // valid value [0,7]
    mfxU16       EnableRoundingInter;       // tri-state option
    mfxU16       RoundingOffsetInter;       // valid value [0,7]

    mfxU16       reserved[24];
} mfxExtAVCRoundingOffset;


typedef struct {
    mfxExtBuffer Header;

    mfxU16  NumRect;
    mfxU16  reserved1[11];

    struct {
        mfxU32  Left;
        mfxU32  Top;
        mfxU32  Right;
        mfxU32  Bottom;

        mfxU16  reserved2[8];
    } Rect[256];
} mfxExtDirtyRect;

typedef struct {
    mfxExtBuffer Header;

    mfxU16  NumRect;
    mfxU16  reserved1[11];

    struct {
        mfxU32  DestLeft;
        mfxU32  DestTop;
        mfxU32  DestRight;
        mfxU32  DestBottom;

        mfxU32  SourceLeft;
        mfxU32  SourceTop;
        mfxU16  reserved2[4];
    } Rect[256];
} mfxExtMoveRect;


/* Angle */
enum {
    MFX_ANGLE_0     =   0,
    MFX_ANGLE_90    =  90,
    MFX_ANGLE_180   = 180,
    MFX_ANGLE_270   = 270
};

typedef struct {
    mfxExtBuffer Header;

    mfxU16 Angle;
    mfxU16 reserved[11];
} mfxExtVPPRotation;

typedef struct {
    mfxExtBuffer Header;

    mfxU16  SliceSizeOverflow;
    mfxU16  NumSliceNonCopliant;
    mfxU16  NumEncodedSlice;
    mfxU16  NumSliceSizeAlloc;
    union {
        mfxU16  *SliceSize;
        mfxU64  reserved1;
    };

    mfxU16 reserved[20];
} mfxExtEncodedSlicesInfo;

/* ScalingMode */
enum {
    MFX_SCALING_MODE_DEFAULT    = 0,
    MFX_SCALING_MODE_LOWPOWER   = 1,
    MFX_SCALING_MODE_QUALITY    = 2
};

typedef struct {
    mfxExtBuffer Header;

    mfxU16 ScalingMode;
    mfxU16 reserved[11];
} mfxExtVPPScaling;


typedef mfxExtAVCRefListCtrl mfxExtHEVCRefListCtrl;
typedef mfxExtAVCRefLists mfxExtHEVCRefLists;
typedef mfxExtAvcTemporalLayers mfxExtHEVCTemporalLayers;

/* MirroringType */
enum
{
    MFX_MIRRORING_DISABLED   = 0,
    MFX_MIRRORING_HORIZONTAL = 1,
    MFX_MIRRORING_VERTICAL   = 2
};

typedef struct {
    mfxExtBuffer Header;

    mfxU16 Type;
    mfxU16 reserved[11];
} mfxExtVPPMirroring;

typedef struct {
    mfxExtBuffer Header;

    mfxU16 StickTop;     /* tri-state option */
    mfxU16 StickBottom;  /* tri-state option */
    mfxU16 StickLeft;    /* tri-state option */
    mfxU16 StickRight;   /* tri-state option */
    mfxU16 reserved[8];
} mfxExtMVOverPicBoundaries;

typedef struct {
    mfxExtBuffer Header;

    mfxU16 Enable;        /* tri-state option */
    mfxU16 reserved[11];
} mfxExtVPPColorFill;


/* ChromaSiting */
enum {
    MFX_CHROMA_SITING_UNKNOWN             = 0x0000,
    MFX_CHROMA_SITING_VERTICAL_TOP        = 0x0001, /* Chroma samples are co-sited vertically on the top with the luma samples. */
    MFX_CHROMA_SITING_VERTICAL_CENTER     = 0x0002, /* Chroma samples are not co-sited vertically with the luma samples. */
    MFX_CHROMA_SITING_VERTICAL_BOTTOM     = 0x0004, /* Chroma samples are co-sited vertically on the bottom with the luma samples. */
    MFX_CHROMA_SITING_HORIZONTAL_LEFT     = 0x0010, /* Chroma samples are co-sited horizontally on the left with the luma samples. */
    MFX_CHROMA_SITING_HORIZONTAL_CENTER   = 0x0020  /* Chroma samples are not co-sited horizontally with the luma samples. */
};

typedef struct {
    mfxExtBuffer Header;

    mfxU16 ChromaSiting;
    mfxU16 reserved[27];
} mfxExtColorConversion;


/* VP9ReferenceFrame */
enum {
    MFX_VP9_REF_INTRA   = 0,
    MFX_VP9_REF_LAST    = 1,
    MFX_VP9_REF_GOLDEN  = 2,
    MFX_VP9_REF_ALTREF  = 3
};

/* SegmentIdBlockSize */
enum {
    MFX_VP9_SEGMENT_ID_BLOCK_SIZE_UNKNOWN =  0,
    MFX_VP9_SEGMENT_ID_BLOCK_SIZE_8x8     =  8,
    MFX_VP9_SEGMENT_ID_BLOCK_SIZE_16x16   = 16,
    MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32   = 32,
    MFX_VP9_SEGMENT_ID_BLOCK_SIZE_64x64   = 64,
};

/* SegmentFeature */
enum {
    MFX_VP9_SEGMENT_FEATURE_QINDEX      = 0x0001,
    MFX_VP9_SEGMENT_FEATURE_LOOP_FILTER = 0x0002,
    MFX_VP9_SEGMENT_FEATURE_REFERENCE   = 0x0004,
    MFX_VP9_SEGMENT_FEATURE_SKIP        = 0x0008 /* (0,0) MV, no residual */
};

typedef struct {
    mfxU16  FeatureEnabled;         /* see enum SegmentFeature */
    mfxI16  QIndexDelta;
    mfxI16  LoopFilterLevelDelta;
    mfxU16  ReferenceFrame;        /* see enum VP9ReferenceFrame */
    mfxU16  reserved[12];
} mfxVP9SegmentParam;

typedef struct {
    mfxExtBuffer        Header;
    mfxU16              NumSegments;            /* 0..8 */
    mfxVP9SegmentParam  Segment[8];
    mfxU16              SegmentIdBlockSize;     /* see enum SegmentIdBlockSize */
    mfxU32              NumSegmentIdAlloc;      /* >= (Ceil(Width / SegmentIdBlockSize) * Ceil(Height / SegmentIdBlockSize)) */
    union {
        mfxU8           *SegmentId;             /*[NumSegmentIdAlloc] = 0..7, index in Segment array, blocks of SegmentIdBlockSize map */
        mfxU64          reserved1;
    };
    mfxU16  reserved[52];
} mfxExtVP9Segmentation;

typedef struct {
    mfxU16 FrameRateScale;  /* Layer[n].FrameRateScale = Layer[n - 1].FrameRateScale * (uint)m */
    mfxU16 TargetKbps;      /* affected by BRCParamMultiplier, Layer[n].TargetKbps > Layer[n - 1].TargetKbps */
    mfxU16 reserved[14];
} mfxVP9TemporalLayer;

typedef struct {
    mfxExtBuffer        Header;
    mfxVP9TemporalLayer Layer[8];
    mfxU16              reserved[60];
} mfxExtVP9TemporalLayers;

typedef struct {
    mfxExtBuffer Header;

    mfxU16  FrameWidth;
    mfxU16  FrameHeight;

    mfxU16  WriteIVFHeaders;        /* tri-state option */

    mfxI16  reserved1[6];
    mfxI16  QIndexDeltaLumaDC;
    mfxI16  QIndexDeltaChromaAC;
    mfxI16  QIndexDeltaChromaDC;
    mfxU16  reserved[112];
} mfxExtVP9Param;


/* Multi-Frame Mode */
enum {
    MFX_MF_DEFAULT = 0,
    MFX_MF_DISABLED = 1,
    MFX_MF_AUTO = 2,
    MFX_MF_MANUAL = 3
};

/* Multi-Frame Initialization parameters */
typedef struct {
    mfxExtBuffer Header;

    mfxU16      MFMode;
    mfxU16      MaxNumFrames;

    mfxU16      reserved[58];
} mfxExtMultiFrameParam;

/* Multi-Frame Run-time controls */
typedef struct {
    mfxExtBuffer Header;

    mfxU32      Timeout;      /* timeout in millisecond */
    mfxU16      Flush;        /* Flush internal frame buffer, e.g. submit all collected frames. */

    mfxU16      reserved[57];
} mfxExtMultiFrameControl;

typedef struct {
    mfxU16 Type;
    mfxU16 reserved1;
    mfxU32 Offset;
    mfxU32 Size;
    mfxU32 reserved[5];
} mfxEncodedUnitInfo;

typedef struct {
    mfxExtBuffer Header;

    union {
        mfxEncodedUnitInfo *UnitInfo;
        mfxU64  reserved1;
    };
    mfxU16 NumUnitsAlloc;
    mfxU16 NumUnitsEncoded;

    mfxU16 reserved[22];
} mfxExtEncodedUnitsInfo;



/* MCTF initialization & runtime */
typedef struct {
    mfxExtBuffer Header;
    mfxU16       FilterStrength;
    mfxU16       reserved[27];
} mfxExtVppMctf;


#ifdef __cplusplus
} // extern "C"
#endif

#endif
