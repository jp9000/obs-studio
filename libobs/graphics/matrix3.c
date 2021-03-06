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

#include <string.h>
#include "matrix3.h"
#include "matrix4.h"
#include "plane.h"
#include "quat.h"

void matrix3_from_quat(struct matrix3 *dst, const struct quat *q)
{
	float norm = quat_dot(q, q);
	float s = (norm > 0.0f) ? (2.0f / norm) : 0.0f;

	float xx = q->x * q->x * s;
	float yy = q->y * q->y * s;
	float zz = q->z * q->z * s;
	float xy = q->x * q->y * s;
	float xz = q->x * q->z * s;
	float yz = q->y * q->z * s;
	float wx = q->w * q->x * s;
	float wy = q->w * q->y * s;
	float wz = q->w * q->z * s;

	vec3_set(&dst->x, 1.0f - (yy + zz), xy + wz, xz - wy);
	vec3_set(&dst->y, xy - wz, 1.0f - (xx + zz), yz + wx);
	vec3_set(&dst->z, xz + wy, yz - wx, 1.0f - (xx + yy));
	vec3_zero(&dst->t);
}

void matrix3_from_axisang(struct matrix3 *dst, const struct axisang *aa)
{
	struct quat q;
	quat_from_axisang(&q, aa);
	matrix3_from_quat(dst, &q);
}

void matrix3_from_matrix4(struct matrix3 *dst, const struct matrix4 *m)
{
	dst->x.m = m->x.m;
	dst->y.m = m->y.m;
	dst->z.m = m->z.m;
	dst->t.m = m->t.m;
	dst->x.w = 0.0f;
	dst->y.w = 0.0f;
	dst->z.w = 0.0f;
	dst->t.w = 0.0f;
}

void matrix3_mul(struct matrix3 *dst, const struct matrix3 *m1,
		 const struct matrix3 *m2)
{
	if (dst == m2) {
		struct matrix3 temp;
		vec3_rotate(&temp.x, &m1->x, m2);
		vec3_rotate(&temp.y, &m1->y, m2);
		vec3_rotate(&temp.z, &m1->z, m2);
		vec3_transform3x4(&temp.t, &m1->t, m2);
		matrix3_copy(dst, &temp);
	} else {
		vec3_rotate(&dst->x, &m1->x, m2);
		vec3_rotate(&dst->y, &m1->y, m2);
		vec3_rotate(&dst->z, &m1->z, m2);
		vec3_transform3x4(&dst->t, &m1->t, m2);
	}
}

void matrix3_rotate(struct matrix3 *dst, const struct matrix3 *m,
		    const struct quat *q)
{
	struct matrix3 temp;
	matrix3_from_quat(&temp, q);
	matrix3_mul(dst, m, &temp);
}

void matrix3_rotate_aa(struct matrix3 *dst, const struct matrix3 *m,
		       const struct axisang *aa)
{
	struct matrix3 temp;
	matrix3_from_axisang(&temp, aa);
	matrix3_mul(dst, m, &temp);
}

void matrix3_scale(struct matrix3 *dst, const struct matrix3 *m,
		   const struct vec3 *v)
{
	vec3_mul(&dst->x, &m->x, v);
	vec3_mul(&dst->y, &m->y, v);
	vec3_mul(&dst->z, &m->z, v);
	vec3_mul(&dst->t, &m->t, v);
}

void matrix3_transpose(struct matrix3 *dst, const struct matrix3 *m)
{
	__m128 tmp1, tmp2;
	vec3_rotate(&dst->t, &m->t, m);
	vec3_neg(&dst->t, &dst->t);

	tmp1 = _mm_movelh_ps(m->x.m, m->y.m);
	tmp2 = _mm_movehl_ps(m->y.m, m->x.m);
	dst->x.m = _mm_shuffle_ps(tmp1, m->z.m, _MM_SHUFFLE(3, 0, 2, 0));
	dst->y.m = _mm_shuffle_ps(tmp1, m->z.m, _MM_SHUFFLE(3, 1, 3, 1));
	dst->z.m = _mm_shuffle_ps(tmp2, m->z.m, _MM_SHUFFLE(3, 2, 2, 0));
}

void matrix3_inv(struct matrix3 *dst, const struct matrix3 *m)
{
	struct matrix4 m4;
	matrix4_from_matrix3(&m4, m);
	matrix4_inv((struct matrix4 *)dst, &m4);
	dst->t.w = 0.0f;
}

void matrix3_mirror(struct matrix3 *dst, const struct matrix3 *m,
		    const struct plane *p)
{
	vec3_mirrorv(&dst->x, &m->x, &p->dir);
	vec3_mirrorv(&dst->y, &m->y, &p->dir);
	vec3_mirrorv(&dst->z, &m->z, &p->dir);
	vec3_mirror(&dst->t, &m->t, p);
}

void matrix3_mirrorv(struct matrix3 *dst, const struct matrix3 *m,
		     const struct vec3 *v)
{
	vec3_mirrorv(&dst->x, &m->x, v);
	vec3_mirrorv(&dst->y, &m->y, v);
	vec3_mirrorv(&dst->z, &m->z, v);
	vec3_mirrorv(&dst->t, &m->t, v);
}
