/******************************************************************************
    Copyright (C) 2014 by Ruwen Hahn <palana@stunned.de>

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
#include "video-io.h"

#define MAKE_FOURCC(a, b, c, d) \
	((uint32_t)(((d) << 24) | ((c) << 16) | ((b) << 8) | (a)))

enum video_format video_format_from_fourcc(uint32_t fourcc)
{
	switch (fourcc) {
	case MAKE_FOURCC('U', 'Y', 'V', 'Y'):
	case MAKE_FOURCC('H', 'D', 'Y', 'C'):
	case MAKE_FOURCC('U', 'Y', 'N', 'V'):
	case MAKE_FOURCC('U', 'Y', 'N', 'Y'):
	case MAKE_FOURCC('u', 'y', 'v', '1'):
	case MAKE_FOURCC('2', 'v', 'u', 'y'):
	case MAKE_FOURCC('2', 'V', 'u', 'y'):
		return VIDEO_FORMAT_UYVY;

	case MAKE_FOURCC('Y', 'U', 'Y', '2'):
	case MAKE_FOURCC('Y', '4', '2', '2'):
	case MAKE_FOURCC('V', '4', '2', '2'):
	case MAKE_FOURCC('V', 'Y', 'U', 'Y'):
	case MAKE_FOURCC('Y', 'U', 'N', 'V'):
	case MAKE_FOURCC('y', 'u', 'v', '2'):
	case MAKE_FOURCC('y', 'u', 'v', 's'):
		return VIDEO_FORMAT_YUY2;

	case MAKE_FOURCC('Y', 'V', 'Y', 'U'):
		return VIDEO_FORMAT_YVYU;

	case MAKE_FOURCC('Y', '8', '0', '0'):
		return VIDEO_FORMAT_Y800;
	}
	return VIDEO_FORMAT_NONE;
}
