/******************************************************************************
    Copyright (C) 2013-2014 by Hugh Bailey <obs.jim@gmail.com>

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

#include "util/platform.h"
#include "util/dstr.h"

#include "obs-defs.h"
#include "obs-internal.h"
#include "obs-module.h"

extern const char *get_module_extension(void);

static inline int req_func_not_found(const char *name, const char *path)
{
	blog(LOG_DEBUG,
	     "Required module function '%s' in module '%s' not "
	     "found, loading of module failed",
	     name, path);
	return MODULE_MISSING_EXPORTS;
}

static int load_module_exports(struct obs_module *mod, const char *path)
{
	mod->load = os_dlsym(mod->module, "obs_module_load");
	if (!mod->load)
		return req_func_not_found("obs_module_load", path);

	mod->set_pointer = os_dlsym(mod->module, "obs_module_set_pointer");
	if (!mod->set_pointer)
		return req_func_not_found("obs_module_set_pointer", path);

	mod->ver = os_dlsym(mod->module, "obs_module_ver");
	if (!mod->ver)
		return req_func_not_found("obs_module_ver", path);

	/* optional exports */
	mod->unload = os_dlsym(mod->module, "obs_module_unload");
	mod->post_load = os_dlsym(mod->module, "obs_module_post_load");
	mod->set_locale = os_dlsym(mod->module, "obs_module_set_locale");
	mod->free_locale = os_dlsym(mod->module, "obs_module_free_locale");
	mod->name = os_dlsym(mod->module, "obs_module_name");
	mod->description = os_dlsym(mod->module, "obs_module_description");
	mod->author = os_dlsym(mod->module, "obs_module_author");
	mod->get_string = os_dlsym(mod->module, "obs_module_get_string");
	return MODULE_SUCCESS;
}

bool obs_module_get_locale_string(const obs_module_t *mod,
				  const char *lookup_string,
				  const char **translated_string)
{
	if (mod->get_string) {
		return mod->get_string(lookup_string, translated_string);
	}

	return false;
}

const char *obs_module_get_locale_text(const obs_module_t *mod,
				       const char *text)
{
	const char *str = text;
	obs_module_get_locale_string(mod, text, &str);
	return str;
}

static inline char *get_module_name(const char *file)
{
	static size_t ext_len = 0;
	struct dstr name = {0};

	if (ext_len == 0) {
		const char *ext = get_module_extension();
		ext_len = strlen(ext);
	}

	dstr_copy(&name, file);
	dstr_resize(&name, name.len - ext_len);
	return name.array;
}

#ifdef _WIN32
extern void reset_win32_symbol_paths(void);
#endif

int obs_open_module(obs_module_t **module, const char *path,
		    const char *data_path)
{
	struct obs_module mod = {0};
	int errorcode;

	if (!module || !path || !obs)
		return MODULE_ERROR;

#ifdef __APPLE__
	/* HACK: Do not load obsolete obs-browser build on macOS; the
	 * obs-browser plugin used to live in the Application Support
	 * directory. */
	if (astrstri(path, "Library/Application Support") != NULL &&
	    astrstri(path, "obs-browser") != NULL) {
		blog(LOG_WARNING, "Ignoring old obs-browser.so version");
		return MODULE_ERROR;
	}
#endif

	blog(LOG_DEBUG, "---------------------------------");

	mod.module = os_dlopen(path);
	if (!mod.module) {
		blog(LOG_WARNING, "Module '%s' not loaded", path);
		return MODULE_FILE_NOT_FOUND;
	}

	errorcode = load_module_exports(&mod, path);
	if (errorcode != MODULE_SUCCESS)
		return errorcode;

	mod.bin_path = bstrdup(path);
	mod.file = strrchr(mod.bin_path, '/');
	mod.file = (!mod.file) ? mod.bin_path : (mod.file + 1);
	mod.mod_name = get_module_name(mod.file);
	mod.data_path = bstrdup(data_path);
	mod.next = obs->first_module;

	if (mod.file) {
		blog(LOG_DEBUG, "Loading module: %s", mod.file);
	}

	*module = bmemdup(&mod, sizeof(mod));
	obs->first_module = (*module);
	mod.set_pointer(*module);

	if (mod.set_locale)
		mod.set_locale(obs->locale);

	return MODULE_SUCCESS;
}

