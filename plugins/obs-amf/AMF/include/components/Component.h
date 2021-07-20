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

/**
 ***************************************************************************************************
 * @file  Component.h
 * @brief AMFComponent interface declaration
 ***************************************************************************************************
 */
#ifndef AMF_Component_h
#define AMF_Component_h
#pragma once

#include "../core/Data.h"
#include "../core/PropertyStorageEx.h"
#include "../core/Surface.h"
#include "../core/Context.h"
#include "ComponentCaps.h"

#if defined(__cplusplus)
namespace amf
{
#endif
    //----------------------------------------------------------------------------------------------
    // AMFDataAllocatorCB interface
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFDataAllocatorCB : public AMFInterface
    {
    public:
        AMF_DECLARE_IID(0x4bf46198, 0x8b7b, 0x49d0, 0xaa, 0x72, 0x48, 0xd4, 0x7, 0xce, 0x24, 0xc5 )

        virtual AMF_RESULT AMF_STD_CALL AllocBuffer(AMF_MEMORY_TYPE type, amf_size size, AMFBuffer** ppBuffer) = 0;
        virtual AMF_RESULT AMF_STD_CALL AllocSurface(AMF_MEMORY_TYPE type, AMF_SURFACE_FORMAT format,
            amf_int32 width, amf_int32 height, amf_int32 hPitch, amf_int32 vPitch, AMFSurface** ppSurface) = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFDataAllocatorCB> AMFDataAllocatorCBPtr;
#else // #if defined(__cplusplus)
    AMF_DECLARE_IID(AMFDataAllocatorCB, 0x4bf46198, 0x8b7b, 0x49d0, 0xaa, 0x72, 0x48, 0xd4, 0x7, 0xce, 0x24, 0xc5 )
    typedef struct AMFDataAllocatorCB AMFDataAllocatorCB;

    typedef struct AMFDataAllocatorCBVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFDataAllocatorCB* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFDataAllocatorCB* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFDataAllocatorCB* pThis, const struct AMFGuid *interfaceID, void** ppInterface);
        // AMFDataAllocatorCB interface 
        AMF_RESULT (AMF_STD_CALL *AllocBuffer)(AMFDataAllocatorCB* pThis, AMF_MEMORY_TYPE type, amf_size size, AMFBuffer** ppBuffer);
        AMF_RESULT (AMF_STD_CALL *AllocSurface)(AMFDataAllocatorCB* pThis, AMF_MEMORY_TYPE type, AMF_SURFACE_FORMAT format,
            amf_int32 width, amf_int32 height, amf_int32 hPitch, amf_int32 vPitch, AMFSurface** ppSurface);
    } AMFDataAllocatorCBVtbl;

    struct AMFDataAllocatorCB
    {
        const AMFDataAllocatorCBVtbl *pVtbl;
    };

#endif // #if defined(__cplusplus)
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFComponentOptimizationCallback
    {
    public:
        virtual AMF_RESULT AMF_STD_CALL OnComponentOptimizationProgress(amf_uint percent) = 0;
    };
#else // #if defined(__cplusplus)
    typedef struct AMFComponentOptimizationCallback AMFComponentOptimizationCallback;
    typedef struct AMFComponentOptimizationCallbackVtbl
    {
        // AMFDataAllocatorCB interface 
        AMF_RESULT (AMF_STD_CALL *OnComponentOptimizationProgress)(AMFComponentOptimizationCallback* pThis, amf_uint percent);
    } AMFComponentOptimizationCallbackVtbl;

    struct AMFComponentOptimizationCallback
    {
        const AMFComponentOptimizationCallbackVtbl *pVtbl;
    };

