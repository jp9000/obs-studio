/*
Copyright (C) 2019 by Gary Kramlich <grim@reaperworld.com>

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
*/

#pragma once

#include <obs-module.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Update a devices properties to include all v4l controls that it supports.
 *
 * @param dev handle to the v4l2 device
 * @param props properties for the device
 * @param data the settings for the source
 *
 * @return negative on failure
 */
int_fast32_t v4l2_update_controls(int_fast32_t dev, obs_properties_t *props,
				  obs_data_t *settings);

#ifdef __cplusplus
}
#endif
