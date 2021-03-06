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
#include "math-defs.h"
#include "vec3.h"

#include "../util/sse-intrin.h"

/*
 * Quaternion math
 *
 *   Generally used to represent rotational data more than anything.  Allows
 * for efficient and correct rotational interpolation without suffering from
 * things like gimbal lock.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct matrix3;
struct matrix4;
struct axisang;

struct quat {
	union {
		struct {
			float x, y, z, w;
		};
		float ptr[4];
		__m128 m;
	};
};

static inline void quat_identity(struct quat *q)
{
	q->m = _mm_setzero_ps();
	q->w = 1.0f;
}

static inline void quat_set(struct quat *dst, float x, float y, float z,
			    float w)
{
	dst->m = _mm_set_ps(x, y, z, w);
}

static inline void quat_copy(struct quat *dst, const struct quat *q)
{
	dst->m = q->m;
}

static inline void quat_add(struct quat *dst, const struct quat *q1,
			    const struct quat *q2)
{
	dst->m = _mm_add_ps(q1->m, q2->m);
}

static inline void quat_sub(struct quat *dst, const struct quat *q1,
			    const struct quat *q2)
{
	dst->m = _mm_sub_ps(q1->m, q2->m);
}

EXPORT void quat_mul(struct quat *dst, const struct quat *q1,
		     const struct quat *q2);

static inline void quat_addf(struct quat *dst, const struct quat *q, float f)
{
	dst->m = _mm_add_ps(q->m, _mm_set1_ps(f));
}

static inline void quat_subf(struct quat *dst, const struct quat *q, float f)
{
	dst->m = _mm_sub_ps(q->m, _mm_set1_ps(f));
}

static inline void quat_mulf(struct quat *dst, const struct quat *q, float f)
{
	dst->m = _mm_mul_ps(q->m, _mm_set1_ps(f));
}

static inline void quat_divf(struct quat *dst, const struct quat *q, float f)
{
	dst->m = _mm_div_ps(q->m, _mm_set1_ps(f));
}

static inline float quat_dot(const struct quat *q1, const struct quat *q2)
{
	struct vec3 add;
	__m128 mul = _mm_mul_ps(q1->m, q2->m);
	add.m = _mm_add_ps(_mm_movehl_ps(mul, mul), mul);
	add.m = _mm_add_ps(_mm_shuffle_ps(add.m, add.m, 0x55), add.m);
	return add.x;
}

static inline void quat_inv(struct quat *dst, const struct quat *q)
{
	dst->x = -q->x;
	dst->y = -q->y;
	dst->z = -q->z;
}

static inline void quat_neg(struct quat *dst, const struct quat *q)
{
	dst->x = -q->x;
	dst->y = -q->y;
	dst->z = -q->z;
	dst->w = -q->w;
}

static inline float quat_len(const struct quat *q)
{
	float dot_val = quat_dot(q, q);
	return (dot_val > 0.0f) ? sqrtf(dot_val) : 0.0f;
}

static inline float quat_dist(const struct quat *q1, const struct quat *q2)
{
	struct quat temp;
	float dot_val;

	quat_sub(&temp, q1, q2);
	dot_val = quat_dot(&temp, &temp);
	return (dot_val > 0.0f) ? sqrtf(dot_val) : 0.0f;
}

static inline void quat_norm(struct quat *dst, const struct quat *q)
{
	float dot_val = quat_dot(q, q);
	dst->m = (dot_val > 0.0f)
			 ? _mm_mul_ps(q->m, _mm_set1_ps(1.0f / sqrtf(dot_val)))
			 : _mm_setzero_ps();
}

static inline bool quat_close(const struct quat *q1, const struct quat *q2,
			      float epsilon)
{
	struct quat test;
	quat_sub(&test, q1, q2);
	return test.x < epsilon && test.y < epsilon && test.z < epsilon &&
	       test.w < epsilon;
}

EXPORT void quat_from_axisang(struct quat *dst, const struct axisang *aa);
EXPORT void quat_from_matrix3(struct quat *dst, const struct matrix3 *m);
EXPORT void quat_from_matrix4(struct quat *dst, const struct matrix4 *m);

EXPORT void quat_get_dir(struct vec3 *dst, const struct quat *q);
EXPORT void quat_set_look_dir(struct quat *dst, const struct vec3 *dir);

EXPORT void quat_log(struct quat *dst, const struct quat *q);
EXPORT void quat_exp(struct quat *dst, const struct quat *q);

EXPORT void quat_interpolate(struct quat *dst, const struct quat *q1,
			     const struct quat *q2, float t);
EXPORT void quat_get_tangent(struct quat *dst, const struct quat *prev,
			     const struct quat *q, const struct quat *next);
EXPORT void quat_interpolate_cubic(struct quat *dst, const struct quat *q1,
				   const struct quat *q2, const struct quat *m1,
				   const struct quat *m2, float t);

#ifdef __cplusplus
}
#endif
