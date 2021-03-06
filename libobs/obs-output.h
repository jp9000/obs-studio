/******************************************************************************
    Copyright (C) 2013-2014 by Hugh Bailey <obs.jim@gmail.com>

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

#ifdef __cplusplus
extern "C" {
#endif

#define OBS_OUTPUT_VIDEO (1 << 0)
#define OBS_OUTPUT_AUDIO (1 << 1)
#define OBS_OUTPUT_AV (OBS_OUTPUT_VIDEO | OBS_OUTPUT_AUDIO)
#define OBS_OUTPUT_ENCODED (1 << 2)
#define OBS_OUTPUT_SERVICE (1 << 3)
#define OBS_OUTPUT_MULTI_TRACK (1 << 4)
#define OBS_OUTPUT_CAN_PAUSE (1 << 5)

struct encoder_packet;

struct obs_output_info {
	/* required */
	const char *id;

	uint32_t flags;

	const char *(*get_name)(void *type_data);

	void *(*create)(obs_data_t *settings, obs_output_t *output);
	void (*destroy)(void *data);

	bool (*start)(void *data);
	void (*stop)(void *data, uint64_t ts);

	void (*raw_video)(void *data, struct video_data *frame);
	void (*raw_audio)(void *data, struct audio_data *frames);

	void (*encoded_packet)(void *data, struct encoder_packet *packet);

	/* optional */
	void (*update)(void *data, obs_data_t *settings);

	void (*get_defaults)(obs_data_t *settings);

	obs_properties_t *(*get_properties)(void *data);

	void (*unused1)(void *data);

	uint64_t (*get_total_bytes)(void *data);

	int (*get_dropped_frames)(void *data);

	void *type_data;
	void (*free_type_data)(void *type_data);

	float (*get_congestion)(void *data);
	int (*get_connect_time_ms)(void *data);

	/* only used with encoded outputs, separated with semicolon */
	const char *encoded_video_codecs;
	const char *encoded_audio_codecs;

	/* raw audio callback for multi track outputs */
	void (*raw_audio2)(void *data, size_t idx, struct audio_data *frames);
};

EXPORT void obs_register_output_s(const struct obs_output_info *info,
				  size_t size);

#define obs_register_output(info) \
	obs_register_output_s(info, sizeof(struct obs_output_info))

#ifdef __cplusplus
}
#endif