#endif //#if defined(__cplusplus)
    //----------------------------------------------------------------------------------------------
    // AMFComponent interface
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFComponent : public AMFPropertyStorageEx
    {
    public:
        AMF_DECLARE_IID(0x8b51e5e4, 0x455d, 0x4034, 0xa7, 0x46, 0xde, 0x1b, 0xed, 0xc3, 0xc4, 0x6)

        virtual AMF_RESULT  AMF_STD_CALL Init(AMF_SURFACE_FORMAT format,amf_int32 width,amf_int32 height) = 0;
        virtual AMF_RESULT  AMF_STD_CALL ReInit(amf_int32 width,amf_int32 height) = 0;
        virtual AMF_RESULT  AMF_STD_CALL Terminate() = 0;
        virtual AMF_RESULT  AMF_STD_CALL Drain() = 0;
        virtual AMF_RESULT  AMF_STD_CALL Flush() = 0;

        virtual AMF_RESULT  AMF_STD_CALL SubmitInput(AMFData* pData) = 0;
        virtual AMF_RESULT  AMF_STD_CALL QueryOutput(AMFData** ppData) = 0;
        virtual AMFContext* AMF_STD_CALL GetContext() = 0;
        virtual AMF_RESULT  AMF_STD_CALL SetOutputDataAllocatorCB(AMFDataAllocatorCB* callback) = 0;

        virtual AMF_RESULT  AMF_STD_CALL GetCaps(AMFCaps** ppCaps) = 0;
        virtual AMF_RESULT  AMF_STD_CALL Optimize(AMFComponentOptimizationCallback* pCallback) = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFComponent> AMFComponentPtr;
#else // #if defined(__cplusplus)
    AMF_DECLARE_IID(AMFComponent, 0x8b51e5e4, 0x455d, 0x4034, 0xa7, 0x46, 0xde, 0x1b, 0xed, 0xc3, 0xc4, 0x6)
    typedef struct AMFComponent AMFComponent;

    typedef struct AMFComponentVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFComponent* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFComponent* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFComponent* pThis, const struct AMFGuid *interfaceID, void** ppInterface);

        // AMFPropertyStorage interface
        AMF_RESULT          (AMF_STD_CALL *SetProperty)(AMFComponent* pThis, const wchar_t* name, AMFVariantStruct value);
        AMF_RESULT          (AMF_STD_CALL *GetProperty)(AMFComponent* pThis, const wchar_t* name, AMFVariantStruct* pValue);
        amf_bool            (AMF_STD_CALL *HasProperty)(AMFComponent* pThis, const wchar_t* name);
        amf_size            (AMF_STD_CALL *GetPropertyCount)(AMFComponent* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyAt)(AMFComponent* pThis, amf_size index, wchar_t* name, amf_size nameSize, AMFVariantStruct* pValue);
        AMF_RESULT          (AMF_STD_CALL *Clear)(AMFComponent* pThis);
        AMF_RESULT          (AMF_STD_CALL *AddTo)(AMFComponent* pThis, AMFPropertyStorage* pDest, amf_bool overwrite, amf_bool deep);
        AMF_RESULT          (AMF_STD_CALL *CopyTo)(AMFComponent* pThis, AMFPropertyStorage* pDest, amf_bool deep);
        void                (AMF_STD_CALL *AddObserver)(AMFComponent* pThis, AMFPropertyStorageObserver* pObserver);
        void                (AMF_STD_CALL *RemoveObserver)(AMFComponent* pThis, AMFPropertyStorageObserver* pObserver);

        // AMFPropertyStorageEx interface

        amf_size            (AMF_STD_CALL *GetPropertiesInfoCount)(AMFComponent* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyInfoAt)(AMFComponent* pThis, amf_size index, const AMFPropertyInfo** ppInfo);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyInfo)(AMFComponent* pThis, const wchar_t* name, const AMFPropertyInfo** ppInfo);
        AMF_RESULT          (AMF_STD_CALL *ValidateProperty)(AMFComponent* pThis, const wchar_t* name, AMFVariantStruct value, AMFVariantStruct* pOutValidated);

        // AMFComponent interface

        AMF_RESULT  (AMF_STD_CALL *Init)(AMFComponent* pThis, AMF_SURFACE_FORMAT format,amf_int32 width,amf_int32 height);
        AMF_RESULT  (AMF_STD_CALL *ReInit)(AMFComponent* pThis, amf_int32 width,amf_int32 height);
        AMF_RESULT  (AMF_STD_CALL *Terminate)(AMFComponent* pThis);
        AMF_RESULT  (AMF_STD_CALL *Drain)(AMFComponent* pThis);
        AMF_RESULT  (AMF_STD_CALL *Flush)(AMFComponent* pThis);

        AMF_RESULT  (AMF_STD_CALL *SubmitInput)(AMFComponent* pThis, AMFData* pData);
        AMF_RESULT  (AMF_STD_CALL *QueryOutput)(AMFComponent* pThis, AMFData** ppData);
        AMFContext* (AMF_STD_CALL *GetContext)(AMFComponent* pThis);
        AMF_RESULT  (AMF_STD_CALL *SetOutputDataAllocatorCB)(AMFComponent* pThis, AMFDataAllocatorCB* callback);

        AMF_RESULT  (AMF_STD_CALL *GetCaps)(AMFComponent* pThis, AMFCaps** ppCaps);
        AMF_RESULT  (AMF_STD_CALL *Optimize)(AMFComponent* pThis, AMFComponentOptimizationCallback* pCallback);
    } AMFComponentVtbl;

    struct AMFComponent
    {
        const AMFComponentVtbl *pVtbl;
    };

