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

#include <obs.h>
#include <util/dstr.h>
#include <util/platform.h>
#include <util/threading.h>
#include <util/circlebuf.h>

#include "obs-scripting-internal.h"
#include "obs-scripting-callback.h"
#include "obs-scripting-config.h"

#if COMPILE_LUA
extern obs_script_t *obs_lua_script_create(const char *path,
					   obs_data_t *settings);
extern bool obs_lua_script_load(obs_script_t *s);
extern void obs_lua_script_unload(obs_script_t *s);
extern void obs_lua_script_destroy(obs_script_t *s);
extern void obs_lua_load(void);
extern void obs_lua_unload(void);

extern obs_properties_t *obs_lua_script_get_properties(obs_script_t *script);
extern void obs_lua_script_update(obs_script_t *script, obs_data_t *settings);
extern void obs_lua_script_save(obs_script_t *script);
#endif

#if COMPILE_PYTHON
extern obs_script_t *obs_python_script_create(const char *path,
					      obs_data_t *settings);
extern bool obs_python_script_load(obs_script_t *s);
extern void obs_python_script_unload(obs_script_t *s);
extern void obs_python_script_destroy(obs_script_t *s);
extern void obs_python_load(void);
extern void obs_python_unload(void);

extern obs_properties_t *obs_python_script_get_properties(obs_script_t *script);
extern void obs_python_script_update(obs_script_t *script,
				     obs_data_t *settings);
extern void obs_python_script_save(obs_script_t *script);
#endif

pthread_mutex_t detach_mutex;
struct script_callback *detached_callbacks;

static struct dstr file_filter = {0};
static bool scripting_loaded = false;

static const char *supported_formats[] = {
#if COMPILE_LUA
	"lua",
#endif
#if COMPILE_PYTHON
	"py",
#endif
	NULL};

/* -------------------------------------------- */

static pthread_mutex_t defer_call_mutex;
static struct circlebuf defer_call_queue;
static bool defer_call_exit = false;
static os_sem_t *defer_call_semaphore;
static pthread_t defer_call_thread;

struct defer_call {
	defer_call_cb call;
	void *cb;
};

static void *defer_thread(void *unused)
{
	UNUSED_PARAMETER(unused);

	while (os_sem_wait(defer_call_semaphore) == 0) {
		struct defer_call info;

		pthread_mutex_lock(&defer_call_mutex);
		if (defer_call_exit) {
			pthread_mutex_unlock(&defer_call_mutex);
			return NULL;
		}

		circlebuf_pop_front(&defer_call_queue, &info, sizeof(info));
		pthread_mutex_unlock(&defer_call_mutex);

		info.call(info.cb);
	}

	return NULL;
}

void defer_call_post(defer_call_cb call, void *cb)
{
	struct defer_call info;
	info.call = call;
	info.cb = cb;

	pthread_mutex_lock(&defer_call_mutex);
	if (!defer_call_exit)
		circlebuf_push_back(&defer_call_queue, &info, sizeof(info));
	pthread_mutex_unlock(&defer_call_mutex);

	os_sem_post(defer_call_semaphore);
}

/* -------------------------------------------- */

bool obs_scripting_load(void)
{
	circlebuf_init(&defer_call_queue);

	if (pthread_mutex_init(&detach_mutex, NULL) != 0) {
		return false;
	}
	if (pthread_mutex_init(&defer_call_mutex, NULL) != 0) {
		pthread_mutex_destroy(&detach_mutex);
		return false;
	}
	if (os_sem_init(&defer_call_semaphore, 0) != 0) {
		pthread_mutex_destroy(&defer_call_mutex);
		pthread_mutex_destroy(&detach_mutex);
		return false;
	}

	if (pthread_create(&defer_call_thread, NULL, defer_thread, NULL) != 0) {
		os_sem_destroy(defer_call_semaphore);
		pthread_mutex_destroy(&defer_call_mutex);
		pthread_mutex_destroy(&detach_mutex);
		return false;
	}

#if COMPILE_LUA
	obs_lua_load();
#endif

#if COMPILE_PYTHON
	obs_python_load();
#ifndef _WIN32 /* don't risk python startup load issues on windows */
	obs_scripting_load_python(NULL);
#endif
#endif

	scripting_loaded = true;
	return true;
}

void obs_scripting_unload(void)
{
	if (!scripting_loaded)
		return;

		/* ---------------------- */

#if COMPILE_LUA
	obs_lua_unload();
#endif

#if COMPILE_PYTHON
	obs_python_unload();
#endif

	dstr_free(&file_filter);

	/* ---------------------- */

	int total_detached = 0;

	pthread_mutex_lock(&detach_mutex);

	struct script_callback *cur = detached_callbacks;
	while (cur) {
		struct script_callback *next = cur->next;
		just_free_script_callback(cur);
		cur = next;

		++total_detached;
	}

	pthread_mutex_unlock(&detach_mutex);
	pthread_mutex_destroy(&detach_mutex);

	blog(LOG_INFO, "[Scripting] Total detached callbacks: %d",
	     total_detached);

	/* ---------------------- */

	pthread_mutex_lock(&defer_call_mutex);

	/* TODO */

	defer_call_exit = true;
	circlebuf_free(&defer_call_queue);

	pthread_mutex_unlock(&defer_call_mutex);

	os_sem_post(defer_call_semaphore);
	pthread_join(defer_call_thread, NULL);

	pthread_mutex_destroy(&defer_call_mutex);
	os_sem_destroy(defer_call_semaphore);

	scripting_loaded = false;
}

