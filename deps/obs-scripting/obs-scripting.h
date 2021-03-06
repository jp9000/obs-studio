/******************************************************************************
    Copyright (C) 2017 by Hugh Bailey <jim@obsproject.com>

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

#include <stdarg.h>
#include <util/c99defs.h>
#include <obs-data.h>
#include <obs-properties.h>

#ifdef __cplusplus
extern "C" {
#endif

struct obs_script;
typedef struct obs_script obs_script_t;

enum obs_script_lang {
	OBS_SCRIPT_LANG_UNKNOWN,
	OBS_SCRIPT_LANG_LUA,
	OBS_SCRIPT_LANG_PYTHON
};

EXPORT bool obs_scripting_load(void);
EXPORT void obs_scripting_unload(void);
EXPORT const char **obs_scripting_supported_formats(void);

typedef void (*scripting_log_handler_t)(void *p, obs_script_t *script, int lvl,
					const char *msg);

EXPORT void obs_scripting_set_log_callback(scripting_log_handler_t handler,
					   void *param);

EXPORT bool obs_scripting_python_runtime_linked(void);
EXPORT bool obs_scripting_python_loaded(void);
EXPORT bool obs_scripting_load_python(const char *python_path);

EXPORT obs_script_t *obs_script_create(const char *path, obs_data_t *settings);
EXPORT void obs_script_destroy(obs_script_t *script);

EXPORT const char *obs_script_get_description(const obs_script_t *script);
EXPORT const char *obs_script_get_path(const obs_script_t *script);
EXPORT const char *obs_script_get_file(const obs_script_t *script);
EXPORT enum obs_script_lang obs_script_get_lang(const obs_script_t *script);

EXPORT obs_properties_t *obs_script_get_properties(obs_script_t *script);
EXPORT obs_data_t *obs_script_save(obs_script_t *script);
EXPORT obs_data_t *obs_script_get_settings(obs_script_t *script);
EXPORT void obs_script_update(obs_script_t *script, obs_data_t *settings);

EXPORT bool obs_script_loaded(const obs_script_t *script);
EXPORT bool obs_script_reload(obs_script_t *script);

#ifdef __cplusplus
}
#endif
