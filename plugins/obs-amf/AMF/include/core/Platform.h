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

#ifndef AMF_Platform_h
#define AMF_Platform_h
#pragma once

//----------------------------------------------------------------------------------------------
// export declaration
//----------------------------------------------------------------------------------------------
#if defined(_WIN32)
    #if defined(AMF_CORE_STATIC)
        #define AMF_CORE_LINK
    #else
        #if defined(AMF_CORE_EXPORTS)
            #define AMF_CORE_LINK __declspec(dllexport)
        #else
            #define AMF_CORE_LINK __declspec(dllimport)
        #endif
    #endif
#elif defined(__linux)        
        #if defined(AMF_CORE_EXPORTS)
            #define AMF_CORE_LINK __attribute__((visibility("default")))
        #else
            #define AMF_CORE_LINK
        #endif
#else 
    #define AMF_CORE_LINK
#endif // #ifdef _WIN32

#define AMF_MACRO_STRING2(x) #x
#define AMF_MACRO_STRING(x) AMF_MACRO_STRING2(x)

#define AMF_TODO(_todo) (__FILE__ "(" AMF_MACRO_STRING(__LINE__) "): TODO: "_todo)


 #if defined(__GNUC__) || defined(__clang__)
     #define AMF_ALIGN(n) __attribute__((aligned(n)))
 #elif defined(_MSC_VER) || defined(__INTEL_COMPILER)
     #define AMF_ALIGN(n) __declspec(align(n))
 #else
    #define AMF_ALIGN(n)
//     #error Need to define AMF_ALIGN
 #endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#if defined(_WIN32)


#ifndef NOMINMAX
#define NOMINMAX
#endif
    #define AMF_STD_CALL            __stdcall
    #define AMF_CDECL_CALL          __cdecl
    #define AMF_FAST_CALL           __fastcall
#if defined(__GNUC__) || defined(__clang__)
    #define AMF_INLINE              inline
    #define AMF_FORCEINLINE         inline
#else
    #define AMF_INLINE              __inline
    #define AMF_FORCEINLINE         __forceinline
#endif
    #define AMF_NO_VTABLE           __declspec(novtable)

    #define AMFPRId64   "I64d"
    #define LPRId64    L"I64d"

    #define AMFPRIud64   "Iu64d"
    #define LPRIud64    L"Iu64d"

    #define AMFPRIx64   "I64x"
    #define LPRIx64    L"I64x"

#else // !WIN32 - Linux and Mac

    #define AMF_STD_CALL
    #define AMF_CDECL_CALL
    #define AMF_FAST_CALL
#if defined(__GNUC__) || defined(__clang__)
    #define AMF_INLINE              inline
    #define AMF_FORCEINLINE         inline
#else
    #define AMF_INLINE              __inline__
    #define AMF_FORCEINLINE         __inline__
#endif
    #define AMF_NO_VTABLE           

    #if !defined(AMFPRId64)
        #define AMFPRId64    "lld"
        #define LPRId64     L"lld"

        #define AMFPRIud64    "ulld"
        #define LPRIud64     L"ulld"

        #define AMFPRIx64    "llx"
        #define LPRIx64     L"llx"
    #endif

#endif // WIN32


#if defined(_WIN32)
#define AMF_WEAK __declspec( selectany ) 
#elif defined (__GNUC__) || defined (__GCC__) || defined(__clang__)//GCC or CLANG
#define AMF_WEAK __attribute__((weak))
#endif

#define amf_countof(x) (sizeof(x) / sizeof(x[0]))

//-------------------------------------------------------------------------------------------------
// basic data types
//-------------------------------------------------------------------------------------------------
typedef     int64_t             amf_int64;
typedef     int32_t             amf_int32;
typedef     int16_t             amf_int16;
typedef     int8_t              amf_int8;

typedef     uint64_t            amf_uint64;
typedef     uint32_t            amf_uint32;
typedef     uint16_t            amf_uint16;
typedef     uint8_t             amf_uint8;
typedef     size_t              amf_size;

typedef     void*               amf_handle;
typedef     double              amf_double;
typedef     float               amf_float;

typedef     void                amf_void;

#if defined(__cplusplus)
typedef     bool                amf_bool;
#else
typedef     amf_uint8           amf_bool;
#define     true                1 
#define     false               0 
#endif

typedef     long                amf_long; 
typedef     int                 amf_int; 
typedef     unsigned long       amf_ulong; 
typedef     unsigned int        amf_uint; 

typedef     amf_int64           amf_pts;     // in 100 nanosecs

typedef amf_uint32              amf_flags;

#define AMF_SECOND          10000000L    // 1 second in 100 nanoseconds
#define AMF_MILLISECOND		(AMF_SECOND / 1000)

#define AMF_MIN(a, b) ((a) < (b) ? (a) : (b))
#define AMF_MAX(a, b) ((a) > (b) ? (a) : (b))

#define AMF_BITS_PER_BYTE 8

#if defined(_WIN32)
    #define PATH_SEPARATOR_WSTR         L"\\"
    #define PATH_SEPARATOR_WCHAR        L'\\'
