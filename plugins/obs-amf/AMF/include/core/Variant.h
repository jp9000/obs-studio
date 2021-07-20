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

#ifndef AMF_Variant_h
#define AMF_Variant_h
#pragma once
#if defined(_MSC_VER)
    #pragma warning(disable: 4996)
#endif

#include "Interface.h"
#include <locale.h>
#include <wchar.h>
#include <string.h>

#if defined(__cplusplus)
namespace amf
{
#endif
    //----------------------------------------------------------------------------------------------
    // variant types
    //----------------------------------------------------------------------------------------------
    typedef enum AMF_VARIANT_TYPE
    {
        AMF_VARIANT_EMPTY           = 0,

        AMF_VARIANT_BOOL            = 1,
        AMF_VARIANT_INT64           = 2,
        AMF_VARIANT_DOUBLE          = 3,

        AMF_VARIANT_RECT            = 4,
        AMF_VARIANT_SIZE            = 5,
        AMF_VARIANT_POINT           = 6,
        AMF_VARIANT_RATE            = 7,
        AMF_VARIANT_RATIO           = 8,
        AMF_VARIANT_COLOR           = 9,

        AMF_VARIANT_STRING          = 10,  // value is char*
        AMF_VARIANT_WSTRING         = 11,  // value is wchar_t*
        AMF_VARIANT_INTERFACE       = 12,  // value is AMFInterface*
        AMF_VARIANT_FLOAT           = 13,

        AMF_VARIANT_FLOAT_SIZE      = 14,
        AMF_VARIANT_FLOAT_POINT2D   = 15,
        AMF_VARIANT_FLOAT_POINT3D   = 16,
        AMF_VARIANT_FLOAT_VECTOR4D  = 17
    } AMF_VARIANT_TYPE;
    //----------------------------------------------------------------------------------------------
    // variant struct
   //----------------------------------------------------------------------------------------------
    typedef struct AMFVariantStruct
    {
        AMF_VARIANT_TYPE            type;
        union
        {
            amf_bool                boolValue;
            amf_int64               int64Value;
            amf_double              doubleValue;
            char*                   stringValue;
            wchar_t*                wstringValue;
            AMFInterface*           pInterface;
            struct AMFRect          rectValue;
            struct AMFSize          sizeValue;
            struct AMFPoint         pointValue;
            struct AMFRate          rateValue;
            struct AMFRatio         ratioValue;
            struct AMFColor         colorValue;
            amf_float               floatValue;
            struct AMFFloatSize     floatSizeValue;
            struct AMFFloatPoint2D  floatPoint2DValue;
            struct AMFFloatPoint3D  floatPoint3DValue;
            struct AMFFloatVector4D floatVector4DValue;
        };
    } AMFVariantStruct;
    //----------------------------------------------------------------------------------------------
    // variant accessors
    //----------------------------------------------------------------------------------------------

    static AMF_INLINE AMF_VARIANT_TYPE     AMF_STD_CALL AMFVariantGetType(const AMFVariantStruct* _variant) { return (_variant)->type; }
#if defined(__cplusplus)
    static AMF_INLINE AMF_VARIANT_TYPE&    AMF_STD_CALL AMFVariantGetType(AMFVariantStruct* _variant) { return (_variant)->type; }
#endif
    static AMF_INLINE amf_bool             AMF_STD_CALL AMFVariantGetBool(const AMFVariantStruct* _variant) { return (_variant)->boolValue; }
    static AMF_INLINE amf_int64            AMF_STD_CALL AMFVariantGetInt64(const AMFVariantStruct* _variant) { return (_variant)->int64Value; }
    static AMF_INLINE amf_double           AMF_STD_CALL AMFVariantGetDouble(const AMFVariantStruct* _variant) { return (_variant)->doubleValue; }
    static AMF_INLINE amf_float            AMF_STD_CALL AMFVariantGetFloat(const AMFVariantStruct* _variant) { return (_variant)->floatValue; }
    static AMF_INLINE const char*          AMF_STD_CALL AMFVariantGetString(const AMFVariantStruct* _variant) { return (_variant)->stringValue; }
    static AMF_INLINE const wchar_t*       AMF_STD_CALL AMFVariantGetWString(const AMFVariantStruct* _variant) { return (_variant)->wstringValue; }
#if defined(__cplusplus)
    static AMF_INLINE const AMFInterface*  AMF_STD_CALL AMFVariantGetInterface(const AMFVariantStruct* _variant) { return (_variant)->pInterface; }
#endif
    static AMF_INLINE AMFInterface*        AMF_STD_CALL AMFVariantGetInterface(AMFVariantStruct* _variant) { return (_variant)->pInterface; }

#if defined(__cplusplus)
    static AMF_INLINE const AMFRect &      AMF_STD_CALL AMFVariantGetRect (const AMFVariantStruct* _variant) { return (_variant)->rectValue; }
    static AMF_INLINE const AMFSize &      AMF_STD_CALL AMFVariantGetSize (const AMFVariantStruct* _variant) { return (_variant)->sizeValue; }
    static AMF_INLINE const AMFPoint&      AMF_STD_CALL AMFVariantGetPoint(const AMFVariantStruct* _variant) { return (_variant)->pointValue; }
    static AMF_INLINE const AMFFloatSize& AMF_STD_CALL AMFVariantGetFloatSize(const AMFVariantStruct* _variant) { return (_variant)->floatSizeValue; }
    static AMF_INLINE const AMFFloatPoint2D& AMF_STD_CALL AMFVariantGetFloatPoint2D(const AMFVariantStruct* _variant) { return (_variant)->floatPoint2DValue; }
    static AMF_INLINE const AMFFloatPoint3D& AMF_STD_CALL AMFVariantGetFloatPoint3D(const AMFVariantStruct* _variant) { return (_variant)->floatPoint3DValue; }
    static AMF_INLINE const AMFFloatVector4D& AMF_STD_CALL AMFVariantGetFloatVector4D(const AMFVariantStruct* _variant) { return (_variant)->floatVector4DValue; }
    static AMF_INLINE const AMFRate &      AMF_STD_CALL AMFVariantGetRate (const AMFVariantStruct* _variant) { return (_variant)->rateValue; }
    static AMF_INLINE const AMFRatio&      AMF_STD_CALL AMFVariantGetRatio(const AMFVariantStruct* _variant) { return (_variant)->ratioValue; }
    static AMF_INLINE const AMFColor&      AMF_STD_CALL AMFVariantGetColor(const AMFVariantStruct* _variant) { return (_variant)->colorValue; }
#else // #if defined(__cplusplus)
    static AMF_INLINE const AMFRect        AMF_STD_CALL AMFVariantGetRect (const AMFVariantStruct* _variant) { return (_variant)->rectValue; }
    static AMF_INLINE const AMFSize        AMF_STD_CALL AMFVariantGetSize (const AMFVariantStruct* _variant) { return (_variant)->sizeValue; }
    static AMF_INLINE const AMFPoint       AMF_STD_CALL AMFVariantGetPoint(const AMFVariantStruct* _variant) { return (_variant)->pointValue; }
    static AMF_INLINE const AMFFloatSize  AMF_STD_CALL AMFVariantGetFloatSize(const AMFVariantStruct* _variant) { return (_variant)->floatSizeValue; }
    static AMF_INLINE const AMFFloatPoint2D  AMF_STD_CALL AMFVariantGetFloatPoint2D(const AMFVariantStruct* _variant) { return (_variant)->floatPoint2DValue; }
    static AMF_INLINE const AMFFloatPoint3D  AMF_STD_CALL AMFVariantGetFloatPoint3D(const AMFVariantStruct* _variant) { return (_variant)->floatPoint3DValue; }
    static AMF_INLINE const AMFFloatVector4D  AMF_STD_CALL AMFVariantGetFloatVector4D(const AMFVariantStruct* _variant) { return (_variant)->floatVector4DValue; }
    static AMF_INLINE const AMFRate        AMF_STD_CALL AMFVariantGetRate (const AMFVariantStruct* _variant) { return (_variant)->rateValue; }
    static AMF_INLINE const AMFRatio       AMF_STD_CALL AMFVariantGetRatio(const AMFVariantStruct* _variant) { return (_variant)->ratioValue; }
    static AMF_INLINE const AMFColor       AMF_STD_CALL AMFVariantGetColor(const AMFVariantStruct* _variant) { return (_variant)->colorValue; }
#endif // #if defined(__cplusplus)


    #define AMFVariantEmpty(_variant)     0
    #define AMFVariantBool(_variant)      (_variant)->boolValue
    #define AMFVariantInt64(_variant)     (_variant)->int64Value
    #define AMFVariantDouble(_variant)    (_variant)->doubleValue
    #define AMFVariantFloat(_variant)    (_variant)->floatValue

    #define AMFVariantRect(_variant)      (_variant)->rectValue
    #define AMFVariantSize(_variant)      (_variant)->sizeValue
    #define AMFVariantPoint(_variant)     (_variant)->pointValue
    #define AMFVariantFloatSize(_variant)     (_variant)->floatSizeValue
    #define AMFVariantFloatPoint2D(_variant)     (_variant)->floatPoint2DValue
    #define AMFVariantFloatPoint3D(_variant)     (_variant)->floatPoint3DValue
    #define AMFVariantFloatVector4D(_variant)     (_variant)->floatVector4DValue
    #define AMFVariantRate(_variant)      (_variant)->rateValue
    #define AMFVariantRatio(_variant)     (_variant)->ratioValue
    #define AMFVariantColor(_variant)     (_variant)->colorValue

