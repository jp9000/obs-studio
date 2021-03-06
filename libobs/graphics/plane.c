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

#include "../util/c99defs.h"
#include "matrix3.h"
#include "plane.h"

void plane_from_tri(struct plane *dst, const struct vec3 *v1,
		    const struct vec3 *v2, const struct vec3 *v3)
{
	struct vec3 temp;

	vec3_sub(&temp, v2, v1);
	vec3_sub(&dst->dir, v3, v1);
	vec3_cross(&dst->dir, &temp, &dst->dir);
	vec3_norm(&dst->dir, &dst->dir);
	dst->dist = vec3_dot(v1, &dst->dir);
}

void plane_transform(struct plane *dst, const struct plane *p,
		     const struct matrix4 *m)
{
	struct vec3 temp;

	vec3_zero(&temp);

	vec3_transform(&dst->dir, &p->dir, m);
	vec3_norm(&dst->dir, &dst->dir);

	vec3_transform(&temp, &temp, m);
	dst->dist = p->dist - vec3_dot(&dst->dir, &temp);
}

void plane_transform3x4(struct plane *dst, const struct plane *p,
			const struct matrix3 *m)
{
	struct vec3 temp;

	vec3_transform3x4(&dst->dir, &p->dir, m);
	vec3_norm(&dst->dir, &dst->dir);

	vec3_transform3x4(&temp, &m->t, m);
	dst->dist = p->dist - vec3_dot(&dst->dir, &temp);
}

bool plane_intersection_ray(const struct plane *p, const struct vec3 *orig,
			    const struct vec3 *dir, float *t)
{
	float c = vec3_dot(&p->dir, dir);

	if (fabsf(c) < EPSILON) {
		*t = 0.0f;
		return false;
	} else {
		*t = (p->dist - vec3_dot(&p->dir, orig)) / c;
		return true;
	}
}

bool plane_intersection_line(const struct plane *p, const struct vec3 *v1,
			     const struct vec3 *v2, float *t)
{
	float p1_dist, p2_dist, p1_abs_dist, dist2;
	bool p1_over, p2_over;

	p1_dist = vec3_plane_dist(v1, p);
	p2_dist = vec3_plane_dist(v2, p);

	if (close_float(p1_dist, 0.0f, EPSILON)) {
		if (close_float(p2_dist, 0.0f, EPSILON))
			return false;

		*t = 0.0f;
		return true;
	} else if (close_float(p2_dist, 0.0f, EPSILON)) {
		*t = 1.0f;
		return true;
	}

	p1_over = (p1_dist > 0.0f);
	p2_over = (p2_dist > 0.0f);

	if (p1_over == p2_over)
		return false;

	p1_abs_dist = fabsf(p1_dist);
	dist2 = p1_abs_dist + fabsf(p2_dist);
	if (dist2 < EPSILON)
		return false;

	*t = p1_abs_dist / dist2;
	return true;
}

bool plane_tri_inside(const struct plane *p, const struct vec3 *v1,
		      const struct vec3 *v2, const struct vec3 *v3,
		      float precision)
{
	/* bit 1: part or all is behind the plane      */
	/* bit 2: part or all is in front of the plane */
	int sides = 0;
	float d1 = vec3_plane_dist(v1, p);
	float d2 = vec3_plane_dist(v2, p);
	float d3 = vec3_plane_dist(v3, p);

	if (d1 >= precision)
		sides = 2;
	else if (d1 <= -precision)
		sides = 1;

	if (d2 >= precision)
		sides |= 2;
	else if (d2 <= -precision)
		sides |= 1;

	if (d3 >= precision)
		sides |= 2;
	else if (d3 <= -precision)
		sides |= 1;

	return sides;
}

bool plane_line_inside(const struct plane *p, const struct vec3 *v1,
		       const struct vec3 *v2, float precision)
{
	/* bit 1: part or all is behind the plane      */
	/* bit 2: part or all is in front of the plane */
	int sides = 0;
	float d1 = vec3_plane_dist(v1, p);
	float d2 = vec3_plane_dist(v2, p);

	if (d1 >= precision)
		sides = 2;
	else if (d1 <= -precision)
		sides = 1;

	if (d2 >= precision)
		sides |= 2;
	else if (d2 <= -precision)
		sides |= 1;

	return sides;
}
