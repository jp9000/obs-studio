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

#ifndef AMF_CurrentTime_h
#define AMF_CurrentTime_h

#include "public/include/core/Platform.h"
#include "public/include/core/Interface.h"

namespace amf
{
	// Current time interface class. This interface object can be passed
	// as a property to components requiring synchronized timing. The
	// implementation is:
	// - first call to Get() starts time and returns 0
	// - subsequent calls to Get() returns values relative to 0
	// - Reset() puts time back at 0 at next Get() call
	//
	class AMF_NO_VTABLE AMFCurrentTime : public AMFInterface
	{
	public:
		virtual amf_pts AMF_STD_CALL Get() = 0;

		virtual void AMF_STD_CALL Reset() = 0;
	};

	//----------------------------------------------------------------------------------------------
	// smart pointer
	//----------------------------------------------------------------------------------------------
	typedef AMFInterfacePtr_T<AMFCurrentTime> AMFCurrentTimePtr;
	//----------------------------------------------------------------------------------------------}
}
#endif // AMF_CurrentTime_h
