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

#include "vec3.h"
#include "axisang.h"

/* 3x4 Matrix */

#ifdef __cplusplus
extern "C" {
#endif

struct matrix4;

struct matrix3 {
	struct vec3 x;
	struct vec3 y;
	struct vec3 z;
	struct vec3 t;
};

static inline void matrix3_copy(struct matrix3 *dst, const struct matrix3 *m)
{
	vec3_copy(&dst->x, &m->x);
	vec3_copy(&dst->y, &m->y);
	vec3_copy(&dst->z, &m->z);
	vec3_copy(&dst->t, &m->t);
}

static inline void matrix3_identity(struct matrix3 *dst)
{
	vec3_zero(&dst->x);
	vec3_zero(&dst->y);
	vec3_zero(&dst->z);
	vec3_zero(&dst->t);
	dst->x.x = dst->y.y = dst->z.z = 1.0f;
}

EXPORT void matrix3_from_quat(struct matrix3 *dst, const struct quat *q);
EXPORT void matrix3_from_axisang(struct matrix3 *dst, const struct axisang *aa);
EXPORT void matrix3_from_matrix4(struct matrix3 *dst, const struct matrix4 *m);

EXPORT void matrix3_mul(struct matrix3 *dst, const struct matrix3 *m1,
			const struct matrix3 *m2);
static inline void matrix3_translate(struct matrix3 *dst,
				     const struct matrix3 *m,
				     const struct vec3 *v)
{
	vec3_sub(&dst->t, &m->t, v);
}

EXPORT void matrix3_rotate(struct matrix3 *dst, const struct matrix3 *m,
			   const struct quat *q);
EXPORT void matrix3_rotate_aa(struct matrix3 *dst, const struct matrix3 *m,
			      const struct axisang *aa);
EXPORT void matrix3_scale(struct matrix3 *dst, const struct matrix3 *m,
			  const struct vec3 *v);
EXPORT void matrix3_transpose(struct matrix3 *dst, const struct matrix3 *m);
EXPORT void matrix3_inv(struct matrix3 *dst, const struct matrix3 *m);

EXPORT void matrix3_mirror(struct matrix3 *dst, const struct matrix3 *m,
			   const struct plane *p);
EXPORT void matrix3_mirrorv(struct matrix3 *dst, const struct matrix3 *m,
			    const struct vec3 *v);

static inline void matrix3_translate3f(struct matrix3 *dst,
				       const struct matrix3 *m, float x,
				       float y, float z)
{
	struct vec3 v;
	vec3_set(&v, x, y, z);
	matrix3_translate(dst, m, &v);
}

static inline void matrix3_rotate_aa4f(struct matrix3 *dst,
				       const struct matrix3 *m, float x,
				       float y, float z, float rot)
{
	struct axisang aa;
	axisang_set(&aa, x, y, z, rot);
	matrix3_rotate_aa(dst, m, &aa);
}

static inline void matrix3_scale3f(struct matrix3 *dst, const struct matrix3 *m,
				   float x, float y, float z)
{
	struct vec3 v;
	vec3_set(&v, x, y, z);
	matrix3_scale(dst, m, &v);
}

#ifdef __cplusplus
}
#endif
