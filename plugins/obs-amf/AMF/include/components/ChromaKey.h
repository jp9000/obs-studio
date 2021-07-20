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
/**
 ***************************************************************************************************
 * @file  ChromaKey.h
 * @brief AMFChromaKey interface declaration
 ***************************************************************************************************
 */
#ifndef __AMFChromaKey_h__
#define __AMFChromaKey_h__
#pragma once

#include "public/include/components/Component.h"

#define AMFChromaKey                  L"AMFChromaKey"

// static properties 
#define AMF_CHROMAKEY_COLOR           L"ChromaKeyColor"         // amf_uint64 (default=0x992A1E), YUV Green key Color
#define AMF_CHROMAKEY_COLOR_EX        L"ChromaKeyColorEX"       // amf_uint64 (default=0), YUV Green key Color, secondary
#define AMF_CHROMAKEY_RANGE_MIN       L"ChromaKeyRangeMin"      // amf_uint64 (default=20)   color tolerance low, 0~255
#define AMF_CHROMAKEY_RANGE_MAX       L"ChromaKeyRangeMax"      // amf_uint64 (default=22)   color tolerance high, 0~255
#define AMF_CHROMAKEY_RANGE_EXT       L"ChromaKeyRangeExt"      // amf_uint64 (default=40)   color tolerance extended, 0~255
#define AMF_CHROMAKEY_SPILL_MODE      L"ChromaKeySpillMode"     // amf_uint64 (default=0)    spill suppression mode
#define AMF_CHROMAKEY_RANGE_SPILL     L"ChromaKeyRangeSpill"    // amf_uint64 (default=5)    spill suppression threshold
#define AMF_CHROMAKEY_LUMA_LOW        L"ChromaKeyLumaLow"       // amf_uint64 (default=16)   minimum luma value for processing
#define AMF_CHROMAKEY_INPUT_COUNT     L"InputCount"             // amf_uint64 (default=2)    number of inputs
#define AMF_CHROMAKEY_COLOR_POS       L"KeyColorPos"            // amf_uint64 (default=0)    key color position from the surface
#define AMF_CHROMAKEY_OUT_FORMAT      L"ChromaKeyOutFormat"     // amf_uint64 (default=RGBA) output format
#define AMF_CHROMAKEY_MEMORY_TYPE     L"ChromaKeyMemoryType"    // amf_uint64 (default=DX11) mmeory type
#define AMF_CHROMAKEY_COLOR_ADJ       L"ChromaKeyColorAdj"      // amf_uint64 (default=0)    endble color adjustment
#define AMF_CHROMAKEY_COLOR_ADJ_THRE  L"ChromaKeyColorAdjThre"  // amf_uint64 (default=0)    color adjustment threshold
#define AMF_CHROMAKEY_COLOR_ADJ_THRE2 L"ChromaKeyColorAdjThre2" // amf_uint64 (default=0)    color adjustment threshold
#define AMF_CHROMAKEY_BYPASS          L"ChromaKeyBypass"        // amf_uint64 (default=0)    disable chromakey
#define AMF_CHROMAKEY_EDGE            L"ChromaKeyEdge"          // amf_uint64 (default=0)    endble edge detection
#define AMF_CHROMAKEY_BOKEH           L"ChromaKeyBokeh"         // amf_uint64 (default=0)    endble background bokeh
#define AMF_CHROMAKEY_BOKEH_RADIUS    L"ChromaKeyBokehRadius"   // amf_uint64 (default=7)    background bokeh radius
#define AMF_CHROMAKEY_DEBUG           L"ChromaKeyDebug"         // amf_uint64 (default=0)    endble debug mode

#define AMF_CHROMAKEY_POSX            L"ChromaKeyPosX"          // amf_uint64 (default=0)    positionX
#define AMF_CHROMAKEY_POSY            L"ChromaKeyPosY"          // amf_uint64 (default=0)    positionY

extern "C"
{
    AMF_RESULT AMF_CDECL_CALL AMFCreateComponentChromaKey(amf::AMFContext* pContext, amf::AMFComponentEx** ppComponent);
}
#endif //#ifndef __AMFChromaKey_h__