    #define AMFVariantString(_variant)    (_variant)->stringValue
    #define AMFVariantWString(_variant)   (_variant)->wstringValue
    #define AMFVariantInterface(_variant) (_variant)->pInterface
    //----------------------------------------------------------------------------------------------
    // variant hleper functions
    //----------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantInit(AMFVariantStruct* pVariant);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantClear(AMFVariantStruct* pVariant);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantCompare(const AMFVariantStruct* pFirst, const AMFVariantStruct* pSecond, amf_bool* equal);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantCopy(AMFVariantStruct* pDest, const AMFVariantStruct* pSrc);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignBool(AMFVariantStruct* pDest, amf_bool value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignInt64(AMFVariantStruct* pDest, amf_int64 value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignDouble(AMFVariantStruct* pDest, amf_double value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignFloat(AMFVariantStruct* pDest, amf_float value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignString(AMFVariantStruct* pDest, const char* value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignWString(AMFVariantStruct* pDest, const wchar_t* value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignInterface(AMFVariantStruct* pDest, AMFInterface* value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignRect(AMFVariantStruct* pDest, const AMFRect* value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignSize(AMFVariantStruct* pDest, const AMFSize* value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignPoint(AMFVariantStruct* pDest, const AMFPoint* value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignFloatSize(AMFVariantStruct* pDest, const AMFFloatSize* value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignFloatPoint2D(AMFVariantStruct* pDest, const AMFFloatPoint2D* value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignFloatPoint3D(AMFVariantStruct* pDest, const AMFFloatPoint3D* value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignFloatVector4D(AMFVariantStruct* pDest, const AMFFloatVector4D* value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignRate(AMFVariantStruct* pDest, const AMFRate* value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignRatio(AMFVariantStruct* pDest, const AMFRatio* value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignColor(AMFVariantStruct* pDest, const AMFColor* value);

#if defined(__cplusplus)
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignRect(AMFVariantStruct* pDest, const AMFRect& value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignSize(AMFVariantStruct* pDest, const AMFSize& value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignPoint(AMFVariantStruct* pDest, const AMFPoint& value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignFloatSize(AMFVariantStruct* pDest, const AMFFloatSize& value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignFloatPoint2D(AMFVariantStruct* pDest, const AMFFloatPoint2D& value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignFloatPoint3D(AMFVariantStruct* pDest, const AMFFloatPoint3D& value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignFloatVector4D(AMFVariantStruct* pDest, const AMFFloatVector4D& value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignRate(AMFVariantStruct* pDest, const AMFRate& value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignRatio(AMFVariantStruct* pDest, const AMFRatio& value);
    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantAssignColor(AMFVariantStruct* pDest, const AMFColor& value);

    static AMF_INLINE AMF_RESULT       AMF_CDECL_CALL AMFVariantChangeType(AMFVariantStruct* pDest, const AMFVariantStruct* pSrc, AMF_VARIANT_TYPE newType);
#endif
    static AMF_INLINE char*            AMF_CDECL_CALL AMFVariantDuplicateString(const char* from);
    static AMF_INLINE void             AMF_CDECL_CALL AMFVariantFreeString(char* from);
    static AMF_INLINE wchar_t*         AMF_CDECL_CALL AMFVariantDuplicateWString(const wchar_t* from);
    static AMF_INLINE void             AMF_CDECL_CALL AMFVariantFreeWString(wchar_t* from);

#if defined(__cplusplus)
    //----------------------------------------------------------------------------------------------
    // AMF_INLINE Variant helper class
    //----------------------------------------------------------------------------------------------
    class AMFVariant : public AMFVariantStruct
    {
    public:
        class String;
        class WString;

    public:
        AMFVariant() {  AMFVariantInit(this); }
        explicit AMFVariant(const AMFVariantStruct& other) { AMFVariantInit(this); AMFVariantCopy(this, const_cast<AMFVariantStruct*>(&other)); }

        explicit AMFVariant(const AMFVariantStruct* pOther);
        template<typename T>
        explicit AMFVariant(const AMFInterfacePtr_T<T>& pValue);

        AMFVariant(const AMFVariant& other) { AMFVariantInit(this); AMFVariantCopy(this, const_cast<AMFVariantStruct*>(static_cast<const AMFVariantStruct*>(&other))); }

        explicit AMF_INLINE AMFVariant(amf_bool value)          { AMFVariantInit(this); AMFVariantAssignBool(this, value); }
        explicit AMF_INLINE AMFVariant(amf_int64 value)         { AMFVariantInit(this); AMFVariantAssignInt64(this, value); }
        explicit AMF_INLINE AMFVariant(amf_uint64 value)        { AMFVariantInit(this); AMFVariantAssignInt64(this, (amf_int64)value); }
        explicit AMF_INLINE AMFVariant(amf_int32 value)         { AMFVariantInit(this); AMFVariantAssignInt64(this, value); }
        explicit AMF_INLINE AMFVariant(amf_uint32 value)        { AMFVariantInit(this); AMFVariantAssignInt64(this, value); }
        explicit AMF_INLINE AMFVariant(amf_double value)        { AMFVariantInit(this); AMFVariantAssignDouble(this, value); }
        explicit AMF_INLINE AMFVariant(amf_float value)         { AMFVariantInit(this); AMFVariantAssignFloat(this, value); }
        explicit AMF_INLINE AMFVariant(const AMFRect & value)   { AMFVariantInit(this); AMFVariantAssignRect(this, &value); }
        explicit AMF_INLINE AMFVariant(const AMFSize & value)   { AMFVariantInit(this); AMFVariantAssignSize(this, &value); }
        explicit AMF_INLINE AMFVariant(const AMFPoint& value)   { AMFVariantInit(this); AMFVariantAssignPoint(this, &value); }
        explicit AMF_INLINE AMFVariant(const AMFFloatSize& value) { AMFVariantInit(this); AMFVariantAssignFloatSize(this, &value); }
        explicit AMF_INLINE AMFVariant(const AMFFloatPoint2D& value) { AMFVariantInit(this); AMFVariantAssignFloatPoint2D(this, &value); }
        explicit AMF_INLINE AMFVariant(const AMFFloatPoint3D& value) { AMFVariantInit(this); AMFVariantAssignFloatPoint3D(this, &value); }
        explicit AMF_INLINE AMFVariant(const AMFFloatVector4D& value) { AMFVariantInit(this); AMFVariantAssignFloatVector4D(this, &value); }
        explicit AMF_INLINE AMFVariant(const AMFRate & value)   { AMFVariantInit(this); AMFVariantAssignRate(this, &value); }
        explicit AMF_INLINE AMFVariant(const AMFRatio& value)   { AMFVariantInit(this); AMFVariantAssignRatio(this, &value); }
        explicit AMF_INLINE AMFVariant(const AMFColor& value)   { AMFVariantInit(this); AMFVariantAssignColor(this, &value); }
        explicit AMF_INLINE AMFVariant(const char* value)       { AMFVariantInit(this); AMFVariantAssignString(this, value); }
        explicit AMF_INLINE AMFVariant(const wchar_t* value)    { AMFVariantInit(this); AMFVariantAssignWString(this, value); }
        explicit AMF_INLINE AMFVariant(AMFInterface* pValue)    { AMFVariantInit(this); AMFVariantAssignInterface(this, pValue); }

        ~AMFVariant() { AMFVariantClear(this); }

        AMFVariant& operator=(const AMFVariantStruct& other);
        AMFVariant& operator=(const AMFVariantStruct* pOther);
        AMFVariant& operator=(const AMFVariant& other);

        AMFVariant& operator=(amf_bool          value)      { AMFVariantAssignBool(this, value); return *this;}
        AMFVariant& operator=(amf_int64         value)      { AMFVariantAssignInt64(this, value); return *this;}
        AMFVariant& operator=(amf_uint64        value)      { AMFVariantAssignInt64(this, (amf_int64)value);  return *this;}
        AMFVariant& operator=(amf_int32         value)      { AMFVariantAssignInt64(this, value);  return *this;}
        AMFVariant& operator=(amf_uint32        value)      { AMFVariantAssignInt64(this, value);  return *this;}
        AMFVariant& operator=(amf_double        value)      { AMFVariantAssignDouble(this, value);  return *this;}
        AMFVariant& operator=(amf_float        value)       { AMFVariantAssignFloat(this, value);  return *this; }
        AMFVariant& operator=(const AMFRect &   value)      { AMFVariantAssignRect(this, &value);  return *this;}
        AMFVariant& operator=(const AMFSize &   value)      { AMFVariantAssignSize(this, &value);  return *this;}
        AMFVariant& operator=(const AMFPoint&   value)      { AMFVariantAssignPoint(this, &value);  return *this;}
        AMFVariant& operator=(const AMFFloatSize&   value) { AMFVariantAssignFloatSize(this, &value);  return *this; }
        AMFVariant& operator=(const AMFFloatPoint2D&   value) { AMFVariantAssignFloatPoint2D(this, &value);  return *this; }
        AMFVariant& operator=(const AMFFloatPoint3D&   value) { AMFVariantAssignFloatPoint3D(this, &value);  return *this; }
        AMFVariant& operator=(const AMFFloatVector4D&   value) { AMFVariantAssignFloatVector4D(this, &value);  return *this; }
        AMFVariant& operator=(const AMFRate &   value)      { AMFVariantAssignRate(this, &value);  return *this;}
        AMFVariant& operator=(const AMFRatio&   value)      { AMFVariantAssignRatio(this, &value);  return *this;}
        AMFVariant& operator=(const AMFColor&   value)      { AMFVariantAssignColor(this, &value);  return *this;}
        AMFVariant& operator=(const char*       value)      { AMFVariantAssignString(this, value);  return *this;}
        AMFVariant& operator=(const wchar_t*    value)      { AMFVariantAssignWString(this, value);  return *this;}
        AMFVariant& operator=(AMFInterface*     value)      { AMFVariantAssignInterface(this, value);  return *this;}

        template<typename T> AMFVariant& operator=(const AMFInterfacePtr_T<T>& value);

        operator amf_bool() const           { return ToBool();       }
        operator amf_int64() const          { return ToInt64();      }
        operator amf_uint64() const         { return ToUInt64();     }
        operator amf_int32() const          { return ToInt32();      }
        operator amf_uint32() const         { return ToUInt32();     }
        operator amf_double() const         { return ToDouble();     }
        operator amf_float() const          { return ToFloat();      }
        operator AMFRect () const           { return ToRect ();      }
        operator AMFSize () const           { return ToSize ();      }
        operator AMFPoint() const           { return ToPoint();      }
        operator AMFFloatSize() const       { return ToFloatSize(); }
        operator AMFFloatPoint2D() const    { return ToFloatPoint2D(); }
        operator AMFFloatPoint3D() const    { return ToFloatPoint3D(); }
        operator AMFFloatVector4D() const   { return ToFloatVector4D(); }
        operator AMFRate () const           { return ToRate ();      }
        operator AMFRatio() const           { return ToRatio();      }
        operator AMFColor() const           { return ToColor();      }
        operator AMFInterface*() const      { return ToInterface();  }

        AMF_INLINE amf_bool         ToBool() const      { return Empty() ? false        : GetValue<amf_bool,   AMF_VARIANT_BOOL>(AMFVariantGetBool); }
        AMF_INLINE amf_int64        ToInt64() const     { return Empty() ? 0            : GetValue<amf_int64,  AMF_VARIANT_INT64>(AMFVariantGetInt64); }
        AMF_INLINE amf_uint64       ToUInt64() const    { return Empty() ? 0            : GetValue<amf_uint64, AMF_VARIANT_INT64>(AMFVariantGetInt64); }
        AMF_INLINE amf_int32        ToInt32() const     { return Empty() ? 0            : GetValue<amf_int32,  AMF_VARIANT_INT64>(AMFVariantGetInt64); }
        AMF_INLINE amf_uint32       ToUInt32() const    { return Empty() ? 0            : GetValue<amf_uint32, AMF_VARIANT_INT64>(AMFVariantGetInt64); }
        AMF_INLINE amf_double       ToDouble() const    { return Empty() ? 0            : GetValue<amf_double, AMF_VARIANT_DOUBLE>(AMFVariantGetDouble); }
        AMF_INLINE amf_float        ToFloat() const     { return Empty() ? 0            : GetValue<amf_float,  AMF_VARIANT_FLOAT>(AMFVariantGetFloat); }
        AMF_INLINE AMFRect          ToRect () const     { return Empty() ? AMFRect()    : GetValue<AMFRect,  AMF_VARIANT_RECT>(AMFVariantGetRect); }
        AMF_INLINE AMFSize          ToSize () const     { return Empty() ? AMFSize()    : GetValue<AMFSize,  AMF_VARIANT_SIZE>(AMFVariantGetSize); }
        AMF_INLINE AMFPoint         ToPoint() const     { return Empty() ? AMFPoint()   : GetValue<AMFPoint, AMF_VARIANT_POINT>(AMFVariantGetPoint); }
        AMF_INLINE AMFFloatSize     ToFloatSize() const { return Empty() ? AMFFloatSize() : GetValue<AMFFloatSize, AMF_VARIANT_FLOAT_SIZE>(AMFVariantGetFloatSize); }
        AMF_INLINE AMFFloatPoint2D  ToFloatPoint2D() const { return Empty() ? AMFFloatPoint2D() : GetValue<AMFFloatPoint2D, AMF_VARIANT_FLOAT_POINT2D>(AMFVariantGetFloatPoint2D); }
        AMF_INLINE AMFFloatPoint3D  ToFloatPoint3D() const { return Empty() ? AMFFloatPoint3D() : GetValue<AMFFloatPoint3D, AMF_VARIANT_FLOAT_POINT3D>(AMFVariantGetFloatPoint3D); }
        AMF_INLINE AMFFloatVector4D ToFloatVector4D() const { return Empty() ? AMFFloatVector4D() : GetValue<AMFFloatVector4D, AMF_VARIANT_FLOAT_VECTOR4D>(AMFVariantGetFloatVector4D); }
        AMF_INLINE AMFRate          ToRate () const     { return Empty() ? AMFRate()    : GetValue<AMFRate,  AMF_VARIANT_RATE>(AMFVariantGetRate); }
        AMF_INLINE AMFRatio         ToRatio() const     { return Empty() ? AMFRatio()   : GetValue<AMFRatio, AMF_VARIANT_RATIO>(AMFVariantGetRatio); }
        AMF_INLINE AMFColor         ToColor() const     { return Empty() ? AMFColor()   : GetValue<AMFColor, AMF_VARIANT_COLOR>(AMFVariantGetColor); }
        AMF_INLINE AMFInterface*    ToInterface() const { return AMFVariantGetType(this) == AMF_VARIANT_INTERFACE ? this->pInterface : NULL; }
        AMF_INLINE String           ToString() const;
        AMF_INLINE WString          ToWString() const;

        bool operator==(const AMFVariantStruct& other) const;
        bool operator==(const AMFVariantStruct* pOther) const;

        bool operator!=(const AMFVariantStruct& other) const;
        bool operator!=(const AMFVariantStruct* pOther) const;

        void Clear() { AMFVariantClear(this); }

        void Attach(AMFVariantStruct& variant);
        AMFVariantStruct Detach();

        AMFVariantStruct& GetVariant();

        void ChangeType(AMF_VARIANT_TYPE type, const AMFVariant* pSrc = NULL);

        bool Empty() const;
    private:
        template<class ReturnType, AMF_VARIANT_TYPE variantType, typename Getter>
        ReturnType GetValue(Getter getter) const;
    };
    //----------------------------------------------------------------------------------------------
    // helper String class
    //----------------------------------------------------------------------------------------------
    class AMFVariant::String
    {
        friend class AMFVariant;
    private:
        void Free()
        {
            if (m_Str != NULL)
            {
                AMFVariantFreeString(m_Str);
                m_Str = NULL;
            }
        }
    public:
        String() :m_Str(NULL){}
        String(const char* str) : m_Str(NULL)
        {
            m_Str = AMFVariantDuplicateString(str);
        }
        String(const String& p_other) : m_Str(NULL)
        {
            operator=(p_other);
        }

#if (__cplusplus == 201103L) || defined(__GXX_EXPERIMENTAL_CXX0X) || (_MSC_VER >= 1600)
        String(String&& p_other) : m_Str(NULL)
        {
            operator=(p_other);
        }
#endif
        ~String()
        {
            Free();
        }

        char& operator[](size_t index)
        {
            if (index >= size())
            {
                resize(index);
            }
            return m_Str[index];
        }

        String& operator=(const String& p_other)
        {
            Free();
            m_Str = AMFVariantDuplicateString(p_other.m_Str);
            return *this;
        }
#if (__cplusplus == 201103L) || defined(__GXX_EXPERIMENTAL_CXX0X) || (_MSC_VER >= 1600)
        String& operator=(String&& p_other)
        {
            Free();
            m_Str = p_other.m_Str;
            p_other.m_Str = NULL;    //    Transfer the ownership
            return *this;
        }
#endif
        bool operator==(const String& p_other) const
        {
            if (m_Str == NULL && p_other.m_Str == NULL)
            {
                return true;
            }
            else if ((m_Str == NULL && p_other.m_Str != NULL) || (m_Str != NULL && p_other.m_Str == NULL))
            {
                return false;
            }
            return strcmp(c_str(), p_other.c_str()) == 0;
        }
        const char* c_str() const { return m_Str; }
        size_t size() const
        {
            if(m_Str == NULL)
            {
                return 0;
            }
            return (size_t)strlen(m_Str);
        }

        AMF_INLINE size_t length() const { return size(); }
        
        void resize(size_t sizeAlloc)
        {
            if(sizeAlloc == 0)
            {
                Free();
                return;
            }
            char* str = (char*)amf_variant_alloc(sizeof(char)*(sizeAlloc + 1));
            if(m_Str != NULL)
            {
                size_t copySize = sizeAlloc;
                if(copySize > size())
                {
                    copySize = size();
                }
                memcpy(str, m_Str, copySize * sizeof(char));
                Free();
                str[sizeAlloc] = 0;
            }
            m_Str = str;
        }
    private:
        char*    m_Str;
    };
    //----------------------------------------------------------------------------------------------
    // helper WString class
    //----------------------------------------------------------------------------------------------
    class AMFVariant::WString
    {
        friend class AMFVariant;
    private:
        void Free()
        {
            if (m_Str != NULL)
            {
                AMFVariantFreeWString(m_Str);
                m_Str = NULL;
            }
        }
    public:
        WString() :m_Str(NULL){}
        WString(const wchar_t* str) : m_Str(NULL)
        {
            m_Str = AMFVariantDuplicateWString(str);
        }
        WString(const WString& p_other) : m_Str(NULL)
        {
            operator=(p_other);
        }
#if (__cplusplus == 201103L) || defined(__GXX_EXPERIMENTAL_CXX0X) || (_MSC_VER >= 1600)
        WString(WString&& p_other) : m_Str(NULL)
        {
            operator=(p_other);
        }
#endif
        ~WString()
        {
            Free();
        }

        WString& operator=(const WString& p_other)
        {
            Free();
            m_Str = AMFVariantDuplicateWString(p_other.m_Str);
            return *this;
        }
#if (__cplusplus == 201103L) || defined(__GXX_EXPERIMENTAL_CXX0X) || (_MSC_VER >= 1600)
        WString& operator=(WString&& p_other)
        {
            Free();
            m_Str = p_other.m_Str;
            p_other.m_Str = NULL;    //    Transfer the ownership
            return *this;
        }
#endif
        wchar_t& operator[](size_t index)
        {
            if (index >= size())
            {
                resize(index);
            }
            return m_Str[index];
        }

        bool operator==(const WString& p_other) const
        {
            if (m_Str == NULL && p_other.m_Str == NULL)
            {
                return true;
            }
            else if ((m_Str == NULL && p_other.m_Str != NULL) || (m_Str != NULL && p_other.m_Str == NULL))
            {
                return false;
            }
            return wcscmp(c_str(), p_other.c_str()) == 0;
        }

        const wchar_t* c_str() const { return m_Str; }
        size_t size()  const
        {
            if(m_Str == NULL)
            {
                return 0;
            }
            return (size_t)wcslen(m_Str);
        }

        AMF_INLINE size_t length() const { return size(); }
        
        void resize(size_t sizeAlloc)
        {
            if(sizeAlloc == 0)
            {
                Free();
                return;
            }
            wchar_t* str = (wchar_t*)amf_variant_alloc(sizeof(wchar_t)*(sizeAlloc + 1));
            if(m_Str != NULL)
            {
                size_t copySize = sizeAlloc;
                if(copySize > size())
                {
                    copySize = size();
                }
                memcpy(str, m_Str, copySize * sizeof(wchar_t));
                Free();
                str[sizeAlloc] = 0;
            }
            m_Str = str;
        }
    private:
        wchar_t*    m_Str;
    };
    //-------------------------------------------------------------------------------------------------
    AMFVariant::String       AMFVariant::ToString() const
    {
        String temp = GetValue<String, AMF_VARIANT_STRING>(AMFVariantGetString); 
        return String(temp.c_str());
    }
    //-------------------------------------------------------------------------------------------------
    AMFVariant::WString      AMFVariant::ToWString() const
    {
        WString temp = GetValue<WString, AMF_VARIANT_WSTRING>(AMFVariantGetWString);
        return WString(temp.c_str());
    }
#endif // defined(__cplusplus)
    //----------------------------------------------------------------------------------------------
    // AMF_INLINE implementation of helper functions 
    //----------------------------------------------------------------------------------------------
    #define AMF_VARIANT_RETURN_IF_INVALID_POINTER(p) \
       { \
            if(p == NULL) \
                    { \
                 return AMF_INVALID_POINTER; \
            } \
       }
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantInit(AMFVariantStruct* pVariant)
    {
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pVariant);
        pVariant->type = AMF_VARIANT_EMPTY;
        return AMF_OK;
    }
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantClear(AMFVariantStruct* pVariant)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pVariant);

        switch(AMFVariantGetType(pVariant))
        {
        case AMF_VARIANT_STRING:
            amf_variant_free(AMFVariantString(pVariant));
            pVariant->type = AMF_VARIANT_EMPTY;
            break;

        case AMF_VARIANT_WSTRING:
            amf_variant_free(AMFVariantWString(pVariant));
            pVariant->type = AMF_VARIANT_EMPTY;
            break;

        case AMF_VARIANT_INTERFACE:
            if(AMFVariantInterface(pVariant) != NULL)
            {
#if defined(__cplusplus)
                AMFVariantInterface(pVariant)->Release();
#else
                AMFVariantInterface(pVariant)->pVtbl->Release(AMFVariantInterface(pVariant));
#endif
                AMFVariantInterface(pVariant) = NULL;
            }
            pVariant->type = AMF_VARIANT_EMPTY;
            break;

        default:
            pVariant->type = AMF_VARIANT_EMPTY;
            break;
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantCompare(const AMFVariantStruct* pFirst, const AMFVariantStruct* pSecond, amf_bool* bEqual)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pFirst);
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pSecond);
    
        if(pFirst == pSecond)
        {
            *bEqual = true;
        }
        else if(AMFVariantGetType(pFirst) != AMFVariantGetType(pSecond))
        {
            *bEqual = false;
        }
        else
        {
            switch(AMFVariantGetType(pFirst))
            {
            case AMF_VARIANT_EMPTY:
                *bEqual = true;
                break;
            case AMF_VARIANT_BOOL:
                *bEqual = AMFVariantGetBool(pFirst) == AMFVariantBool(pSecond);
                break;
            case AMF_VARIANT_INT64:
                *bEqual = AMFVariantGetInt64(pFirst) == AMFVariantInt64(pSecond);
                break;
            case AMF_VARIANT_DOUBLE:
                *bEqual = AMFVariantGetDouble(pFirst) == AMFVariantDouble(pSecond);
                break;
            case AMF_VARIANT_FLOAT:
                *bEqual = AMFVariantGetFloat(pFirst) == AMFVariantFloat(pSecond);
                break;
            case AMF_VARIANT_RECT:
#if defined(__cplusplus)
                *bEqual = AMFVariantGetRect(pFirst) == AMFVariantGetRect(pSecond);
#else
                *bEqual = memcmp(&pFirst->rectValue, &pSecond->rectValue, sizeof(AMFRect)) == 0;
#endif
                break;
            case AMF_VARIANT_SIZE:
#if defined(__cplusplus)
                *bEqual = AMFVariantGetSize(pFirst) == AMFVariantGetSize(pSecond);
#else
                *bEqual = memcmp(&pFirst->sizeValue, &pSecond->sizeValue, sizeof(AMFSize)) == 0;
#endif
                break;
            case AMF_VARIANT_POINT:
#if defined(__cplusplus)
                *bEqual = AMFVariantGetPoint(pFirst) == AMFVariantGetPoint(pSecond);
#else
                *bEqual = memcmp(&pFirst->pointValue, &pSecond->pointValue, sizeof(AMFPoint)) == 0;
#endif
                break;
            case AMF_VARIANT_FLOAT_SIZE:
#if defined(__cplusplus)
                *bEqual = AMFVariantGetFloatSize(pFirst) == AMFVariantGetFloatSize(pSecond);
#else
                *bEqual = memcmp(&pFirst->floatSizeValue, &pSecond->floatSizeValue, sizeof(AMFFloatPoint2D)) == 0;
#endif
                break;
            case AMF_VARIANT_FLOAT_POINT2D:
#if defined(__cplusplus)
                *bEqual = AMFVariantGetFloatPoint2D(pFirst) == AMFVariantGetFloatPoint2D(pSecond);
#else
                *bEqual = memcmp(&pFirst->floatPoint2DValue, &pSecond->floatPoint2DValue, sizeof(AMFFloatPoint2D)) == 0;
#endif
                break;
            case AMF_VARIANT_FLOAT_POINT3D:
#if defined(__cplusplus)
                *bEqual = AMFVariantGetFloatPoint3D(pFirst) == AMFVariantGetFloatPoint3D(pSecond);
#else
                *bEqual = memcmp(&pFirst->floatPoint3DValue, &pSecond->floatPoint3DValue, sizeof(AMFFloatPoint3D)) == 0;
#endif
                break;
            case AMF_VARIANT_FLOAT_VECTOR4D:
#if defined(__cplusplus)
                *bEqual = AMFVariantGetFloatVector4D(pFirst) == AMFVariantGetFloatVector4D(pSecond);
#else
                *bEqual = memcmp(&pFirst->floatVector4DValue, &pSecond->floatVector4DValue, sizeof(AMFFloatPoint3D)) == 0;
#endif
                break;
            case AMF_VARIANT_RATE:
#if defined(__cplusplus)
                *bEqual = AMFVariantGetRate(pFirst) == AMFVariantGetRate(pSecond);
#else
                *bEqual = memcmp(&pFirst->rateValue, &pSecond->rateValue, sizeof(AMFRate)) == 0;
#endif
                break;
            case AMF_VARIANT_RATIO:
#if defined(__cplusplus)
                *bEqual = AMFVariantGetRatio(pFirst) == AMFVariantGetRatio(pSecond);
#else
                *bEqual = memcmp(&pFirst->ratioValue, &pSecond->ratioValue, sizeof(AMFRatio)) == 0;
#endif
                break;
            case AMF_VARIANT_COLOR:
#if defined(__cplusplus)
                *bEqual = AMFVariantGetColor(pFirst) == AMFVariantGetColor(pSecond);
#else
                *bEqual = memcmp(&pFirst->colorValue, &pSecond->colorValue, sizeof(AMFColor)) == 0;
#endif
                break;
            case AMF_VARIANT_STRING:
                *bEqual = strcmp(AMFVariantString(pFirst), AMFVariantString(pSecond)) == 0;
                break;
            case AMF_VARIANT_WSTRING:
                *bEqual = wcscmp(AMFVariantWString(pFirst), AMFVariantWString(pSecond)) == 0;
                break;
            case AMF_VARIANT_INTERFACE:
                *bEqual = AMFVariantInterface(pFirst) == AMFVariantInterface(pSecond);
                break;
            default:
                errRet = AMF_INVALID_ARG;
                break;
            }
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantCopy(AMFVariantStruct* pDest, const AMFVariantStruct* pSrc)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pSrc);
        if(pDest != pSrc)
        {
            switch(AMFVariantGetType(pSrc))
            {
            case AMF_VARIANT_EMPTY:
                errRet = AMFVariantClear(pDest);
                break;
            case AMF_VARIANT_BOOL:
                errRet = AMFVariantAssignBool(pDest, AMFVariantBool(pSrc));
                break;
            case AMF_VARIANT_INT64:
                errRet = AMFVariantAssignInt64(pDest, AMFVariantInt64(pSrc));
                break;
            case AMF_VARIANT_DOUBLE:
                errRet = AMFVariantAssignDouble(pDest, AMFVariantDouble(pSrc));
                break;
            case AMF_VARIANT_FLOAT:
                errRet = AMFVariantAssignFloat(pDest, AMFVariantFloat(pSrc));
                break;
            case AMF_VARIANT_RECT:
                errRet = AMFVariantAssignRect(pDest, &pSrc->rectValue);
                break;
            case AMF_VARIANT_SIZE:
                errRet = AMFVariantAssignSize(pDest, &pSrc->sizeValue);
                break;
            case AMF_VARIANT_POINT:
                errRet = AMFVariantAssignPoint(pDest, &pSrc->pointValue);
                break;
            case AMF_VARIANT_FLOAT_SIZE:
                errRet = AMFVariantAssignFloatSize(pDest, &pSrc->floatSizeValue);
                break;
            case AMF_VARIANT_FLOAT_POINT2D:
                errRet = AMFVariantAssignFloatPoint2D(pDest, &pSrc->floatPoint2DValue);
                break;
            case AMF_VARIANT_FLOAT_POINT3D:
                errRet = AMFVariantAssignFloatPoint3D(pDest, &pSrc->floatPoint3DValue);
                break;
            case AMF_VARIANT_FLOAT_VECTOR4D:
                errRet = AMFVariantAssignFloatVector4D(pDest, &pSrc->floatVector4DValue);
                break;
            case AMF_VARIANT_RATE:
                errRet = AMFVariantAssignRate(pDest, &pSrc->rateValue);
                break;
            case AMF_VARIANT_RATIO:
                errRet = AMFVariantAssignRatio(pDest, &pSrc->ratioValue);
                break;
            case AMF_VARIANT_COLOR:
                errRet = AMFVariantAssignColor(pDest, &pSrc->colorValue);
                break;
            case AMF_VARIANT_STRING:
                errRet = AMFVariantAssignString(pDest, AMFVariantString(pSrc));
                break;
            case AMF_VARIANT_WSTRING:
                errRet = AMFVariantAssignWString(pDest, AMFVariantWString(pSrc));
                break;
            case AMF_VARIANT_INTERFACE:
                errRet = AMFVariantAssignInterface(pDest, AMFVariantInterface(pSrc));
                break;
            default:
                errRet = AMF_INVALID_ARG;
                break;
            }
        }
        return errRet;
    }
    #define AMFVariantTypeEmpty         AMF_VARIANT_EMPTY

    #define AMFVariantTypeBool          AMF_VARIANT_BOOL
    #define AMFVariantTypeInt64         AMF_VARIANT_INT64
    #define AMFVariantTypeDouble        AMF_VARIANT_DOUBLE
    #define AMFVariantTypeFloat         AMF_VARIANT_FLOAT

    #define AMFVariantTypeRect          AMF_VARIANT_RECT
    #define AMFVariantTypeSize          AMF_VARIANT_SIZE
    #define AMFVariantTypePoint         AMF_VARIANT_POINT
    #define AMFVariantTypeFloatPoint2D  AMF_VARIANT_FLOAT_POINT2D
    #define AMFVariantTypeFloatPoint3D  AMF_VARIANT_FLOAT_POINT3D
    #define AMFVariantTypeFloatVector4D AMF_VARIANT_FLOAT_VECTOR4D
    
    #define AMFVariantTypeRate          AMF_VARIANT_RATE
    #define AMFVariantTypeRatio         AMF_VARIANT_RATIO
    #define AMFVariantTypeColor         AMF_VARIANT_COLOR

    #define AMFVariantTypeString        AMF_VARIANT_STRING
    #define AMFVariantTypeWString       AMF_VARIANT_WSTRING
    #define AMFVariantTypeInterface     AMF_VARIANT_INTERFACE

#if defined(__cplusplus)
    
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignString(AMFVariantStruct* pDest, const AMFVariant::String& value)
    {
        return AMFVariantAssignString(pDest, value.c_str());
    }
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignWString(AMFVariantStruct* pDest, const AMFVariant::WString& value)
    {
        return AMFVariantAssignWString(pDest, value.c_str());
    }
        
    static AMF_INLINE amf_bool AMFConvertEmptyToBool(void*, AMF_RESULT& res) { res = AMF_OK; return false; }
    static AMF_INLINE amf_int64 AMFConvertEmptyToInt64(void*, AMF_RESULT& res) {res = AMF_OK; return 0; }
    static AMF_INLINE amf_double AMFConvertEmptyToDouble(void*, AMF_RESULT& res) {res = AMF_OK; return 0; }
    static AMF_INLINE amf_float AMFConvertEmptyToFloat(void*, AMF_RESULT& res) { res = AMF_OK; return 0; }

    
    static AMF_INLINE AMFVariant::String AMFConvertEmptyToString(void*, AMF_RESULT& res) {res = AMF_OK; return ""; }
    static AMF_INLINE AMFVariant::WString AMFConvertEmptyToWString(void*, AMF_RESULT& res) {res = AMF_OK; return L""; }
    static AMF_INLINE amf_int64 AMFConvertBoolToInt64(bool value, AMF_RESULT& res){res = AMF_OK; return value ? 1 : 0;}
    static AMF_INLINE amf_double AMFConvertBoolToDouble(bool value, AMF_RESULT& res){res = AMF_OK; return value ? 1.0 : 0.0;}
    static AMF_INLINE amf_float AMFConvertBoolToFloat(bool value, AMF_RESULT& res) { res = AMF_OK; return value ? 1.0f : 0.0f; }
    static AMF_INLINE AMFVariant::String AMFConvertBoolToString(bool value, AMF_RESULT& res){res = AMF_OK; return value ? "true" : "false";}
    static AMF_INLINE AMFVariant::WString AMFConvertBoolToWString(bool value, AMF_RESULT& res){res = AMF_OK; return value ? L"true" : L"false";}
    static AMF_INLINE bool AMFConvertInt64ToBool(amf_int64 value, AMF_RESULT& res){res = AMF_OK;return value != 0;}
    static AMF_INLINE amf_double AMFConvertInt64ToDouble(amf_int64 value, AMF_RESULT& res){res = AMF_OK;return (amf_double)value;}
    static AMF_INLINE amf_float AMFConvertInt64ToFloat(amf_int64 value, AMF_RESULT& res) { res = AMF_OK; return (amf_float)value; }
    static AMF_INLINE AMFVariant::String AMFConvertInt64ToString(amf_int64 value, AMF_RESULT& res)
    {
        res = AMF_OK;
        char buff[0xFF];
        sprintf(buff, "%" AMFPRId64, (long long)value);
        return buff;
    }
    static AMF_INLINE AMFVariant::WString AMFConvertInt64ToWString(amf_int64 value, AMF_RESULT& res)
    {
        res = AMF_OK;
        wchar_t buff[0xFF];
        swprintf(buff, 0xFF, L"%" LPRId64, (long long)value);
        return buff;
    }

    static AMF_INLINE bool AMFConvertDoubleToBool(amf_double value, AMF_RESULT& res){res = AMF_OK;return value != 0;}
    static AMF_INLINE bool AMFConvertFloatToBool(amf_float value, AMF_RESULT& res) { res = AMF_OK; return value != 0; }
    static AMF_INLINE amf_int64 AMFConvertDoubleToInt64(amf_double value, AMF_RESULT& res){res = AMF_OK;return amf_int64(value);}
    static AMF_INLINE amf_int64 AMFConvertFloatToInt64(amf_float value, AMF_RESULT& res) { res = AMF_OK; return amf_int64(value); }
    static AMF_INLINE AMFVariant::String AMFConvertDoubleToString(amf_double value, AMF_RESULT& res)
    {
        res = AMF_OK;
        char buff[0xFF];
        sprintf(buff, "%lf", value);
        return buff;
    }
    static AMF_INLINE AMFVariant::String AMFConvertFloatToString(amf_float value, AMF_RESULT& res)
    {
        res = AMF_OK;
        char buff[0xFF];
        sprintf(buff, "%f", value);
        return buff;
    }
    static AMF_INLINE AMFVariant::WString AMFConvertDoubleToWString(amf_double value, AMF_RESULT& res)
    {
        res = AMF_OK;
        wchar_t buff[0xFF];
        swprintf(buff, 0xFF, L"%lf", value);
        return buff;
    }
    static AMF_INLINE AMFVariant::WString AMFConvertFloatToWString(amf_float value, AMF_RESULT& res)
    {
        res = AMF_OK;
        wchar_t buff[0xFF];
        swprintf(buff, 0xFF, L"%f", value);
        return buff;
    }

    static AMF_INLINE bool AMFConvertStringToBool(const AMFVariant::String& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        AMFVariant::String tmp = value;
        if(( tmp == "true") || ( tmp == "True") || ( tmp == "TRUE") || ( tmp == "1") )
        {
            return true;
        }
        else
        {
            if(( tmp == "false") || ( tmp == "False") || ( tmp == "FALSE") || ( tmp == "0") )
            {
                return false;
            }
        }
        res = AMF_INVALID_ARG;
        return false;
    }

    static AMF_INLINE amf_int64 AMFConvertStringToInt64(const AMFVariant::String& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        long long tmp = 0;
        int readElements = 0;

        if(value.size() > 2 && ( value.c_str()[0] == '0') && ( value.c_str()[1] == 'x') )
        {
            readElements = sscanf(value.c_str(), "0x%" AMFPRIx64, &tmp);
        }
        else if(value.size() > 0)
        {
            readElements = sscanf(value.c_str(), "%" AMFPRId64, &tmp);
        }
        if(readElements)
        {
            return tmp;
        }
        res = AMF_INVALID_ARG;
        return 0;
    }

    static AMF_INLINE amf_double AMFConvertStringToDouble(const AMFVariant::String& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        amf_double tmp = 0;
        int readElements = 0;
        if(value.size() > 0)
        { 
            readElements = sscanf(value.c_str(), "%lf", &tmp);
        }
        if(readElements)
        {
            return tmp;
        }
        res = AMF_INVALID_ARG;
        return 0;
    }
    static AMF_INLINE amf_float AMFConvertStringToFloat(const AMFVariant::String& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        amf_float tmp = 0;
        int readElements = 0;
        if (value.size() > 0)
        {
            readElements = sscanf(value.c_str(), "%f", &tmp);
        }
        if (readElements)
        {
            return tmp;
        }
        res = AMF_INVALID_ARG;
        return 0;
    }

    static AMF_INLINE AMFVariant::WString AMFConvertStringToWString(const AMFVariant::String& value, AMF_RESULT& res)
    {
        res = AMF_OK;
//        return amf_from_utf8_to_unicode(value);
        AMFVariant::WString result;
        if(0 == value.size())
        {
            return result;
        }
        const char* pUtf8Buff = value.c_str();

#if defined(_WIN32)
        _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
        int UnicodeBuffSize = ::MultiByteToWideChar(CP_UTF8, 0, pUtf8Buff, -1, NULL, 0);
        if(0 == UnicodeBuffSize)
        {
            return result;
        }
        UnicodeBuffSize += 8; // get some extra space
        result.resize(UnicodeBuffSize);
        UnicodeBuffSize = ::MultiByteToWideChar(CP_UTF8, 0, pUtf8Buff, -1, (LPWSTR)result.c_str(), UnicodeBuffSize);
        UnicodeBuffSize--;

#elif defined(__ANDROID__)
        // on android mbstowcs cannot be used to define length
        char* old_locale = setlocale(LC_CTYPE, "en_US.UTF8");

        mbstate_t mbs;
        mbrlen(NULL, 0, &mbs);
        int len = value.size();
        const char* pt = pUtf8Buff;
        int UnicodeBuffSize = 0;
        while(len > 0)
        {
            size_t length = mbrlen (pt, len, &mbs); //MM TODO Android always return 1
            if((length == 0) || (length > len))
            {
                break;
            }
            UnicodeBuffSize++;
            len -= length;
            pt += length;
        }
        UnicodeBuffSize += 8; // get some extra space
        result.resize(UnicodeBuffSize);

        mbrlen (NULL, 0, &mbs);
        len = value.size();
        pt = pUtf8Buff;
        UnicodeBuffSize = 0;
        while(len > 0)                            
        {
            size_t length = mbrlen (pt, len, &mbs);
            if((length == 0) || (length > len))
            {
                break;
            }
            mbrtowc(&((wchar_t*)(result.c_str()))[UnicodeBuffSize], pt, length, &mbs);     //MM TODO Android always return 1 char
            UnicodeBuffSize++;
            len -= length;
            pt += length;
        }
        setlocale(LC_CTYPE, old_locale);

 #else
        char* old_locale = setlocale(LC_CTYPE, "en_US.UTF8");
        int UnicodeBuffSize = mbstowcs(NULL, pUtf8Buff, 0);
        if(0 == UnicodeBuffSize)
        {
            return result;
        }
        UnicodeBuffSize += 8; // get some extra space
        result.resize(UnicodeBuffSize);
        UnicodeBuffSize = mbstowcs((wchar_t*)result.c_str(), pUtf8Buff, UnicodeBuffSize + 1);
        setlocale(LC_CTYPE, old_locale);
#endif
        result.resize(UnicodeBuffSize);
        return result;
    }
    static AMF_INLINE AMFVariant::String AMFConvertWStringToString(const AMFVariant::WString& value, AMF_RESULT& res)
    {
        res = AMF_OK;
//      return amf_from_unicode_to_utf8(value);
        AMFVariant::String result;
        if(0 == value.size())
        {
            return result;
        }

        const wchar_t* pwBuff = value.c_str();

#if defined(_WIN32)
        _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
        int Utf8BuffSize = ::WideCharToMultiByte(CP_UTF8, 0, pwBuff, -1, NULL, 0, NULL, NULL);
        if(0 == Utf8BuffSize)
        {
            return result;
        }
        Utf8BuffSize += 8; // get some extra space
        result.resize(Utf8BuffSize);
        Utf8BuffSize = ::WideCharToMultiByte(CP_UTF8, 0, pwBuff, -1, (LPSTR)result.c_str(), Utf8BuffSize, NULL, NULL);
        Utf8BuffSize--;
#elif defined(__ANDROID__)
        char* old_locale = setlocale(LC_CTYPE, "en_US.UTF8");
        int Utf8BuffSize = value.length();
        if(0 == Utf8BuffSize)
        {
            return result;
        }
        Utf8BuffSize += 8; // get some extra space
        result.resize(Utf8BuffSize);

        mbstate_t mbs;
        mbrlen(NULL, 0, &mbs);

        Utf8BuffSize = 0;
        for( int i = 0; i < value.length(); i++)
        {
            //MM TODO Android - not implemented
            //int written = wcrtomb(&result[Utf8BuffSize], pwBuff[i], &mbs);
            ((char*)(result.c_str()))[Utf8BuffSize] = (char)(pwBuff[i]);
            int written = 1;
            // temp replacement
            Utf8BuffSize += written;
        }
        setlocale(LC_CTYPE, old_locale);

#else
        char* old_locale = setlocale(LC_CTYPE, "en_US.UTF8");
        int Utf8BuffSize = wcstombs(NULL, pwBuff, 0);
        if(0 == Utf8BuffSize)
        {
            return result;
        }
        Utf8BuffSize += 8; // get some extra space
        result.resize(Utf8BuffSize);
        Utf8BuffSize = wcstombs((char*)result.c_str(), pwBuff, Utf8BuffSize + 1);

        setlocale(LC_CTYPE, old_locale);
#endif
        result.resize(Utf8BuffSize);
        return result;
    }


    static AMF_INLINE bool AMFConvertWStringToBool(const AMFVariant::WString& value, AMF_RESULT& res)
    {
        return AMFConvertStringToBool(AMFConvertWStringToString(value, res), res);
    }
    static AMF_INLINE amf_int64 AMFConvertWStringToInt64(const AMFVariant::WString& value, AMF_RESULT& res)
    {
        return AMFConvertStringToInt64(AMFConvertWStringToString(value, res), res);
    }
    static AMF_INLINE amf_double AMFConvertWStringToDouble(const AMFVariant::WString& value, AMF_RESULT& res)
    {
        return AMFConvertStringToDouble(AMFConvertWStringToString(value, res), res);
    }
    static AMF_INLINE amf_float AMFConvertWStringToFloat(const AMFVariant::WString& value, AMF_RESULT& res)
    {
        return AMFConvertStringToFloat(AMFConvertWStringToString(value, res), res);
    }

    static AMF_INLINE AMFVariant::String AMF_STD_CALL AMFConvertRectToString(const AMFRect& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        char buff[0xFF];
        sprintf(buff, "%d,%d,%d,%d", value.left, value.top, value.right, value.bottom);
        return buff;
    }
    static AMF_INLINE AMFVariant::String AMF_STD_CALL AMFConvertSizeToString(const AMFSize& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        char buff[0xFF];
        sprintf(buff, "%d,%d", value.width, value.height);
        return buff;
    }
    static AMF_INLINE AMFVariant::String AMF_STD_CALL AMFConvertPointToString(const AMFPoint& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        char buff[0xFF];
        sprintf(buff, "%d,%d", value.x, value.y);
        return buff;
    }
    static AMF_INLINE AMFVariant::String AMF_STD_CALL AMFConvertFloatSizeToString(const AMFFloatSize& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        char buff[0xFF];
        sprintf(buff, "%f,%f", value.width, value.height);
        return buff;
    }
    static AMF_INLINE AMFVariant::String AMF_STD_CALL AMFConvertFloatPoint2DToString(const AMFFloatPoint2D& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        char buff[0xFF];
        sprintf(buff, "%f,%f", value.x, value.y);
        return buff;
    }
    static AMF_INLINE AMFVariant::String AMF_STD_CALL AMFConvertFloatPoint3DToString(const AMFFloatPoint3D& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        char buff[0xFF];
        sprintf(buff, "%f,%f,%f", value.x, value.y, value.z);
        return buff;
    }
    static AMF_INLINE AMFVariant::String AMF_STD_CALL AMFConvertFloatVector4DToString(const AMFFloatVector4D& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        char buff[0xFF];
        sprintf(buff, "%f,%f,%f,%f", value.x, value.y, value.z, value.w);
        return buff;
    }
    static AMF_INLINE AMFVariant::String AMF_STD_CALL AMFConvertRateToString(const AMFRate& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        char buff[0xFF];
        sprintf(buff, "%d,%d", value.num, value.den);
        return buff;
    }
    static AMF_INLINE AMFVariant::String AMF_STD_CALL AMFConvertRatioToString(const AMFRatio& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        char buff[0xFF];
        sprintf(buff, "%d,%d", value.num, value.den);
        return buff;
    }
    static AMF_INLINE AMFVariant::String AMF_STD_CALL AMFConvertColorToString(const AMFColor& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        char buff[0xFF];
        sprintf(buff, "%d,%d,%d,%d", value.r, value.g, value.b, value.a);
        return buff;
    }

    static AMF_INLINE AMFRect  AMF_STD_CALL AMFConvertStringToRect(const AMFVariant::String& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        AMFRect tmp = {};
        int readElements = 0;
        if(value.size() > 0)
        {
            readElements = sscanf(value.c_str(), "%d,%d,%d,%d", &tmp.left, &tmp.top, &tmp.right, &tmp.bottom);
        }
        if(readElements)
        {
            return tmp;
        }
        res = AMF_INVALID_ARG;
        return tmp;
    }

    static AMF_INLINE AMFSize  AMF_STD_CALL AMFConvertStringToSize(const AMFVariant::String& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        AMFSize tmp = {};
        int readElements = 0;
        if(value.size() > 0)
        {
            readElements = sscanf(value.c_str(), "%d,%d", &tmp.width, &tmp.height);
        }
        if(readElements)
        {
            return tmp;
        }
        res = AMF_INVALID_ARG;
        return tmp;
    }
    static AMF_INLINE AMFPoint AMF_STD_CALL AMFConvertStringToPoint(const AMFVariant::String& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        AMFPoint tmp = {};
        int readElements = 0;
        if(value.size() > 0)
        {
            readElements = sscanf(value.c_str(), "%d,%d", &tmp.x, &tmp.y);
        }
        if(readElements)
        {
            return tmp;
        }
        res = AMF_INVALID_ARG;
        return tmp;
    }
    static AMF_INLINE AMFFloatSize AMF_STD_CALL AMFConvertStringToFloatSize(const AMFVariant::String& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        AMFFloatSize tmp = {};
        int readElements = 0;
        if (value.size() > 0)
        {
            readElements = sscanf(value.c_str(), "%f,%f", &tmp.width, &tmp.height);
        }
        if (readElements)
        {
            return tmp;
        }
        res = AMF_INVALID_ARG;
        return tmp;
    }
    static AMF_INLINE AMFFloatPoint2D AMF_STD_CALL AMFConvertStringToFloatPoint2D(const AMFVariant::String& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        AMFFloatPoint2D tmp = {};
        int readElements = 0;
        if (value.size() > 0)
        {
            readElements = sscanf(value.c_str(), "%f,%f", &tmp.x, &tmp.y);
        }
        if (readElements)
        {
            return tmp;
        }
        res = AMF_INVALID_ARG;
        return tmp;
    }
    static AMF_INLINE AMFFloatPoint3D AMF_STD_CALL AMFConvertStringToFloatPoint3D(const AMFVariant::String& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        AMFFloatPoint3D tmp = {};
        int readElements = 0;
        if (value.size() > 0)
        {
            readElements = sscanf(value.c_str(), "%f,%f,%f", &tmp.x, &tmp.y, &tmp.z);
        }
        if (readElements)
        {
            return tmp;
        }
        res = AMF_INVALID_ARG;
        return tmp;
    }
    static AMF_INLINE AMFFloatVector4D AMF_STD_CALL AMFConvertStringToFloatVector4D(const AMFVariant::String& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        AMFFloatVector4D tmp = {};
        int readElements = 0;
        if (value.size() > 0)
        {
            readElements = sscanf(value.c_str(), "%f,%f,%f,%f", &tmp.x, &tmp.y, &tmp.z, &tmp.w);
        }
        if (readElements)
        {
            return tmp;
        }
        res = AMF_INVALID_ARG;
        return tmp;
    }
    static AMF_INLINE AMFRate  AMF_STD_CALL AMFConvertStringToRate(const AMFVariant::String& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        AMFRate tmp = {};
        int readElements = 0;
        if(value.size() > 0)
        {
            readElements = sscanf(value.c_str(), "%d,%d", &tmp.num, &tmp.den);
        }
        if(readElements)
        {
            return tmp;
        }
        res = AMF_INVALID_ARG;
        return tmp;
    }
    static AMF_INLINE AMFRatio AMF_STD_CALL AMFConvertStringToRatio(const AMFVariant::String& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        AMFRatio tmp = {};
        int readElements = 0;
        if(value.size() > 0)
        {
            readElements = sscanf(value.c_str(), "%d,%d", &tmp.num, &tmp.den);
        }
        if(readElements)
        {
            return tmp;
        }
        res = AMF_INVALID_ARG;
        return tmp;
    }
    static AMF_INLINE AMFColor AMF_STD_CALL AMFConvertStringToColor(const AMFVariant::String& value, AMF_RESULT& res)
    {
        res = AMF_OK;
        int readElements = 0;
        amf_uint32 r = 0;
        amf_uint32 g = 0;
        amf_uint32 b = 0;
        amf_uint32 a = 0;
        if(value.size() > 0)
        { 
            readElements = sscanf(value.c_str(), "%u,%u,%u,%u", &r, &g, &b, &a);
        }
        if(readElements)
        {
            return AMFConstructColor((amf_uint8)r, (amf_uint8)g, (amf_uint8)b, (amf_uint8)a);
        }
        res = AMF_INVALID_ARG;
        return AMFConstructColor(0, 0, 0, 255);
    }
///////////////////////
    static AMF_INLINE AMFVariant::WString AMF_STD_CALL AMFConvertRectToWString(const AMFRect& value, AMF_RESULT& res)
    {
        return AMFConvertStringToWString(AMFConvertRectToString(value, res), res);
    }
    static AMF_INLINE AMFVariant::WString AMF_STD_CALL AMFConvertSizeToWString(const AMFSize& value, AMF_RESULT& res)
    {
        return AMFConvertStringToWString(AMFConvertSizeToString(value, res), res);
    }
    static AMF_INLINE AMFVariant::WString AMF_STD_CALL AMFConvertPointToWString(const AMFPoint& value, AMF_RESULT& res)
    {
        return AMFConvertStringToWString(AMFConvertPointToString(value, res), res);
    }
    static AMF_INLINE AMFVariant::WString AMF_STD_CALL AMFConvertFloatSizeToWString(const AMFFloatSize& value, AMF_RESULT& res)
    {
        return AMFConvertStringToWString(AMFConvertFloatSizeToString(value, res), res);
    }
    static AMF_INLINE AMFVariant::WString AMF_STD_CALL AMFConvertFloatPoint2DToWString(const AMFFloatPoint2D& value, AMF_RESULT& res)
    {
        return AMFConvertStringToWString(AMFConvertFloatPoint2DToString(value, res), res);
    }
    static AMF_INLINE AMFVariant::WString AMF_STD_CALL AMFConvertFloatPoint3DToWString(const AMFFloatPoint3D& value, AMF_RESULT& res)
    {
        return AMFConvertStringToWString(AMFConvertFloatPoint3DToString(value, res), res);
    }
    static AMF_INLINE AMFVariant::WString AMF_STD_CALL AMFConvertFloatVector4DToWString(const AMFFloatVector4D& value, AMF_RESULT& res)
    {
        return AMFConvertStringToWString(AMFConvertFloatVector4DToString(value, res), res);
    }
    static AMF_INLINE AMFVariant::WString AMF_STD_CALL AMFConvertRateToWString(const AMFRate& value, AMF_RESULT& res)
    {
        return AMFConvertStringToWString(AMFConvertRateToString(value, res), res);
    }
    static AMF_INLINE AMFVariant::WString AMF_STD_CALL AMFConvertRatioToWString(const AMFRatio& value, AMF_RESULT& res)
    {
        return AMFConvertStringToWString(AMFConvertRatioToString(value, res), res);
    }
    static AMF_INLINE AMFVariant::WString AMF_STD_CALL AMFConvertColorToWString(const AMFColor& value, AMF_RESULT& res)
    {
        return AMFConvertStringToWString(AMFConvertColorToString(value, res), res);
    }

    static AMF_INLINE AMFRect  AMF_STD_CALL AMFConvertWStringToRect(const AMFVariant::WString& value, AMF_RESULT& res)
    {
        return AMFConvertStringToRect(AMFConvertWStringToString(value, res), res);
    }

    static AMF_INLINE AMFSize  AMF_STD_CALL AMFConvertWStringToSize(const AMFVariant::WString& value, AMF_RESULT& res)
    {
        return AMFConvertStringToSize(AMFConvertWStringToString(value, res), res);
    }
    static AMF_INLINE AMFPoint AMF_STD_CALL AMFConvertWStringToPoint(const AMFVariant::WString& value, AMF_RESULT& res)
    {
        return AMFConvertStringToPoint(AMFConvertWStringToString(value, res), res);
    }
    static AMF_INLINE AMFFloatSize AMF_STD_CALL AMFConvertWStringToFloatSize(const AMFVariant::WString& value, AMF_RESULT& res)
    {
        return AMFConvertStringToFloatSize(AMFConvertWStringToString(value, res), res);
    }
    static AMF_INLINE AMFFloatPoint2D AMF_STD_CALL AMFConvertWStringToFloatPoint2D(const AMFVariant::WString& value, AMF_RESULT& res)
    {
        return AMFConvertStringToFloatPoint2D(AMFConvertWStringToString(value, res), res);
    }
    static AMF_INLINE AMFFloatPoint3D AMF_STD_CALL AMFConvertWStringToFloatPoint3D(const AMFVariant::WString& value, AMF_RESULT& res)
    {
        return AMFConvertStringToFloatPoint3D(AMFConvertWStringToString(value, res), res);
    }
    static AMF_INLINE AMFFloatVector4D AMF_STD_CALL AMFConvertWStringToFloatVector4D(const AMFVariant::WString& value, AMF_RESULT& res)
    {
        return AMFConvertStringToFloatVector4D(AMFConvertWStringToString(value, res), res);
    }
    static AMF_INLINE AMFRate  AMF_STD_CALL AMFConvertWStringToRate(const AMFVariant::WString& value, AMF_RESULT& res)
    {
        return AMFConvertStringToRate(AMFConvertWStringToString(value, res), res);
    }
    static AMF_INLINE AMFRatio AMF_STD_CALL AMFConvertWStringToRatio(const AMFVariant::WString& value, AMF_RESULT& res)
    {
        return AMFConvertStringToRatio(AMFConvertWStringToString(value, res), res);
    }
    static AMF_INLINE AMFColor AMF_STD_CALL AMFConvertWStringToColor(const AMFVariant::WString& value, AMF_RESULT& res)
    {
        return AMFConvertStringToColor(AMFConvertWStringToString(value, res), res);
    }

    //-------------------------------------------------------------------------------------------------
    #define AMFConvertTool(srcType, dstType)\
        if(AMFVariantGetType(pSrc) == AMFVariantType##srcType && newType == AMFVariantType##dstType)\
        {\
            AMF_RESULT res = AMF_OK;\
            AMFVariantAssign##dstType(pDest, AMFConvert##srcType##To##dstType(AMFVariant##srcType(pSrc), res));\
            return res;\
        }\
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantChangeType(AMFVariantStruct* pDest, const AMFVariantStruct* pSrc, AMF_VARIANT_TYPE newType)
    {
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);

        if(pSrc == 0)
        {
            pSrc = pDest;
        }

        if(AMFVariantGetType(pSrc) == newType)
        {
            if(pDest == pSrc)
            {
                return AMF_OK;
            }
            return AMFVariantCopy(pDest, pSrc);
        }
        AMFVariantClear(pDest);

        AMFConvertTool(Empty, Bool);
        AMFConvertTool(Empty, Int64);
        AMFConvertTool(Empty, Double);
        AMFConvertTool(Empty, Float);
        AMFConvertTool(Empty, String);
        AMFConvertTool(Empty, WString);

        AMFConvertTool(Bool, Int64);
        AMFConvertTool(Bool, Double);
        AMFConvertTool(Bool, Float);
        AMFConvertTool(Bool, String);
        AMFConvertTool(Bool, WString);

        AMFConvertTool(Int64, Bool);
        AMFConvertTool(Int64, Double);
        AMFConvertTool(Int64, Float);
        AMFConvertTool(Int64, String);
        AMFConvertTool(Int64, WString);

        AMFConvertTool(Double, Bool);
        AMFConvertTool(Double, Int64);
        AMFConvertTool(Double, String);
        AMFConvertTool(Double, String);

        AMFConvertTool(Float, Bool);
        AMFConvertTool(Float, Int64);
        AMFConvertTool(Float, String);
        AMFConvertTool(Float, String);

        AMFConvertTool(String, Bool);
        AMFConvertTool(String, Int64);
        AMFConvertTool(String, Double);
        AMFConvertTool(String, Float);
        AMFConvertTool(String, WString);

        AMFConvertTool(WString, Bool);
        AMFConvertTool(WString, Int64);
        AMFConvertTool(WString, Double);
        AMFConvertTool(WString, Float);
        AMFConvertTool(WString, String);

        AMFConvertTool(String, Rect);
        AMFConvertTool(String, Size);
        AMFConvertTool(String, Point);
        AMFConvertTool(String, Rate);
        AMFConvertTool(String, Ratio);
        AMFConvertTool(String, Color);

        AMFConvertTool(Rect , String);
        AMFConvertTool(Size , String);
        AMFConvertTool(Point, String);
        AMFConvertTool(Rate , String);
        AMFConvertTool(Ratio, String);
        AMFConvertTool(Color, String);

        AMFConvertTool(WString, Rect);
        AMFConvertTool(WString, Size);
        AMFConvertTool(WString, Point);
        AMFConvertTool(WString, Rate);
        AMFConvertTool(WString, Ratio);
        AMFConvertTool(WString, Color);

        AMFConvertTool(Rect , WString);
        AMFConvertTool(Size , WString);
        AMFConvertTool(Point, WString);
        AMFConvertTool(Rate , WString);
        AMFConvertTool(Ratio, WString);
        AMFConvertTool(Color, WString);

        return AMF_INVALID_ARG;
    }
#endif // #if defined(__cplusplus)

    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignBool(AMFVariantStruct* pDest, amf_bool value)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);

        errRet = AMFVariantClear(pDest);
        if(errRet == AMF_OK)
        {
            pDest->type = AMF_VARIANT_BOOL;
            AMFVariantBool(pDest) = value;
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignInt64(AMFVariantStruct* pDest, amf_int64 value)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);

        errRet = AMFVariantClear(pDest);
        if(errRet == AMF_OK)
        {
            pDest->type = AMF_VARIANT_INT64;
            AMFVariantInt64(pDest) = value;
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignDouble(AMFVariantStruct* pDest, amf_double value)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);

        errRet = AMFVariantClear(pDest);
        if(errRet == AMF_OK)
        {
            pDest->type = AMF_VARIANT_DOUBLE;
            AMFVariantDouble(pDest) = value;
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignFloat(AMFVariantStruct* pDest, amf_float value)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);

        errRet = AMFVariantClear(pDest);
        if (errRet == AMF_OK)
        {
            pDest->type = AMF_VARIANT_FLOAT;
            AMFVariantFloat(pDest) = value;
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignString(AMFVariantStruct* pDest, const char* pValue)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pValue);

        errRet = AMFVariantClear(pDest);
        if(errRet == AMF_OK)
        {
            const size_t size = (strlen(pValue) + 1);
            pDest->type = AMF_VARIANT_STRING;
            AMFVariantString(pDest) = (char*)amf_variant_alloc(size * sizeof(char));
            if(AMFVariantString(pDest))
            {
                memcpy(AMFVariantString(pDest), pValue, size * sizeof(char));
            }
            else
            {
                errRet = AMF_OUT_OF_MEMORY;
            }
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignWString(AMFVariantStruct* pDest, const wchar_t* pValue)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pValue);

        errRet = AMFVariantClear(pDest);
        if(errRet == AMF_OK)
        {
            const size_t size = (wcslen(pValue) + 1);
            pDest->type = AMF_VARIANT_WSTRING;
            AMFVariantWString(pDest) = (wchar_t*)amf_variant_alloc(size * sizeof(wchar_t));
            if(AMFVariantWString(pDest))
            {
                memcpy(AMFVariantWString(pDest), pValue, size * sizeof(wchar_t));
            }
            else
            {
                errRet = AMF_OUT_OF_MEMORY;
            }
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignInterface(AMFVariantStruct* pDest, AMFInterface* pValue)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);
        //AMF_VARIANT_RETURN_IF_INVALID_POINTER(pValue);//can be NULL

        errRet = AMFVariantClear(pDest);
        if(errRet == AMF_OK)
        {
            pDest->type = AMF_VARIANT_INTERFACE;
            AMFVariantInterface(pDest) = pValue;
            if(AMFVariantInterface(pDest))
            {
#if defined(__cplusplus)
                AMFVariantInterface(pDest)->Acquire();
#else
                AMFVariantInterface(pDest)->pVtbl->Acquire(AMFVariantInterface(pDest));
#endif
            }
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignRect(AMFVariantStruct* pDest, const AMFRect& value)
    {
        return AMFVariantAssignRect(pDest, &value);
    }
#endif
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignRect (AMFVariantStruct* pDest, const AMFRect* value)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);

        errRet = AMFVariantClear(pDest);
        if(errRet == AMF_OK)
        {
            pDest->type = AMF_VARIANT_RECT;
            AMFVariantRect(pDest) = *value;
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignSize (AMFVariantStruct* pDest, const AMFSize& value)
    {
        return AMFVariantAssignSize (pDest, &value);
    }
#endif
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignSize (AMFVariantStruct* pDest, const AMFSize* value)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);

        errRet = AMFVariantClear(pDest);
        if(errRet == AMF_OK)
        {
            pDest->type = AMF_VARIANT_SIZE;
            AMFVariantSize(pDest) = *value;
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignPoint(AMFVariantStruct* pDest, const AMFPoint& value)
    {
        return AMFVariantAssignPoint(pDest, &value);
    }
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignFloatSize(AMFVariantStruct* pDest, const AMFFloatSize& value)
    {
        return AMFVariantAssignFloatSize(pDest, &value);
    }
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignFloatPoint2D(AMFVariantStruct* pDest, const AMFFloatPoint2D& value)
    {
        return AMFVariantAssignFloatPoint2D(pDest, &value);
    }
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignFloatPoint3D(AMFVariantStruct* pDest, const AMFFloatPoint3D& value)
    {
        return AMFVariantAssignFloatPoint3D(pDest, &value);
    }
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignFloatVector4D(AMFVariantStruct* pDest, const AMFFloatVector4D& value)
    {
        return AMFVariantAssignFloatVector4D(pDest, &value);
    }
#endif
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignPoint(AMFVariantStruct* pDest, const AMFPoint* value)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);

        errRet = AMFVariantClear(pDest);
        if(errRet == AMF_OK)
        {
            pDest->type = AMF_VARIANT_POINT;
            AMFVariantPoint(pDest) = *value;
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignFloatSize(AMFVariantStruct* pDest, const AMFFloatSize* value)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);

        errRet = AMFVariantClear(pDest);
        if (errRet == AMF_OK)
        {
            pDest->type = AMF_VARIANT_FLOAT_SIZE;
            AMFVariantFloatSize(pDest) = *value;
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignFloatPoint2D(AMFVariantStruct* pDest, const AMFFloatPoint2D* value)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);

        errRet = AMFVariantClear(pDest);
        if (errRet == AMF_OK)
        {
            pDest->type = AMF_VARIANT_FLOAT_POINT2D;
            AMFVariantFloatPoint2D(pDest) = *value;
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignFloatPoint3D(AMFVariantStruct* pDest, const AMFFloatPoint3D* value)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);

        errRet = AMFVariantClear(pDest);
        if (errRet == AMF_OK)
        {
            pDest->type = AMF_VARIANT_FLOAT_POINT3D;
            AMFVariantFloatPoint3D(pDest) = *value;
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignFloatVector4D(AMFVariantStruct* pDest, const AMFFloatVector4D* value)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);

        errRet = AMFVariantClear(pDest);
        if (errRet == AMF_OK)
        {
            pDest->type = AMF_VARIANT_FLOAT_VECTOR4D;
            AMFVariantFloatVector4D(pDest) = *value;
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignRate (AMFVariantStruct* pDest, const AMFRate& value)
    {
        return AMFVariantAssignRate (pDest, &value);
    }
#endif
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignRate (AMFVariantStruct* pDest, const AMFRate* value)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);

        errRet = AMFVariantClear(pDest);
        if(errRet == AMF_OK)
        {
            pDest->type = AMF_VARIANT_RATE;
            AMFVariantRate(pDest) = *value;
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignRatio(AMFVariantStruct* pDest, const AMFRatio& value)
    {
        return AMFVariantAssignRatio(pDest, &value);
    }
#endif
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignRatio(AMFVariantStruct* pDest, const AMFRatio* value)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);

        errRet = AMFVariantClear(pDest);
        if(errRet == AMF_OK)
        {
            pDest->type = AMF_VARIANT_RATIO;
            AMFVariantRatio(pDest) = *value;
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignColor(AMFVariantStruct* pDest, const AMFColor& value)
    {
        return AMFVariantAssignColor(pDest, &value);
    }
#endif
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE AMF_RESULT AMF_CDECL_CALL AMFVariantAssignColor(AMFVariantStruct* pDest, const AMFColor* value)
    {
        AMF_RESULT errRet = AMF_OK;
        AMF_VARIANT_RETURN_IF_INVALID_POINTER(pDest);

        errRet = AMFVariantClear(pDest);
        if(errRet == AMF_OK)
        {
            pDest->type = AMF_VARIANT_COLOR;
            AMFVariantColor(pDest) = *value;
        }
        return errRet;
    }
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE char* AMF_CDECL_CALL AMFVariantDuplicateString(const char* from)
    {
        char* ret = 0;
        if(from)
        {
            ret = (char*)amf_variant_alloc(sizeof(char)*(strlen(from) + 1));
            if(ret)
            {
                strcpy(ret, from);
            }
        }
        return ret;
    }
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE void AMF_CDECL_CALL AMFVariantFreeString(char* from)
    {
        amf_variant_free(from);
    }
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE wchar_t* AMF_CDECL_CALL AMFVariantDuplicateWString(const wchar_t* from)
    {
        wchar_t* ret = 0;
        if(from)
        {
            ret = (wchar_t*)amf_variant_alloc(sizeof(wchar_t)*(wcslen(from) + 1));
            if(ret)
            {
                wcscpy(ret, from);
            }
        }
        return ret;
    }
    //-------------------------------------------------------------------------------------------------
    static AMF_INLINE void AMF_CDECL_CALL AMFVariantFreeWString(wchar_t* from)
    {
        amf_variant_free(from);
    }
    //----------------------------------------------------------------------------------------------
    // AMF_INLINE implementation of AMFVariant class
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    AMF_INLINE AMFVariant::AMFVariant(const AMFVariantStruct* pOther)
    {
        AMFVariantInit(this);
        if(pOther != NULL)
        {
            AMFVariantCopy(this, const_cast<AMFVariantStruct*>(pOther));
        }
    }
    //-------------------------------------------------------------------------------------------------
    template<typename T>
    AMFVariant::AMFVariant(const AMFInterfacePtr_T<T>& pValue)
    {
        AMFVariantInit(this);
        AMFVariantAssignInterface(this, pValue);
    }
    //-------------------------------------------------------------------------------------------------
    template<class ReturnType, AMF_VARIANT_TYPE variantType, typename Getter>
    ReturnType AMFVariant::GetValue(Getter getter) const
    {
        ReturnType str = ReturnType();
        if(AMFVariantGetType(this) == variantType)
        {
            str = static_cast<ReturnType>(getter(this));
        }
        else
        {
            AMFVariant varDest;
            varDest.ChangeType(variantType, this);
            if(varDest.type != AMF_VARIANT_EMPTY)
            {
                str = static_cast<ReturnType>(getter(&varDest));
            }
        }
        return str;
    }
    //-------------------------------------------------------------------------------------------------
    AMF_INLINE AMFVariant& AMFVariant::operator=(const AMFVariantStruct& other)
    {
        AMFVariantCopy(this, const_cast<AMFVariantStruct*>(&other));
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    AMF_INLINE AMFVariant& AMFVariant::operator=(const AMFVariantStruct* pOther)
    {
        if(pOther != NULL)
        {
            AMFVariantCopy(this, const_cast<AMFVariantStruct*>(pOther));
        }
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    AMF_INLINE AMFVariant& AMFVariant::operator=(const AMFVariant& other)
    {
        AMFVariantCopy(this,
                const_cast<AMFVariantStruct*>(static_cast<const AMFVariantStruct*>(&other)));
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    template<typename T>
    AMFVariant& AMFVariant::operator=(const AMFInterfacePtr_T<T>& value)
    {
        AMFVariantAssignInterface(this, value);
        return *this;
    }
    //-------------------------------------------------------------------------------------------------
    AMF_INLINE bool AMFVariant::operator==(const AMFVariantStruct& other) const
    {
        return *this == &other;
    }
    //-------------------------------------------------------------------------------------------------
    AMF_INLINE bool AMFVariant::operator==(const AMFVariantStruct* pOther) const
    {
        //TODO: double check
        amf_bool ret = false;
        if(pOther == NULL)
        {
            ret = false;
        }
        else
        {
            AMFVariantCompare(this, pOther, &ret);
        }
        return ret;
    }
    //-------------------------------------------------------------------------------------------------
    AMF_INLINE bool AMFVariant::operator!=(const AMFVariantStruct& other) const
    {
        return !(*this == &other);
    }
    //-------------------------------------------------------------------------------------------------
    AMF_INLINE bool AMFVariant::operator!=(const AMFVariantStruct* pOther) const
    {
        return !(*this == pOther);
    }
    //-------------------------------------------------------------------------------------------------
    AMF_INLINE void AMFVariant::Attach(AMFVariantStruct& variant)
    {
        Clear();
        memcpy(this, &variant, sizeof(variant));
        AMFVariantGetType(&variant) = AMF_VARIANT_EMPTY;
    }
    //-------------------------------------------------------------------------------------------------
    AMF_INLINE AMFVariantStruct AMFVariant::Detach()
    {
        AMFVariantStruct varResult = *this;
        AMFVariantGetType(this) = AMF_VARIANT_EMPTY;
        return varResult;
    }
    //-------------------------------------------------------------------------------------------------
    AMF_INLINE AMFVariantStruct& AMFVariant::GetVariant()
    {
        return *static_cast<AMFVariantStruct*>(this);
    }
    //-------------------------------------------------------------------------------------------------
    AMF_INLINE void AMFVariant::ChangeType(AMF_VARIANT_TYPE newType, const AMFVariant* pSrc)
    {
        AMFVariantChangeType(this, pSrc, newType);
    }
    //-------------------------------------------------------------------------------------------------
    AMF_INLINE bool AMFVariant::Empty() const
    {
        return type == AMF_VARIANT_EMPTY;
    }
    //-------------------------------------------------------------------------------------------------
#endif // #if defined(__cplusplus)

#if defined(__cplusplus)
} //namespace amf
#endif

#endif //#ifndef AMF_Variant_h
