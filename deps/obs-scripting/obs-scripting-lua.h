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

/* ---------------------------- */

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4189)
#pragma warning(disable : 4244)
#pragma warning(disable : 4267)
#endif

#define SWIG_TYPE_TABLE obslua
#include "swig/swigluarun.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif
/* ---------------------------- */

#include <util/threading.h>
#include <util/base.h>
#include <util/bmem.h>

#include "obs-scripting-internal.h"
#include "obs-scripting-callback.h"

#define do_log(level, format, ...) blog(level, "[Lua] " format, ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)

/* ------------------------------------------------------------ */

struct obs_lua_script;
struct lua_obs_callback;

extern THREAD_LOCAL struct lua_obs_callback *current_lua_cb;
extern THREAD_LOCAL struct obs_lua_script *current_lua_script;

/* ------------------------------------------------------------ */

struct lua_obs_callback;

struct obs_lua_script {
	obs_script_t base;

	struct dstr dir;
	struct dstr log_chunk;

	pthread_mutex_t mutex;
	lua_State *script;

	struct script_callback *first_callback;

	int update;
	int get_properties;
	int save;

	int tick;
	struct obs_lua_script *next_tick;
	struct obs_lua_script **p_prev_next_tick;

	bool defined_sources;
};

#define lock_callback()                                                \
	struct obs_lua_script *__last_script = current_lua_script;     \
	struct lua_obs_callback *__last_callback = current_lua_cb;     \
	current_lua_cb = cb;                                           \
	current_lua_script = (struct obs_lua_script *)cb->base.script; \
	pthread_mutex_lock(&current_lua_script->mutex);
#define unlock_callback()                                 \
	pthread_mutex_unlock(&current_lua_script->mutex); \
	current_lua_script = __last_script;               \
	current_lua_cb = __last_callback;

/* ------------------------------------------------ */

struct lua_obs_callback {
	struct script_callback base;

	lua_State *script;
	int reg_idx;
};

static inline struct lua_obs_callback *
add_lua_obs_callback_extra(lua_State *script, int stack_idx, size_t extra_size)
{
	struct obs_lua_script *data = current_lua_script;
	struct lua_obs_callback *cb =
		add_script_callback(&data->first_callback, (obs_script_t *)data,
				    sizeof(*cb) + extra_size);

	lua_pushvalue(script, stack_idx);
	cb->reg_idx = luaL_ref(script, LUA_REGISTRYINDEX);
	cb->script = script;
	return cb;
}

static inline struct lua_obs_callback *add_lua_obs_callback(lua_State *script,
							    int stack_idx)
{
	return add_lua_obs_callback_extra(script, stack_idx, 0);
}

static inline void *lua_obs_callback_extra_data(struct lua_obs_callback *cb)
{
	return (void *)&cb[1];
}

static inline struct obs_lua_script *
lua_obs_callback_script(struct lua_obs_callback *cb)
{
	return (struct obs_lua_script *)cb->base.script;
}

static inline struct lua_obs_callback *
find_next_lua_obs_callback(lua_State *script, struct lua_obs_callback *cb,
			   int stack_idx)
{
	struct obs_lua_script *data = current_lua_script;

	cb = cb ? (struct lua_obs_callback *)cb->base.next
		: (struct lua_obs_callback *)data->first_callback;

	while (cb) {
		lua_rawgeti(script, LUA_REGISTRYINDEX, cb->reg_idx);
		bool match = lua_rawequal(script, -1, stack_idx);
		lua_pop(script, 1);

		if (match)
			break;

		cb = (struct lua_obs_callback *)cb->base.next;
	}

	return cb;
}

static inline struct lua_obs_callback *find_lua_obs_callback(lua_State *script,
							     int stack_idx)
{
	return find_next_lua_obs_callback(script, NULL, stack_idx);
}

static inline void remove_lua_obs_callback(struct lua_obs_callback *cb)
{
	remove_script_callback(&cb->base);
	luaL_unref(cb->script, LUA_REGISTRYINDEX, cb->reg_idx);
}

static inline void just_free_lua_obs_callback(struct lua_obs_callback *cb)
{
	just_free_script_callback(&cb->base);
}

static inline void free_lua_obs_callback(struct lua_obs_callback *cb)
{
	free_script_callback(&cb->base);
}

/* ------------------------------------------------ */

static int is_ptr(lua_State *script, int idx)
{
	return lua_isuserdata(script, idx) || lua_isnil(script, idx);
}

static int is_table(lua_State *script, int idx)
{
	return lua_istable(script, idx);
}

static int is_function(lua_State *script, int idx)
{
	return lua_isfunction(script, idx);
}

typedef int (*param_cb)(lua_State *script, int idx);

static inline bool verify_args1_(lua_State *script, param_cb param1_check,
				 const char *func)
{
	if (lua_gettop(script) != 1) {
		warn("Wrong number of parameters for %s", func);
		return false;
	}
	if (!param1_check(script, 1)) {
		warn("Wrong parameter type for parameter %d of %s", 1, func);
		return false;
	}

	return true;
}

#define verify_args1(script, param1_check) \
	verify_args1_(script, param1_check, __FUNCTION__)

static inline bool call_func_(lua_State *script, int reg_idx, int args,
			      int rets, const char *func,
			      const char *display_name)
{
	if (reg_idx == LUA_REFNIL)
		return false;

	struct obs_lua_script *data = current_lua_script;

	lua_rawgeti(script, LUA_REGISTRYINDEX, reg_idx);
	lua_insert(script, -1 - args);

	if (lua_pcall(script, args, rets, 0) != 0) {
		script_warn(&data->base, "Failed to call %s for %s: %s", func,
			    display_name, lua_tostring(script, -1));
		lua_pop(script, 1);
		return false;
	}

	return true;
}

bool ls_get_libobs_obj_(lua_State *script, const char *type, int lua_idx,
			void *libobs_out, const char *id, const char *func,
			int line);
bool ls_push_libobs_obj_(lua_State *script, const char *type, void *libobs_in,
			 bool ownership, const char *id, const char *func,
			 int line);

extern void add_lua_source_functions(lua_State *script);
