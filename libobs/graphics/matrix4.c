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

#include "math-defs.h"
#include "matrix4.h"
#include "matrix3.h"
#include "quat.h"

void matrix4_from_matrix3(struct matrix4 *dst, const struct matrix3 *m)
{
	dst->x.m = m->x.m;
	dst->y.m = m->y.m;
	dst->z.m = m->z.m;
	dst->t.m = m->t.m;
	dst->t.w = 1.0f;
}

void matrix4_from_quat(struct matrix4 *dst, const struct quat *q)
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

	vec4_set(&dst->x, 1.0f - (yy + zz), xy + wz, xz - wy, 0.0f);
	vec4_set(&dst->y, xy - wz, 1.0f - (xx + zz), yz + wx, 0.0f);
	vec4_set(&dst->z, xz + wy, yz - wx, 1.0f - (xx + yy), 0.0f);
	vec4_set(&dst->t, 0.0f, 0.0f, 0.0f, 1.0f);
}

void matrix4_from_axisang(struct matrix4 *dst, const struct axisang *aa)
{
	struct quat q;
	quat_from_axisang(&q, aa);
	matrix4_from_quat(dst, &q);
}

void matrix4_mul(struct matrix4 *dst, const struct matrix4 *m1,
		 const struct matrix4 *m2)
{
	const struct vec4 *m1v = (const struct vec4 *)m1;
	const float *m2f = (const float *)m2;
	struct vec4 out[4];
	int i, j;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			struct vec4 temp;
			vec4_set(&temp, m2f[j], m2f[j + 4], m2f[j + 8],
				 m2f[j + 12]);
			out[i].ptr[j] = vec4_dot(&m1v[i], &temp);
		}
	}

	matrix4_copy(dst, (struct matrix4 *)out);
}

static inline void get_3x3_submatrix(float *dst, const struct matrix4 *m, int i,
				     int j)
{
	const float *mf = (const float *)m;
	int ti, tj, idst, jdst;

	for (ti = 0; ti < 4; ti++) {
		if (ti < i)
			idst = ti;
		else if (ti > i)
			idst = ti - 1;
		else
			continue;

		for (tj = 0; tj < 4; tj++) {
			if (tj < j)
				jdst = tj;
			else if (tj > j)
				jdst = tj - 1;
			else
				continue;

			dst[(idst * 3) + jdst] = mf[(ti * 4) + tj];
		}
	}
}

static inline float get_3x3_determinant(const float *m)
{
	return (m[0] * ((m[4] * m[8]) - (m[7] * m[5]))) -
	       (m[1] * ((m[3] * m[8]) - (m[6] * m[5]))) +
	       (m[2] * ((m[3] * m[7]) - (m[6] * m[4])));
}

float matrix4_determinant(const struct matrix4 *m)
{
	const float *mf = (const float *)m;
	float det, result = 0.0f, i = 1.0f;
	float m3x3[9];
	int n;

	for (n = 0; n < 4; n++, i = -i) { // NOLINT(clang-tidy-cert-flp30-c)
		get_3x3_submatrix(m3x3, m, 0, n);

		det = get_3x3_determinant(m3x3);
		result += mf[n] * det * i;
	}

	return result;
}

void matrix4_translate3v(struct matrix4 *dst, const struct matrix4 *m,
			 const struct vec3 *v)
{
	struct matrix4 temp;
	vec4_set(&temp.x, 1.0f, 0.0f, 0.0f, 0.0f);
	vec4_set(&temp.y, 0.0f, 1.0f, 0.0f, 0.0f);
	vec4_set(&temp.z, 0.0f, 0.0f, 1.0f, 0.0f);
	vec4_from_vec3(&temp.t, v);

	matrix4_mul(dst, m, &temp);
}

void matrix4_translate4v(struct matrix4 *dst, const struct matrix4 *m,
			 const struct vec4 *v)
{
	struct matrix4 temp;
	vec4_set(&temp.x, 1.0f, 0.0f, 0.0f, 0.0f);
	vec4_set(&temp.y, 0.0f, 1.0f, 0.0f, 0.0f);
	vec4_set(&temp.z, 0.0f, 0.0f, 1.0f, 0.0f);
	vec4_copy(&temp.t, v);

	matrix4_mul(dst, m, &temp);
}

void matrix4_rotate(struct matrix4 *dst, const struct matrix4 *m,
		    const struct quat *q)
{
	struct matrix4 temp;
	matrix4_from_quat(&temp, q);
	matrix4_mul(dst, m, &temp);
}

void matrix4_rotate_aa(struct matrix4 *dst, const struct matrix4 *m,
		       const struct axisang *aa)
{
	struct matrix4 temp;
	matrix4_from_axisang(&temp, aa);
	matrix4_mul(dst, m, &temp);
}

