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

#include "obs.h"
#include "obs-avc.h"
#include "util/array-serializer.h"

bool obs_avc_keyframe(const uint8_t *data, size_t size)
{
	const uint8_t *nal_start, *nal_end;
	const uint8_t *end = data + size;
	int type;

	nal_start = obs_avc_find_startcode(data, end);
	while (true) {
		while (nal_start < end && !*(nal_start++))
			;

		if (nal_start == end)
			break;

		type = nal_start[0] & 0x1F;

		if (type == OBS_NAL_SLICE_IDR || type == OBS_NAL_SLICE)
			return (type == OBS_NAL_SLICE_IDR);

		nal_end = obs_avc_find_startcode(nal_start, end);
		nal_start = nal_end;
	}

	return false;
}

/* NOTE: I noticed that FFmpeg does some unusual special handling of certain
 * scenarios that I was unaware of, so instead of just searching for {0, 0, 1}
 * we'll just use the code from FFmpeg - http://www.ffmpeg.org/ */
static const uint8_t *ff_avc_find_startcode_internal(const uint8_t *p,
						     const uint8_t *end)
{
	const uint8_t *a = p + 4 - ((intptr_t)p & 3);

	for (end -= 3; p < a && p < end; p++) {
		if (p[0] == 0 && p[1] == 0 && p[2] == 1)
			return p;
	}

	for (end -= 3; p < end; p += 4) {
		uint32_t x = *(const uint32_t *)p;

		if ((x - 0x01010101) & (~x) & 0x80808080) {
			if (p[1] == 0) {
				if (p[0] == 0 && p[2] == 1)
					return p;
				if (p[2] == 0 && p[3] == 1)
					return p + 1;
			}

			if (p[3] == 0) {
				if (p[2] == 0 && p[4] == 1)
					return p + 2;
				if (p[4] == 0 && p[5] == 1)
					return p + 3;
			}
		}
	}

	for (end += 3; p < end; p++) {
		if (p[0] == 0 && p[1] == 0 && p[2] == 1)
			return p;
	}

	return end + 3;
}

const uint8_t *obs_avc_find_startcode(const uint8_t *p, const uint8_t *end)
{
	const uint8_t *out = ff_avc_find_startcode_internal(p, end);
	if (p < out && out < end && !out[-1])
		out--;
	return out;
}

static inline int get_drop_priority(int priority)
{
	return priority;
}

static void serialize_avc_data(struct serializer *s, const uint8_t *data,
			       size_t size, bool *is_keyframe, int *priority)
{
	const uint8_t *nal_start, *nal_end;
	const uint8_t *end = data + size;
	int type;

	nal_start = obs_avc_find_startcode(data, end);
	while (true) {
		while (nal_start < end && !*(nal_start++))
			;

		if (nal_start == end)
			break;

		type = nal_start[0] & 0x1F;

		if (type == OBS_NAL_SLICE_IDR || type == OBS_NAL_SLICE) {
			if (is_keyframe)
				*is_keyframe = (type == OBS_NAL_SLICE_IDR);
			if (priority)
				*priority = nal_start[0] >> 5;
		}

		nal_end = obs_avc_find_startcode(nal_start, end);
		s_wb32(s, (uint32_t)(nal_end - nal_start));
		s_write(s, nal_start, nal_end - nal_start);
		nal_start = nal_end;
	}
}

void obs_parse_avc_packet(struct encoder_packet *avc_packet,
			  const struct encoder_packet *src)
{
	struct array_output_data output;
	struct serializer s;
	long ref = 1;

	array_output_serializer_init(&s, &output);
	*avc_packet = *src;

	serialize(&s, &ref, sizeof(ref));
	serialize_avc_data(&s, src->data, src->size, &avc_packet->keyframe,
			   &avc_packet->priority);

	avc_packet->data = output.bytes.array + sizeof(ref);
	avc_packet->size = output.bytes.num - sizeof(ref);
	avc_packet->drop_priority = get_drop_priority(avc_packet->priority);
}

