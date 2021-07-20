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

#ifndef AMF_Interface_h
#define AMF_Interface_h
#pragma once

#include "Result.h"

#if defined(__cplusplus)
namespace amf
{
#endif
#if defined(__cplusplus)
    #define AMF_DECLARE_IID(_data1, _data2, _data3, _data41, _data42, _data43, _data44, _data45, _data46, _data47, _data48) \
        static AMF_INLINE const amf::AMFGuid IID() \
        { \
            amf::AMFGuid uid = {_data1, _data2, _data3, _data41, _data42, _data43, _data44, _data45, _data46, _data47, _data48}; \
            return uid; \
        }
#else
#define AMF_DECLARE_IID(name, _data1, _data2, _data3, _data41, _data42, _data43, _data44, _data45, _data46, _data47, _data48) \
        AMF_INLINE static const AMFGuid IID_##name(void) \
        { \
            AMFGuid uid = {_data1, _data2, _data3, _data41, _data42, _data43, _data44, _data45, _data46, _data47, _data48}; \
            return uid; \
        }
#endif

    //------------------------------------------------------------------------
    // AMFInterface interface  - base class for all AMF interfaces
    //------------------------------------------------------------------------
#if defined(__cplusplus)
    class AMF_NO_VTABLE AMFInterface
    {
    public:
        AMF_DECLARE_IID(0x9d872f34, 0x90dc, 0x4b93, 0xb6, 0xb2, 0x6c, 0xa3, 0x7c, 0x85, 0x25, 0xdb)

        virtual amf_long            AMF_STD_CALL Acquire() = 0;
        virtual amf_long            AMF_STD_CALL Release() = 0;
        virtual AMF_RESULT          AMF_STD_CALL QueryInterface(const AMFGuid& interfaceID, void** ppInterface) = 0;
    };
#else
    AMF_DECLARE_IID(AMFInterface, 0x9d872f34, 0x90dc, 0x4b93, 0xb6, 0xb2, 0x6c, 0xa3, 0x7c, 0x85, 0x25, 0xdb)
    typedef struct AMFInterface AMFInterface;

    typedef struct AMFInterfaceVtbl
    {
        // AMFInterface interface
        amf_long            (AMF_STD_CALL *Acquire)(AMFInterface* pThis);
        amf_long            (AMF_STD_CALL *Release)(AMFInterface* pThis);
        enum AMF_RESULT     (AMF_STD_CALL *QueryInterface)(AMFInterface* pThis, const struct AMFGuid *interfaceID, void** ppInterface);
    } AMFInterfaceVtbl;

    struct AMFInterface
    {
        const AMFInterfaceVtbl *pVtbl;
    };
#endif
    //------------------------------------------------------------------------
    // template for AMF smart pointer
    //------------------------------------------------------------------------
#if defined(__cplusplus)
    template<class _Interf>
    class AMFInterfacePtr_T
    {
    private:
        _Interf* m_pInterf;

        void InternalAcquire()
        {
            if(m_pInterf != NULL)
            {
                m_pInterf->Acquire();
            }
        }
        void InternalRelease()
        {
            if(m_pInterf != NULL)
            {
                m_pInterf->Release();
            }
        }
    public:
        AMFInterfacePtr_T() : m_pInterf(NULL)
        {}

        AMFInterfacePtr_T(const AMFInterfacePtr_T<_Interf>& p) : m_pInterf(p.m_pInterf)
        {
            InternalAcquire();
        }

        AMFInterfacePtr_T(_Interf* pInterface) : m_pInterf(pInterface)
        {
            InternalAcquire();
        }

        template<class _OtherInterf>
        explicit AMFInterfacePtr_T(const AMFInterfacePtr_T<_OtherInterf>& cp) : m_pInterf(NULL)
        {
            void* pInterf = NULL;
            if((cp == NULL) || (cp->QueryInterface(_Interf::IID(), &pInterf) != AMF_OK))
            {
                pInterf = NULL;
            }
            m_pInterf = static_cast<_Interf*>(pInterf);
        }

        template<class _OtherInterf>
        explicit AMFInterfacePtr_T(_OtherInterf* cp) : m_pInterf(NULL)
        {
            void* pInterf = NULL;
            if((cp == NULL) || (cp->QueryInterface(_Interf::IID(), &pInterf) != AMF_OK))
            {
                pInterf = NULL;
            }
            m_pInterf = static_cast<_Interf*>(pInterf);
        }

        ~AMFInterfacePtr_T()
        {
            InternalRelease();
        }

        AMFInterfacePtr_T& operator=(_Interf* pInterface)
        {
            if(m_pInterf != pInterface)
            {
                _Interf* pOldInterface = m_pInterf;
                m_pInterf = pInterface;
                InternalAcquire();
                if(pOldInterface != NULL)
                {
                    pOldInterface->Release();
                }
            }
            return *this;
        }

        AMFInterfacePtr_T& operator=(const AMFInterfacePtr_T<_Interf>& cp)
        {
            return operator=(cp.m_pInterf);
        }

        void Attach(_Interf* pInterface)
        {
            InternalRelease();
            m_pInterf = pInterface;
        }

        _Interf* Detach()
        {
            _Interf* const pOld = m_pInterf;
            m_pInterf = NULL;
            return pOld;
        }
        void Release()
        {
            InternalRelease();
            m_pInterf = NULL;
        }

        operator _Interf*() const
        {
            return m_pInterf;
        }

        _Interf& operator*() const
        {
            return *m_pInterf;
        }

        // Returns the address of the interface pointer contained in this
        // class. This is required for initializing from C-style factory function to
        // avoid getting an incorrect ref count at the beginning.

        _Interf** operator&()
        {
            InternalRelease();
            m_pInterf = 0;
            return &m_pInterf;
        }

        _Interf* operator->() const
        {
            return m_pInterf;
        }

        bool operator==(const AMFInterfacePtr_T<_Interf>& p)
        {
            return (m_pInterf == p.m_pInterf);
        }

        bool operator==(_Interf* p)
        {
            return (m_pInterf == p);
        }

        bool operator!=(const AMFInterfacePtr_T<_Interf>& p)
        {
            return !(operator==(p));
        }
        bool operator!=(_Interf* p)
        {
            return !(operator==(p));
        }

        _Interf* GetPtr()
        {
            return m_pInterf;
        }

        const _Interf* GetPtr() const
        {
            return m_pInterf;
        }
    };
    //----------------------------------------------------------------------------------------------
    // smart pointer
    //----------------------------------------------------------------------------------------------
    typedef AMFInterfacePtr_T<AMFInterface> AMFInterfacePtr;
    //----------------------------------------------------------------------------------------------
#endif

#if defined(__cplusplus)
}
#endif

#endif //#ifndef AMF_Interface_h
