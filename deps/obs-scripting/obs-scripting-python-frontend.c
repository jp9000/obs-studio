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

#include <obs-module.h>
#include <obs-frontend-api.h>

#include "obs-scripting-python.h"

#define libobs_to_py(type, obs_obj, ownership, py_obj)                        \
	libobs_to_py_(#type " *", obs_obj, ownership, py_obj, NULL, __func__, \
		      __LINE__)
#define py_to_libobs(type, py_obj, libobs_out) \
	py_to_libobs_(#type " *", py_obj, libobs_out, NULL, __func__, __LINE__)

/* ----------------------------------- */

static PyObject *get_scene_names(PyObject *self, PyObject *args)
{
	char **names = obs_frontend_get_scene_names();
	char **name = names;

	PyObject *list = PyList_New(0);

	while (name && *name) {
		PyObject *py_name = PyUnicode_FromString(*name);
		if (py_name) {
			PyList_Append(list, py_name);
			Py_DECREF(py_name);
		}
		name++;
	}

	UNUSED_PARAMETER(self);
	UNUSED_PARAMETER(args);

	bfree(names);
	return list;
}

static PyObject *get_scenes(PyObject *self, PyObject *args)
{
	struct obs_frontend_source_list list = {0};
	obs_frontend_get_scenes(&list);

	PyObject *ret = PyList_New(0);

	for (size_t i = 0; i < list.sources.num; i++) {
		obs_source_t *source = list.sources.array[i];
		PyObject *py_source;

		if (libobs_to_py(obs_source_t, source, false, &py_source)) {
			PyList_Append(ret, py_source);
			Py_DECREF(py_source);
		}
	}

	UNUSED_PARAMETER(self);
	UNUSED_PARAMETER(args);

	da_free(list.sources);
	return ret;
}

static PyObject *get_current_scene(PyObject *self, PyObject *args)
{
	obs_source_t *source = obs_frontend_get_current_scene();

	PyObject *py_source;
	if (!libobs_to_py(obs_source_t, source, false, &py_source)) {
		obs_source_release(source);
		return python_none();
	}

	UNUSED_PARAMETER(self);
	UNUSED_PARAMETER(args);
	return py_source;
}

static PyObject *set_current_scene(PyObject *self, PyObject *args)
{
	PyObject *py_source;
	obs_source_t *source = NULL;

	if (!parse_args(args, "O", &py_source))
		return python_none();
	if (!py_to_libobs(obs_source_t, py_source, &source))
		return python_none();

	UNUSED_PARAMETER(self);

	obs_frontend_set_current_scene(source);
	return python_none();
}

static PyObject *get_transitions(PyObject *self, PyObject *args)
{
	struct obs_frontend_source_list list = {0};
	obs_frontend_get_transitions(&list);

	PyObject *ret = PyList_New(0);

	for (size_t i = 0; i < list.sources.num; i++) {
		obs_source_t *source = list.sources.array[i];
		PyObject *py_source;

		if (libobs_to_py(obs_source_t, source, false, &py_source)) {
			PyList_Append(ret, py_source);
			Py_DECREF(py_source);
		}
	}

	UNUSED_PARAMETER(self);
	UNUSED_PARAMETER(args);

	da_free(list.sources);
	return ret;
}

static PyObject *get_current_transition(PyObject *self, PyObject *args)
{
	obs_source_t *source = obs_frontend_get_current_transition();

	PyObject *py_source;
	if (!libobs_to_py(obs_source_t, source, false, &py_source)) {
		obs_source_release(source);
		return python_none();
	}

	UNUSED_PARAMETER(self);
	UNUSED_PARAMETER(args);
	return py_source;
}

static PyObject *set_current_transition(PyObject *self, PyObject *args)
{
	PyObject *py_source;
	obs_source_t *source = NULL;

	if (!parse_args(args, "O", &py_source))
		return python_none();
	if (!py_to_libobs(obs_source_t, py_source, &source))
		return python_none();

	UNUSED_PARAMETER(self);

	obs_frontend_set_current_transition(source);
	return python_none();
}

static PyObject *get_scene_collections(PyObject *self, PyObject *args)
{
	char **names = obs_frontend_get_scene_collections();
	char **name = names;

	PyObject *list = PyList_New(0);

	while (name && *name) {
		PyObject *py_name = PyUnicode_FromString(*name);
		if (py_name) {
			PyList_Append(list, py_name);
			Py_DECREF(py_name);
		}
		name++;
	}

	UNUSED_PARAMETER(self);
	UNUSED_PARAMETER(args);

	bfree(names);
	return list;
}

static PyObject *get_current_scene_collection(PyObject *self, PyObject *args)
{
	char *name = obs_frontend_get_current_scene_collection();
	PyObject *ret = PyUnicode_FromString(name);
	bfree(name);

	UNUSED_PARAMETER(self);
	UNUSED_PARAMETER(args);
	return ret;
}

static PyObject *set_current_scene_collection(PyObject *self, PyObject *args)
{
	const char *name;
	if (!parse_args(args, "s", &name))
		return python_none();

	UNUSED_PARAMETER(self);

	obs_frontend_set_current_scene_collection(name);
	return python_none();
}

static PyObject *get_profiles(PyObject *self, PyObject *args)
{
	char **names = obs_frontend_get_profiles();
	char **name = names;

	PyObject *list = PyList_New(0);

	while (name && *name) {
		PyObject *py_name = PyUnicode_FromString(*name);
		if (py_name) {
			PyList_Append(list, py_name);
			Py_DECREF(py_name);
		}
		name++;
	}

	UNUSED_PARAMETER(self);
	UNUSED_PARAMETER(args);

	bfree(names);
	return list;
}

static PyObject *get_current_profile(PyObject *self, PyObject *args)
{
	char *name = obs_frontend_get_current_profile();
	PyObject *ret = PyUnicode_FromString(name);
	bfree(name);

	UNUSED_PARAMETER(self);
	UNUSED_PARAMETER(args);
	return ret;
}

static PyObject *set_current_profile(PyObject *self, PyObject *args)
{
	const char *name;
	if (!parse_args(args, "s", &name))
		return python_none();

	UNUSED_PARAMETER(self);

	obs_frontend_set_current_profile(name);
	return python_none();
}

/* ----------------------------------- */

static void frontend_save_callback(obs_data_t *save_data, bool saving,
				   void *priv)
{
	struct python_obs_callback *cb = priv;

	if (cb->base.removed) {
		obs_frontend_remove_save_callback(frontend_save_callback, cb);
		return;
	}

	lock_python();

	PyObject *py_save_data;

	if (libobs_to_py(obs_data_t, save_data, false, &py_save_data)) {
		PyObject *args = Py_BuildValue("(Op)", py_save_data, saving);

		struct python_obs_callback *last_cb = cur_python_cb;
		cur_python_cb = cb;
		cur_python_script = (struct obs_python_script *)cb->base.script;

		PyObject *py_ret = PyObject_CallObject(cb->func, args);
		Py_XDECREF(py_ret);
		py_error();

		cur_python_script = NULL;
		cur_python_cb = last_cb;

		Py_XDECREF(args);
		Py_XDECREF(py_save_data);
	}

	unlock_python();
}

static PyObject *remove_save_callback(PyObject *self, PyObject *args)
{
	struct obs_python_script *script = cur_python_script;
	PyObject *py_cb = NULL;

	UNUSED_PARAMETER(self);

	if (!parse_args(args, "O", &py_cb))
		return python_none();
	if (!py_cb || !PyFunction_Check(py_cb))
		return python_none();

	struct python_obs_callback *cb =
		find_python_obs_callback(script, py_cb);
	if (cb)
		remove_python_obs_callback(cb);
	return python_none();
}

static void add_save_callback_defer(void *cb)
{
	obs_frontend_add_save_callback(frontend_save_callback, cb);
}

static PyObject *add_save_callback(PyObject *self, PyObject *args)
{
	struct obs_python_script *script = cur_python_script;
	PyObject *py_cb = NULL;

	UNUSED_PARAMETER(self);

	if (!parse_args(args, "O", &py_cb))
		return python_none();
	if (!py_cb || !PyFunction_Check(py_cb))
		return python_none();

	struct python_obs_callback *cb = add_python_obs_callback(script, py_cb);
	defer_call_post(add_save_callback_defer, cb);
	return python_none();
}

static void frontend_event_callback(enum obs_frontend_event event, void *priv)
{
	struct python_obs_callback *cb = priv;

	if (cb->base.removed) {
		obs_frontend_remove_event_callback(frontend_event_callback, cb);
		return;
	}

	lock_python();

	PyObject *args = Py_BuildValue("(i)", event);

	struct python_obs_callback *last_cb = cur_python_cb;
	cur_python_cb = cb;
	cur_python_script = (struct obs_python_script *)cb->base.script;

	PyObject *py_ret = PyObject_CallObject(cb->func, args);
	Py_XDECREF(py_ret);
	py_error();

	cur_python_script = NULL;
	cur_python_cb = last_cb;

	Py_XDECREF(args);

	unlock_python();
}

static PyObject *remove_event_callback(PyObject *self, PyObject *args)
{
	struct obs_python_script *script = cur_python_script;
	PyObject *py_cb = NULL;

	UNUSED_PARAMETER(self);

	if (!parse_args(args, "O", &py_cb))
		return python_none();
	if (!py_cb || !PyFunction_Check(py_cb))
		return python_none();

	struct python_obs_callback *cb =
		find_python_obs_callback(script, py_cb);
	if (cb)
		remove_python_obs_callback(cb);
	return python_none();
}

static void add_event_callback_defer(void *cb)
{
	obs_frontend_add_event_callback(frontend_event_callback, cb);
}

static PyObject *add_event_callback(PyObject *self, PyObject *args)
{
	struct obs_python_script *script = cur_python_script;
	PyObject *py_cb = NULL;

	UNUSED_PARAMETER(self);

	if (!parse_args(args, "O", &py_cb))
		return python_none();
	if (!py_cb || !PyFunction_Check(py_cb))
		return python_none();

	struct python_obs_callback *cb = add_python_obs_callback(script, py_cb);
	defer_call_post(add_event_callback_defer, cb);
	return python_none();
}

/* ----------------------------------- */

void add_python_frontend_funcs(PyObject *module)
{
	static PyMethodDef funcs[] = {
#define DEF_FUNC(c) {"obs_frontend_" #c, c, METH_VARARGS, NULL}

		DEF_FUNC(get_scene_names),
		DEF_FUNC(get_scenes),
		DEF_FUNC(get_current_scene),
		DEF_FUNC(set_current_scene),
		DEF_FUNC(get_transitions),
		DEF_FUNC(get_current_transition),
		DEF_FUNC(set_current_transition),
		DEF_FUNC(get_scene_collections),
		DEF_FUNC(get_current_scene_collection),
		DEF_FUNC(set_current_scene_collection),
		DEF_FUNC(get_profiles),
		DEF_FUNC(get_current_profile),
		DEF_FUNC(set_current_profile),
		DEF_FUNC(remove_save_callback),
		DEF_FUNC(add_save_callback),
		DEF_FUNC(remove_event_callback),
		DEF_FUNC(add_event_callback),

#undef DEF_FUNC
		{0}};

	add_functions_to_py_module(module, funcs);
}