bool obs_init_module(obs_module_t *module)
{
	if (!module || !obs)
		return false;
	if (module->loaded)
		return true;

	const char *profile_name =
		profile_store_name(obs_get_profiler_name_store(),
				   "obs_init_module(%s)", module->file);
	profile_start(profile_name);

	module->loaded = module->load();
	if (!module->loaded)
		blog(LOG_WARNING, "Failed to initialize module '%s'",
		     module->file);

	profile_end(profile_name);
	return module->loaded;
}

void obs_log_loaded_modules(void)
{
	blog(LOG_INFO, "  Loaded Modules:");

	for (obs_module_t *mod = obs->first_module; !!mod; mod = mod->next)
		blog(LOG_INFO, "    %s", mod->file);
}

const char *obs_get_module_file_name(obs_module_t *module)
{
	return module ? module->file : NULL;
}

const char *obs_get_module_name(obs_module_t *module)
{
	return (module && module->name) ? module->name() : NULL;
}

const char *obs_get_module_author(obs_module_t *module)
{
	return (module && module->author) ? module->author() : NULL;
}

const char *obs_get_module_description(obs_module_t *module)
{
	return (module && module->description) ? module->description() : NULL;
}

const char *obs_get_module_binary_path(obs_module_t *module)
{
	return module ? module->bin_path : NULL;
}

const char *obs_get_module_data_path(obs_module_t *module)
{
	return module ? module->data_path : NULL;
}

obs_module_t *obs_get_module(const char *name)
{
	obs_module_t *module = obs->first_module;
	while (module) {
		if (strcmp(module->mod_name, name) == 0) {
			return module;
		}

		module = module->next;
	}

	return NULL;
}

void *obs_get_module_lib(obs_module_t *module)
{
	return module ? module->module : NULL;
}

char *obs_find_module_file(obs_module_t *module, const char *file)
{
	struct dstr output = {0};

	if (!file)
		file = "";

	if (!module)
		return NULL;

	dstr_copy(&output, module->data_path);
	if (!dstr_is_empty(&output) && dstr_end(&output) != '/' && *file)
		dstr_cat_ch(&output, '/');
	dstr_cat(&output, file);

	if (!os_file_exists(output.array))
		dstr_free(&output);
	return output.array;
}

char *obs_module_get_config_path(obs_module_t *module, const char *file)
{
	struct dstr output = {0};

	dstr_copy(&output, obs->module_config_path);
	if (!dstr_is_empty(&output) && dstr_end(&output) != '/')
		dstr_cat_ch(&output, '/');
	dstr_cat(&output, module->mod_name);
	dstr_cat_ch(&output, '/');
	dstr_cat(&output, file);

	return output.array;
}

void obs_add_module_path(const char *bin, const char *data)
{
	struct obs_module_path omp;

	if (!obs || !bin || !data)
		return;

	omp.bin = bstrdup(bin);
	omp.data = bstrdup(data);
	da_push_back(obs->module_paths, &omp);
}

static void load_all_callback(void *param, const struct obs_module_info *info)
{
	obs_module_t *module;

	if (!os_is_obs_plugin(info->bin_path))
		blog(LOG_WARNING, "Skipping module '%s', not an OBS plugin",
		     info->bin_path);

	int code = obs_open_module(&module, info->bin_path, info->data_path);
	if (code != MODULE_SUCCESS) {
		blog(LOG_DEBUG, "Failed to load module file '%s': %d",
		     info->bin_path, code);
		return;
	}

	obs_init_module(module);

	UNUSED_PARAMETER(param);
}

static const char *obs_load_all_modules_name = "obs_load_all_modules";
#ifdef _WIN32
static const char *reset_win32_symbol_paths_name = "reset_win32_symbol_paths";
#endif

void obs_load_all_modules(void)
{
	profile_start(obs_load_all_modules_name);
	obs_find_modules(load_all_callback, NULL);
#ifdef _WIN32
	profile_start(reset_win32_symbol_paths_name);
	reset_win32_symbol_paths();
	profile_end(reset_win32_symbol_paths_name);
#endif
	profile_end(obs_load_all_modules_name);
}

void obs_post_load_modules(void)
{
	for (obs_module_t *mod = obs->first_module; !!mod; mod = mod->next)
		if (mod->post_load)
			mod->post_load();
}

static inline void make_data_dir(struct dstr *parsed_data_dir,
				 const char *data_dir, const char *name)
{
	dstr_copy(parsed_data_dir, data_dir);
	dstr_replace(parsed_data_dir, "%module%", name);
	if (dstr_end(parsed_data_dir) == '/')
		dstr_resize(parsed_data_dir, parsed_data_dir->len - 1);
}

