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

#include "vec4.h"
#include "vec3.h"
#include "matrix4.h"

void vec4_from_vec3(struct vec4 *dst, const struct vec3 *v)
{
	dst->m = v->m;
	dst->w = 1.0f;
}

void vec4_transform(struct vec4 *dst, const struct vec4 *v,
		    const struct matrix4 *m)
{
	struct vec4 temp;
	struct matrix4 transpose;

	matrix4_transpose(&transpose, m);

	temp.x = vec4_dot(&transpose.x, v);
	temp.y = vec4_dot(&transpose.y, v);
	temp.z = vec4_dot(&transpose.z, v);
	temp.w = vec4_dot(&transpose.t, v);

	vec4_copy(dst, &temp);
}
