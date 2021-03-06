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
#include "vec3.h"

#ifdef __cplusplus
extern "C" {
#endif

struct matrix3;
struct matrix4;

struct plane {
	struct vec3 dir;
	float dist;
};

static inline void plane_copy(struct plane *dst, const struct plane *p)
{
	vec3_copy(&dst->dir, &p->dir);
	dst->dist = p->dist;
}

static inline void plane_set(struct plane *dst, const struct vec3 *dir,
			     float dist)
{
	vec3_copy(&dst->dir, dir);
	dst->dist = dist;
}

static inline void plane_setf(struct plane *dst, float a, float b, float c,
			      float d)
{
	vec3_set(&dst->dir, a, b, c);
	dst->dist = d;
}

EXPORT void plane_from_tri(struct plane *dst, const struct vec3 *v1,
			   const struct vec3 *v2, const struct vec3 *v3);

EXPORT void plane_transform(struct plane *dst, const struct plane *p,
			    const struct matrix4 *m);
EXPORT void plane_transform3x4(struct plane *dst, const struct plane *p,
			       const struct matrix3 *m);

EXPORT bool plane_intersection_ray(const struct plane *p,
				   const struct vec3 *orig,
				   const struct vec3 *dir, float *t);
EXPORT bool plane_intersection_line(const struct plane *p,
				    const struct vec3 *v1,
				    const struct vec3 *v2, float *t);

EXPORT bool plane_tri_inside(const struct plane *p, const struct vec3 *v1,
			     const struct vec3 *v2, const struct vec3 *v3,
			     float precision);

EXPORT bool plane_line_inside(const struct plane *p, const struct vec3 *v1,
			      const struct vec3 *v2, float precision);

static inline bool plane_close(const struct plane *p1, const struct plane *p2,
			       float precision)
{
	return vec3_close(&p1->dir, &p2->dir, precision) &&
	       close_float(p1->dist, p2->dist, precision);
}

static inline bool plane_coplanar(const struct plane *p1,
				  const struct plane *p2, float precision)
{
	float cos_angle = vec3_dot(&p1->dir, &p2->dir);

	if (close_float(cos_angle, 1.0f, precision))
		return close_float(p1->dist, p2->dist, precision);
	else if (close_float(cos_angle, -1.0f, precision))
		return close_float(-p1->dist, p2->dist, precision);

	return false;
}

#ifdef __cplusplus
}
#endif