static char *make_data_directory(const char *module_name, const char *data_dir)
{
	struct dstr parsed_data_dir = {0};
	bool found = false;

	make_data_dir(&parsed_data_dir, data_dir, module_name);

	found = os_file_exists(parsed_data_dir.array);

	if (!found && astrcmpi_n(module_name, "lib", 3) == 0)
		make_data_dir(&parsed_data_dir, data_dir, module_name + 3);

	return parsed_data_dir.array;
}

static bool parse_binary_from_directory(struct dstr *parsed_bin_path,
					const char *bin_path, const char *file)
{
	struct dstr directory = {0};
	bool found = true;

	dstr_copy(&directory, bin_path);
	dstr_replace(&directory, "%module%", file);
	if (dstr_end(&directory) != '/')
		dstr_cat_ch(&directory, '/');

	dstr_copy_dstr(parsed_bin_path, &directory);
	dstr_cat(parsed_bin_path, file);
	dstr_cat(parsed_bin_path, get_module_extension());

	if (!os_file_exists(parsed_bin_path->array)) {
		/* if the file doesn't exist, check with 'lib' prefix */
		dstr_copy_dstr(parsed_bin_path, &directory);
		dstr_cat(parsed_bin_path, "lib");
		dstr_cat(parsed_bin_path, file);
		dstr_cat(parsed_bin_path, get_module_extension());

		/* if neither exist, don't include this as a library */
		if (!os_file_exists(parsed_bin_path->array)) {
			dstr_free(parsed_bin_path);
			found = false;
		}
	}

	dstr_free(&directory);
	return found;
}

static void process_found_module(struct obs_module_path *omp, const char *path,
				 bool directory,
				 obs_find_module_callback_t callback,
				 void *param)
{
	struct obs_module_info info;
	struct dstr name = {0};
	struct dstr parsed_bin_path = {0};
	const char *file;
	char *parsed_data_dir;
	bool bin_found = true;

	file = strrchr(path, '/');
	file = file ? (file + 1) : path;

	if (strcmp(file, ".") == 0 || strcmp(file, "..") == 0)
		return;

	dstr_copy(&name, file);
	if (!directory) {
		char *ext = strrchr(name.array, '.');
		if (ext)
			dstr_resize(&name, ext - name.array);

		dstr_copy(&parsed_bin_path, path);
	} else {
		bin_found = parse_binary_from_directory(&parsed_bin_path,
							omp->bin, file);
	}

	parsed_data_dir = make_data_directory(name.array, omp->data);

	if (parsed_data_dir && bin_found) {
		info.bin_path = parsed_bin_path.array;
		info.data_path = parsed_data_dir;
		callback(param, &info);
	}

	bfree(parsed_data_dir);
	dstr_free(&name);
	dstr_free(&parsed_bin_path);
}

static void find_modules_in_path(struct obs_module_path *omp,
				 obs_find_module_callback_t callback,
				 void *param)
{
	struct dstr search_path = {0};
	char *module_start;
	bool search_directories = false;
	os_glob_t *gi;

	dstr_copy(&search_path, omp->bin);

	module_start = strstr(search_path.array, "%module%");
	if (module_start) {
		dstr_resize(&search_path, module_start - search_path.array);
		search_directories = true;
	}

	if (!dstr_is_empty(&search_path) && dstr_end(&search_path) != '/')
		dstr_cat_ch(&search_path, '/');

	dstr_cat_ch(&search_path, '*');
	if (!search_directories)
		dstr_cat(&search_path, get_module_extension());

	if (os_glob(search_path.array, 0, &gi) == 0) {
		for (size_t i = 0; i < gi->gl_pathc; i++) {
			if (search_directories == gi->gl_pathv[i].directory)
				process_found_module(omp, gi->gl_pathv[i].path,
						     search_directories,
						     callback, param);
		}

		os_globfree(gi);
	}

	dstr_free(&search_path);
}

void obs_find_modules(obs_find_module_callback_t callback, void *param)
{
	if (!obs)
		return;

	for (size_t i = 0; i < obs->module_paths.num; i++) {
		struct obs_module_path *omp = obs->module_paths.array + i;
		find_modules_in_path(omp, callback, param);
	}
}

void obs_enum_modules(obs_enum_module_callback_t callback, void *param)
{
	struct obs_module *module;
	if (!obs)
		return;

	module = obs->first_module;
	while (module) {
		callback(param, module);
		module = module->next;
	}
}

