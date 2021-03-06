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
#include "bounds.h"
#include "matrix3.h"
#include "matrix4.h"
#include "plane.h"

void bounds_move(struct bounds *dst, const struct bounds *b,
		 const struct vec3 *v)
{
	vec3_add(&dst->min, &b->min, v);
	vec3_add(&dst->max, &b->max, v);
}

void bounds_scale(struct bounds *dst, const struct bounds *b,
		  const struct vec3 *v)
{
	vec3_mul(&dst->min, &b->min, v);
	vec3_mul(&dst->max, &b->max, v);
}

void bounds_merge(struct bounds *dst, const struct bounds *b1,
		  const struct bounds *b2)
{
	vec3_min(&dst->min, &b1->min, &b2->min);
	vec3_max(&dst->max, &b1->max, &b2->max);
}

void bounds_merge_point(struct bounds *dst, const struct bounds *b,
			const struct vec3 *v)
{
	vec3_min(&dst->min, &b->min, v);
	vec3_max(&dst->max, &b->max, v);
}

void bounds_get_point(struct vec3 *dst, const struct bounds *b, unsigned int i)
{
	if (i > 8)
		return;

	/*
	 * Note:
	 * 0 = min.x,min.y,min.z
	 * 1 = min.x,min.y,MAX.z
	 * 2 = min.x,MAX.y,min.z
	 * 3 = min.x,MAX.y,MAX.z
	 * 4 = MAX.x,min.y,min.z
	 * 5 = MAX.x,min.y,MAX.z

	 * 6 = MAX.x,MAX.y,min.z
	 * 7 = MAX.x,MAX.y,MAX.z
	 */

	if (i > 3) {
		dst->x = b->max.x;
		i -= 4;
	} else {
		dst->x = b->min.x;
	}

	if (i > 1) {
		dst->y = b->max.y;
		i -= 2;
	} else {
		dst->y = b->min.y;
	}

	dst->z = (i == 1) ? b->max.z : b->min.z;
}

void bounds_get_center(struct vec3 *dst, const struct bounds *b)
{
	vec3_sub(dst, &b->max, &b->min);
	vec3_mulf(dst, dst, 0.5f);
	vec3_add(dst, dst, &b->min);
}

void bounds_transform(struct bounds *dst, const struct bounds *b,
		      const struct matrix4 *m)
{
	struct bounds temp;
	bool b_init = false;
	int i;

	memset(&temp, 0, sizeof(temp));

	for (i = 0; i < 8; i++) {
		struct vec3 p;
		bounds_get_point(&p, b, i);
		vec3_transform(&p, &p, m);

		if (!b_init) {
			vec3_copy(&temp.min, &p);
			vec3_copy(&temp.max, &p);
			b_init = true;
		} else {
			if (p.x < temp.min.x)
				temp.min.x = p.x;
			else if (p.x > temp.max.x)
				temp.max.x = p.x;

			if (p.y < temp.min.y)
				temp.min.y = p.y;
			else if (p.y > temp.max.y)
				temp.max.y = p.y;

			if (p.z < temp.min.z)
				temp.min.z = p.z;
			else if (p.z > temp.max.z)
				temp.max.z = p.z;
		}
	}

	bounds_copy(dst, &temp);
}

void bounds_transform3x4(struct bounds *dst, const struct bounds *b,
			 const struct matrix3 *m)
{
	struct bounds temp;
	bool b_init = false;
	int i;

	memset(&temp, 0, sizeof(temp));

	for (i = 0; i < 8; i++) {
		struct vec3 p;
		bounds_get_point(&p, b, i);
		vec3_transform3x4(&p, &p, m);

		if (!b_init) {
			vec3_copy(&temp.min, &p);
			vec3_copy(&temp.max, &p);
			b_init = true;
		} else {
			if (p.x < temp.min.x)
				temp.min.x = p.x;
			else if (p.x > temp.max.x)
				temp.max.x = p.x;

			if (p.y < temp.min.y)
				temp.min.y = p.y;
			else if (p.y > temp.max.y)
				temp.max.y = p.y;

			if (p.z < temp.min.z)
				temp.min.z = p.z;
			else if (p.z > temp.max.z)
				temp.max.z = p.z;
		}
	}

	bounds_copy(dst, &temp);
}

