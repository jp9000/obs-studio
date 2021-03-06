/******************************************************************************
    Copyright (C) 2014 by Ruwen Hahn <palana@stunned.de>

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

#include "../util/c99defs.h"

#pragma once

struct media_remux_job;
typedef struct media_remux_job *media_remux_job_t;

typedef bool(media_remux_progress_callback)(void *data, float percent);

#ifdef __cplusplus
extern "C" {
#endif

EXPORT bool media_remux_job_create(media_remux_job_t *job,
				   const char *in_filename,
				   const char *out_filename);
EXPORT bool media_remux_job_process(media_remux_job_t job,
				    media_remux_progress_callback callback,
				    void *data);
EXPORT void media_remux_job_destroy(media_remux_job_t job);

#ifdef __cplusplus
}
#endif
