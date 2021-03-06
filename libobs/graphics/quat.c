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

#include "quat.h"
#include "vec3.h"
#include "matrix3.h"
#include "matrix4.h"
#include "axisang.h"

static inline void quat_vec3(struct vec3 *v, const struct quat *q)
{
	v->m = q->m;
	v->w = 0.0f;
}

void quat_mul(struct quat *dst, const struct quat *q1, const struct quat *q2)
{
	struct vec3 q1axis, q2axis;
	struct vec3 temp1, temp2;

	quat_vec3(&q1axis, q1);
	quat_vec3(&q2axis, q2);

	vec3_mulf(&temp1, &q2axis, q1->w);
	vec3_mulf(&temp2, &q1axis, q2->w);
	vec3_add(&temp1, &temp1, &temp2);
	vec3_cross(&temp2, &q1axis, &q2axis);
	vec3_add((struct vec3 *)dst, &temp1, &temp2);

	dst->w = (q1->w * q2->w) - vec3_dot(&q1axis, &q2axis);
}

void quat_from_axisang(struct quat *dst, const struct axisang *aa)
{
	float halfa = aa->w * 0.5f;
	float sine = sinf(halfa);

	dst->x = aa->x * sine;
	dst->y = aa->y * sine;
	dst->z = aa->z * sine;
	dst->w = cosf(halfa);
}

struct f4x4 {
	float ptr[4][4];
};

void quat_from_matrix3(struct quat *dst, const struct matrix3 *m)
{
	quat_from_matrix4(dst, (const struct matrix4 *)m);
}

void quat_from_matrix4(struct quat *dst, const struct matrix4 *m)
{
	float tr = (m->x.x + m->y.y + m->z.z);
	float inv_half;
	float four_d;
	int i, j, k;

	if (tr > 0.0f) {
		four_d = sqrtf(tr + 1.0f);
		dst->w = four_d * 0.5f;

		inv_half = 0.5f / four_d;
		dst->x = (m->y.z - m->z.y) * inv_half;
		dst->y = (m->z.x - m->x.z) * inv_half;
		dst->z = (m->x.y - m->y.x) * inv_half;
	} else {
		struct f4x4 *val = (struct f4x4 *)m;

		i = (m->x.x > m->y.y) ? 0 : 1;

		if (m->z.z > val->ptr[i][i])
			i = 2;

		j = (i + 1) % 3;
		k = (i + 2) % 3;

		/* ---------------------------------- */

		four_d = sqrtf(
			(val->ptr[i][i] - val->ptr[j][j] - val->ptr[k][k]) +
			1.0f);

		dst->ptr[i] = four_d * 0.5f;

		inv_half = 0.5f / four_d;
		dst->ptr[j] = (val->ptr[i][j] + val->ptr[j][i]) * inv_half;
		dst->ptr[k] = (val->ptr[i][k] + val->ptr[k][i]) * inv_half;
		dst->w = (val->ptr[j][k] - val->ptr[k][j]) * inv_half;
	}
}

void quat_get_dir(struct vec3 *dst, const struct quat *q)
{
	struct matrix3 m;
	matrix3_from_quat(&m, q);
	vec3_copy(dst, &m.z);
}

void quat_set_look_dir(struct quat *dst, const struct vec3 *dir)
{
	struct vec3 new_dir;
	struct quat xz_rot, yz_rot;
	bool xz_valid;
	bool yz_valid;
	struct axisang aa;

	vec3_norm(&new_dir, dir);
	vec3_neg(&new_dir, &new_dir);

	quat_identity(&xz_rot);
	quat_identity(&yz_rot);

	xz_valid = close_float(new_dir.x, 0.0f, EPSILON) ||
		   close_float(new_dir.z, 0.0f, EPSILON);
	yz_valid = close_float(new_dir.y, 0.0f, EPSILON);

	if (xz_valid) {
		axisang_set(&aa, 0.0f, 1.0f, 0.0f,
			    atan2f(new_dir.x, new_dir.z));

		quat_from_axisang(&xz_rot, &aa);
	}
	if (yz_valid) {
		axisang_set(&aa, -1.0f, 0.0f, 0.0f, asinf(new_dir.y));
		quat_from_axisang(&yz_rot, &aa);
	}

	if (!xz_valid)
		quat_copy(dst, &yz_rot);
	else if (!yz_valid)
		quat_copy(dst, &xz_rot);
	else
		quat_mul(dst, &xz_rot, &yz_rot);
}

void quat_log(struct quat *dst, const struct quat *q)
{
	float angle = acosf(q->w);
	float sine = sinf(angle);
	float w = q->w;

	quat_copy(dst, q);
	dst->w = 0.0f;

	if ((fabsf(w) < 1.0f) && (fabsf(sine) >= EPSILON)) {
		sine = angle / sine;
		quat_mulf(dst, dst, sine);
	}
}

void quat_exp(struct quat *dst, const struct quat *q)
{
	float length = sqrtf(q->x * q->x + q->y * q->y + q->z * q->z);
	float sine = sinf(length);

	quat_copy(dst, q);
	sine = (length > EPSILON) ? (sine / length) : 1.0f;
	quat_mulf(dst, dst, sine);
	dst->w = cosf(length);
}

void quat_interpolate(struct quat *dst, const struct quat *q1,
		      const struct quat *q2, float t)
{
	float dot = quat_dot(q1, q2);
	float anglef = acosf(dot);
	float sine, sinei, sinet, sineti;
	struct quat temp;

	if (anglef >= EPSILON) {
		sine = sinf(anglef);
		sinei = 1 / sine;
		sinet = sinf(anglef * t) * sinei;
		sineti = sinf(anglef * (1.0f - t)) * sinei;

		quat_mulf(&temp, q1, sineti);
		quat_mulf(dst, q2, sinet);
		quat_add(dst, &temp, dst);
	} else {
		quat_sub(&temp, q2, q1);
		quat_mulf(&temp, &temp, t);
		quat_add(dst, &temp, q1);
	}
}

void quat_get_tangent(struct quat *dst, const struct quat *prev,
		      const struct quat *q, const struct quat *next)
{
	struct quat temp;

	quat_sub(&temp, q, prev);
	quat_add(&temp, &temp, next);
	quat_sub(&temp, &temp, q);
	quat_mulf(dst, &temp, 0.5f);
}

void quat_interpolate_cubic(struct quat *dst, const struct quat *q1,
			    const struct quat *q2, const struct quat *m1,
			    const struct quat *m2, float t)
{
	struct quat temp1, temp2;

	quat_interpolate(&temp1, q1, q2, t);
	quat_interpolate(&temp2, m1, m2, t);
	quat_interpolate(dst, &temp1, &temp2, 2.0f * (1.0f - t) * t);
}
