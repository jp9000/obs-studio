/******************************************************************************
    Copyright (C) 2020 by Georges Basile Stavracas Neto <georges.stavracas@gmail.com>

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

#include "obs-internal.h"

struct obs_nix_hotkeys_vtable {
	bool (*init)(struct obs_core_hotkeys *hotkeys);

	void (*free)(struct obs_core_hotkeys *hotkeys);

	bool (*is_pressed)(obs_hotkeys_platform_t *context, obs_key_t key);

	void (*key_to_str)(obs_key_t key, struct dstr *dstr);

	obs_key_t (*key_from_virtual_key)(int sym);

	int (*key_to_virtual_key)(obs_key_t key);
};

#ifdef __cplusplus
}
#endif