static inline bool has_start_code(const uint8_t *data)
{
	if (data[0] != 0 || data[1] != 0)
		return false;

	return data[2] == 1 || (data[2] == 0 && data[3] == 1);
}

static void get_sps_pps(const uint8_t *data, size_t size, const uint8_t **sps,
			size_t *sps_size, const uint8_t **pps, size_t *pps_size)
{
	const uint8_t *nal_start, *nal_end;
	const uint8_t *end = data + size;
	int type;

	nal_start = obs_avc_find_startcode(data, end);
	while (true) {
		while (nal_start < end && !*(nal_start++))
			;

		if (nal_start == end)
			break;

		nal_end = obs_avc_find_startcode(nal_start, end);

		type = nal_start[0] & 0x1F;
		if (type == OBS_NAL_SPS) {
			*sps = nal_start;
			*sps_size = nal_end - nal_start;
		} else if (type == OBS_NAL_PPS) {
			*pps = nal_start;
			*pps_size = nal_end - nal_start;
		}

		nal_start = nal_end;
	}
}

size_t obs_parse_avc_header(uint8_t **header, const uint8_t *data, size_t size)
{
	struct array_output_data output;
	struct serializer s;
	const uint8_t *sps = NULL, *pps = NULL;
	size_t sps_size = 0, pps_size = 0;

	array_output_serializer_init(&s, &output);

	if (size <= 6)
		return 0;

	if (!has_start_code(data)) {
		*header = bmemdup(data, size);
		return size;
	}

	get_sps_pps(data, size, &sps, &sps_size, &pps, &pps_size);
	if (!sps || !pps || sps_size < 4)
		return 0;

	s_w8(&s, 0x01);
	s_write(&s, sps + 1, 3);
	s_w8(&s, 0xff);
	s_w8(&s, 0xe1);

	s_wb16(&s, (uint16_t)sps_size);
	s_write(&s, sps, sps_size);
	s_w8(&s, 0x01);
	s_wb16(&s, (uint16_t)pps_size);
	s_write(&s, pps, pps_size);

	*header = output.bytes.array;
	return output.bytes.num;
}

void obs_extract_avc_headers(const uint8_t *packet, size_t size,
			     uint8_t **new_packet_data, size_t *new_packet_size,
			     uint8_t **header_data, size_t *header_size,
			     uint8_t **sei_data, size_t *sei_size)
{
	DARRAY(uint8_t) new_packet;
	DARRAY(uint8_t) header;
	DARRAY(uint8_t) sei;
	const uint8_t *nal_start, *nal_end, *nal_codestart;
	const uint8_t *end = packet + size;
	int type;

	da_init(new_packet);
	da_init(header);
	da_init(sei);

	nal_start = obs_avc_find_startcode(packet, end);
	nal_end = NULL;
	while (nal_end != end) {
		nal_codestart = nal_start;

		while (nal_start < end && !*(nal_start++))
			;

		if (nal_start == end)
			break;

		type = nal_start[0] & 0x1F;

		nal_end = obs_avc_find_startcode(nal_start, end);
		if (!nal_end)
			nal_end = end;

		if (type == OBS_NAL_SPS || type == OBS_NAL_PPS) {
			da_push_back_array(header, nal_codestart,
					   nal_end - nal_codestart);
		} else if (type == OBS_NAL_SEI) {
			da_push_back_array(sei, nal_codestart,
					   nal_end - nal_codestart);

		} else {
			da_push_back_array(new_packet, nal_codestart,
					   nal_end - nal_codestart);
		}

		nal_start = nal_end;
	}

	*new_packet_data = new_packet.array;
	*new_packet_size = new_packet.num;
	*header_data = header.array;
	*header_size = header.num;
	*sei_data = sei.array;
	*sei_size = sei.num;
}
