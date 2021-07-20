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
* @file  Version.h
* @brief Version declaration
***************************************************************************************************
*/
#ifndef AMF_Version_h
#define AMF_Version_h
#pragma once

#include "Platform.h"

#define AMF_MAKE_FULL_VERSION(VERSION_MAJOR, VERSION_MINOR, VERSION_RELEASE, VERSION_BUILD_NUM)    ( ((amf_uint64)(VERSION_MAJOR) << 48ull) | ((amf_uint64)(VERSION_MINOR) << 32ull) | ((amf_uint64)(VERSION_RELEASE) << 16ull)  | (amf_uint64)(VERSION_BUILD_NUM))

#define AMF_GET_MAJOR_VERSION(x)      ((x >> 48ull) & 0xFFFF)
#define AMF_GET_MINOR_VERSION(x)      ((x >> 32ull) & 0xFFFF)
#define AMF_GET_SUBMINOR_VERSION(x)   ((x >> 16ull) & 0xFFFF)
#define AMF_GET_BUILD_VERSION(x)      ((x >>  0ull) & 0xFFFF)

#define AMF_VERSION_MAJOR       1
#define AMF_VERSION_MINOR       4
#define AMF_VERSION_RELEASE     18
#define AMF_VERSION_BUILD_NUM   0

#define AMF_FULL_VERSION AMF_MAKE_FULL_VERSION(AMF_VERSION_MAJOR, AMF_VERSION_MINOR, AMF_VERSION_RELEASE, AMF_VERSION_BUILD_NUM)

#endif //#ifndef AMF_Version_h
