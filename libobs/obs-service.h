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

/**
 * @file
 * @brief header for modules implementing services.
 *
 * Services are modules that implement provider specific settings for outputs.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct obs_service_resolution {
	int cx;
	int cy;
};

struct obs_service_info {
	/* required */
	const char *id;

	const char *(*get_name)(void *type_data);
	void *(*create)(obs_data_t *settings, obs_service_t *service);
	void (*destroy)(void *data);

	/* optional */
	void (*activate)(void *data, obs_data_t *settings);
	void (*deactivate)(void *data);

	void (*update)(void *data, obs_data_t *settings);

	void (*get_defaults)(obs_data_t *settings);

	obs_properties_t *(*get_properties)(void *data);

	/**
	 * Called when getting ready to start up an output, before the encoders
	 * and output are initialized
	 *
	 * @param  data    Internal service data
	 * @param  output  Output context
	 * @return         true to allow the output to start up,
	 *                 false to prevent output from starting up
	 */
	bool (*initialize)(void *data, obs_output_t *output);

	const char *(*get_url)(void *data);
	const char *(*get_key)(void *data);

	const char *(*get_username)(void *data);
	const char *(*get_password)(void *data);

	bool (*deprecated_1)();

	void (*apply_encoder_settings)(void *data,
				       obs_data_t *video_encoder_settings,
				       obs_data_t *audio_encoder_settings);

	void *type_data;
	void (*free_type_data)(void *type_data);

	const char *(*get_output_type)(void *data);

	void (*get_supported_resolutions)(
		void *data, struct obs_service_resolution **resolutions,
		size_t *count);
	void (*get_max_fps)(void *data, int *fps);

	void (*get_max_bitrate)(void *data, int *video_bitrate,
				int *audio_bitrate);
};

EXPORT void obs_register_service_s(const struct obs_service_info *info,
				   size_t size);

#define obs_register_service(info) \
	obs_register_service_s(info, sizeof(struct obs_service_info))

#ifdef __cplusplus
}
#endif