bool bounds_intersection_ray(const struct bounds *b, const struct vec3 *orig,
			     const struct vec3 *dir, float *t)
{
	float t_max = M_INFINITE;
	float t_min = -M_INFINITE;
	struct vec3 center, max_offset, box_offset;
	int i;

	bounds_get_center(&center, b);
	vec3_sub(&max_offset, &b->max, &center);
	vec3_sub(&box_offset, &center, orig);

	for (i = 0; i < 3; i++) {
		float e = box_offset.ptr[i];
		float f = dir->ptr[i];

		if (fabsf(f) > 0.0f) {
			float fi = 1.0f / f;
			float t1 = (e + max_offset.ptr[i]) * fi;
			float t2 = (e - max_offset.ptr[i]) * fi;
			if (t1 > t2) {
				if (t2 > t_min)
					t_min = t2;
				if (t1 < t_max)
					t_max = t1;
			} else {
				if (t1 > t_min)
					t_min = t1;
				if (t2 < t_max)
					t_max = t2;
			}
			if (t_min > t_max)
				return false;
			if (t_max < 0.0f)
				return false;
		} else if ((-e - max_offset.ptr[i]) > 0.0f ||
			   (-e + max_offset.ptr[i]) < 0.0f) {
			return false;
		}
	}

	*t = (t_min > 0.0f) ? t_min : t_max;
	return true;
}

bool bounds_intersection_line(const struct bounds *b, const struct vec3 *p1,
			      const struct vec3 *p2, float *t)
{
	struct vec3 dir;
	float length;

	vec3_sub(&dir, p2, p1);
	length = vec3_len(&dir);
	if (length <= TINY_EPSILON)
		return false;

	vec3_mulf(&dir, &dir, 1.0f / length);

	if (!bounds_intersection_ray(b, p1, &dir, t))
		return false;

	*t /= length;
	return true;
}

bool bounds_plane_test(const struct bounds *b, const struct plane *p)
{
	struct vec3 vmin, vmax;
	int i;

	for (i = 0; i < 3; i++) {
		if (p->dir.ptr[i] >= 0.0f) {
			vmin.ptr[i] = b->min.ptr[i];
			vmax.ptr[i] = b->max.ptr[i];
		} else {
			vmin.ptr[i] = b->max.ptr[i];
			vmax.ptr[i] = b->min.ptr[i];
		}
	}

	if (vec3_plane_dist(&vmin, p) > 0.0f)
		return BOUNDS_OUTSIDE;

	if (vec3_plane_dist(&vmax, p) >= 0.0f)
		return BOUNDS_PARTIAL;

	return BOUNDS_INSIDE;
}

bool bounds_under_plane(const struct bounds *b, const struct plane *p)
{
	struct vec3 vmin;

	vmin.x = (p->dir.x < 0.0f) ? b->max.x : b->min.x;
	vmin.y = (p->dir.y < 0.0f) ? b->max.y : b->min.y;
	vmin.z = (p->dir.z < 0.0f) ? b->max.z : b->min.z;

	return (vec3_dot(&vmin, &p->dir) <= p->dist);
}

bool bounds_intersects(const struct bounds *b, const struct bounds *test,
		       float epsilon)
{
	return ((b->min.x - test->max.x) <= epsilon) &&
	       ((test->min.x - b->max.x) <= epsilon) &&
	       ((b->min.y - test->max.y) <= epsilon) &&
	       ((test->min.y - b->max.y) <= epsilon) &&
	       ((b->min.z - test->max.z) <= epsilon) &&
	       ((test->min.z - b->max.z) <= epsilon);
}

bool bounds_intersects_obb(const struct bounds *b, const struct bounds *test,
			   const struct matrix4 *m, float epsilon)
{
	struct bounds b_tr, test_tr;
	struct matrix4 m_inv;

	matrix4_inv(&m_inv, m);

	bounds_transform(&b_tr, b, m);
	bounds_transform(&test_tr, test, &m_inv);

	return bounds_intersects(b, &test_tr, epsilon) &&
	       bounds_intersects(&b_tr, test, epsilon);
}

bool bounds_intersects_obb3x4(const struct bounds *b, const struct bounds *test,
			      const struct matrix3 *m, float epsilon)
{
	struct bounds b_tr, test_tr;
	struct matrix3 m_inv;

	matrix3_transpose(&m_inv, m);

	bounds_transform3x4(&b_tr, b, m);
	bounds_transform3x4(&test_tr, test, &m_inv);

	return bounds_intersects(b, &test_tr, epsilon) &&
	       bounds_intersects(&b_tr, test, epsilon);
}

static inline float vec3or_offset_len(const struct bounds *b,
				      const struct vec3 *v)
{
	struct vec3 temp1, temp2;
	vec3_sub(&temp1, &b->max, &b->min);
	vec3_abs(&temp2, v);
	return vec3_dot(&temp1, &temp2);
}

float bounds_min_dist(const struct bounds *b, const struct plane *p)
{
	struct vec3 center;
	float vec_len = vec3or_offset_len(b, &p->dir) * 0.5f;
	float center_dist;

	bounds_get_center(&center, b);
	center_dist = vec3_plane_dist(&center, p);

	return p->dist + center_dist - vec_len;
}
