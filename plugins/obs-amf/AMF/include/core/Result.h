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

#ifndef AMF_Result_h
#define AMF_Result_h
#pragma once

#include "Platform.h"

//----------------------------------------------------------------------------------------------
// result codes
//----------------------------------------------------------------------------------------------

typedef enum AMF_RESULT
{
    AMF_OK                                   = 0,
    AMF_FAIL                                    ,

// common errors
    AMF_UNEXPECTED                              ,

    AMF_ACCESS_DENIED                           ,
    AMF_INVALID_ARG                             ,
    AMF_OUT_OF_RANGE                            ,

    AMF_OUT_OF_MEMORY                           ,
    AMF_INVALID_POINTER                         ,

    AMF_NO_INTERFACE                            ,
    AMF_NOT_IMPLEMENTED                         ,
    AMF_NOT_SUPPORTED                           ,
    AMF_NOT_FOUND                               ,

    AMF_ALREADY_INITIALIZED                     ,
    AMF_NOT_INITIALIZED                         ,

    AMF_INVALID_FORMAT                          ,// invalid data format

    AMF_WRONG_STATE                             ,
    AMF_FILE_NOT_OPEN                           ,// cannot open file

// device common codes
    AMF_NO_DEVICE                               ,

// device directx
    AMF_DIRECTX_FAILED                          ,
// device opencl 
    AMF_OPENCL_FAILED                           ,
// device opengl 
    AMF_GLX_FAILED                              ,//failed to use GLX
// device XV 
    AMF_XV_FAILED                               , //failed to use Xv extension
// device alsa
    AMF_ALSA_FAILED                             ,//failed to use ALSA

// component common codes

    //result codes
    AMF_EOF                                     ,
    AMF_REPEAT                                  ,
    AMF_INPUT_FULL                              ,//returned by AMFComponent::SubmitInput if input queue is full
    AMF_RESOLUTION_CHANGED                      ,//resolution changed client needs to Drain/Terminate/Init
    AMF_RESOLUTION_UPDATED                      ,//resolution changed in adaptive mode. New ROI will be set on output on newly decoded frames

    //error codes
    AMF_INVALID_DATA_TYPE                       ,//invalid data type
    AMF_INVALID_RESOLUTION                      ,//invalid resolution (width or height)
    AMF_CODEC_NOT_SUPPORTED                     ,//codec not supported
    AMF_SURFACE_FORMAT_NOT_SUPPORTED            ,//surface format not supported
    AMF_SURFACE_MUST_BE_SHARED                  ,//surface should be shared (DX11: (MiscFlags & D3D11_RESOURCE_MISC_SHARED) == 0, DX9: No shared handle found)

// component video decoder
    AMF_DECODER_NOT_PRESENT                     ,//failed to create the decoder
    AMF_DECODER_SURFACE_ALLOCATION_FAILED       ,//failed to create the surface for decoding
    AMF_DECODER_NO_FREE_SURFACES                ,

// component video encoder
    AMF_ENCODER_NOT_PRESENT                     ,//failed to create the encoder

// component video processor

// component video conveter

// component dem
    AMF_DEM_ERROR                               ,
    AMF_DEM_PROPERTY_READONLY                   ,
    AMF_DEM_REMOTE_DISPLAY_CREATE_FAILED        ,
    AMF_DEM_START_ENCODING_FAILED               ,
    AMF_DEM_QUERY_OUTPUT_FAILED                 ,

// component TAN
    AMF_TAN_CLIPPING_WAS_REQUIRED               , // Resulting data was truncated to meet output type's value limits.
    AMF_TAN_UNSUPPORTED_VERSION                 , // Not supported version requested, solely for TANCreateContext().

    AMF_NEED_MORE_INPUT                         ,//returned by AMFComponent::SubmitInput did not produce buffer
} AMF_RESULT;

#endif //#ifndef AMF_Result_h
