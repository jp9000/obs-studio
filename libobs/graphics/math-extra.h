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

/*
 * A few general math functions that I couldn't really decide where to put.
 *
 *   Polar/Cart conversion, torque functions (for smooth movement), percentage,
 * random floats.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct vec2;
struct vec3;

EXPORT void polar_to_cart(struct vec3 *dst, const struct vec3 *v);
EXPORT void cart_to_polar(struct vec3 *dst, const struct vec3 *v);

EXPORT void norm_to_polar(struct vec2 *dst, const struct vec3 *norm);
EXPORT void polar_to_norm(struct vec3 *dst, const struct vec2 *polar);

EXPORT float calc_torquef(float val1, float val2, float torque,
			  float min_adjust, float t);

EXPORT void calc_torque(struct vec3 *dst, const struct vec3 *v1,
			const struct vec3 *v2, float torque, float min_adjust,
			float t);

static inline float get_percentage(float start, float end, float mid)
{
	return (mid - start) / (end - start);
}

static inline float get_percentagei(int start, int end, int mid)
{
	return (float)(mid - start) / (float)(end - start);
}

EXPORT float rand_float(int positive_only);

#ifdef __cplusplus
}
#endif
