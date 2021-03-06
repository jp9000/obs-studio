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

struct vec2 {
	union {
		struct {
			float x, y;
		};
		float ptr[2];
	};
};

static inline void vec2_zero(struct vec2 *dst)
{
	dst->x = 0.0f;
	dst->y = 0.0f;
}

static inline void vec2_set(struct vec2 *dst, float x, float y)
{
	dst->x = x;
	dst->y = y;
}

static inline void vec2_copy(struct vec2 *dst, const struct vec2 *v)
{
	dst->x = v->x;
	dst->y = v->y;
}

static inline void vec2_add(struct vec2 *dst, const struct vec2 *v1,
			    const struct vec2 *v2)
{
	vec2_set(dst, v1->x + v2->x, v1->y + v2->y);
}

static inline void vec2_sub(struct vec2 *dst, const struct vec2 *v1,
			    const struct vec2 *v2)
{
	vec2_set(dst, v1->x - v2->x, v1->y - v2->y);
}

static inline void vec2_mul(struct vec2 *dst, const struct vec2 *v1,
			    const struct vec2 *v2)
{
	vec2_set(dst, v1->x * v2->x, v1->y * v2->y);
}

static inline void vec2_div(struct vec2 *dst, const struct vec2 *v1,
			    const struct vec2 *v2)
{
	vec2_set(dst, v1->x / v2->x, v1->y / v2->y);
}

static inline void vec2_addf(struct vec2 *dst, const struct vec2 *v, float f)
{
	vec2_set(dst, v->x + f, v->y + f);
}

static inline void vec2_subf(struct vec2 *dst, const struct vec2 *v, float f)
{
	vec2_set(dst, v->x - f, v->y - f);
}

static inline void vec2_mulf(struct vec2 *dst, const struct vec2 *v, float f)
{
	vec2_set(dst, v->x * f, v->y * f);
}

static inline void vec2_divf(struct vec2 *dst, const struct vec2 *v, float f)
{
	vec2_set(dst, v->x / f, v->y / f);
}

static inline void vec2_neg(struct vec2 *dst, const struct vec2 *v)
{
	vec2_set(dst, -v->x, -v->y);
}

static inline float vec2_dot(const struct vec2 *v1, const struct vec2 *v2)
{
	return v1->x * v2->x + v1->y * v2->y;
}

static inline float vec2_len(const struct vec2 *v)
{
	return sqrtf(v->x * v->x + v->y * v->y);
}

static inline float vec2_dist(const struct vec2 *v1, const struct vec2 *v2)
{
	struct vec2 temp;
	vec2_sub(&temp, v1, v2);
	return vec2_len(&temp);
}

static inline void vec2_minf(struct vec2 *dst, const struct vec2 *v, float val)
{
	if (v->x < val)
		dst->x = val;
	if (v->y < val)
		dst->y = val;
}

static inline void vec2_min(struct vec2 *dst, const struct vec2 *v,
			    const struct vec2 *min_v)
{
	if (v->x < min_v->x)
		dst->x = min_v->x;
	if (v->y < min_v->y)
		dst->y = min_v->y;
}

static inline void vec2_maxf(struct vec2 *dst, const struct vec2 *v, float val)
{
	if (v->x > val)
		dst->x = val;
	if (v->y > val)
		dst->y = val;
}

static inline void vec2_max(struct vec2 *dst, const struct vec2 *v,
			    const struct vec2 *max_v)
{
	if (v->x > max_v->x)
		dst->x = max_v->x;
	if (v->y > max_v->y)
		dst->y = max_v->y;
}

EXPORT void vec2_abs(struct vec2 *dst, const struct vec2 *v);
EXPORT void vec2_floor(struct vec2 *dst, const struct vec2 *v);
EXPORT void vec2_ceil(struct vec2 *dst, const struct vec2 *v);
EXPORT int vec2_close(const struct vec2 *v1, const struct vec2 *v2,
		      float epsilon);
EXPORT void vec2_norm(struct vec2 *dst, const struct vec2 *v);

#ifdef __cplusplus
}
#endif