const char **obs_scripting_supported_formats(void)
{
	return supported_formats;
}

static inline bool pointer_valid(const void *x, const char *name,
				 const char *func)
{
	if (!x) {
		blog(LOG_WARNING, "obs-scripting: [%s] %s is null", func, name);
		return false;
	}

	return true;
}

#define ptr_valid(x) pointer_valid(x, #x, __FUNCTION__)

obs_script_t *obs_script_create(const char *path, obs_data_t *settings)
{
	obs_script_t *script = NULL;
	const char *ext;

	if (!scripting_loaded)
		return NULL;
	if (!ptr_valid(path))
		return NULL;

	ext = strrchr(path, '.');
	if (!ext)
		return NULL;

#if COMPILE_LUA
	if (strcmp(ext, ".lua") == 0) {
		script = obs_lua_script_create(path, settings);
	} else
#endif
#if COMPILE_PYTHON
		if (strcmp(ext, ".py") == 0) {
		script = obs_python_script_create(path, settings);
	} else
#endif
	{
		blog(LOG_WARNING, "Unsupported/unknown script type: %s", path);
	}

	return script;
}

const char *obs_script_get_description(const obs_script_t *script)
{
	return ptr_valid(script) ? script->desc.array : NULL;
}

const char *obs_script_get_path(const obs_script_t *script)
{
	const char *path = ptr_valid(script) ? script->path.array : "";
	return path ? path : "";
}

const char *obs_script_get_file(const obs_script_t *script)
{
	const char *file = ptr_valid(script) ? script->file.array : "";
	return file ? file : "";
}

enum obs_script_lang obs_script_get_lang(const obs_script_t *script)
{
	return ptr_valid(script) ? script->type : OBS_SCRIPT_LANG_UNKNOWN;
}

obs_data_t *obs_script_get_settings(obs_script_t *script)
{
	obs_data_t *settings;

	if (!ptr_valid(script))
		return NULL;

	settings = script->settings;
	obs_data_addref(settings);
	return settings;
}

obs_properties_t *obs_script_get_properties(obs_script_t *script)
{
	obs_properties_t *props = NULL;

	if (!ptr_valid(script))
		return NULL;
#if COMPILE_LUA
	if (script->type == OBS_SCRIPT_LANG_LUA) {
		props = obs_lua_script_get_properties(script);
		goto out;
	}
#endif
#if COMPILE_PYTHON
	if (script->type == OBS_SCRIPT_LANG_PYTHON) {
		props = obs_python_script_get_properties(script);
		goto out;
	}
#endif

out:
	if (!props)
		props = obs_properties_create();
	return props;
}

obs_data_t *obs_script_save(obs_script_t *script)
{
	obs_data_t *settings;

	if (!ptr_valid(script))
		return NULL;

#if COMPILE_LUA
	if (script->type == OBS_SCRIPT_LANG_LUA) {
		obs_lua_script_save(script);
		goto out;
	}
#endif
#if COMPILE_PYTHON
	if (script->type == OBS_SCRIPT_LANG_PYTHON) {
		obs_python_script_save(script);
		goto out;
	}
#endif

out:
	settings = script->settings;
	obs_data_addref(settings);
	return settings;
}

static void clear_queue_signal(void *p_event)
{
	os_event_t *event = p_event;
	os_event_signal(event);
}

static void clear_call_queue(void)
{
	os_event_t *event;
	if (os_event_init(&event, OS_EVENT_TYPE_AUTO) != 0)
		return;

	defer_call_post(clear_queue_signal, event);

	os_event_wait(event);
	os_event_destroy(event);
}

void obs_script_update(obs_script_t *script, obs_data_t *settings)
{
	if (!ptr_valid(script))
		return;
#if COMPILE_LUA
	if (script->type == OBS_SCRIPT_LANG_LUA) {
		obs_lua_script_update(script, settings);
	}
#endif
#if COMPILE_PYTHON
	if (script->type == OBS_SCRIPT_LANG_PYTHON) {
		obs_python_script_update(script, settings);
	}
#endif
}

bool obs_script_reload(obs_script_t *script)
{
	if (!scripting_loaded)
		return false;
	if (!ptr_valid(script))
		return false;

#if COMPILE_LUA
	if (script->type == OBS_SCRIPT_LANG_LUA) {
		obs_lua_script_unload(script);
		clear_call_queue();
		obs_lua_script_load(script);
		goto out;
	}
#endif
#if COMPILE_PYTHON
	if (script->type == OBS_SCRIPT_LANG_PYTHON) {
		obs_python_script_unload(script);
		clear_call_queue();
		obs_python_script_load(script);
		goto out;
	}
#endif

out:
	return script->loaded;
}

bool obs_script_loaded(const obs_script_t *script)
{
	return ptr_valid(script) ? script->loaded : false;
}

void obs_script_destroy(obs_script_t *script)
{
	if (!script)
		return;

#if COMPILE_LUA
	if (script->type == OBS_SCRIPT_LANG_LUA) {
		obs_lua_script_unload(script);
		obs_lua_script_destroy(script);
		return;
	}
#endif
#if COMPILE_PYTHON
	if (script->type == OBS_SCRIPT_LANG_PYTHON) {
		obs_python_script_unload(script);
		obs_python_script_destroy(script);
		return;
	}
#endif
}

#if !COMPILE_PYTHON
bool obs_scripting_load_python(const char *python_path)
{
	UNUSED_PARAMETER(python_path);
	return false;
}

bool obs_scripting_python_loaded(void)
{
	return false;
}

bool obs_scripting_python_runtime_linked(void)
{
	return (bool)true;
}
#endif
