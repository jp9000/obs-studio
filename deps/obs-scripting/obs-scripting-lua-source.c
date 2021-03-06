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
#include "cstrcache.h"

#include <obs-module.h>

/* ========================================================================= */

static inline const char *get_table_string_(lua_State *script, int idx,
					    const char *name, const char *func)
{
	const char *str = "";

	lua_pushstring(script, name);
	lua_gettable(script, idx - 1);
	if (!lua_isstring(script, -1))
		warn("%s: no item '%s' of type %s", func, name, "string");
	else
		str = cstrcache_get(lua_tostring(script, -1));
	lua_pop(script, 1);

	return str;
}

static inline int get_table_int_(lua_State *script, int idx, const char *name,
				 const char *func)
{
	int val = 0;

	lua_pushstring(script, name);
	lua_gettable(script, idx - 1);
	val = (int)lua_tointeger(script, -1);
	lua_pop(script, 1);

	UNUSED_PARAMETER(func);

	return val;
}

static inline void get_callback_from_table_(lua_State *script, int idx,
					    const char *name, int *p_reg_idx,
					    const char *func)
{
	*p_reg_idx = LUA_REFNIL;

	lua_pushstring(script, name);
	lua_gettable(script, idx - 1);
	if (!lua_isfunction(script, -1)) {
		if (!lua_isnil(script, -1)) {
			warn("%s: item '%s' is not a function", func, name);
		}
		lua_pop(script, 1);
	} else {
		*p_reg_idx = luaL_ref(script, LUA_REGISTRYINDEX);
	}
}

#define get_table_string(script, idx, name) \
	get_table_string_(script, idx, name, __FUNCTION__)
#define get_table_int(script, idx, name) \
	get_table_int_(script, idx, name, __FUNCTION__)
#define get_callback_from_table(script, idx, name, p_reg_idx) \
	get_callback_from_table_(script, idx, name, p_reg_idx, __FUNCTION__)

bool ls_get_libobs_obj_(lua_State *script, const char *type, int lua_idx,
			void *libobs_out, const char *id, const char *func,
			int line)
{
	swig_type_info *info = SWIG_TypeQuery(script, type);
	if (info == NULL) {
		warn("%s:%d: SWIG could not find type: %s%s%s", func, line,
		     id ? id : "", id ? "::" : "", type);
		return false;
	}

	int ret = SWIG_ConvertPtr(script, lua_idx, libobs_out, info, 0);
	if (!SWIG_IsOK(ret)) {
		warn("%s:%d: SWIG failed to convert lua object to obs "
		     "object: %s%s%s",
		     func, line, id ? id : "", id ? "::" : "", type);
		return false;
	}

	return true;
}

#define ls_get_libobs_obj(type, lua_index, obs_obj)                            \
	ls_get_libobs_obj_(ls->script, #type " *", lua_index, obs_obj, ls->id, \
			   __FUNCTION__, __LINE__)

bool ls_push_libobs_obj_(lua_State *script, const char *type, void *libobs_in,
			 bool ownership, const char *id, const char *func,
			 int line)
{
	swig_type_info *info = SWIG_TypeQuery(script, type);
	if (info == NULL) {
		warn("%s:%d: SWIG could not find type: %s%s%s", func, line,
		     id ? id : "", id ? "::" : "", type);
		return false;
	}

	SWIG_NewPointerObj(script, libobs_in, info, (int)ownership);
	return true;
}

#define ls_push_libobs_obj(type, obs_obj, ownership)                    \
	ls_push_libobs_obj_(ls->script, #type " *", obs_obj, ownership, \
			    ls->id, __FUNCTION__, __LINE__)

/* ========================================================================= */

struct obs_lua_data;

struct obs_lua_source {
	struct obs_lua_script *data;

	lua_State *script;
	const char *id;
	const char *display_name;
	int func_create;
	int func_destroy;
	int func_get_width;
	int func_get_height;
	int func_get_defaults;
	int func_get_properties;
	int func_update;
	int func_activate;
	int func_deactivate;
	int func_show;
	int func_hide;
	int func_video_tick;
	int func_video_render;
	int func_save;
	int func_load;

