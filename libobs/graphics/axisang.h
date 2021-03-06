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

#ifdef __cplusplus
extern "C" {
#endif

struct quat;

struct axisang {
	union {
		struct {
			float x, y, z, w;
		};
		float ptr[4];
	};
};

static inline void axisang_zero(struct axisang *dst)
{
	dst->x = 0.0f;
	dst->y = 0.0f;
	dst->z = 0.0f;
	dst->w = 0.0f;
}

static inline void axisang_copy(struct axisang *dst, struct axisang *aa)
{
	dst->x = aa->x;
	dst->y = aa->y;
	dst->z = aa->z;
	dst->w = aa->w;
}

static inline void axisang_set(struct axisang *dst, float x, float y, float z,
			       float w)
{
	dst->x = x;
	dst->y = y;
	dst->z = z;
	dst->w = w;
}

EXPORT void axisang_from_quat(struct axisang *dst, const struct quat *q);

#ifdef __cplusplus
}
#endif
