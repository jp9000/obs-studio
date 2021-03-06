/******************************************************************************
    Copyright (C) 2019 by Jason Francis <cycl0ps@tuta.io>

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

#include "obs-nix-platform.h"

static enum obs_nix_platform_type obs_nix_platform = OBS_NIX_PLATFORM_X11_GLX;

static void *obs_nix_platform_display = NULL;

void obs_set_nix_platform(enum obs_nix_platform_type platform)
{
	obs_nix_platform = platform;
}

enum obs_nix_platform_type obs_get_nix_platform(void)
{
	return obs_nix_platform;
}

void obs_set_nix_platform_display(void *display)
{
	obs_nix_platform_display = display;
}

void *obs_get_nix_platform_display(void)
{
	return obs_nix_platform_display;
}
