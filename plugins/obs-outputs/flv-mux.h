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

#include <obs.h>

#define MILLISECOND_DEN 1000

static int32_t get_ms_time(struct encoder_packet *packet, int64_t val)
{
	return (int32_t)(val * MILLISECOND_DEN / packet->timebase_den);
}

extern void write_file_info(FILE *file, int64_t duration_ms, int64_t size);

extern void flv_meta_data(obs_output_t *context, uint8_t **output, size_t *size,
			  bool write_header);
extern void flv_additional_meta_data(obs_output_t *context, uint8_t **output,
				     size_t *size);
extern void flv_packet_mux(struct encoder_packet *packet, int32_t dts_offset,
			   uint8_t **output, size_t *size, bool is_header);
extern void flv_additional_packet_mux(struct encoder_packet *packet,
				      int32_t dts_offset, uint8_t **output,
				      size_t *size, bool is_header,
				      size_t index);
