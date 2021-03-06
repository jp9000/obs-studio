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

#include "util/c99defs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct encoder_packet;

enum { OBS_NAL_UNKNOWN = 0,
       OBS_NAL_SLICE = 1,
       OBS_NAL_SLICE_DPA = 2,
       OBS_NAL_SLICE_DPB = 3,
       OBS_NAL_SLICE_DPC = 4,
       OBS_NAL_SLICE_IDR = 5,
       OBS_NAL_SEI = 6,
       OBS_NAL_SPS = 7,
       OBS_NAL_PPS = 8,
       OBS_NAL_AUD = 9,
       OBS_NAL_FILLER = 12,
};

enum { OBS_NAL_PRIORITY_DISPOSABLE = 0,
       OBS_NAL_PRIORITY_LOW = 1,
       OBS_NAL_PRIORITY_HIGH = 2,
       OBS_NAL_PRIORITY_HIGHEST = 3,
};

/* Helpers for parsing AVC NAL units.  */

EXPORT bool obs_avc_keyframe(const uint8_t *data, size_t size);
EXPORT const uint8_t *obs_avc_find_startcode(const uint8_t *p,
					     const uint8_t *end);
EXPORT void obs_parse_avc_packet(struct encoder_packet *avc_packet,
				 const struct encoder_packet *src);
EXPORT size_t obs_parse_avc_header(uint8_t **header, const uint8_t *data,
				   size_t size);
EXPORT void obs_extract_avc_headers(const uint8_t *packet, size_t size,
				    uint8_t **new_packet_data,
				    size_t *new_packet_size,
				    uint8_t **header_data, size_t *header_size,
				    uint8_t **sei_data, size_t *sei_size);

#ifdef __cplusplus
}
#endif