#endif // #if defined(__cplusplus)
    //----------------------------------------------------------------------------------------------
    // AMFInput interface
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFInput : public AMFPropertyStorageEx
    {
    public:
        AMF_DECLARE_IID(0x1181eee7, 0x95f2, 0x434a, 0x9b, 0x96, 0xea, 0x55, 0xa, 0xa7, 0x84, 0x89)

        virtual AMF_RESULT  AMF_STD_CALL SubmitInput(AMFData* pData) = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFInput> AMFInputPtr;
#else // #if defined(__cplusplus)
    AMF_DECLARE_IID(AMFInput, 0x1181eee7, 0x95f2, 0x434a, 0x9b, 0x96, 0xea, 0x55, 0xa, 0xa7, 0x84, 0x89)
    typedef struct AMFInput AMFInput;

    typedef struct AMFInputVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFInput* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFInput* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFInput* pThis, const struct AMFGuid *interfaceID, void** ppInterface);

        // AMFPropertyStorage interface
        AMF_RESULT          (AMF_STD_CALL *SetProperty)(AMFInput* pThis, const wchar_t* name, AMFVariantStruct value);
        AMF_RESULT          (AMF_STD_CALL *GetProperty)(AMFInput* pThis, const wchar_t* name, AMFVariantStruct* pValue);
        amf_bool            (AMF_STD_CALL *HasProperty)(AMFInput* pThis, const wchar_t* name);
        amf_size            (AMF_STD_CALL *GetPropertyCount)(AMFInput* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyAt)(AMFInput* pThis, amf_size index, wchar_t* name, amf_size nameSize, AMFVariantStruct* pValue);
        AMF_RESULT          (AMF_STD_CALL *Clear)(AMFInput* pThis);
        AMF_RESULT          (AMF_STD_CALL *AddTo)(AMFInput* pThis, AMFPropertyStorage* pDest, amf_bool overwrite, amf_bool deep);
        AMF_RESULT          (AMF_STD_CALL *CopyTo)(AMFInput* pThis, AMFPropertyStorage* pDest, amf_bool deep);
        void                (AMF_STD_CALL *AddObserver)(AMFInput* pThis, AMFPropertyStorageObserver* pObserver);
        void                (AMF_STD_CALL *RemoveObserver)(AMFInput* pThis, AMFPropertyStorageObserver* pObserver);

        // AMFPropertyStorageEx interface

        amf_size            (AMF_STD_CALL *GetPropertiesInfoCount)(AMFInput* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyInfoAt)(AMFInput* pThis, amf_size index, const AMFPropertyInfo** ppInfo);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyInfo)(AMFInput* pThis, const wchar_t* name, const AMFPropertyInfo** ppInfo);
        AMF_RESULT          (AMF_STD_CALL *ValidateProperty)(AMFInput* pThis, const wchar_t* name, AMFVariantStruct value, AMFVariantStruct* pOutValidated);

        // AMFInput interface
        AMF_RESULT          (AMF_STD_CALL *SubmitInput)(AMFInput* pThis, AMFData* pData);

    } AMFInputVtbl;

    struct AMFInput
    {
        const AMFInputVtbl *pVtbl;
    };