	pthread_mutex_t definition_mutex;
	struct obs_lua_data *first_source;

	struct obs_lua_source *next;
	struct obs_lua_source **p_prev_next;
};

extern pthread_mutex_t lua_source_def_mutex;
struct obs_lua_source *first_source_def = NULL;

struct obs_lua_data {
	obs_source_t *source;
	struct obs_lua_source *ls;
	int lua_data_ref;

	struct obs_lua_data *next;
	struct obs_lua_data **p_prev_next;
};

#define call_func(name, args, rets)                                \
	call_func_(ls->script, ls->func_##name, args, rets, #name, \
		   ls->display_name)
#define have_func(name) (ls->func_##name != LUA_REFNIL)
#define ls_push_data() \
	lua_rawgeti(ls->script, LUA_REGISTRYINDEX, ld->lua_data_ref)
#define ls_pop(count) lua_pop(ls->script, count)
#define lock_script()                                              \
	struct obs_lua_script *__data = ls->data;                  \
	struct obs_lua_script *__prev_script = current_lua_script; \
	current_lua_script = __data;                               \
	pthread_mutex_lock(&__data->mutex);
#define unlock_script()                       \
	pthread_mutex_unlock(&__data->mutex); \
	current_lua_script = __prev_script;

static const char *obs_lua_source_get_name(void *type_data)
{
	struct obs_lua_source *ls = type_data;
	return ls->display_name;
}

static void *obs_lua_source_create(obs_data_t *settings, obs_source_t *source)
{
	struct obs_lua_source *ls = obs_source_get_type_data(source);
	struct obs_lua_data *data = NULL;

	pthread_mutex_lock(&ls->definition_mutex);
	if (!ls->script)
		goto fail;
	if (!have_func(create))
		goto fail;

	lock_script();

	ls_push_libobs_obj(obs_data_t, settings, false);
	ls_push_libobs_obj(obs_source_t, source, false);
	call_func(create, 2, 1);

	int lua_data_ref = luaL_ref(ls->script, LUA_REGISTRYINDEX);
	if (lua_data_ref != LUA_REFNIL) {
		data = bmalloc(sizeof(*data));
		data->source = source;
		data->ls = ls;
		data->lua_data_ref = lua_data_ref;
	}

	unlock_script();

	if (data) {
		struct obs_lua_data *next = ls->first_source;
		data->next = next;
		data->p_prev_next = &ls->first_source;
		if (next)
			next->p_prev_next = &data->next;
		ls->first_source = data;
	}

fail:
	pthread_mutex_unlock(&ls->definition_mutex);
	return data;
}

static void call_destroy(struct obs_lua_data *ld)
{
	struct obs_lua_source *ls = ld->ls;

	ls_push_data();
	call_func(destroy, 1, 0);
	luaL_unref(ls->script, LUA_REGISTRYINDEX, ld->lua_data_ref);
	ld->lua_data_ref = LUA_REFNIL;
}

static void obs_lua_source_destroy(void *data)
{
	struct obs_lua_data *ld = data;
	struct obs_lua_source *ls = ld->ls;
	struct obs_lua_data *next;

	pthread_mutex_lock(&ls->definition_mutex);
	if (!ls->script)
		goto fail;
	if (!have_func(destroy))
		goto fail;

	lock_script();
	call_destroy(ld);
	unlock_script();

fail:
	next = ld->next;
	*ld->p_prev_next = next;
	if (next)
		next->p_prev_next = ld->p_prev_next;

	bfree(data);
	pthread_mutex_unlock(&ls->definition_mutex);
}

static uint32_t obs_lua_source_get_width(void *data)
{
	struct obs_lua_data *ld = data;
	struct obs_lua_source *ls = ld->ls;
	uint32_t width = 0;

	pthread_mutex_lock(&ls->definition_mutex);
	if (!ls->script)
		goto fail;
	if (!have_func(get_width))
		goto fail;

	lock_script();

	ls_push_data();
	if (call_func(get_width, 1, 1)) {
		width = (uint32_t)lua_tointeger(ls->script, -1);
		ls_pop(1);
	}

	unlock_script();

fail:
	pthread_mutex_unlock(&ls->definition_mutex);
	return width;
}

