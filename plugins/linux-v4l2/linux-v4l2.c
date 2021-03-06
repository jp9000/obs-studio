/*
Copyright (C) 2014 by Leonhard Oelke <leonhard@in-verted.de>

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
#include <obs-module.h>
#include <util/platform.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("linux-v4l2", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Video4Linux2(V4L2) sources/virtual camera";
}

extern struct obs_source_info v4l2_input;
extern struct obs_output_info virtualcam_info;
extern bool loopback_module_available();

bool obs_module_load(void)
{
	obs_register_source(&v4l2_input);

	obs_data_t *obs_settings = obs_data_create();

	if (loopback_module_available()) {
		obs_register_output(&virtualcam_info);
		obs_data_set_bool(obs_settings, "vcamEnabled", true);
	} else {
		obs_data_set_bool(obs_settings, "vcamEnabled", false);
		blog(LOG_WARNING,
		     "v4l2loopback not installed, virtual camera disabled");
	}

	obs_apply_private_data(obs_settings);
	obs_data_release(obs_settings);

	return true;
}
