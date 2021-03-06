/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>

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

#pragma once

#include "../util/c99defs.h"
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795f
#endif

#define RAD(val) ((val)*0.0174532925199432957692369076848f)
#define DEG(val) ((val)*57.295779513082320876798154814105f)
#define LARGE_EPSILON 1e-2f
#define EPSILON 1e-4f
#define TINY_EPSILON 1e-5f
#define M_INFINITE 3.4e38f

static inline bool close_float(float f1, float f2, float precision)
{
	return fabsf(f1 - f2) <= precision;
}

#ifdef __cplusplus
}
#endif