void free_module(struct obs_module *mod)
{
	if (!mod)
		return;

	if (mod->module) {
		if (mod->free_locale)
			mod->free_locale();

		if (mod->loaded && mod->unload)
			mod->unload();

		/* there is no real reason to close the dynamic libraries,
		 * and sometimes this can cause issues. */
		/* os_dlclose(mod->module); */
	}

	bfree(mod->mod_name);
	bfree(mod->bin_path);
	bfree(mod->data_path);
	bfree(mod);
}

lookup_t *obs_module_load_locale(obs_module_t *module,
				 const char *default_locale, const char *locale)
{
	struct dstr str = {0};
	lookup_t *lookup = NULL;

	if (!module || !default_locale || !locale) {
		blog(LOG_WARNING, "obs_module_load_locale: Invalid parameters");
		return NULL;
	}

	dstr_copy(&str, "locale/");
	dstr_cat(&str, default_locale);
	dstr_cat(&str, ".ini");

	char *file = obs_find_module_file(module, str.array);
	if (file)
		lookup = text_lookup_create(file);

	bfree(file);

	if (!lookup) {
		blog(LOG_WARNING, "Failed to load '%s' text for module: '%s'",
		     default_locale, module->file);
		goto cleanup;
	}

	if (astrcmpi(locale, default_locale) == 0)
		goto cleanup;

	dstr_copy(&str, "/locale/");
	dstr_cat(&str, locale);
	dstr_cat(&str, ".ini");

	file = obs_find_module_file(module, str.array);

	if (!text_lookup_add(lookup, file))
		blog(LOG_WARNING, "Failed to load '%s' text for module: '%s'",
		     locale, module->file);

	bfree(file);
cleanup:
	dstr_free(&str);
	return lookup;
}

#define REGISTER_OBS_DEF(size_var, structure, dest, info)               \
	do {                                                            \
		struct structure data = {0};                            \
		if (!size_var) {                                        \
			blog(LOG_ERROR, "Tried to register " #structure \
					" outside of obs_module_load"); \
			return;                                         \
		}                                                       \
                                                                        \
		if (size_var > sizeof(data)) {                          \
			blog(LOG_ERROR,                                 \
			     "Tried to register " #structure            \
			     " with size %llu which is more "           \
			     "than libobs currently supports "          \
			     "(%llu)",                                  \
			     (long long unsigned)size_var,              \
			     (long long unsigned)sizeof(data));         \
			goto error;                                     \
		}                                                       \
                                                                        \
		memcpy(&data, info, size_var);                          \
		da_push_back(dest, &data);                              \
	} while (false)

#define CHECK_REQUIRED_VAL(type, info, val, func)                       \
	do {                                                            \
		if ((offsetof(type, val) + sizeof(info->val) > size) || \
		    !info->val) {                                       \
			blog(LOG_ERROR,                                 \
			     "Required value '" #val "' for "           \
			     "'%s' not found.  " #func " failed.",      \
			     info->id);                                 \
			goto error;                                     \
		}                                                       \
	} while (false)

#define HANDLE_ERROR(size_var, structure, info)                            \
	do {                                                               \
		struct structure data = {0};                               \
		if (!size_var)                                             \
			return;                                            \
                                                                           \
		memcpy(&data, info,                                        \
		       sizeof(data) < size_var ? sizeof(data) : size_var); \
                                                                           \
		if (info->type_data && info->free_type_data)               \
			info->free_type_data(info->type_data);             \
	} while (false)

