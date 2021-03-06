/******************************************************************************* *\

Copyright (C) 2010-2018 Intel Corporation.  All rights reserved.

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

File Name: mfxjpeg.h

*******************************************************************************/
#ifndef __MFX_JPEG_H__
#define __MFX_JPEG_H__

#include "mfxdefs.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* CodecId */
enum {
    MFX_CODEC_JPEG    = MFX_MAKEFOURCC('J','P','E','G')
};

/* CodecProfile, CodecLevel */
enum
{
    MFX_PROFILE_JPEG_BASELINE      = 1
};

enum
{
    MFX_ROTATION_0      = 0,
    MFX_ROTATION_90     = 1,
    MFX_ROTATION_180    = 2,
    MFX_ROTATION_270    = 3
};

enum {
    MFX_EXTBUFF_JPEG_QT      = MFX_MAKEFOURCC('J','P','G','Q'),
    MFX_EXTBUFF_JPEG_HUFFMAN = MFX_MAKEFOURCC('J','P','G','H')
};

enum {
    MFX_JPEG_COLORFORMAT_UNKNOWN = 0,
    MFX_JPEG_COLORFORMAT_YCbCr   = 1,
    MFX_JPEG_COLORFORMAT_RGB     = 2
};

enum {
    MFX_SCANTYPE_UNKNOWN        = 0,
    MFX_SCANTYPE_INTERLEAVED    = 1,
    MFX_SCANTYPE_NONINTERLEAVED = 2
};

enum {
    MFX_CHROMAFORMAT_JPEG_SAMPLING = 6
};

typedef struct {
    mfxExtBuffer    Header;

    mfxU16  reserved[7];
    mfxU16  NumTable;

    mfxU16    Qm[4][64];
} mfxExtJPEGQuantTables;

typedef struct {
    mfxExtBuffer    Header;

    mfxU16  reserved[2];
    mfxU16  NumDCTable;
    mfxU16  NumACTable;

    struct {
        mfxU8   Bits[16];
        mfxU8   Values[12];
    } DCTables[4];

    struct {
        mfxU8   Bits[16];
        mfxU8   Values[162];
    } ACTables[4];
} mfxExtJPEGHuffmanTables;

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */

#endif // __MFX_JPEG_H__