#elif defined(__linux) || defined(__APPLE__) // Linux & Apple
    #define PATH_SEPARATOR_WSTR          L"/"
    #define PATH_SEPARATOR_WCHAR         L'/'
#endif

typedef struct AMFRect
{
    amf_int32 left;
    amf_int32 top;
    amf_int32 right;
    amf_int32 bottom;
#if defined(__cplusplus)
    bool operator==(const AMFRect& other) const
    {
         return left == other.left && top == other.top && right == other.right && bottom == other.bottom; 
    }
    AMF_INLINE bool operator!=(const AMFRect& other) const { return !operator==(other); }
    amf_int32 Width() const { return right - left; }
    amf_int32 Height() const { return bottom - top; }
#endif
} AMFRect;

static AMF_INLINE struct AMFRect AMFConstructRect(amf_int32 left, amf_int32 top, amf_int32 right, amf_int32 bottom)
{
    struct AMFRect object = {left, top, right, bottom};
    return object;
}

typedef struct AMFSize
{
    amf_int32 width;
    amf_int32 height;
#if defined(__cplusplus)
    bool operator==(const AMFSize& other) const
    {
         return width == other.width && height == other.height; 
    }
    AMF_INLINE bool operator!=(const AMFSize& other) const { return !operator==(other); }
#endif
} AMFSize;

static AMF_INLINE struct AMFSize AMFConstructSize(amf_int32 width, amf_int32 height)
{
    struct AMFSize object = {width, height};
    return object;
}

typedef struct AMFPoint
{
    amf_int32 x;
    amf_int32 y;
#if defined(__cplusplus)
    bool operator==(const AMFPoint& other) const
    {
         return x == other.x && y == other.y; 
    }
    AMF_INLINE bool operator!=(const AMFPoint& other) const { return !operator==(other); }
#endif
} AMFPoint;

static AMF_INLINE struct AMFPoint AMFConstructPoint(amf_int32 x, amf_int32 y)
{
    struct AMFPoint object = { x, y };
    return object;
}

typedef struct AMFFloatPoint2D
{
    amf_float x;
    amf_float y;
#if defined(__cplusplus)
    bool operator==(const AMFFloatPoint2D& other) const
    {
        return x == other.x && y == other.y;
    }
    AMF_INLINE bool operator!=(const AMFFloatPoint2D& other) const { return !operator==(other); }
#endif
} AMFFloatPoint2D;

static AMF_INLINE struct AMFFloatPoint2D AMFConstructFloatPoint2D(amf_float x, amf_float y)
{
    struct AMFFloatPoint2D object = {x, y};
    return object;
}
typedef struct AMFFloatSize
{
    amf_float width;
    amf_float height;
#if defined(__cplusplus)
    bool operator==(const AMFFloatSize& other) const
    {
        return width == other.width && height == other.height;
    }
    AMF_INLINE bool operator!=(const AMFFloatSize& other) const { return !operator==(other); }
#endif
} AMFFloatSize;

static AMF_INLINE struct AMFFloatSize AMFConstructFloatSize(amf_float w, amf_float h)
{
    struct AMFFloatSize object = { w, h };
    return object;
}