#define source_warn(format, ...) \
	blog(LOG_WARNING, "obs_register_source: " format, ##__VA_ARGS__)
#define output_warn(format, ...) \
	blog(LOG_WARNING, "obs_register_output: " format, ##__VA_ARGS__)
#define encoder_warn(format, ...) \
	blog(LOG_WARNING, "obs_register_encoder: " format, ##__VA_ARGS__)
#define service_warn(format, ...) \
	blog(LOG_WARNING, "obs_register_service: " format, ##__VA_ARGS__)

void obs_register_source_s(const struct obs_source_info *info, size_t size)
{
	struct obs_source_info data = {0};
	struct darray *array = NULL;

	if (info->type == OBS_SOURCE_TYPE_INPUT) {
		array = &obs->input_types.da;
	} else if (info->type == OBS_SOURCE_TYPE_FILTER) {
		array = &obs->filter_types.da;
	} else if (info->type == OBS_SOURCE_TYPE_TRANSITION) {
		array = &obs->transition_types.da;
	} else if (info->type != OBS_SOURCE_TYPE_SCENE) {
		source_warn("Tried to register unknown source type: %u",
			    info->type);
		goto error;
	}

	if (get_source_info2(info->id, info->version)) {
		source_warn("Source '%s' already exists!  "
			    "Duplicate library?",
			    info->id);
		goto error;
	}

	memcpy(&data, info, size);

	/* mark audio-only filters as an async filter categorically */
	if (data.type == OBS_SOURCE_TYPE_FILTER) {
		if ((data.output_flags & OBS_SOURCE_VIDEO) == 0)
			data.output_flags |= OBS_SOURCE_ASYNC;
	}

	if (data.type == OBS_SOURCE_TYPE_TRANSITION) {
		if (data.get_width)
			source_warn("get_width ignored registering "
				    "transition '%s'",
				    data.id);
		if (data.get_height)
			source_warn("get_height ignored registering "
				    "transition '%s'",
				    data.id);
		data.output_flags |= OBS_SOURCE_COMPOSITE | OBS_SOURCE_VIDEO |
				     OBS_SOURCE_CUSTOM_DRAW;
	}

	if ((data.output_flags & OBS_SOURCE_COMPOSITE) != 0) {
		if ((data.output_flags & OBS_SOURCE_AUDIO) != 0) {
			source_warn("Source '%s': Composite sources "
				    "cannot be audio sources",
				    info->id);
			goto error;
		}
		if ((data.output_flags & OBS_SOURCE_ASYNC) != 0) {
			source_warn("Source '%s': Composite sources "
				    "cannot be async sources",
				    info->id);
			goto error;
		}
	}

#define CHECK_REQUIRED_VAL_(info, val, func) \
	CHECK_REQUIRED_VAL(struct obs_source_info, info, val, func)
	CHECK_REQUIRED_VAL_(info, get_name, obs_register_source);

	if (info->type != OBS_SOURCE_TYPE_FILTER &&
	    info->type != OBS_SOURCE_TYPE_TRANSITION &&
	    (info->output_flags & OBS_SOURCE_VIDEO) != 0 &&
	    (info->output_flags & OBS_SOURCE_ASYNC) == 0) {
		CHECK_REQUIRED_VAL_(info, get_width, obs_register_source);
		CHECK_REQUIRED_VAL_(info, get_height, obs_register_source);
	}

	if ((data.output_flags & OBS_SOURCE_COMPOSITE) != 0) {
		CHECK_REQUIRED_VAL_(info, audio_render, obs_register_source);
	}
#undef CHECK_REQUIRED_VAL_

	if (size > sizeof(data)) {
		source_warn("Tried to register obs_source_info with size "
			    "%llu which is more than libobs currently "
			    "supports (%llu)",
			    (long long unsigned)size,
			    (long long unsigned)sizeof(data));
		goto error;
	}

	/* version-related stuff */
	data.unversioned_id = data.id;
	if (data.version) {
		struct dstr versioned_id = {0};
		dstr_printf(&versioned_id, "%s_v%d", data.id,
			    (int)data.version);
		data.id = versioned_id.array;
	} else {
		data.id = bstrdup(data.id);
	}

	if (array)
		darray_push_back(sizeof(struct obs_source_info), array, &data);
	da_push_back(obs->source_types, &data);
	return;

error:
	HANDLE_ERROR(size, obs_source_info, info);
}

void obs_register_output_s(const struct obs_output_info *info, size_t size)
{
	if (find_output(info->id)) {
		output_warn("Output id '%s' already exists!  "
			    "Duplicate library?",
			    info->id);
		goto error;
	}

#define CHECK_REQUIRED_VAL_(info, val, func) \
	CHECK_REQUIRED_VAL(struct obs_output_info, info, val, func)
	CHECK_REQUIRED_VAL_(info, get_name, obs_register_output);
	CHECK_REQUIRED_VAL_(info, create, obs_register_output);
	CHECK_REQUIRED_VAL_(info, destroy, obs_register_output);
	CHECK_REQUIRED_VAL_(info, start, obs_register_output);
	CHECK_REQUIRED_VAL_(info, stop, obs_register_output);

	if (info->flags & OBS_OUTPUT_ENCODED) {
		CHECK_REQUIRED_VAL_(info, encoded_packet, obs_register_output);
	} else {
		if (info->flags & OBS_OUTPUT_VIDEO)
			CHECK_REQUIRED_VAL_(info, raw_video,
					    obs_register_output);

		if (info->flags & OBS_OUTPUT_AUDIO) {
			if (info->flags & OBS_OUTPUT_MULTI_TRACK) {
				CHECK_REQUIRED_VAL_(info, raw_audio2,
						    obs_register_output);
			} else {
				CHECK_REQUIRED_VAL_(info, raw_audio,
						    obs_register_output);
			}
		}
	}
#undef CHECK_REQUIRED_VAL_

	REGISTER_OBS_DEF(size, obs_output_info, obs->output_types, info);
	return;

error:
	HANDLE_ERROR(size, obs_output_info, info);
}

void obs_register_encoder_s(const struct obs_encoder_info *info, size_t size)
{
	if (find_encoder(info->id)) {
		encoder_warn("Encoder id '%s' already exists!  "
			     "Duplicate library?",
			     info->id);
		goto error;
	}

#define CHECK_REQUIRED_VAL_(info, val, func) \
	CHECK_REQUIRED_VAL(struct obs_encoder_info, info, val, func)
	CHECK_REQUIRED_VAL_(info, get_name, obs_register_encoder);
	CHECK_REQUIRED_VAL_(info, create, obs_register_encoder);
	CHECK_REQUIRED_VAL_(info, destroy, obs_register_encoder);

	if ((info->caps & OBS_ENCODER_CAP_PASS_TEXTURE) != 0)
		CHECK_REQUIRED_VAL_(info, encode_texture, obs_register_encoder);
	else
		CHECK_REQUIRED_VAL_(info, encode, obs_register_encoder);

	if (info->type == OBS_ENCODER_AUDIO)
		CHECK_REQUIRED_VAL_(info, get_frame_size, obs_register_encoder);
#undef CHECK_REQUIRED_VAL_

	REGISTER_OBS_DEF(size, obs_encoder_info, obs->encoder_types, info);
	return;

error:
	HANDLE_ERROR(size, obs_encoder_info, info);
}

void obs_register_service_s(const struct obs_service_info *info, size_t size)
{
	if (find_service(info->id)) {
		service_warn("Service id '%s' already exists!  "
			     "Duplicate library?",
			     info->id);
		goto error;
	}

#define CHECK_REQUIRED_VAL_(info, val, func) \
	CHECK_REQUIRED_VAL(struct obs_service_info, info, val, func)
	CHECK_REQUIRED_VAL_(info, get_name, obs_register_service);
	CHECK_REQUIRED_VAL_(info, create, obs_register_service);
	CHECK_REQUIRED_VAL_(info, destroy, obs_register_service);
#undef CHECK_REQUIRED_VAL_

	REGISTER_OBS_DEF(size, obs_service_info, obs->service_types, info);
	return;

error:
	HANDLE_ERROR(size, obs_service_info, info);
}

void obs_register_modal_ui_s(const struct obs_modal_ui *info, size_t size)
{
#define CHECK_REQUIRED_VAL_(info, val, func) \
	CHECK_REQUIRED_VAL(struct obs_modal_ui, info, val, func)
	CHECK_REQUIRED_VAL_(info, task, obs_register_modal_ui);
	CHECK_REQUIRED_VAL_(info, target, obs_register_modal_ui);
	CHECK_REQUIRED_VAL_(info, exec, obs_register_modal_ui);
#undef CHECK_REQUIRED_VAL_

	REGISTER_OBS_DEF(size, obs_modal_ui, obs->modal_ui_callbacks, info);
	return;

error:
	HANDLE_ERROR(size, obs_modal_ui, info);
}

void obs_register_modeless_ui_s(const struct obs_modeless_ui *info, size_t size)
{
#define CHECK_REQUIRED_VAL_(info, val, func) \
	CHECK_REQUIRED_VAL(struct obs_modeless_ui, info, val, func)
	CHECK_REQUIRED_VAL_(info, task, obs_register_modeless_ui);
	CHECK_REQUIRED_VAL_(info, target, obs_register_modeless_ui);
	CHECK_REQUIRED_VAL_(info, create, obs_register_modeless_ui);
#undef CHECK_REQUIRED_VAL_

	REGISTER_OBS_DEF(size, obs_modeless_ui, obs->modeless_ui_callbacks,
			 info);
	return;

error:
	HANDLE_ERROR(size, obs_modeless_ui, info);
}