#endif // #if defined(__cplusplus)

    //----------------------------------------------------------------------------------------------
    // AMFOutput interface
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFOutput : public AMFPropertyStorageEx
    {
    public:
        AMF_DECLARE_IID(0x86a8a037, 0x912c, 0x4698, 0xb0, 0x46, 0x7, 0x5a, 0x1f, 0xac, 0x6b, 0x97);

        virtual AMF_RESULT  AMF_STD_CALL QueryOutput(AMFData** ppData) = 0;
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFOutput> AMFOutputPtr;
#else // #if defined(__cplusplus)
    AMF_DECLARE_IID(AMFOutput, 0x86a8a037, 0x912c, 0x4698, 0xb0, 0x46, 0x7, 0x5a, 0x1f, 0xac, 0x6b, 0x97);
    typedef struct AMFOutput AMFOutput;

    typedef struct AMFOutputVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFOutput* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFOutput* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFOutput* pThis, const struct AMFGuid *interfaceID, void** ppInterface);

        // AMFPropertyStorage interface
        AMF_RESULT          (AMF_STD_CALL *SetProperty)(AMFOutput* pThis, const wchar_t* name, AMFVariantStruct value);
        AMF_RESULT          (AMF_STD_CALL *GetProperty)(AMFOutput* pThis, const wchar_t* name, AMFVariantStruct* pValue);
        amf_bool            (AMF_STD_CALL *HasProperty)(AMFOutput* pThis, const wchar_t* name);
        amf_size            (AMF_STD_CALL *GetPropertyCount)(AMFOutput* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyAt)(AMFOutput* pThis, amf_size index, wchar_t* name, amf_size nameSize, AMFVariantStruct* pValue);
        AMF_RESULT          (AMF_STD_CALL *Clear)(AMFOutput* pThis);
        AMF_RESULT          (AMF_STD_CALL *AddTo)(AMFOutput* pThis, AMFPropertyStorage* pDest, amf_bool overwrite, amf_bool deep);
        AMF_RESULT          (AMF_STD_CALL *CopyTo)(AMFOutput* pThis, AMFPropertyStorage* pDest, amf_bool deep);
        void                (AMF_STD_CALL *AddObserver)(AMFOutput* pThis, AMFPropertyStorageObserver* pObserver);
        void                (AMF_STD_CALL *RemoveObserver)(AMFOutput* pThis, AMFPropertyStorageObserver* pObserver);

        // AMFPropertyStorageEx interface

        amf_size            (AMF_STD_CALL *GetPropertiesInfoCount)(AMFOutput* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyInfoAt)(AMFOutput* pThis, amf_size index, const AMFPropertyInfo** ppInfo);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyInfo)(AMFOutput* pThis, const wchar_t* name, const AMFPropertyInfo** ppInfo);
        AMF_RESULT          (AMF_STD_CALL *ValidateProperty)(AMFOutput* pThis, const wchar_t* name, AMFVariantStruct value, AMFVariantStruct* pOutValidated);

        // AMFOutput interface
        AMF_RESULT          (AMF_STD_CALL *QueryOutput)(AMFOutput* pThis, AMFData** ppData);

    } AMFOutputVtbl;

    struct AMFOutput
    {
        const AMFOutputVtbl *pVtbl;
    };