static uint32_t obs_lua_source_get_height(void *data)
{
	struct obs_lua_data *ld = data;
	struct obs_lua_source *ls = ld->ls;
	uint32_t height = 0;

	pthread_mutex_lock(&ls->definition_mutex);
	if (!ls->script)
		goto fail;
	if (!have_func(get_height))
		goto fail;

	lock_script();

	ls_push_data();
	if (call_func(get_height, 1, 1)) {
		height = (uint32_t)lua_tointeger(ls->script, -1);
		ls_pop(1);
	}

	unlock_script();

fail:
	pthread_mutex_unlock(&ls->definition_mutex);
	return height;
}

static void obs_lua_source_get_defaults(void *type_data, obs_data_t *settings)
{
	struct obs_lua_source *ls = type_data;

	pthread_mutex_lock(&ls->definition_mutex);
	if (!ls->script)
		goto fail;
	if (!have_func(get_defaults))
		goto fail;

	lock_script();

	ls_push_libobs_obj(obs_data_t, settings, false);
	call_func(get_defaults, 1, 0);

	unlock_script();

fail:
	pthread_mutex_unlock(&ls->definition_mutex);
}

static obs_properties_t *obs_lua_source_get_properties(void *data)
{
	struct obs_lua_data *ld = data;
	struct obs_lua_source *ls = ld->ls;
	obs_properties_t *props = NULL;

	pthread_mutex_lock(&ls->definition_mutex);
	if (!ls->script)
		goto fail;
	if (!have_func(get_properties))
		goto fail;

	lock_script();

	ls_push_data();
	if (call_func(get_properties, 1, 1)) {
		ls_get_libobs_obj(obs_properties_t, -1, &props);
		ls_pop(1);
	}

	unlock_script();

fail:
	pthread_mutex_unlock(&ls->definition_mutex);
	return props;
}

static void obs_lua_source_update(void *data, obs_data_t *settings)
{
	struct obs_lua_data *ld = data;
	struct obs_lua_source *ls = ld->ls;

	pthread_mutex_lock(&ls->definition_mutex);
	if (!ls->script)
		goto fail;
	if (!have_func(update))
		goto fail;

	lock_script();

	ls_push_data();
	ls_push_libobs_obj(obs_data_t, settings, false);
	call_func(update, 2, 0);

	unlock_script();

fail:
	pthread_mutex_unlock(&ls->definition_mutex);
}

#define DEFINE_VOID_DATA_CALLBACK(name)               \
	static void obs_lua_source_##name(void *data) \
	{                                             \
		struct obs_lua_data *ld = data;       \
		struct obs_lua_source *ls = ld->ls;   \
		if (!have_func(name))                 \
			return;                       \
		lock_script();                        \
		ls_push_data();                       \
		call_func(name, 1, 0);                \
		unlock_script();                      \
	}
DEFINE_VOID_DATA_CALLBACK(activate)
DEFINE_VOID_DATA_CALLBACK(deactivate)
DEFINE_VOID_DATA_CALLBACK(show)
DEFINE_VOID_DATA_CALLBACK(hide)
#undef DEFINE_VOID_DATA_CALLBACK

static void obs_lua_source_video_tick(void *data, float seconds)
{
	struct obs_lua_data *ld = data;
	struct obs_lua_source *ls = ld->ls;

	pthread_mutex_lock(&ls->definition_mutex);
	if (!ls->script)
		goto fail;
	if (!have_func(video_tick))
		goto fail;

	lock_script();

	ls_push_data();
	lua_pushnumber(ls->script, (double)seconds);
	call_func(video_tick, 2, 0);

	unlock_script();

fail:
	pthread_mutex_unlock(&ls->definition_mutex);
}

