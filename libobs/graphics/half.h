/******************************************************************************
    Copyright (C) 2020 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

/******************************************************************************
                               The MIT License (MIT)

Copyright (c) 2011-2019 Microsoft Corp

Permission is hereby granted, free of charge, to any person obtaining a copy of this 
software and associated documentation files (the "Software"), to deal in the Software 
without restriction, including without limitation the rights to use, copy, modify, 
merge, publish, distribute, sublicense, and/or sell copies of the Software, and to 
permit persons to whom the Software is furnished to do so, subject to the following 
conditions: 

The above copyright notice and this permission notice shall be included in all copies 
or substantial portions of the Software.  

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF 
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE 
OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

#pragma once

#include "math-defs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct half {
	uint16_t u;
};

/* adapted from DirectXMath XMConvertFloatToHalf */
static struct half half_from_float(float f)
{
	uint32_t Result;

	uint32_t IValue;
	memcpy(&IValue, &f, sizeof(IValue));
	uint32_t Sign = (IValue & 0x80000000U) >> 16U;
	IValue = IValue & 0x7FFFFFFFU; // Hack off the sign

	if (IValue > 0x477FE000U) {
		// The number is too large to be represented as a half.  Saturate to infinity.
		if (((IValue & 0x7F800000) == 0x7F800000) &&
		    ((IValue & 0x7FFFFF) != 0)) {
			Result = 0x7FFF; // NAN
		} else {
			Result = 0x7C00U; // INF
		}
	} else if (!IValue) {
		Result = 0;
	} else {
		if (IValue < 0x38800000U) {
			// The number is too small to be represented as a normalized half.
			// Convert it to a denormalized value.
			uint32_t Shift = 113U - (IValue >> 23U);
			IValue = (0x800000U | (IValue & 0x7FFFFFU)) >> Shift;
		} else {
			// Rebias the exponent to represent the value as a normalized half.
			IValue += 0xC8000000U;
		}

		Result = ((IValue + 0x0FFFU + ((IValue >> 13U) & 1U)) >> 13U) &
			 0x7FFFU;
	}

	struct half h;
	h.u = (uint16_t)(Result | Sign);
	return h;
}

static struct half half_from_bits(uint16_t u)
{
	struct half h;
	h.u = u;
	return h;
}

#ifdef __cplusplus
}
#endif