#endif // #if defined(__cplusplus)
    //----------------------------------------------------------------------------------------------
    // AMFComponent interface
    //----------------------------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFComponentEx : public AMFComponent
    {
    public:
        AMF_DECLARE_IID(0xfda792af, 0x8712, 0x44df, 0x8e, 0xa0, 0xdf, 0xfa, 0xad, 0x2c, 0x80, 0x93)

        virtual amf_int32   AMF_STD_CALL GetInputCount() = 0;
        virtual amf_int32   AMF_STD_CALL GetOutputCount() = 0;

        virtual AMF_RESULT  AMF_STD_CALL GetInput(amf_int32 index, AMFInput** ppInput) = 0;
        virtual AMF_RESULT  AMF_STD_CALL GetOutput(amf_int32 index, AMFOutput** ppOutput) = 0;

    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFComponentEx> AMFComponentExPtr;
#else // #if defined(__cplusplus)
    AMF_DECLARE_IID(AMFComponentEx, 0xfda792af, 0x8712, 0x44df, 0x8e, 0xa0, 0xdf, 0xfa, 0xad, 0x2c, 0x80, 0x93)
    typedef struct AMFComponentEx AMFComponentEx;

    typedef struct AMFComponentExVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFComponentEx* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFComponentEx* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFComponentEx* pThis, const struct AMFGuid *interfaceID, void** ppInterface);

        // AMFPropertyStorage interface
        AMF_RESULT          (AMF_STD_CALL *SetProperty)(AMFComponentEx* pThis, const wchar_t* name, AMFVariantStruct value);
        AMF_RESULT          (AMF_STD_CALL *GetProperty)(AMFComponentEx* pThis, const wchar_t* name, AMFVariantStruct* pValue);
        amf_bool            (AMF_STD_CALL *HasProperty)(AMFComponentEx* pThis, const wchar_t* name);
        amf_size            (AMF_STD_CALL *GetPropertyCount)(AMFComponentEx* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyAt)(AMFComponentEx* pThis, amf_size index, wchar_t* name, amf_size nameSize, AMFVariantStruct* pValue);
        AMF_RESULT          (AMF_STD_CALL *Clear)(AMFComponentEx* pThis);
        AMF_RESULT          (AMF_STD_CALL *AddTo)(AMFComponentEx* pThis, AMFPropertyStorage* pDest, amf_bool overwrite, amf_bool deep);
        AMF_RESULT          (AMF_STD_CALL *CopyTo)(AMFComponentEx* pThis, AMFPropertyStorage* pDest, amf_bool deep);
        void                (AMF_STD_CALL *AddObserver)(AMFComponentEx* pThis, AMFPropertyStorageObserver* pObserver);
        void                (AMF_STD_CALL *RemoveObserver)(AMFComponentEx* pThis, AMFPropertyStorageObserver* pObserver);

        // AMFPropertyStorageEx interface

        amf_size            (AMF_STD_CALL *GetPropertiesInfoCount)(AMFComponentEx* pThis);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyInfoAt)(AMFComponentEx* pThis, amf_size index, const AMFPropertyInfo** ppInfo);
        AMF_RESULT          (AMF_STD_CALL *GetPropertyInfo)(AMFComponentEx* pThis, const wchar_t* name, const AMFPropertyInfo** ppInfo);
        AMF_RESULT          (AMF_STD_CALL *ValidateProperty)(AMFComponentEx* pThis, const wchar_t* name, AMFVariantStruct value, AMFVariantStruct* pOutValidated);

        // AMFComponent interface

        AMF_RESULT  (AMF_STD_CALL *Init)(AMFComponentEx* pThis, AMF_SURFACE_FORMAT format,amf_int32 width,amf_int32 height);
        AMF_RESULT  (AMF_STD_CALL *ReInit)(AMFComponentEx* pThis, amf_int32 width,amf_int32 height);
        AMF_RESULT  (AMF_STD_CALL *Terminate)(AMFComponentEx* pThis);
        AMF_RESULT  (AMF_STD_CALL *Drain)(AMFComponentEx* pThis);
        AMF_RESULT  (AMF_STD_CALL *Flush)(AMFComponentEx* pThis);

        AMF_RESULT  (AMF_STD_CALL *SubmitInput)(AMFComponentEx* pThis, AMFData* pData);
        AMF_RESULT  (AMF_STD_CALL *QueryOutput)(AMFComponentEx* pThis, AMFData** ppData);
        AMFContext* (AMF_STD_CALL *GetContext)(AMFComponentEx* pThis);
        AMF_RESULT  (AMF_STD_CALL *SetOutputDataAllocatorCB)(AMFComponentEx* pThis, AMFDataAllocatorCB* callback);

        AMF_RESULT  (AMF_STD_CALL *GetCaps)(AMFComponentEx* pThis, AMFCaps** ppCaps);
        AMF_RESULT  (AMF_STD_CALL *Optimize)(AMFComponentEx* pThis, AMFComponentOptimizationCallback* pCallback);

        // AMFComponentEx interface

        amf_int32   (AMF_STD_CALL *GetInputCount)(AMFComponentEx* pThis);
        amf_int32   (AMF_STD_CALL *GetOutputCount)(AMFComponentEx* pThis);

        AMF_RESULT  (AMF_STD_CALL *GetInput)(AMFComponentEx* pThis, amf_int32 index, AMFInput** ppInput);
        AMF_RESULT  (AMF_STD_CALL *GetOutput)(AMFComponentEx* pThis, amf_int32 index, AMFOutput** ppOutput);


    } AMFComponentExVtbl;

    struct AMFComponentEx
    {
        const AMFComponentExVtbl *pVtbl;
    };
#endif // #if defined(__cplusplus)


#if defined(__cplusplus)
} // namespace
#endif

typedef enum AMF_STREAM_TYPE_ENUM
{
    AMF_STREAM_UNKNOWN              = 0,
    AMF_STREAM_VIDEO                = 1,
    AMF_STREAM_AUDIO                = 2,
    AMF_STREAM_DATA                 = 3,
} AMF_STREAM_TYPE_ENUM;