static void obs_lua_source_video_render(void *data, gs_effect_t *effect)
{
	struct obs_lua_data *ld = data;
	struct obs_lua_source *ls = ld->ls;

	pthread_mutex_lock(&ls->definition_mutex);
	if (!ls->script)
		goto fail;
	if (!have_func(video_render))
		goto fail;

	lock_script();

	ls_push_data();
	ls_push_libobs_obj(gs_effect_t, effect, false);
	call_func(video_render, 2, 0);

	unlock_script();

fail:
	pthread_mutex_unlock(&ls->definition_mutex);
}

static void obs_lua_source_save(void *data, obs_data_t *settings)
{
	struct obs_lua_data *ld = data;
	struct obs_lua_source *ls = ld->ls;

	pthread_mutex_lock(&ls->definition_mutex);
	if (!ls->script)
		goto fail;
	if (!have_func(save))
		goto fail;

	lock_script();

	ls_push_data();
	ls_push_libobs_obj(obs_data_t, settings, false);
	call_func(save, 2, 0);

	unlock_script();

fail:
	pthread_mutex_unlock(&ls->definition_mutex);
}

static void obs_lua_source_load(void *data, obs_data_t *settings)
{
	struct obs_lua_data *ld = data;
	struct obs_lua_source *ls = ld->ls;

	pthread_mutex_lock(&ls->definition_mutex);
	if (!ls->script)
		goto fail;
	if (!have_func(load))
		goto fail;

	lock_script();

	ls_push_data();
	ls_push_libobs_obj(obs_data_t, settings, false);
	call_func(load, 2, 0);

	unlock_script();

fail:
	pthread_mutex_unlock(&ls->definition_mutex);
}

static void source_type_unload(struct obs_lua_source *ls)
{
#define unref(name)                                      \
	luaL_unref(ls->script, LUA_REGISTRYINDEX, name); \
	name = LUA_REFNIL

	unref(ls->func_create);
	unref(ls->func_destroy);
	unref(ls->func_get_width);
	unref(ls->func_get_height);
	unref(ls->func_get_defaults);
	unref(ls->func_get_properties);
	unref(ls->func_update);
	unref(ls->func_activate);
	unref(ls->func_deactivate);
	unref(ls->func_show);
	unref(ls->func_hide);
	unref(ls->func_video_tick);
	unref(ls->func_video_render);
	unref(ls->func_save);
	unref(ls->func_load);
#undef unref
}

static void obs_lua_source_free_type_data(void *type_data)
{
	struct obs_lua_source *ls = type_data;

	pthread_mutex_lock(&ls->definition_mutex);

	if (ls->script) {
		lock_script();
		source_type_unload(ls);
		unlock_script();
		ls->script = NULL;
	}

	pthread_mutex_unlock(&ls->definition_mutex);
	pthread_mutex_destroy(&ls->definition_mutex);
	bfree(ls);
}

EXPORT void obs_enable_source_type(const char *name, bool enable);

static inline struct obs_lua_source *find_existing(const char *id)
{
	struct obs_lua_source *existing = NULL;

	pthread_mutex_lock(&lua_source_def_mutex);
	struct obs_lua_source *ls = first_source_def;
	while (ls) {
		/* can compare pointers here due to string table */
		if (ls->id == id) {
			existing = ls;
			break;
		}

		ls = ls->next;
	}
	pthread_mutex_unlock(&lua_source_def_mutex);

	return existing;
}

