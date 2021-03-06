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

#include "math-defs.h"
#include "vec4.h"

#include "../util/sse-intrin.h"

#ifdef __cplusplus
extern "C" {
#endif

struct plane;
struct matrix3;
struct matrix4;
struct quat;

struct vec3 {
	union {
		struct {
			float x, y, z, w;
		};
		float ptr[4];
		__m128 m;
	};
};

static inline void vec3_zero(struct vec3 *v)
{
	v->m = _mm_setzero_ps();
}

static inline void vec3_set(struct vec3 *dst, float x, float y, float z)
{
	dst->m = _mm_set_ps(0.0f, z, y, x);
}

static inline void vec3_copy(struct vec3 *dst, const struct vec3 *v)
{
	dst->m = v->m;
}

EXPORT void vec3_from_vec4(struct vec3 *dst, const struct vec4 *v);

static inline void vec3_add(struct vec3 *dst, const struct vec3 *v1,
			    const struct vec3 *v2)
{
	dst->m = _mm_add_ps(v1->m, v2->m);
	dst->w = 0.0f;
}

static inline void vec3_sub(struct vec3 *dst, const struct vec3 *v1,
			    const struct vec3 *v2)
{
	dst->m = _mm_sub_ps(v1->m, v2->m);
	dst->w = 0.0f;
}

static inline void vec3_mul(struct vec3 *dst, const struct vec3 *v1,
			    const struct vec3 *v2)
{
	dst->m = _mm_mul_ps(v1->m, v2->m);
}

static inline void vec3_div(struct vec3 *dst, const struct vec3 *v1,
			    const struct vec3 *v2)
{
	dst->m = _mm_div_ps(v1->m, v2->m);
	dst->w = 0.0f;
}

static inline void vec3_addf(struct vec3 *dst, const struct vec3 *v, float f)
{
	dst->m = _mm_add_ps(v->m, _mm_set1_ps(f));
	dst->w = 0.0f;
}

static inline void vec3_subf(struct vec3 *dst, const struct vec3 *v, float f)
{
	dst->m = _mm_sub_ps(v->m, _mm_set1_ps(f));
	dst->w = 0.0f;
}

static inline void vec3_mulf(struct vec3 *dst, const struct vec3 *v, float f)
{
	dst->m = _mm_mul_ps(v->m, _mm_set1_ps(f));
}

static inline void vec3_divf(struct vec3 *dst, const struct vec3 *v, float f)
{
	dst->m = _mm_div_ps(v->m, _mm_set1_ps(f));
	dst->w = 0.0f;
}

static inline float vec3_dot(const struct vec3 *v1, const struct vec3 *v2)
{
	struct vec3 add;
	__m128 mul = _mm_mul_ps(v1->m, v2->m);
	add.m = _mm_add_ps(_mm_movehl_ps(mul, mul), mul);
	add.m = _mm_add_ps(_mm_shuffle_ps(add.m, add.m, 0x55), add.m);
	return add.x;
}

static inline void vec3_cross(struct vec3 *dst, const struct vec3 *v1,
			      const struct vec3 *v2)
{
	__m128 s1v1 = _mm_shuffle_ps(v1->m, v1->m, _MM_SHUFFLE(3, 0, 2, 1));
	__m128 s1v2 = _mm_shuffle_ps(v2->m, v2->m, _MM_SHUFFLE(3, 1, 0, 2));
	__m128 s2v1 = _mm_shuffle_ps(v1->m, v1->m, _MM_SHUFFLE(3, 1, 0, 2));
	__m128 s2v2 = _mm_shuffle_ps(v2->m, v2->m, _MM_SHUFFLE(3, 0, 2, 1));
	dst->m = _mm_sub_ps(_mm_mul_ps(s1v1, s1v2), _mm_mul_ps(s2v1, s2v2));
}

static inline void vec3_neg(struct vec3 *dst, const struct vec3 *v)
{
	dst->x = -v->x;
	dst->y = -v->y;
	dst->z = -v->z;
	dst->w = 0.0f;
}

static inline float vec3_len(const struct vec3 *v)
{
	float dot_val = vec3_dot(v, v);
	return (dot_val > 0.0f) ? sqrtf(dot_val) : 0.0f;
}

static inline float vec3_dist(const struct vec3 *v1, const struct vec3 *v2)
{
	struct vec3 temp;
	float dot_val;

	vec3_sub(&temp, v1, v2);
	dot_val = vec3_dot(&temp, &temp);
	return (dot_val > 0.0f) ? sqrtf(dot_val) : 0.0f;
}

static inline void vec3_norm(struct vec3 *dst, const struct vec3 *v)
{
	float dot_val = vec3_dot(v, v);
	dst->m = (dot_val > 0.0f)
			 ? _mm_mul_ps(v->m, _mm_set1_ps(1.0f / sqrtf(dot_val)))
			 : _mm_setzero_ps();
}

static inline bool vec3_close(const struct vec3 *v1, const struct vec3 *v2,
			      float epsilon)
{
	struct vec3 test;
	vec3_sub(&test, v1, v2);
	return test.x < epsilon && test.y < epsilon && test.z < epsilon;
}

static inline void vec3_min(struct vec3 *dst, const struct vec3 *v1,
			    const struct vec3 *v2)
{
	dst->m = _mm_min_ps(v1->m, v2->m);
	dst->w = 0.0f;
}

static inline void vec3_minf(struct vec3 *dst, const struct vec3 *v, float f)
{
	dst->m = _mm_min_ps(v->m, _mm_set1_ps(f));
	dst->w = 0.0f;
}

static inline void vec3_max(struct vec3 *dst, const struct vec3 *v1,
			    const struct vec3 *v2)
{
	dst->m = _mm_max_ps(v1->m, v2->m);
	dst->w = 0.0f;
}

static inline void vec3_maxf(struct vec3 *dst, const struct vec3 *v, float f)
{
	dst->m = _mm_max_ps(v->m, _mm_set1_ps(f));
	dst->w = 0.0f;
}

static inline void vec3_abs(struct vec3 *dst, const struct vec3 *v)
{
	dst->x = fabsf(v->x);
	dst->y = fabsf(v->y);
	dst->z = fabsf(v->z);
	dst->w = 0.0f;
}

static inline void vec3_floor(struct vec3 *dst, const struct vec3 *v)
{
	dst->x = floorf(v->x);
	dst->y = floorf(v->y);
	dst->z = floorf(v->z);
	dst->w = 0.0f;
}

static inline void vec3_ceil(struct vec3 *dst, const struct vec3 *v)
{
	dst->x = ceilf(v->x);
	dst->y = ceilf(v->y);
	dst->z = ceilf(v->z);
	dst->w = 0.0f;
}

EXPORT float vec3_plane_dist(const struct vec3 *v, const struct plane *p);

EXPORT void vec3_transform(struct vec3 *dst, const struct vec3 *v,
			   const struct matrix4 *m);

EXPORT void vec3_rotate(struct vec3 *dst, const struct vec3 *v,
			const struct matrix3 *m);
EXPORT void vec3_transform3x4(struct vec3 *dst, const struct vec3 *v,
			      const struct matrix3 *m);

EXPORT void vec3_mirror(struct vec3 *dst, const struct vec3 *v,
			const struct plane *p);
EXPORT void vec3_mirrorv(struct vec3 *dst, const struct vec3 *v,
			 const struct vec3 *vec);

EXPORT void vec3_rand(struct vec3 *dst, int positive_only);

#ifdef __cplusplus
}
#endif
