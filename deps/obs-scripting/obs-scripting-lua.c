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

#include "obs-scripting-lua.h"
#include "obs-scripting-config.h"
#include <util/platform.h>
#include <util/base.h>
#include <util/dstr.h>

#include <obs.h>

/* ========================================================================= */

#if ARCH_BITS == 64
#define ARCH_DIR "64bit"
#else
#define ARCH_DIR "32bit"
#endif

#ifdef __APPLE__
#define SO_EXT "dylib"
#elif _WIN32
#define SO_EXT "dll"
#else
#define SO_EXT "so"
#endif

static const char *startup_script_template = "\
for val in pairs(package.preload) do\n\
	package.preload[val] = nil\n\
end\n\
package.cpath = package.cpath .. \";\" .. \"%s/Contents/MacOS/?.so\" .. \";\" .. \"%s\" .. \"/?." SO_EXT
					     "\"\n\
require \"obslua\"\n";

static const char *get_script_path_func = "\
function script_path()\n\
	 return \"%s\"\n\
end\n\
package.path = package.path .. \";\" .. script_path() .. \"/?.lua\"\n";

static char *startup_script = NULL;

static pthread_mutex_t tick_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct obs_lua_script *first_tick_script = NULL;

pthread_mutex_t lua_source_def_mutex = PTHREAD_MUTEX_INITIALIZER;

#define ls_get_libobs_obj(type, lua_index, obs_obj)                      \
	ls_get_libobs_obj_(script, #type " *", lua_index, obs_obj, NULL, \
			   __FUNCTION__, __LINE__)
#define ls_push_libobs_obj(type, obs_obj, ownership)                      \
	ls_push_libobs_obj_(script, #type " *", obs_obj, ownership, NULL, \
			    __FUNCTION__, __LINE__)