static int obs_lua_register_source(lua_State *script)
{
	struct obs_lua_source ls = {0};
	struct obs_lua_source *existing = NULL;
	struct obs_lua_source *v = NULL;
	struct obs_source_info info = {0};
	const char *id;

	if (!verify_args1(script, is_table))
		goto fail;

	id = get_table_string(script, -1, "id");
	if (!id || !*id)
		goto fail;

	/* redefinition */
	existing = find_existing(id);
	if (existing) {
		if (existing->script) {
			existing = NULL;
			goto fail;
		}

		pthread_mutex_lock(&existing->definition_mutex);
	}

	v = existing ? existing : &ls;

	v->script = script;
	v->id = id;

	info.id = v->id;
	info.type = (enum obs_source_type)get_table_int(script, -1, "type");

	info.output_flags = get_table_int(script, -1, "output_flags");

	lua_pushstring(script, "get_name");
	lua_gettable(script, -2);
	if (lua_pcall(script, 0, 1, 0) == 0) {
		v->display_name = cstrcache_get(lua_tostring(script, -1));
		lua_pop(script, 1);
	}

	if (!v->display_name || !*v->display_name || !*info.id ||
	    !info.output_flags)
		goto fail;

#define get_callback(val)                                                  \
	do {                                                               \
		get_callback_from_table(script, -1, #val, &v->func_##val); \
		info.val = obs_lua_source_##val;                           \
	} while (false)

	get_callback(create);
	get_callback(destroy);
	get_callback(get_width);
	get_callback(get_height);
	get_callback(get_properties);
	get_callback(update);
	get_callback(activate);
	get_callback(deactivate);
	get_callback(show);
	get_callback(hide);
	get_callback(video_tick);
	get_callback(video_render);
	get_callback(save);
	get_callback(load);
#undef get_callback

	get_callback_from_table(script, -1, "get_defaults",
				&v->func_get_defaults);

	if (!existing) {
		ls.data = current_lua_script;

		pthread_mutexattr_t mutexattr;
		pthread_mutexattr_init(&mutexattr);
		pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&ls.definition_mutex, &mutexattr);
		pthread_mutexattr_destroy(&mutexattr);

		info.type_data = bmemdup(&ls, sizeof(ls));
		info.free_type_data = obs_lua_source_free_type_data;
		info.get_name = obs_lua_source_get_name;
		info.get_defaults2 = obs_lua_source_get_defaults;
		obs_register_source(&info);

		pthread_mutex_lock(&lua_source_def_mutex);
		v = info.type_data;

		struct obs_lua_source *next = first_source_def;
		v->next = next;
		if (next)
			next->p_prev_next = &v->next;
		v->p_prev_next = &first_source_def;
		first_source_def = v;

		pthread_mutex_unlock(&lua_source_def_mutex);
	} else {
		existing->script = script;
		existing->data = current_lua_script;
		obs_enable_source_type(id, true);

		struct obs_lua_data *ld = v->first_source;
		while (ld) {
			struct obs_lua_source *ls = v;

			if (have_func(create)) {
				obs_source_t *source = ld->source;
				obs_data_t *settings =
					obs_source_get_settings(source);

				ls_push_libobs_obj(obs_data_t, settings, false);
				ls_push_libobs_obj(obs_source_t, source, false);
				call_func(create, 2, 1);

				ld->lua_data_ref =
					luaL_ref(ls->script, LUA_REGISTRYINDEX);
				obs_data_release(settings);
			}

			ld = ld->next;
		}
	}

fail:
	if (existing) {
		pthread_mutex_unlock(&existing->definition_mutex);
	}
	return 0;
}

/* ========================================================================= */

void add_lua_source_functions(lua_State *script)
{
	lua_getglobal(script, "obslua");

	lua_pushstring(script, "obs_register_source");
	lua_pushcfunction(script, obs_lua_register_source);
	lua_rawset(script, -3);

	lua_pop(script, 1);
}

static inline void undef_source_type(struct obs_lua_script *data,
				     struct obs_lua_source *ls)
{
	pthread_mutex_lock(&ls->definition_mutex);
	pthread_mutex_lock(&data->mutex);

	obs_enable_source_type(ls->id, false);

	struct obs_lua_data *ld = ls->first_source;
	while (ld) {
		call_destroy(ld);
		ld = ld->next;
	}

	source_type_unload(ls);
	ls->script = NULL;

	pthread_mutex_unlock(&data->mutex);
	pthread_mutex_unlock(&ls->definition_mutex);
}

void undef_lua_script_sources(struct obs_lua_script *data)
{
	pthread_mutex_lock(&lua_source_def_mutex);

	struct obs_lua_source *def = first_source_def;
	while (def) {
		if (def->script == data->script)
			undef_source_type(data, def);
		def = def->next;
	}

	pthread_mutex_unlock(&lua_source_def_mutex);
}
