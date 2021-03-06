/* ****************************************************************************** *\

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

File Name: mfxdefs.h

\* ****************************************************************************** */
#ifndef __MFXDEFS_H__
#define __MFXDEFS_H__

#define MFX_VERSION_MAJOR 1
#define MFX_VERSION_MINOR 27

#define MFX_VERSION_NEXT 1028

#ifndef MFX_VERSION
#define MFX_VERSION (MFX_VERSION_MAJOR * 1000 + MFX_VERSION_MINOR)
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#if (defined( _WIN32 ) || defined ( _WIN64 )) && !defined (__GNUC__)
  #define __INT64   __int64
  #define __UINT64  unsigned __int64
#else
  #define __INT64   long long
  #define __UINT64  unsigned long long
#endif

#ifdef _WIN32
    #define MFX_CDECL __cdecl
    #define MFX_STDCALL __stdcall
#else
    #define MFX_CDECL
    #define MFX_STDCALL
#endif /* _WIN32 */

#define MFX_INFINITE 0xFFFFFFFF

typedef unsigned char       mfxU8;
typedef char                mfxI8;
typedef short               mfxI16;
typedef unsigned short      mfxU16;
typedef unsigned int        mfxU32;
typedef int                 mfxI32;
#if defined( _WIN32 ) || defined ( _WIN64 )
typedef unsigned long       mfxUL32;
typedef long                mfxL32;
#else
typedef unsigned int        mfxUL32;
typedef int                 mfxL32;
#endif
typedef float               mfxF32;
typedef double              mfxF64;
typedef __UINT64            mfxU64;
typedef __INT64             mfxI64;
typedef void*               mfxHDL;
typedef mfxHDL              mfxMemId;
typedef void*               mfxThreadTask;
typedef char                mfxChar;

typedef struct {
    mfxI16  x;
    mfxI16  y;
} mfxI16Pair;

typedef struct {
    mfxHDL first;
    mfxHDL second;
} mfxHDLPair;


/*********************************************************************************\
Error message
\*********************************************************************************/
typedef enum
{
    /* no error */
    MFX_ERR_NONE                        = 0,    /* no error */

    /* reserved for unexpected errors */
    MFX_ERR_UNKNOWN                     = -1,   /* unknown error. */

    /* error codes <0 */
    MFX_ERR_NULL_PTR                    = -2,   /* null pointer */
    MFX_ERR_UNSUPPORTED                 = -3,   /* undeveloped feature */
    MFX_ERR_MEMORY_ALLOC                = -4,   /* failed to allocate memory */
    MFX_ERR_NOT_ENOUGH_BUFFER           = -5,   /* insufficient buffer at input/output */
    MFX_ERR_INVALID_HANDLE              = -6,   /* invalid handle */
    MFX_ERR_LOCK_MEMORY                 = -7,   /* failed to lock the memory block */
    MFX_ERR_NOT_INITIALIZED             = -8,   /* member function called before initialization */
    MFX_ERR_NOT_FOUND                   = -9,   /* the specified object is not found */
    MFX_ERR_MORE_DATA                   = -10,  /* expect more data at input */
    MFX_ERR_MORE_SURFACE                = -11,  /* expect more surface at output */
    MFX_ERR_ABORTED                     = -12,  /* operation aborted */
    MFX_ERR_DEVICE_LOST                 = -13,  /* lose the HW acceleration device */
    MFX_ERR_INCOMPATIBLE_VIDEO_PARAM    = -14,  /* incompatible video parameters */
    MFX_ERR_INVALID_VIDEO_PARAM         = -15,  /* invalid video parameters */
    MFX_ERR_UNDEFINED_BEHAVIOR          = -16,  /* undefined behavior */
    MFX_ERR_DEVICE_FAILED               = -17,  /* device operation failure */
    MFX_ERR_MORE_BITSTREAM              = -18,  /* expect more bitstream buffers at output */
    MFX_ERR_INCOMPATIBLE_AUDIO_PARAM    = -19,  /* incompatible audio parameters */
    MFX_ERR_INVALID_AUDIO_PARAM         = -20,  /* invalid audio parameters */
    MFX_ERR_GPU_HANG                    = -21,  /* device operation failure caused by GPU hang */
    MFX_ERR_REALLOC_SURFACE             = -22,  /* bigger output surface required */

    /* warnings >0 */
    MFX_WRN_IN_EXECUTION                = 1,    /* the previous asynchronous operation is in execution */
    MFX_WRN_DEVICE_BUSY                 = 2,    /* the HW acceleration device is busy */
    MFX_WRN_VIDEO_PARAM_CHANGED         = 3,    /* the video parameters are changed during decoding */
    MFX_WRN_PARTIAL_ACCELERATION        = 4,    /* SW is used */
    MFX_WRN_INCOMPATIBLE_VIDEO_PARAM    = 5,    /* incompatible video parameters */
    MFX_WRN_VALUE_NOT_CHANGED           = 6,    /* the value is saturated based on its valid range */
    MFX_WRN_OUT_OF_RANGE                = 7,    /* the value is out of valid range */
    MFX_WRN_FILTER_SKIPPED              = 10,   /* one of requested filters has been skipped */
    MFX_WRN_INCOMPATIBLE_AUDIO_PARAM    = 11,   /* incompatible audio parameters */

    /* threading statuses */
    MFX_TASK_DONE = MFX_ERR_NONE,               /* task has been completed */
    MFX_TASK_WORKING                    = 8,    /* there is some more work to do */
    MFX_TASK_BUSY                       = 9,    /* task is waiting for resources */

    /* plug-in statuses */
    MFX_ERR_MORE_DATA_SUBMIT_TASK       = -10000, /* return MFX_ERR_MORE_DATA but submit internal asynchronous task */

} mfxStatus;


// Application 
#if defined(MFX_DISPATCHER_EXPOSED_PREFIX)

#include "mfxdispatcherprefixedfunctions.h"

#endif // MFX_DISPATCHER_EXPOSED_PREFIX


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MFXDEFS_H__ */