void matrix4_scale(struct matrix4 *dst, const struct matrix4 *m,
		   const struct vec3 *v)
{
	struct matrix4 temp;
	vec4_set(&temp.x, v->x, 0.0f, 0.0f, 0.0f);
	vec4_set(&temp.y, 0.0f, v->y, 0.0f, 0.0f);
	vec4_set(&temp.z, 0.0f, 0.0f, v->z, 0.0f);
	vec4_set(&temp.t, 0.0f, 0.0f, 0.0f, 1.0f);
	matrix4_mul(dst, m, &temp);
}

void matrix4_translate3v_i(struct matrix4 *dst, const struct vec3 *v,
			   const struct matrix4 *m)
{
	struct matrix4 temp;
	vec4_set(&temp.x, 1.0f, 0.0f, 0.0f, 0.0f);
	vec4_set(&temp.y, 0.0f, 1.0f, 0.0f, 0.0f);
	vec4_set(&temp.z, 0.0f, 0.0f, 1.0f, 0.0f);
	vec4_from_vec3(&temp.t, v);

	matrix4_mul(dst, &temp, m);
}

void matrix4_translate4v_i(struct matrix4 *dst, const struct vec4 *v,
			   const struct matrix4 *m)
{
	struct matrix4 temp;
	vec4_set(&temp.x, 1.0f, 0.0f, 0.0f, 0.0f);
	vec4_set(&temp.y, 0.0f, 1.0f, 0.0f, 0.0f);
	vec4_set(&temp.z, 0.0f, 0.0f, 1.0f, 0.0f);
	vec4_copy(&temp.t, v);

	matrix4_mul(dst, &temp, m);
}

void matrix4_rotate_i(struct matrix4 *dst, const struct quat *q,
		      const struct matrix4 *m)
{
	struct matrix4 temp;
	matrix4_from_quat(&temp, q);
	matrix4_mul(dst, &temp, m);
}

void matrix4_rotate_aa_i(struct matrix4 *dst, const struct axisang *aa,
			 const struct matrix4 *m)
{
	struct matrix4 temp;
	matrix4_from_axisang(&temp, aa);
	matrix4_mul(dst, &temp, m);
}

void matrix4_scale_i(struct matrix4 *dst, const struct vec3 *v,
		     const struct matrix4 *m)
{
	struct matrix4 temp;
	vec4_set(&temp.x, v->x, 0.0f, 0.0f, 0.0f);
	vec4_set(&temp.y, 0.0f, v->y, 0.0f, 0.0f);
	vec4_set(&temp.z, 0.0f, 0.0f, v->z, 0.0f);
	vec4_set(&temp.t, 0.0f, 0.0f, 0.0f, 1.0f);
	matrix4_mul(dst, &temp, m);
}

bool matrix4_inv(struct matrix4 *dst, const struct matrix4 *m)
{
	struct vec4 *dstv;
	float det;
	float m3x3[9];
	int i, j, sign;

	if (dst == m) {
		struct matrix4 temp = *m;
		return matrix4_inv(dst, &temp);
	}

	dstv = (struct vec4 *)dst;
	det = matrix4_determinant(m);

	if (fabs(det) < 0.0005f)
		return false;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			sign = 1 - ((i + j) % 2) * 2;
			get_3x3_submatrix(m3x3, m, i, j);
			dstv[j].ptr[i] =
				get_3x3_determinant(m3x3) * (float)sign / det;
		}
	}

	return true;
}

void matrix4_transpose(struct matrix4 *dst, const struct matrix4 *m)
{
	if (dst == m) {
		struct matrix4 temp = *m;
		matrix4_transpose(dst, &temp);
		return;
	}

#ifdef NO_INTRINSICS
	dst->x.x = m->x.x;
	dst->x.y = m->y.x;
	dst->x.z = m->z.x;
	dst->x.w = m->t.x;
	dst->y.x = m->x.y;
	dst->y.y = m->y.y;
	dst->y.z = m->z.y;
	dst->y.w = m->t.y;
	dst->z.x = m->x.z;
	dst->z.y = m->y.z;
	dst->z.z = m->z.z;
	dst->z.w = m->t.z;
	dst->t.x = m->x.w;
	dst->t.y = m->y.w;
	dst->t.z = m->z.w;
	dst->t.w = m->t.w;
#else
	__m128 a0 = _mm_unpacklo_ps(m->x.m, m->z.m);
	__m128 a1 = _mm_unpacklo_ps(m->y.m, m->t.m);
	__m128 a2 = _mm_unpackhi_ps(m->x.m, m->z.m);
	__m128 a3 = _mm_unpackhi_ps(m->y.m, m->t.m);

	dst->x.m = _mm_unpacklo_ps(a0, a1);
	dst->y.m = _mm_unpackhi_ps(a0, a1);
	dst->z.m = _mm_unpacklo_ps(a2, a3);
	dst->t.m = _mm_unpackhi_ps(a2, a3);
#endif
}
