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

#include <stdlib.h>
#include "vec2.h"
#include "vec3.h"
#include "math-defs.h"
#include "math-extra.h"

void polar_to_cart(struct vec3 *dst, const struct vec3 *v)
{
	struct vec3 cart;
	float sinx = cosf(v->x);
	float sinx_z = v->z * sinx;

	cart.x = sinx_z * sinf(v->y);
	cart.z = sinx_z * cosf(v->y);
	cart.y = v->z * sinf(v->x);

	vec3_copy(dst, &cart);
}

void cart_to_polar(struct vec3 *dst, const struct vec3 *v)
{
	struct vec3 polar;
	polar.z = vec3_len(v);

	if (close_float(polar.z, 0.0f, EPSILON)) {
		vec3_zero(&polar);
	} else {
		polar.x = asinf(v->y / polar.z);
		polar.y = atan2f(v->x, v->z);
	}

	vec3_copy(dst, &polar);
}

void norm_to_polar(struct vec2 *dst, const struct vec3 *norm)
{
	dst->x = atan2f(norm->x, norm->z);
	dst->y = asinf(norm->y);
}

void polar_to_norm(struct vec3 *dst, const struct vec2 *polar)
{
	float sinx = sinf(polar->x);

	dst->x = sinx * cosf(polar->y);
	dst->y = sinx * sinf(polar->y);
	dst->z = cosf(polar->x);
}

float calc_torquef(float val1, float val2, float torque, float min_adjust,
		   float t)
{
	float out = val1;
	float dist;
	bool over;

	if (close_float(val1, val2, EPSILON))
		return val2;

	dist = (val2 - val1) * torque;
	over = dist > 0.0f;

	if (over) {
		if (dist < min_adjust) /* prevents from going too slow */
			dist = min_adjust;
		out += dist * t; /* add torque */
		if (out > val2)  /* clamp if overshoot */
			out = val2;
	} else {
		if (dist > -min_adjust)
			dist = -min_adjust;
		out += dist * t;
		if (out < val2)
			out = val2;
	}

	return out;
}

void calc_torque(struct vec3 *dst, const struct vec3 *v1, const struct vec3 *v2,
		 float torque, float min_adjust, float t)
{
	struct vec3 line, dir;
	float orig_dist, torque_dist, adjust_dist;

	if (vec3_close(v1, v2, EPSILON)) {
		vec3_copy(dst, v1);
		return;
	}

	vec3_sub(&line, v2, v1);
	orig_dist = vec3_len(&line);
	vec3_mulf(&dir, &line, 1.0f / orig_dist);

	torque_dist = orig_dist * torque; /* use distance to determine speed */
	if (torque_dist < min_adjust)     /* prevent from going too slow */
		torque_dist = min_adjust;

	adjust_dist = torque_dist * t;

	if (adjust_dist <= (orig_dist - LARGE_EPSILON)) {
		vec3_mulf(dst, &dir, adjust_dist);
		vec3_add(dst, dst, v1); /* add torque */
	} else {
		vec3_copy(dst, v2); /* clamp if overshoot */
	}
}

float rand_float(int positive_only)
{
	if (positive_only)
		return (float)((double)rand() / (double)RAND_MAX);
	else
		return (float)(((double)rand() / (double)RAND_MAX * 2.0) - 1.0);
}