#define call_func(name, args, rets) \
	call_func_(script, cb->reg_idx, args, rets, #name, __FUNCTION__)

/* ========================================================================= */

static void add_hook_functions(lua_State *script);
static int obs_lua_remove_tick_callback(lua_State *script);
static int obs_lua_remove_main_render_callback(lua_State *script);

#if UI_ENABLED
void add_lua_frontend_funcs(lua_State *script);
#endif

static bool load_lua_script(struct obs_lua_script *data)
{
	struct dstr str = {0};
	bool success = false;
	int ret;

	lua_State *script = luaL_newstate();
	if (!script) {
		script_warn(&data->base, "Failed to create new lua state");
		goto fail;
	}

	pthread_mutex_lock(&data->mutex);

	luaL_openlibs(script);
	luaopen_ffi(script);

	if (luaL_dostring(script, startup_script) != 0) {
		script_warn(&data->base, "Error executing startup script 1: %s",
			    lua_tostring(script, -1));
		goto fail;
	}

	dstr_printf(&str, get_script_path_func, data->dir.array);
	ret = luaL_dostring(script, str.array);
	dstr_free(&str);

	if (ret != 0) {
		script_warn(&data->base, "Error executing startup script 2: %s",
			    lua_tostring(script, -1));
		goto fail;
	}

	current_lua_script = data;

	add_lua_source_functions(script);
	add_hook_functions(script);
#if UI_ENABLED
	add_lua_frontend_funcs(script);
#endif

	if (luaL_loadfile(script, data->base.path.array) != 0) {
		script_warn(&data->base, "Error loading file: %s",
			    lua_tostring(script, -1));
		goto fail;
	}

	if (lua_pcall(script, 0, LUA_MULTRET, 0) != 0) {
		script_warn(&data->base, "Error running file: %s",
			    lua_tostring(script, -1));
		goto fail;
	}

	ret = lua_gettop(script);
	if (ret == 1 && lua_isboolean(script, -1)) {
		bool success = lua_toboolean(script, -1);

		if (!success) {
			goto fail;
		}
	}

	lua_getglobal(script, "script_tick");
	if (lua_isfunction(script, -1)) {
		pthread_mutex_lock(&tick_mutex);

		struct obs_lua_script *next = first_tick_script;
		data->next_tick = next;
		data->p_prev_next_tick = &first_tick_script;
		if (next)
			next->p_prev_next_tick = &data->next_tick;
		first_tick_script = data;

		data->tick = luaL_ref(script, LUA_REGISTRYINDEX);

		pthread_mutex_unlock(&tick_mutex);
	}

	lua_getglobal(script, "script_properties");
	if (lua_isfunction(script, -1))
		data->get_properties = luaL_ref(script, LUA_REGISTRYINDEX);
	else
		data->get_properties = LUA_REFNIL;

	lua_getglobal(script, "script_update");
	if (lua_isfunction(script, -1))
		data->update = luaL_ref(script, LUA_REGISTRYINDEX);
	else
		data->update = LUA_REFNIL;

	lua_getglobal(script, "script_save");
	if (lua_isfunction(script, -1))
		data->save = luaL_ref(script, LUA_REGISTRYINDEX);
	else
		data->save = LUA_REFNIL;

	lua_getglobal(script, "script_defaults");
	if (lua_isfunction(script, -1)) {
		ls_push_libobs_obj(obs_data_t, data->base.settings, false);
		if (lua_pcall(script, 1, 0, 0) != 0) {
			script_warn(&data->base,
				    "Error calling "
				    "script_defaults: %s",
				    lua_tostring(script, -1));
		}
	}

	lua_getglobal(script, "script_description");
	if (lua_isfunction(script, -1)) {
		if (lua_pcall(script, 0, 1, 0) != 0) {
			script_warn(&data->base,
				    "Error calling "
				    "script_defaults: %s",
				    lua_tostring(script, -1));
		} else {
			const char *desc = lua_tostring(script, -1);
			dstr_copy(&data->base.desc, desc);
		}
	}

	lua_getglobal(script, "script_load");
	if (lua_isfunction(script, -1)) {
		ls_push_libobs_obj(obs_data_t, data->base.settings, false);
		if (lua_pcall(script, 1, 0, 0) != 0) {
			script_warn(&data->base,
				    "Error calling "
				    "script_load: %s",
				    lua_tostring(script, -1));
		}
	}

	data->script = script;
	success = true;

fail:
	if (script) {
		lua_settop(script, 0);
		pthread_mutex_unlock(&data->mutex);
	}

	if (!success && script) {
		lua_close(script);
	}

	current_lua_script = NULL;
	return success;
}

/* -------------------------------------------- */

THREAD_LOCAL struct lua_obs_callback *current_lua_cb = NULL;
THREAD_LOCAL struct obs_lua_script *current_lua_script = NULL;

/* -------------------------------------------- */

struct lua_obs_timer {
	struct lua_obs_timer *next;
	struct lua_obs_timer **p_prev_next;

	uint64_t last_ts;
	uint64_t interval;
};

static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct lua_obs_timer *first_timer = NULL;

static inline void lua_obs_timer_init(struct lua_obs_timer *timer)
{
	pthread_mutex_lock(&timer_mutex);

	struct lua_obs_timer *next = first_timer;
	timer->next = next;
	timer->p_prev_next = &first_timer;
	if (next)
		next->p_prev_next = &timer->next;
	first_timer = timer;

	pthread_mutex_unlock(&timer_mutex);
}

static inline void lua_obs_timer_remove(struct lua_obs_timer *timer)
{
	struct lua_obs_timer *next = timer->next;
	if (next)
		next->p_prev_next = timer->p_prev_next;
	*timer->p_prev_next = timer->next;
}

static inline struct lua_obs_callback *
lua_obs_timer_cb(struct lua_obs_timer *timer)
{
	return &((struct lua_obs_callback *)timer)[-1];
}

static int timer_remove(lua_State *script)
{
	if (!is_function(script, 1))
		return 0;

	struct lua_obs_callback *cb = find_lua_obs_callback(script, 1);
	if (cb)
		remove_lua_obs_callback(cb);
	return 0;
}

static void timer_call(struct script_callback *p_cb)
{
	struct lua_obs_callback *cb = (struct lua_obs_callback *)p_cb;

	if (p_cb->removed)
		return;

	lock_callback();
	call_func_(cb->script, cb->reg_idx, 0, 0, "timer_cb", __FUNCTION__);
	unlock_callback();
}

static void defer_timer_init(void *p_cb)
{
	struct lua_obs_callback *cb = p_cb;
	struct lua_obs_timer *timer = lua_obs_callback_extra_data(cb);
	lua_obs_timer_init(timer);
}

static int timer_add(lua_State *script)
{
	if (!is_function(script, 1))
		return 0;
	int ms = (int)lua_tointeger(script, 2);
	if (!ms)
		return 0;

	struct lua_obs_callback *cb = add_lua_obs_callback_extra(
		script, 1, sizeof(struct lua_obs_timer));
	struct lua_obs_timer *timer = lua_obs_callback_extra_data(cb);

	timer->interval = (uint64_t)ms * 1000000ULL;
	timer->last_ts = obs_get_video_frame_time();

	defer_call_post(defer_timer_init, cb);
	return 0;
}

/* -------------------------------------------- */

static void obs_lua_main_render_callback(void *priv, uint32_t cx, uint32_t cy)
{
	struct lua_obs_callback *cb = priv;
	lua_State *script = cb->script;

	if (cb->base.removed) {
		obs_remove_main_render_callback(obs_lua_main_render_callback,
						cb);
		return;
	}

	lock_callback();

	lua_pushinteger(script, (lua_Integer)cx);
	lua_pushinteger(script, (lua_Integer)cy);
	call_func(obs_lua_main_render_callback, 2, 0);

	unlock_callback();
}

static int obs_lua_remove_main_render_callback(lua_State *script)
{
	if (!verify_args1(script, is_function))
		return 0;

	struct lua_obs_callback *cb = find_lua_obs_callback(script, 1);
	if (cb)
		remove_lua_obs_callback(cb);
	return 0;
}

static void defer_add_render(void *cb)
{
	obs_add_main_render_callback(obs_lua_main_render_callback, cb);
}

static int obs_lua_add_main_render_callback(lua_State *script)
{
	if (!verify_args1(script, is_function))
		return 0;

	struct lua_obs_callback *cb = add_lua_obs_callback(script, 1);
	defer_call_post(defer_add_render, cb);
	return 0;
}

/* -------------------------------------------- */

static void obs_lua_tick_callback(void *priv, float seconds)
{
	struct lua_obs_callback *cb = priv;
	lua_State *script = cb->script;

	if (cb->base.removed) {
		obs_remove_tick_callback(obs_lua_tick_callback, cb);
		return;
	}

	lock_callback();

	lua_pushnumber(script, (lua_Number)seconds);
	call_func(obs_lua_tick_callback, 1, 0);

	unlock_callback();
}

static int obs_lua_remove_tick_callback(lua_State *script)
{
	if (!verify_args1(script, is_function))
		return 0;

	struct lua_obs_callback *cb = find_lua_obs_callback(script, 1);
	if (cb)
		remove_lua_obs_callback(cb);
	return 0;
}

static void defer_add_tick(void *cb)
{
	obs_add_tick_callback(obs_lua_tick_callback, cb);
}

static int obs_lua_add_tick_callback(lua_State *script)
{
	if (!verify_args1(script, is_function))
		return 0;

	struct lua_obs_callback *cb = add_lua_obs_callback(script, 1);
	defer_call_post(defer_add_tick, cb);
	return 0;
}

/* -------------------------------------------- */

static void calldata_signal_callback(void *priv, calldata_t *cd)
{
	struct lua_obs_callback *cb = priv;
	lua_State *script = cb->script;

	if (cb->base.removed) {
		signal_handler_remove_current();
		return;
	}

	lock_callback();

	ls_push_libobs_obj(calldata_t, cd, false);
	call_func(calldata_signal_callback, 1, 0);

	unlock_callback();
}

static int obs_lua_signal_handler_disconnect(lua_State *script)
{
	signal_handler_t *handler;
	const char *signal;

	if (!ls_get_libobs_obj(signal_handler_t, 1, &handler))
		return 0;
	signal = lua_tostring(script, 2);
	if (!signal)
		return 0;
	if (!is_function(script, 3))
		return 0;

	struct lua_obs_callback *cb = find_lua_obs_callback(script, 3);
	while (cb) {
		signal_handler_t *cb_handler =
			calldata_ptr(&cb->base.extra, "handler");
		const char *cb_signal =
			calldata_string(&cb->base.extra, "signal");

		if (cb_signal && strcmp(signal, cb_signal) == 0 &&
		    handler == cb_handler)
			break;

		cb = find_next_lua_obs_callback(script, cb, 3);
	}

	if (cb)
		remove_lua_obs_callback(cb);
	return 0;
}

static void defer_connect(void *p_cb)
{
	struct script_callback *cb = p_cb;

	signal_handler_t *handler = calldata_ptr(&cb->extra, "handler");
	const char *signal = calldata_string(&cb->extra, "signal");
	signal_handler_connect(handler, signal, calldata_signal_callback, cb);
}

static int obs_lua_signal_handler_connect(lua_State *script)
{
	signal_handler_t *handler;
	const char *signal;

	if (!ls_get_libobs_obj(signal_handler_t, 1, &handler))
		return 0;
	signal = lua_tostring(script, 2);
	if (!signal)
		return 0;
	if (!is_function(script, 3))
		return 0;

	struct lua_obs_callback *cb = add_lua_obs_callback(script, 3);
	calldata_set_ptr(&cb->base.extra, "handler", handler);
	calldata_set_string(&cb->base.extra, "signal", signal);
	defer_call_post(defer_connect, cb);
	return 0;
}

/* -------------------------------------------- */

static void calldata_signal_callback_global(void *priv, const char *signal,
					    calldata_t *cd)
{
	struct lua_obs_callback *cb = priv;
	lua_State *script = cb->script;

	if (cb->base.removed) {
		signal_handler_remove_current();
		return;
	}

	lock_callback();

	lua_pushstring(script, signal);
	ls_push_libobs_obj(calldata_t, cd, false);
	call_func(calldata_signal_callback_global, 2, 0);

	unlock_callback();
}

static int obs_lua_signal_handler_disconnect_global(lua_State *script)
{
	signal_handler_t *handler;

	if (!ls_get_libobs_obj(signal_handler_t, 1, &handler))
		return 0;
	if (!is_function(script, 2))
		return 0;

	struct lua_obs_callback *cb = find_lua_obs_callback(script, 3);
	while (cb) {
		signal_handler_t *cb_handler =
			calldata_ptr(&cb->base.extra, "handler");

		if (handler == cb_handler)
			break;

		cb = find_next_lua_obs_callback(script, cb, 3);
	}

	if (cb)
		remove_lua_obs_callback(cb);
	return 0;
}

static void defer_connect_global(void *p_cb)
{
	struct script_callback *cb = p_cb;

	signal_handler_t *handler = calldata_ptr(&cb->extra, "handler");
	signal_handler_connect_global(handler, calldata_signal_callback_global,
				      cb);
}

static int obs_lua_signal_handler_connect_global(lua_State *script)
{
	signal_handler_t *handler;

	if (!ls_get_libobs_obj(signal_handler_t, 1, &handler))
		return 0;
	if (!is_function(script, 2))
		return 0;

	struct lua_obs_callback *cb = add_lua_obs_callback(script, 2);
	calldata_set_ptr(&cb->base.extra, "handler", handler);
	defer_call_post(defer_connect_global, cb);
	return 0;
}

/* -------------------------------------------- */

static bool enum_sources_proc(void *param, obs_source_t *source)
{
	lua_State *script = param;

	obs_source_get_ref(source);
	ls_push_libobs_obj(obs_source_t, source, false);

	size_t idx = lua_rawlen(script, -2);
	lua_rawseti(script, -2, (int)idx + 1);
	return true;
}

static int enum_sources(lua_State *script)
{
	lua_newtable(script);
	obs_enum_sources(enum_sources_proc, script);
	return 1;
}

/* -------------------------------------------- */

static void source_enum_filters_proc(obs_source_t *source, obs_source_t *filter,
				     void *param)
{
	UNUSED_PARAMETER(source);

	lua_State *script = param;

	obs_source_get_ref(filter);
	ls_push_libobs_obj(obs_source_t, filter, false);

	size_t idx = lua_rawlen(script, -2);
	lua_rawseti(script, -2, (int)idx + 1);
}

static int source_enum_filters(lua_State *script)
{
	obs_source_t *source;
	if (!ls_get_libobs_obj(obs_source_t, 1, &source))
		return 0;

	lua_newtable(script);
	obs_source_enum_filters(source, source_enum_filters_proc, script);
	return 1;
}

/* -------------------------------------------- */

static bool enum_items_proc(obs_scene_t *scene, obs_sceneitem_t *item,
			    void *param)
{
	lua_State *script = param;

	UNUSED_PARAMETER(scene);

	obs_sceneitem_addref(item);
	ls_push_libobs_obj(obs_sceneitem_t, item, false);
	lua_rawseti(script, -2, (int)lua_rawlen(script, -2) + 1);
	return true;
}

static int scene_enum_items(lua_State *script)
{
	obs_scene_t *scene;
	if (!ls_get_libobs_obj(obs_scene_t, 1, &scene))
		return 0;

	lua_newtable(script);
	obs_scene_enum_items(scene, enum_items_proc, script);
	return 1;
}

/* -------------------------------------------- */

static void defer_hotkey_unregister(void *p_cb)
{
	obs_hotkey_unregister((obs_hotkey_id)(uintptr_t)p_cb);
}

static void on_remove_hotkey(void *p_cb)
{
	struct lua_obs_callback *cb = p_cb;
	obs_hotkey_id id = (obs_hotkey_id)calldata_int(&cb->base.extra, "id");

	if (id != OBS_INVALID_HOTKEY_ID)
		defer_call_post(defer_hotkey_unregister, (void *)(uintptr_t)id);
}

static void hotkey_pressed(void *p_cb, bool pressed)
{
	struct lua_obs_callback *cb = p_cb;
	lua_State *script = cb->script;

	if (cb->base.removed)
		return;

	lock_callback();

	lua_pushboolean(script, pressed);
	call_func(hotkey_pressed, 1, 0);

	unlock_callback();
}

static void defer_hotkey_pressed(void *p_cb)
{
	hotkey_pressed(p_cb, true);
}

static void defer_hotkey_unpressed(void *p_cb)
{
	hotkey_pressed(p_cb, false);
}

static void hotkey_callback(void *p_cb, obs_hotkey_id id, obs_hotkey_t *hotkey,
			    bool pressed)
{
	struct lua_obs_callback *cb = p_cb;

	if (cb->base.removed)
		return;

	if (pressed)
		defer_call_post(defer_hotkey_pressed, cb);
	else
		defer_call_post(defer_hotkey_unpressed, cb);

	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(id);
}

static int hotkey_unregister(lua_State *script)
{
	if (!verify_args1(script, is_function))
		return 0;

	struct lua_obs_callback *cb = find_lua_obs_callback(script, 1);
	if (cb)
		remove_lua_obs_callback(cb);
	return 0;
}

static int hotkey_register_frontend(lua_State *script)
{
	obs_hotkey_id id;

	const char *name = lua_tostring(script, 1);
	if (!name)
		return 0;
	const char *desc = lua_tostring(script, 2);
	if (!desc)
		return 0;
	if (!is_function(script, 3))
		return 0;

	struct lua_obs_callback *cb = add_lua_obs_callback(script, 3);
	cb->base.on_remove = on_remove_hotkey;
	id = obs_hotkey_register_frontend(name, desc, hotkey_callback, cb);
	calldata_set_int(&cb->base.extra, "id", id);
	lua_pushinteger(script, (lua_Integer)id);

	if (id == OBS_INVALID_HOTKEY_ID)
		remove_lua_obs_callback(cb);
	return 1;
}

/* -------------------------------------------- */

static bool button_prop_clicked(obs_properties_t *props, obs_property_t *p,
				void *p_cb)
{
	struct lua_obs_callback *cb = p_cb;
	lua_State *script = cb->script;
	bool ret = false;

	if (cb->base.removed)
		return false;

	lock_callback();

	if (!ls_push_libobs_obj(obs_properties_t, props, false))
		goto fail;
	if (!ls_push_libobs_obj(obs_property_t, p, false)) {
		lua_pop(script, 1);
		goto fail;
	}

	call_func(button_prop_clicked, 2, 1);
	if (lua_isboolean(script, -1))
		ret = lua_toboolean(script, -1);

fail:
	unlock_callback();

	return ret;
}

static int properties_add_button(lua_State *script)
{
	obs_properties_t *props;
	obs_property_t *p;

	if (!ls_get_libobs_obj(obs_properties_t, 1, &props))
		return 0;
	const char *name = lua_tostring(script, 2);
	if (!name)
		return 0;
	const char *text = lua_tostring(script, 3);
	if (!text)
		return 0;
	if (!is_function(script, 4))
		return 0;

	struct lua_obs_callback *cb = add_lua_obs_callback(script, 4);
	p = obs_properties_add_button2(props, name, text, button_prop_clicked,
				       cb);

	if (!p || !ls_push_libobs_obj(obs_property_t, p, false))
		return 0;
	return 1;
}

/* -------------------------------------------- */

static bool modified_callback(void *p_cb, obs_properties_t *props,
			      obs_property_t *p, obs_data_t *settings)
{
	struct lua_obs_callback *cb = p_cb;
	lua_State *script = cb->script;
	bool ret = false;

	if (cb->base.removed)
		return false;

	lock_callback();
	if (!ls_push_libobs_obj(obs_properties_t, props, false)) {
		goto fail;
	}
	if (!ls_push_libobs_obj(obs_property_t, p, false)) {
		lua_pop(script, 1);
		goto fail;
	}
	if (!ls_push_libobs_obj(obs_data_t, settings, false)) {
		lua_pop(script, 2);
		goto fail;
	}

	call_func(modified_callback, 3, 1);
	if (lua_isboolean(script, -1))
		ret = lua_toboolean(script, -1);

fail:
	unlock_callback();
	return ret;
}

static int property_set_modified_callback(lua_State *script)
{
	obs_property_t *p;

	if (!ls_get_libobs_obj(obs_property_t, 1, &p))
		return 0;
	if (!is_function(script, 2))
		return 0;

	struct lua_obs_callback *cb = add_lua_obs_callback(script, 2);
	obs_property_set_modified_callback2(p, modified_callback, cb);
	return 0;
}

/* -------------------------------------------- */

static int remove_current_callback(lua_State *script)
{
	UNUSED_PARAMETER(script);
	if (current_lua_cb)
		remove_lua_obs_callback(current_lua_cb);
	return 0;
}

/* -------------------------------------------- */

static int calldata_source(lua_State *script)
{
	calldata_t *cd;
	const char *str;
	int ret = 0;

	if (!ls_get_libobs_obj(calldata_t, 1, &cd))
		goto fail;
	str = lua_tostring(script, 2);
	if (!str)
		goto fail;

	obs_source_t *source = calldata_ptr(cd, str);
	if (ls_push_libobs_obj(obs_source_t, source, false))
		++ret;

fail:
	return ret;
}

static int calldata_sceneitem(lua_State *script)
{
	calldata_t *cd;
	const char *str;
	int ret = 0;

	if (!ls_get_libobs_obj(calldata_t, 1, &cd))
		goto fail;
	str = lua_tostring(script, 2);
	if (!str)
		goto fail;

	obs_sceneitem_t *sceneitem = calldata_ptr(cd, str);
	if (ls_push_libobs_obj(obs_sceneitem_t, sceneitem, false))
		++ret;

fail:
	return ret;
}

/* -------------------------------------------- */

static int source_list_release(lua_State *script)
{
	size_t count = lua_rawlen(script, 1);
	for (size_t i = 0; i < count; i++) {
		obs_source_t *source;

		lua_rawgeti(script, 1, (int)i + 1);
		ls_get_libobs_obj(obs_source_t, -1, &source);
		lua_pop(script, 1);

		obs_source_release(source);
	}
	return 0;
}

static int sceneitem_list_release(lua_State *script)
{
	size_t count = lua_rawlen(script, 1);
	for (size_t i = 0; i < count; i++) {
		obs_sceneitem_t *item;

		lua_rawgeti(script, 1, (int)i + 1);
		ls_get_libobs_obj(obs_sceneitem_t, -1, &item);
		lua_pop(script, 1);

		obs_sceneitem_release(item);
	}
	return 0;
}

/* -------------------------------------------- */

static int hook_print(lua_State *script)
{
	struct obs_lua_script *data = current_lua_script;
	const char *msg = lua_tostring(script, 1);
	if (!msg)
		return 0;

	script_info(&data->base, "%s", msg);
	return 0;
}

static int hook_error(lua_State *script)
{
	struct obs_lua_script *data = current_lua_script;
	const char *msg = lua_tostring(script, 1);
	if (!msg)
		return 0;

	script_error(&data->base, "%s", msg);
	return 0;
}

/* -------------------------------------------- */

static int lua_script_log(lua_State *script)
{
	struct obs_lua_script *data = current_lua_script;
	int log_level = (int)lua_tointeger(script, 1);
	const char *msg = lua_tostring(script, 2);

	if (!msg)
		return 0;

	/* ------------------- */

	dstr_copy(&data->log_chunk, msg);

	const char *start = data->log_chunk.array;
	char *endl = strchr(start, '\n');

	while (endl) {
		*endl = 0;
		script_log(&data->base, log_level, "%s", start);
		*endl = '\n';

		start = endl + 1;
		endl = strchr(start, '\n');
	}

	if (start && *start)
		script_log(&data->base, log_level, "%s", start);
	dstr_resize(&data->log_chunk, 0);

	/* ------------------- */

	return 0;
}

/* -------------------------------------------- */

static void add_hook_functions(lua_State *script)
{
#define add_func(name, func)                     \
	do {                                     \
		lua_pushstring(script, name);    \
		lua_pushcfunction(script, func); \
		lua_rawset(script, -3);          \
	} while (false)

	lua_getglobal(script, "_G");

	add_func("print", hook_print);
	add_func("error", hook_error);

	lua_pop(script, 1);

	/* ------------- */

	lua_getglobal(script, "obslua");

	add_func("script_log", lua_script_log);
	add_func("timer_remove", timer_remove);
	add_func("timer_add", timer_add);
	add_func("obs_enum_sources", enum_sources);
	add_func("obs_source_enum_filters", source_enum_filters);
	add_func("obs_scene_enum_items", scene_enum_items);
	add_func("source_list_release", source_list_release);
	add_func("sceneitem_list_release", sceneitem_list_release);
	add_func("calldata_source", calldata_source);
	add_func("calldata_sceneitem", calldata_sceneitem);
	add_func("obs_add_main_render_callback",
		 obs_lua_add_main_render_callback);
	add_func("obs_remove_main_render_callback",
		 obs_lua_remove_main_render_callback);
	add_func("obs_add_tick_callback", obs_lua_add_tick_callback);
	add_func("obs_remove_tick_callback", obs_lua_remove_tick_callback);
	add_func("signal_handler_connect", obs_lua_signal_handler_connect);
	add_func("signal_handler_disconnect",
		 obs_lua_signal_handler_disconnect);
	add_func("signal_handler_connect_global",
		 obs_lua_signal_handler_connect_global);
	add_func("signal_handler_disconnect_global",
		 obs_lua_signal_handler_disconnect_global);
	add_func("obs_hotkey_unregister", hotkey_unregister);
	add_func("obs_hotkey_register_frontend", hotkey_register_frontend);
	add_func("obs_properties_add_button", properties_add_button);
	add_func("obs_property_set_modified_callback",
		 property_set_modified_callback);
	add_func("remove_current_callback", remove_current_callback);

	lua_pop(script, 1);
#undef add_func
}

/* -------------------------------------------- */

static void lua_tick(void *param, float seconds)
{
	struct obs_lua_script *data;
	struct lua_obs_timer *timer;
	uint64_t ts = obs_get_video_frame_time();

	/* --------------------------------- */
	/* process script_tick calls         */

	pthread_mutex_lock(&tick_mutex);
	data = first_tick_script;
	while (data) {
		lua_State *script = data->script;
		current_lua_script = data;

		pthread_mutex_lock(&data->mutex);

		lua_pushnumber(script, (double)seconds);
		call_func_(script, data->tick, 1, 0, "tick", __FUNCTION__);

		pthread_mutex_unlock(&data->mutex);

		data = data->next_tick;
	}
	current_lua_script = NULL;
	pthread_mutex_unlock(&tick_mutex);

	/* --------------------------------- */
	/* process timers                    */

	pthread_mutex_lock(&timer_mutex);
	timer = first_timer;
	while (timer) {
		struct lua_obs_timer *next = timer->next;
		struct lua_obs_callback *cb = lua_obs_timer_cb(timer);

		if (cb->base.removed) {
			lua_obs_timer_remove(timer);
		} else {
			uint64_t elapsed = ts - timer->last_ts;

			if (elapsed >= timer->interval) {
				timer_call(&cb->base);
				timer->last_ts += timer->interval;
			}
		}

		timer = next;
	}
	pthread_mutex_unlock(&timer_mutex);

	UNUSED_PARAMETER(param);
}

/* -------------------------------------------- */

void obs_lua_script_update(obs_script_t *script, obs_data_t *settings);

bool obs_lua_script_load(obs_script_t *s)
{
	struct obs_lua_script *data = (struct obs_lua_script *)s;
	if (!data->base.loaded) {
		data->base.loaded = load_lua_script(data);
		if (data->base.loaded)
			obs_lua_script_update(s, NULL);
	}

	return data->base.loaded;
}

obs_script_t *obs_lua_script_create(const char *path, obs_data_t *settings)
{
	struct obs_lua_script *data = bzalloc(sizeof(*data));

	data->base.type = OBS_SCRIPT_LANG_LUA;
	data->tick = LUA_REFNIL;

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutex_init_value(&data->mutex);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	if (pthread_mutex_init(&data->mutex, &attr) != 0) {
		bfree(data);
		return NULL;
	}

	dstr_copy(&data->base.path, path);

	char *slash = path && *path ? strrchr(path, '/') : NULL;
	if (slash) {
		slash++;
		dstr_copy(&data->base.file, slash);
		dstr_left(&data->dir, &data->base.path, slash - path);
	} else {
		dstr_copy(&data->base.file, path);
	}

	data->base.settings = obs_data_create();
	if (settings)
		obs_data_apply(data->base.settings, settings);

	obs_lua_script_load((obs_script_t *)data);
	return (obs_script_t *)data;
}

extern void undef_lua_script_sources(struct obs_lua_script *data);

void obs_lua_script_unload(obs_script_t *s)
{
	struct obs_lua_script *data = (struct obs_lua_script *)s;

	if (!s->loaded)
		return;

	lua_State *script = data->script;

	/* ---------------------------- */
	/* undefine source types        */

	undef_lua_script_sources(data);

	/* ---------------------------- */
	/* unhook tick function         */

	if (data->p_prev_next_tick) {
		pthread_mutex_lock(&tick_mutex);

		struct obs_lua_script *next = data->next_tick;
		if (next)
			next->p_prev_next_tick = data->p_prev_next_tick;
		*data->p_prev_next_tick = next;

		pthread_mutex_unlock(&tick_mutex);

		data->p_prev_next_tick = NULL;
		data->next_tick = NULL;
	}

	/* ---------------------------- */
	/* call script_unload           */

	pthread_mutex_lock(&data->mutex);

	lua_getglobal(script, "script_unload");
	lua_pcall(script, 0, 0, 0);

	/* ---------------------------- */
	/* remove all callbacks         */

	struct lua_obs_callback *cb =
		(struct lua_obs_callback *)data->first_callback;
	while (cb) {
		struct lua_obs_callback *next =
			(struct lua_obs_callback *)cb->base.next;
		remove_lua_obs_callback(cb);
		cb = next;
	}

	pthread_mutex_unlock(&data->mutex);

	/* ---------------------------- */
	/* close script                 */

	lua_close(script);
	s->loaded = false;
}

void obs_lua_script_destroy(obs_script_t *s)
{
	struct obs_lua_script *data = (struct obs_lua_script *)s;

	if (data) {
		pthread_mutex_destroy(&data->mutex);
		dstr_free(&data->base.path);
		dstr_free(&data->base.file);
		dstr_free(&data->base.desc);
		obs_data_release(data->base.settings);
		dstr_free(&data->log_chunk);
		dstr_free(&data->dir);
		bfree(data);
	}
}

void obs_lua_script_update(obs_script_t *s, obs_data_t *settings)
{
	struct obs_lua_script *data = (struct obs_lua_script *)s;
	lua_State *script = data->script;

	if (!s->loaded)
		return;
	if (data->update == LUA_REFNIL)
		return;

	if (settings)
		obs_data_apply(s->settings, settings);

	current_lua_script = data;
	pthread_mutex_lock(&data->mutex);

	ls_push_libobs_obj(obs_data_t, s->settings, false);
	call_func_(script, data->update, 1, 0, "script_update", __FUNCTION__);

	pthread_mutex_unlock(&data->mutex);
	current_lua_script = NULL;
}

obs_properties_t *obs_lua_script_get_properties(obs_script_t *s)
{
	struct obs_lua_script *data = (struct obs_lua_script *)s;
	lua_State *script = data->script;
	obs_properties_t *props = NULL;

	if (!s->loaded)
		return NULL;
	if (data->get_properties == LUA_REFNIL)
		return NULL;

	current_lua_script = data;
	pthread_mutex_lock(&data->mutex);

	call_func_(script, data->get_properties, 0, 1, "script_properties",
		   __FUNCTION__);
	ls_get_libobs_obj(obs_properties_t, -1, &props);

	pthread_mutex_unlock(&data->mutex);
	current_lua_script = NULL;

	return props;
}

void obs_lua_script_save(obs_script_t *s)
{
	struct obs_lua_script *data = (struct obs_lua_script *)s;
	lua_State *script = data->script;

	if (!s->loaded)
		return;
	if (data->save == LUA_REFNIL)
		return;

	current_lua_script = data;
	pthread_mutex_lock(&data->mutex);

	ls_push_libobs_obj(obs_data_t, s->settings, false);
	call_func_(script, data->save, 1, 0, "script_save", __FUNCTION__);

	pthread_mutex_unlock(&data->mutex);
	current_lua_script = NULL;
}

/* -------------------------------------------- */

void obs_lua_load(void)
{
	struct dstr dep_paths = {0};
	struct dstr tmp = {0};

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&tick_mutex, NULL);
	pthread_mutex_init(&timer_mutex, &attr);
	pthread_mutex_init(&lua_source_def_mutex, NULL);

	/* ---------------------------------------------- */
	/* Initialize Lua startup script                  */

	char *bundlePath = "./";

#ifdef __APPLE__
	Class nsRunningApplication = objc_lookUpClass("NSRunningApplication");
	SEL currentAppSel = sel_getUid("currentApplication");

	typedef id (*running_app_func)(Class, SEL);
	running_app_func operatingSystemName = (running_app_func)objc_msgSend;
	id app = operatingSystemName(nsRunningApplication, currentAppSel);

	typedef id (*bundle_url_func)(id, SEL);
	bundle_url_func bundleURL = (bundle_url_func)objc_msgSend;
	id url = bundleURL(app, sel_getUid("bundleURL"));

	typedef id (*url_path_func)(id, SEL);
	url_path_func urlPath = (url_path_func)objc_msgSend;

	id path = urlPath(url, sel_getUid("path"));

	typedef id (*string_func)(id, SEL);
	string_func utf8String = (string_func)objc_msgSend;
	bundlePath = (char *)utf8String(path, sel_registerName("UTF8String"));
#endif

	dstr_printf(&tmp, startup_script_template, bundlePath, SCRIPT_DIR);
	startup_script = tmp.array;

	dstr_free(&dep_paths);

	obs_add_tick_callback(lua_tick, NULL);
}

void obs_lua_unload(void)
{
	obs_remove_tick_callback(lua_tick, NULL);

	bfree(startup_script);
	pthread_mutex_destroy(&tick_mutex);
	pthread_mutex_destroy(&timer_mutex);
	pthread_mutex_destroy(&lua_source_def_mutex);
}
