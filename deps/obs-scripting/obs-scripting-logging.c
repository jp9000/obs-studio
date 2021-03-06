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

#include "obs-scripting-internal.h"
#include <util/platform.h>

static scripting_log_handler_t callback = NULL;
static void *param = NULL;

void script_log_va(obs_script_t *script, int level, const char *format,
		   va_list args)
{
	char msg[2048];
	const char *lang = "(Unknown)";
	size_t start_len;

	if (script) {
		switch (script->type) {
		case OBS_SCRIPT_LANG_UNKNOWN:
			lang = "(Unknown language)";
			break;
		case OBS_SCRIPT_LANG_LUA:
			lang = "Lua";
			break;
		case OBS_SCRIPT_LANG_PYTHON:
			lang = "Python";
			break;
		}

		start_len = snprintf(msg, sizeof(msg), "[%s: %s] ", lang,
				     script->file.array);
	} else {
		start_len = snprintf(msg, sizeof(msg), "[Unknown Script] ");
	}

	vsnprintf(msg + start_len, sizeof(msg) - start_len, format, args);

	if (callback)
		callback(param, script, level, msg + start_len);
	blog(level, "%s", msg);
}

void script_log(obs_script_t *script, int level, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	script_log_va(script, level, format, args);
	va_end(args);
}

void obs_scripting_set_log_callback(scripting_log_handler_t handler,
				    void *log_param)
{
	callback = handler;
	param = log_param;
}