typedef enum AMF_STREAM_CODEC_ID_ENUM     // matched codecs from VideoDecoxcderUVD.h
{
    AMF_STREAM_CODEC_ID_UNKNOWN     = 0,
    AMF_STREAM_CODEC_ID_MPEG2       = 1,  // AMFVideoDecoderUVD_MPEG2      
    AMF_STREAM_CODEC_ID_MPEG4       = 2,  // AMFVideoDecoderUVD_MPEG4      
    AMF_STREAM_CODEC_ID_WMV3        = 3,  // AMFVideoDecoderUVD_WMV3       
    AMF_STREAM_CODEC_ID_VC1         = 4,  // AMFVideoDecoderUVD_VC1        
    AMF_STREAM_CODEC_ID_H264_AVC    = 5,  // AMFVideoDecoderUVD_H264_AVC   
    AMF_STREAM_CODEC_ID_H264_MVC    = 6,  // AMFVideoDecoderUVD_H264_MVC   
    AMF_STREAM_CODEC_ID_H264_SVC    = 7,  // AMFVideoDecoderUVD_H264_SVC   
    AMF_STREAM_CODEC_ID_MJPEG       = 8,  // AMFVideoDecoderUVD_MJPEG      
    AMF_STREAM_CODEC_ID_H265_HEVC   = 9,  // AMFVideoDecoderHW_H265_HEVC   
    AMF_STREAM_CODEC_ID_H265_MAIN10 = 10, // AMFVideoDecoderHW_H265_MAIN10 
    AMF_STREAM_CODEC_ID_VP9         = 11, // AMFVideoDecoderHW_VP9
    AMF_STREAM_CODEC_ID_VP9_10BIT   = 12, // AMFVideoDecoderHW_VP9_10BIT   
    AMF_STREAM_CODEC_ID_AV1         = 13, // AMFVideoDecoderHW_AV1 
} AMF_STREAM_CODEC_ID_ENUM;

// common stream properties
#define AMF_STREAM_TYPE                     L"StreamType"           // amf_int64( AMF_STREAM_TYPE_ENUM )
#define AMF_STREAM_ENABLED                  L"Enabled"              // bool( default = false )
#define AMF_STREAM_CODEC_ID                 L"CodecID"              // amf_int64(Video: AMF_STREAM_CODEC_ID_ENUM, Audio: AVCodecID) (default = 0 - uncompressed)
#define AMF_STREAM_BIT_RATE                 L"BitRate"              // amf_int64 (default = codec->bit_rate)
#define AMF_STREAM_EXTRA_DATA               L"ExtraData"            // interface to AMFBuffer - as is from FFMPEG

// video stream properties
#define AMF_STREAM_VIDEO_MEMORY_TYPE        L"VideoMemoryType"      // amf_int64(AMF_MEMORY_TYPE); default = AMF_MEMORY_DX11
#define AMF_STREAM_VIDEO_FORMAT             L"VideoFormat"          // amf_int64(AMF_SURFACE_FORMAT); default = AMF_SURFACE_NV12 (used if AMF_STREAM_CODEC_ID == 0)
#define AMF_STREAM_VIDEO_FRAME_RATE         L"VideoFrameRate"       // AMFRate; default = (30,1) - video frame rate
#define AMF_STREAM_VIDEO_FRAME_SIZE         L"VideoFrameSize"       // AMFSize; default = (1920,1080) - video frame rate
#define AMF_STREAM_VIDEO_SURFACE_POOL       L"VideoSurfacePool"     // amf_int64; default = 5, number of allocated output surfaces
//TODO support interlaced frames

// audio stream properties
#define AMF_STREAM_AUDIO_FORMAT             L"AudioFormat"          // amf_int64(AMF_AUDIO_FORMAT); default = AMFAF_S16
#define AMF_STREAM_AUDIO_SAMPLE_RATE        L"AudioSampleRate"      // amf_int64; default = 48000
#define AMF_STREAM_AUDIO_CHANNELS           L"AudioChannels"        // amf_int64; default = 2
#define AMF_STREAM_AUDIO_CHANNEL_LAYOUT     L"AudioChannelLayout"   // amf_int64 (default = codec->channel_layout)
#define AMF_STREAM_AUDIO_BLOCK_ALIGN        L"AudioBlockAlign"      // amf_int64 (default = codec->block_align)
#define AMF_STREAM_AUDIO_FRAME_SIZE         L"AudioFrameSize"       // amf_int64 (default = codec->frame_size)


#endif //#ifndef AMF_Component_h
