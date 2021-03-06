/******************************************************************************
    Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

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

#include "librtmp/rtmp.h"

static inline AVal *flv_str(AVal *out, const char *str)
{
	out->av_val = (char *)str;
	out->av_len = (int)strlen(str);
	return out;
}

static inline void enc_num_val(char **enc, char *end, const char *name,
			       double val)
{
	AVal s;
	*enc = AMF_EncodeNamedNumber(*enc, end, flv_str(&s, name), val);
}

static inline void enc_bool_val(char **enc, char *end, const char *name,
				bool val)
{
	AVal s;
	*enc = AMF_EncodeNamedBoolean(*enc, end, flv_str(&s, name), val);
}

static inline void enc_str_val(char **enc, char *end, const char *name,
			       const char *val)
{
	AVal s1, s2;
	*enc = AMF_EncodeNamedString(*enc, end, flv_str(&s1, name),
				     flv_str(&s2, val));
}

static inline void enc_str(char **enc, char *end, const char *str)
{
	AVal s;
	*enc = AMF_EncodeString(*enc, end, flv_str(&s, str));
}