typedef struct AMFFloatPoint3D
{
    amf_float x;
    amf_float y;
    amf_float z;
#if defined(__cplusplus)
    bool operator==(const AMFFloatPoint3D& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
    AMF_INLINE bool operator!=(const AMFFloatPoint3D& other) const { return !operator==(other); }
#endif
} AMFFloatPoint3D;

static AMF_INLINE struct AMFFloatPoint3D AMFConstructFloatPoint3D(amf_float x, amf_float y, amf_float z)
{
    struct AMFFloatPoint3D object = { x, y, z };
    return object;
}

typedef struct AMFFloatVector4D
{
    amf_float x;
    amf_float y;
    amf_float z;
    amf_float w;
#if defined(__cplusplus)
    bool operator==(const AMFFloatVector4D& other) const
    {
        return x == other.x && y == other.y && z == other.z && w == other.w;
    }
    AMF_INLINE bool operator!=(const AMFFloatVector4D& other) const { return !operator==(other); }
#endif
} AMFFloatVector4D;

static AMF_INLINE struct AMFFloatVector4D AMFConstructFloatVector4D(amf_float x, amf_float y, amf_float z, amf_float w)
{
    struct AMFFloatVector4D object = { x, y, z, w };
    return object;
}


typedef struct AMFRate
{
    amf_uint32 num;
    amf_uint32 den;
#if defined(__cplusplus)
    bool operator==(const AMFRate& other) const
    {
         return num == other.num && den == other.den; 
    }
    AMF_INLINE bool operator!=(const AMFRate& other) const { return !operator==(other); }
#endif
} AMFRate;

static AMF_INLINE struct AMFRate AMFConstructRate(amf_uint32 num, amf_uint32 den)
{
    struct AMFRate object = {num, den};
    return object;
}

typedef struct AMFRatio
{
    amf_uint32 num;
    amf_uint32 den;
#if defined(__cplusplus)
    bool operator==(const AMFRatio& other) const
    {
         return num == other.num && den == other.den; 
    }
    AMF_INLINE bool operator!=(const AMFRatio& other) const { return !operator==(other); }
#endif
} AMFRatio;

static AMF_INLINE struct AMFRatio AMFConstructRatio(amf_uint32 num, amf_uint32 den)
{
    struct AMFRatio object = {num, den};
    return object;
}

#pragma pack(push, 1)
#if defined(_MSC_VER)
    #pragma warning( push )
#endif
#if defined(WIN32)
#if defined(_MSC_VER)
    #pragma warning(disable : 4200)
    #pragma warning(disable : 4201)
#endif
#endif
typedef struct AMFColor
{
    union
    {
        struct
        {
            amf_uint8 r;
            amf_uint8 g;
            amf_uint8 b;
            amf_uint8 a;
        };
        amf_uint32 rgba;
    };
#if defined(__cplusplus)
    bool operator==(const AMFColor& other) const
    {
         return r == other.r && g == other.g && b == other.b && a == other.a; 
    }
    AMF_INLINE bool operator!=(const AMFColor& other) const { return !operator==(other); }
#endif
} AMFColor;
#if defined(_MSC_VER)
    #pragma warning( pop )
#endif
#pragma pack(pop)


static AMF_INLINE struct AMFColor AMFConstructColor(amf_uint8 r, amf_uint8 g, amf_uint8 b, amf_uint8 a)
{
    struct AMFColor object;
    object.r = r;
    object.g = g;
    object.b = b;
    object.a = a;
    return object;
}

#if defined(_WIN32)
    #include <combaseapi.h>

    #if defined(__cplusplus)
    extern "C"
    {
    #endif
        // allocator
        static AMF_INLINE void* AMF_CDECL_CALL amf_variant_alloc(amf_size count)
        {
            return CoTaskMemAlloc(count);
        }
        static AMF_INLINE void AMF_CDECL_CALL amf_variant_free(void* ptr)
        {
            CoTaskMemFree(ptr);
        }
    #if defined(__cplusplus)
    }
    #endif

#else // defined(_WIN32)
    #include <stdlib.h>
    #if defined(__cplusplus)
    extern "C"
    {
    #endif
        // allocator
        static AMF_INLINE void* AMF_CDECL_CALL amf_variant_alloc(amf_size count)
        {
            return malloc(count);
        }
        static AMF_INLINE void AMF_CDECL_CALL amf_variant_free(void* ptr)
        {
            free(ptr);
        }
    #if defined(__cplusplus)
    }
    #endif
#endif // defined(_WIN32)


#if defined(__cplusplus)
namespace amf
{
#endif
    typedef struct AMFGuid
    {
        amf_uint32 data1;
        amf_uint16 data2;
        amf_uint16 data3;
        amf_uint8 data41;
        amf_uint8 data42;
        amf_uint8 data43;
        amf_uint8 data44;
        amf_uint8 data45;
        amf_uint8 data46;
        amf_uint8 data47;
        amf_uint8 data48;
#if defined(__cplusplus)
        AMFGuid(amf_uint32 _data1, amf_uint16 _data2, amf_uint16 _data3,
                amf_uint8 _data41, amf_uint8 _data42, amf_uint8 _data43, amf_uint8 _data44,
                amf_uint8 _data45, amf_uint8 _data46, amf_uint8 _data47, amf_uint8 _data48)
            : data1 (_data1),
            data2 (_data2),
            data3 (_data3),
            data41(_data41),
            data42(_data42),
            data43(_data43),
            data44(_data44),
            data45(_data45),
            data46(_data46),
            data47(_data47),
            data48(_data48)
        {}

        bool operator==(const AMFGuid& other) const
        {
            return
                data1 == other.data1 &&
                data2 == other.data2 &&
                data3 == other.data3 &&
                data41 == other.data41 &&
                data42 == other.data42 &&
                data43 == other.data43 &&
                data44 == other.data44 &&
                data45 == other.data45 &&
                data46 == other.data46 &&
                data47 == other.data47 &&
                data48 == other.data48;
        }
        AMF_INLINE bool operator!=(const AMFGuid& other) const { return !operator==(other); }
#endif
    } AMFGuid;

#if defined(__cplusplus)
    static AMF_INLINE bool AMFCompareGUIDs(const AMFGuid& guid1, const AMFGuid& guid2)
    {
        return guid1 == guid2;
    }
#else
    static AMF_INLINE amf_bool AMFCompareGUIDs(const struct AMFGuid guid1, const struct AMFGuid guid2)
    {
        return memcmp(&guid1, &guid2, sizeof(guid1)) == 0;
    }
#endif
#if defined(__cplusplus)
}
#endif

#if defined(__APPLE__)
//#include <MacTypes.h>

#define media_status_t int
#define ANativeWindow void
#define JNIEnv void
#define jobject int
#define JavaVM void

#endif

#endif //#ifndef AMF_Platform_h
