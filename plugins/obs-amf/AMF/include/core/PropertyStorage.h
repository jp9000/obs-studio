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

#ifndef AMF_PropertyStorage_h
#define AMF_PropertyStorage_h
#pragma once

#include "Variant.h"

#if defined(__cplusplus)
namespace amf
{
#endif
    //----------------------------------------------------------------------------------------------
    // AMFPropertyStorageObserver interface
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)

    class AMF_NO_VTABLE AMFPropertyStorageObserver
    {
    public:
        virtual void                AMF_STD_CALL OnPropertyChanged(const wchar_t* name) = 0;
    };
#else //#if defined(__cplusplus)
    typedef struct AMFPropertyStorageObserver AMFPropertyStorageObserver;
    typedef struct AMFPropertyStorageObserverVtbl
    {
        void                (AMF_STD_CALL *OnPropertyChanged)(AMFPropertyStorageObserver *pThis, const wchar_t* name);
    } AMFPropertyStorageObserverVtbl;

    struct AMFPropertyStorageObserver
    {
        const AMFPropertyStorageObserverVtbl *pVtbl;
    };

#endif // #if defined(__cplusplus)
#if defined(__cplusplus)
    //----------------------------------------------------------------------------------------------
    // AMFPropertyStorage interface
    //----------------------------------------------------------------------------------------------
    class AMF_NO_VTABLE AMFPropertyStorage : public AMFInterface
    {
    public:
        AMF_DECLARE_IID(0xc7cec05b, 0xcfb9, 0x48af, 0xac, 0xe3, 0xf6, 0x8d, 0xf8, 0x39, 0x5f, 0xe3)

        virtual AMF_RESULT          AMF_STD_CALL SetProperty(const wchar_t* name, AMFVariantStruct value) = 0;
        virtual AMF_RESULT          AMF_STD_CALL GetProperty(const wchar_t* name, AMFVariantStruct* pValue) const = 0;

        virtual amf_bool            AMF_STD_CALL HasProperty(const wchar_t* name) const = 0;
        virtual amf_size            AMF_STD_CALL GetPropertyCount() const = 0;
        virtual AMF_RESULT          AMF_STD_CALL GetPropertyAt(amf_size index, wchar_t* name, amf_size nameSize, AMFVariantStruct* pValue) const = 0;

        virtual AMF_RESULT          AMF_STD_CALL Clear() = 0;
        virtual AMF_RESULT          AMF_STD_CALL AddTo(AMFPropertyStorage* pDest, amf_bool overwrite, amf_bool deep) const= 0;
        virtual AMF_RESULT          AMF_STD_CALL CopyTo(AMFPropertyStorage* pDest, amf_bool deep) const = 0;

        virtual void                AMF_STD_CALL AddObserver(AMFPropertyStorageObserver* pObserver) = 0;
        virtual void                AMF_STD_CALL RemoveObserver(AMFPropertyStorageObserver* pObserver) = 0;

        template<typename _T>
        AMF_RESULT                  AMF_STD_CALL SetProperty(const wchar_t* name, const _T& value);
        template<typename _T>
        AMF_RESULT                  AMF_STD_CALL GetProperty(const wchar_t* name, _T* pValue) const;
        template<typename _T>
        AMF_RESULT                  AMF_STD_CALL GetPropertyString(const wchar_t* name, _T* pValue) const;
        template<typename _T>
        AMF_RESULT                  AMF_STD_CALL GetPropertyWString(const wchar_t* name, _T* pValue) const;

    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFPropertyStorage> AMFPropertyStoragePtr;
    //----------------------------------------------------------------------------------------------

#else // #if defined(__cplusplus)
    typedef struct AMFPropertyStorage AMFPropertyStorage;
        AMF_DECLARE_IID(AMFPropertyStorage, 0xc7cec05b, 0xcfb9, 0x48af, 0xac, 0xe3, 0xf6, 0x8d, 0xf8, 0x39, 0x5f, 0xe3)

    typedef struct AMFPropertyStorageVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFPropertyStorage* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFPropertyStorage* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFPropertyStorage* pThis, const struct AMFGuid *interfaceID, void** ppInterface);

        // AMFPropertyStorage interface
        AMF_RESULT          (AMF_STD_CALL *SetProperty)(AMFPropertyStorage* pThis, const wchar_t* name, AMFVariantStruct value);
        AMF_RESULT          (AMF_STD_CALL *GetProperty)(AMFPropertyStorage* pThis, const wchar_t* name, AMFVariantStruct* pValue);
        amf_bool            (AMF_STD_CALL *HasProperty)(AMFPropertyStorage* pThis, const wchar_t* name);
        amf_size            (AMF_STD_CALL *GetPropertyCount)(AMFPropertyStorage* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyAt)(AMFPropertyStorage* pThis, amf_size index, wchar_t* name, amf_size nameSize, AMFVariantStruct* pValue);
        AMF_RESULT          (AMF_STD_CALL *Clear)(AMFPropertyStorage* pThis);
        AMF_RESULT          (AMF_STD_CALL *AddTo)(AMFPropertyStorage* pThis, AMFPropertyStorage* pDest, amf_bool overwrite, amf_bool deep);
        AMF_RESULT          (AMF_STD_CALL *CopyTo)(AMFPropertyStorage* pThis, AMFPropertyStorage* pDest, amf_bool deep);
        void                (AMF_STD_CALL *AddObserver)(AMFPropertyStorage* pThis, AMFPropertyStorageObserver* pObserver);
        void                (AMF_STD_CALL *RemoveObserver)(AMFPropertyStorage* pThis, AMFPropertyStorageObserver* pObserver);

    } AMFPropertyStorageVtbl;

    struct AMFPropertyStorage
    {
        const AMFPropertyStorageVtbl *pVtbl;
    };

    #define AMF_ASSIGN_PROPERTY_DATA(res, varType, pThis, name, val ) \
    { \
        AMFVariantStruct var = {0}; \
        AMFVariantAssign##varType(&var, val); \
        res = pThis->pVtbl->SetProperty(pThis, name, var ); \
    }

    #define AMF_QUERY_INTERFACE(res, from, InterfaceTypeTo, to) \
    { \
        AMFGuid guid_##InterfaceTypeTo = IID_##InterfaceTypeTo(); \
        res = from->pVtbl->QueryInterface(from, &guid_##InterfaceTypeTo, (void**)&to); \
    }

    #define AMF_ASSIGN_PROPERTY_INTERFACE(res, pThis, name, val) \
    { \
        AMFInterface *amf_interface; \
        AMFVariantStruct var; \
        res = AMFVariantInit(&var); \
        if (res == AMF_OK) \
        { \
            AMF_QUERY_INTERFACE(res, val, AMFInterface, amf_interface)\
            if (res == AMF_OK) \
            { \
                res = AMFVariantAssignInterface(&var, amf_interface); \
                amf_interface->pVtbl->Release(amf_interface); \
                if (res == AMF_OK) \
                { \
                    res = pThis->pVtbl->SetProperty(pThis, name, var); \
                } \
            } \
            AMFVariantClear(&var); \
        } \
    }

    #define AMF_GET_PROPERTY_INTERFACE(res, pThis, name, TargetType, val) \
    { \
        AMFVariantStruct var; \
        res = AMFVariantInit(&var); \
        if (res != AMF_OK) \
        { \
            res = pThis->pVtbl->GetProperty(pThis, name, &var); \
            if (res == AMF_OK) \
            { \
                if (var.type == AMF_VARIANT_INTERFACE && AMFVariantInterface(&var)) \
                { \
                    AMF_QUERY_INTERFACE(res, AMFVariantInterface(&var), TargetType, val); \
                } \
                else \
                { \
                    res = AMF_INVALID_DATA_TYPE; \
                } \
            } \
        } \
        AMFVariantClear(&var); \
    }

    #define AMF_ASSIGN_PROPERTY_TYPE(res, varType, dataType , pThis, name, val )  AMF_ASSIGN_PROPERTY_DATA(res, varType, pThis, name, (dataType)val)

    #define AMF_ASSIGN_PROPERTY_INT64(res, pThis, name, val ) AMF_ASSIGN_PROPERTY_TYPE(res, Int64, amf_int64, pThis, name, val)
    #define AMF_ASSIGN_PROPERTY_DOUBLE(res, pThis, name, val ) AMF_ASSIGN_PROPERTY_TYPE(res, Double, amf_double, pThis, name, val)
    #define AMF_ASSIGN_PROPERTY_BOOL(res, pThis, name, val ) AMF_ASSIGN_PROPERTY_TYPE(res, Bool, amf_bool, pThis, name, val)
    #define AMF_ASSIGN_PROPERTY_RECT(res, pThis, name, val ) AMF_ASSIGN_PROPERTY_DATA(res, Rect, pThis, name, &val)
    #define AMF_ASSIGN_PROPERTY_SIZE(res, pThis, name, val ) AMF_ASSIGN_PROPERTY_DATA(res, Size, pThis, name, &val)
    #define AMF_ASSIGN_PROPERTY_POINT(res, pThis, name, val ) AMF_ASSIGN_PROPERTY_DATA(res, Point, pThis, name, &val)
    #define AMF_ASSIGN_PROPERTY_RATE(res, pThis, name, val ) AMF_ASSIGN_PROPERTY_DATA(res, Rate, pThis, name, &val)
    #define AMF_ASSIGN_PROPERTY_RATIO(res, pThis, name, val ) AMF_ASSIGN_PROPERTY_DATA(res, Ratio, pThis, name, &val)
    #define AMF_ASSIGN_PROPERTY_COLOR(res, pThis, name, val ) AMF_ASSIGN_PROPERTY_DATA(res, Color, pThis, name, &val)

#endif // #if defined(__cplusplus)


#if defined(__cplusplus)
    //----------------------------------------------------------------------------------------------
    // template methods implementations
    //----------------------------------------------------------------------------------------------
    template<typename _T> inline
    AMF_RESULT AMF_STD_CALL AMFPropertyStorage::SetProperty(const wchar_t* name, const _T& value)
    {
        AMF_RESULT err = SetProperty(name, static_cast<const AMFVariantStruct&>(AMFVariant(value)));
        return err;
    }
    //----------------------------------------------------------------------------------------------
    template<typename _T> inline
    AMF_RESULT AMF_STD_CALL AMFPropertyStorage::GetProperty(const wchar_t* name, _T* pValue) const
    {
        AMFVariant var;
        AMF_RESULT err = GetProperty(name, static_cast<AMFVariantStruct*>(&var));
        if(err == AMF_OK)
        {
            *pValue = static_cast<_T>(var);
        }
        return err;
    }
    //----------------------------------------------------------------------------------------------
    template<typename _T> inline
    AMF_RESULT AMF_STD_CALL AMFPropertyStorage::GetPropertyString(const wchar_t* name, _T* pValue) const
    {
        AMFVariant var;
        AMF_RESULT err = GetProperty(name, static_cast<AMFVariantStruct*>(&var));
        if(err == AMF_OK)
        {
            *pValue = var.ToString().c_str();
        }
        return err;
    }
    //----------------------------------------------------------------------------------------------
    template<typename _T> inline
    AMF_RESULT AMF_STD_CALL AMFPropertyStorage::GetPropertyWString(const wchar_t* name, _T* pValue) const
    {
        AMFVariant var;
        AMF_RESULT err = GetProperty(name, static_cast<AMFVariantStruct*>(&var));
        if(err == AMF_OK)
        {
            *pValue = var.ToWString().c_str();
        }
        return err;
    }
    //----------------------------------------------------------------------------------------------
    template<> inline
    AMF_RESULT AMF_STD_CALL AMFPropertyStorage::GetProperty(const wchar_t* name,
            AMFInterface** ppValue) const
    {
        AMFVariant var;
        AMF_RESULT err = GetProperty(name, static_cast<AMFVariantStruct*>(&var));
        if(err == AMF_OK)
        {
            *ppValue = static_cast<AMFInterface*>(var);
        }
        if(*ppValue)
        {
            (*ppValue)->Acquire();
        }
        return err;
    }
#endif // #if defined(__cplusplus)

#if defined(__cplusplus)
} //namespace amf
#endif

#endif // #ifndef AMF_PropertyStorage_h
