/******************************************************************************
    Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

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

#include "util/bmem.h"
#include "util/threading.h"
#include "util/dstr.h"
#include "util/darray.h"
#include "util/platform.h"
#include "graphics/vec2.h"
#include "graphics/vec3.h"
#include "graphics/vec4.h"
#include "graphics/quat.h"
#include "obs-data.h"

#include <jansson.h>

struct obs_data_item {
	volatile long ref;
	struct obs_data *parent;
	struct obs_data_item *next;
	enum obs_data_type type;
	size_t name_len;
	size_t data_len;
	size_t data_size;
	size_t default_len;
	size_t default_size;
	size_t autoselect_size;
	size_t capacity;
};

struct obs_data {
	volatile long ref;
	char *json;
	struct obs_data_item *first_item;
};

struct obs_data_array {
	volatile long ref;
	DARRAY(obs_data_t *) objects;
};

struct obs_data_number {
	enum obs_data_number_type type;
	union {
		long long int_val;
		double double_val;
	};
};

/* ------------------------------------------------------------------------- */
/* Item structure, designed to be one allocation only */

static inline size_t get_align_size(size_t size)
{
	const size_t alignment = base_get_alignment();
	return (size + alignment - 1) & ~(alignment - 1);
}

/* ensures data after the name has alignment (in case of SSE) */
static inline size_t get_name_align_size(const char *name)
{
	size_t name_size = strlen(name) + 1;
	size_t alignment = base_get_alignment();
	size_t total_size;

	total_size = sizeof(struct obs_data_item) + (name_size + alignment - 1);
	total_size &= ~(alignment - 1);

	return total_size - sizeof(struct obs_data_item);
}

static inline char *get_item_name(struct obs_data_item *item)
{
	return (char *)item + sizeof(struct obs_data_item);
}

static inline void *get_data_ptr(obs_data_item_t *item)
{
	return (uint8_t *)get_item_name(item) + item->name_len;
}

static inline void *get_item_data(struct obs_data_item *item)
{
	if (!item->data_size && !item->default_size && !item->autoselect_size)
		return NULL;
	return get_data_ptr(item);
}

static inline void *get_default_data_ptr(obs_data_item_t *item)
{
	return (uint8_t *)get_data_ptr(item) + item->data_len;
}

static inline void *get_item_default_data(struct obs_data_item *item)
{
	return item->default_size ? get_default_data_ptr(item) : NULL;
}

static inline void *get_autoselect_data_ptr(obs_data_item_t *item)
{
	return (uint8_t *)get_default_data_ptr(item) + item->default_len;
}

static inline void *get_item_autoselect_data(struct obs_data_item *item)
{
	return item->autoselect_size ? get_autoselect_data_ptr(item) : NULL;
}

static inline size_t obs_data_item_total_size(struct obs_data_item *item)
{
	return sizeof(struct obs_data_item) + item->name_len + item->data_len +
	       item->default_len + item->autoselect_size;
}

static inline obs_data_t *get_item_obj(struct obs_data_item *item)
{
	if (!item)
		return NULL;

	obs_data_t **data = get_item_data(item);
	if (!data)
		return NULL;

	return *data;
}

static inline obs_data_t *get_item_default_obj(struct obs_data_item *item)
{
	if (!item || !item->default_size)
		return NULL;

	return *(obs_data_t **)get_default_data_ptr(item);
}

static inline obs_data_t *get_item_autoselect_obj(struct obs_data_item *item)
{
	if (!item || !item->autoselect_size)
		return NULL;

	return *(obs_data_t **)get_autoselect_data_ptr(item);
}

static inline obs_data_array_t *get_item_array(struct obs_data_item *item)
{
	obs_data_array_t **array;

	if (!item)
		return NULL;

	array = (obs_data_array_t **)get_item_data(item);
	return array ? *array : NULL;
}

static inline obs_data_array_t *
get_item_default_array(struct obs_data_item *item)
{
	if (!item || !item->default_size)
		return NULL;

	return *(obs_data_array_t **)get_default_data_ptr(item);
}

static inline obs_data_array_t *
get_item_autoselect_array(struct obs_data_item *item)
{
	if (!item || !item->autoselect_size)
		return NULL;

	return *(obs_data_array_t **)get_autoselect_data_ptr(item);
}

static inline void item_data_release(struct obs_data_item *item)
{
	if (!obs_data_item_has_user_value(item))
		return;

	if (item->type == OBS_DATA_OBJECT) {
		obs_data_t *obj = get_item_obj(item);
		obs_data_release(obj);

	} else if (item->type == OBS_DATA_ARRAY) {
		obs_data_array_t *array = get_item_array(item);
		obs_data_array_release(array);
	}
}

static inline void item_default_data_release(struct obs_data_item *item)
{
	if (item->type == OBS_DATA_OBJECT) {
		obs_data_t *obj = get_item_default_obj(item);
		obs_data_release(obj);

	} else if (item->type == OBS_DATA_ARRAY) {
		obs_data_array_t *array = get_item_default_array(item);
		obs_data_array_release(array);
	}
}

static inline void item_autoselect_data_release(struct obs_data_item *item)
{
	if (item->type == OBS_DATA_OBJECT) {
		obs_data_t *obj = get_item_autoselect_obj(item);
		obs_data_release(obj);

	} else if (item->type == OBS_DATA_ARRAY) {
		obs_data_array_t *array = get_item_autoselect_array(item);
		obs_data_array_release(array);
	}
}

static inline void item_data_addref(struct obs_data_item *item)
{
	if (item->type == OBS_DATA_OBJECT) {
		obs_data_t *obj = get_item_obj(item);
		obs_data_addref(obj);

	} else if (item->type == OBS_DATA_ARRAY) {
		obs_data_array_t *array = get_item_array(item);
		obs_data_array_addref(array);
	}
}

static inline void item_default_data_addref(struct obs_data_item *item)
{
	if (!item->data_size)
		return;

	if (item->type == OBS_DATA_OBJECT) {
		obs_data_t *obj = get_item_default_obj(item);
		obs_data_addref(obj);

	} else if (item->type == OBS_DATA_ARRAY) {
		obs_data_array_t *array = get_item_default_array(item);
		obs_data_array_addref(array);
	}
}

static inline void item_autoselect_data_addref(struct obs_data_item *item)
{
	if (item->type == OBS_DATA_OBJECT) {
		obs_data_t *obj = get_item_autoselect_obj(item);
		obs_data_addref(obj);

	} else if (item->type == OBS_DATA_ARRAY) {
		obs_data_array_t *array = get_item_autoselect_array(item);
		obs_data_array_addref(array);
	}
}

static struct obs_data_item *obs_data_item_create(const char *name,
						  const void *data, size_t size,
						  enum obs_data_type type,
						  bool default_data,
						  bool autoselect_data)
{
	struct obs_data_item *item;
	size_t name_size, total_size;

	if (!name || !data)
		return NULL;

	name_size = get_name_align_size(name);
	total_size = name_size + sizeof(struct obs_data_item) + size;

	item = bzalloc(total_size);

	item->capacity = total_size;
	item->type = type;
	item->name_len = name_size;
	item->ref = 1;

	if (default_data) {
		item->default_len = size;
		item->default_size = size;

	} else if (autoselect_data) {
		item->autoselect_size = size;

	} else {
		item->data_len = size;
		item->data_size = size;
	}

	strcpy(get_item_name(item), name);
	memcpy(get_item_data(item), data, size);

	item_data_addref(item);
	return item;
}

static struct obs_data_item **get_item_prev_next(struct obs_data *data,
						 struct obs_data_item *current)
{
	if (!current || !data)
		return NULL;

	struct obs_data_item **prev_next = &data->first_item;
	struct obs_data_item *item = data->first_item;

	while (item) {
		if (item == current)
			return prev_next;

		prev_next = &item->next;
		item = item->next;
	}

	return NULL;
}

static inline void obs_data_item_detach(struct obs_data_item *item)
{
	struct obs_data_item **prev_next =
		get_item_prev_next(item->parent, item);

	if (prev_next) {
		*prev_next = item->next;
		item->next = NULL;
	}
}

static inline void obs_data_item_reattach(struct obs_data_item *old_ptr,
					  struct obs_data_item *new_ptr)
{
	struct obs_data_item **prev_next =
		get_item_prev_next(new_ptr->parent, old_ptr);

	if (prev_next)
		*prev_next = new_ptr;
}

static struct obs_data_item *
obs_data_item_ensure_capacity(struct obs_data_item *item)
{
	size_t new_size = obs_data_item_total_size(item);
	struct obs_data_item *new_item;

	if (item->capacity >= new_size)
		return item;

	new_item = brealloc(item, new_size);
	new_item->capacity = new_size;

	obs_data_item_reattach(item, new_item);
	return new_item;
}

static inline void obs_data_item_destroy(struct obs_data_item *item)
{
	item_data_release(item);
	item_default_data_release(item);
	item_autoselect_data_release(item);
	obs_data_item_detach(item);
	bfree(item);
}

static inline void move_data(obs_data_item_t *old_item, void *old_data,
			     obs_data_item_t *item, void *data, size_t len)
{
	ptrdiff_t old_offset = (uint8_t *)old_data - (uint8_t *)old_item;
	ptrdiff_t new_offset = (uint8_t *)data - (uint8_t *)item;

	if (!old_data)
		return;

	memmove((uint8_t *)item + new_offset, (uint8_t *)item + old_offset,
		len);
}

static inline void obs_data_item_setdata(struct obs_data_item **p_item,
					 const void *data, size_t size,
					 enum obs_data_type type)
{
	if (!p_item || !*p_item)
		return;

	struct obs_data_item *item = *p_item;
	ptrdiff_t old_default_data_pos =
		(uint8_t *)get_default_data_ptr(item) - (uint8_t *)item;
	item_data_release(item);

	item->data_size = size;
	item->type = type;
	item->data_len = (item->default_size || item->autoselect_size)
				 ? get_align_size(size)
				 : size;
	item = obs_data_item_ensure_capacity(item);

	if (item->default_size || item->autoselect_size)
		memmove(get_default_data_ptr(item),
			(uint8_t *)item + old_default_data_pos,
			item->default_len + item->autoselect_size);

	if (size) {
		memcpy(get_item_data(item), data, size);
		item_data_addref(item);
	}

	*p_item = item;
}

static inline void obs_data_item_set_default_data(struct obs_data_item **p_item,
						  const void *data, size_t size,
						  enum obs_data_type type)
{
	if (!p_item || !*p_item)
		return;

	struct obs_data_item *item = *p_item;
	void *old_autoselect_data = get_autoselect_data_ptr(item);
	item_default_data_release(item);

	item->type = type;
	item->default_size = size;
	item->default_len = item->autoselect_size ? get_align_size(size) : size;
	item->data_len = item->data_size ? get_align_size(item->data_size) : 0;
	item = obs_data_item_ensure_capacity(item);

	if (item->autoselect_size)
		move_data(*p_item, old_autoselect_data, item,
			  get_autoselect_data_ptr(item), item->autoselect_size);

	if (size) {
		memcpy(get_item_default_data(item), data, size);
		item_default_data_addref(item);
	}

	*p_item = item;
}

static inline void
obs_data_item_set_autoselect_data(struct obs_data_item **p_item,
				  const void *data, size_t size,
				  enum obs_data_type type)
{
	if (!p_item || !*p_item)
		return;

	struct obs_data_item *item = *p_item;
	item_autoselect_data_release(item);

	item->autoselect_size = size;
	item->type = type;
	item->data_len = item->data_size ? get_align_size(item->data_size) : 0;
	item->default_len =
		item->default_size ? get_align_size(item->default_size) : 0;
	item = obs_data_item_ensure_capacity(item);

	if (size) {
		memcpy(get_item_autoselect_data(item), data, size);
		item_autoselect_data_addref(item);
	}

	*p_item = item;
}

/* ------------------------------------------------------------------------- */

static void obs_data_add_json_item(obs_data_t *data, const char *key,
				   json_t *json);

static inline void obs_data_add_json_object_data(obs_data_t *data, json_t *jobj)
{
	const char *item_key;
	json_t *jitem;

	json_object_foreach (jobj, item_key, jitem) {
		obs_data_add_json_item(data, item_key, jitem);
	}
}

static inline void obs_data_add_json_object(obs_data_t *data, const char *key,
					    json_t *jobj)
{
	obs_data_t *sub_obj = obs_data_create();

	obs_data_add_json_object_data(sub_obj, jobj);
	obs_data_set_obj(data, key, sub_obj);
	obs_data_release(sub_obj);
}

static void obs_data_add_json_array(obs_data_t *data, const char *key,
				    json_t *jarray)
{
	obs_data_array_t *array = obs_data_array_create();
	size_t idx;
	json_t *jitem;

	json_array_foreach (jarray, idx, jitem) {
		obs_data_t *item;

		if (!json_is_object(jitem))
			continue;

		item = obs_data_create();
		obs_data_add_json_object_data(item, jitem);
		obs_data_array_push_back(array, item);
		obs_data_release(item);
	}

	obs_data_set_array(data, key, array);
	obs_data_array_release(array);
}

static void obs_data_add_json_item(obs_data_t *data, const char *key,
				   json_t *json)
{
	if (json_is_object(json))
		obs_data_add_json_object(data, key, json);
	else if (json_is_array(json))
		obs_data_add_json_array(data, key, json);
	else if (json_is_string(json))
		obs_data_set_string(data, key, json_string_value(json));
	else if (json_is_integer(json))
		obs_data_set_int(data, key, json_integer_value(json));
	else if (json_is_real(json))
		obs_data_set_double(data, key, json_real_value(json));
	else if (json_is_true(json))
		obs_data_set_bool(data, key, true);
	else if (json_is_false(json))
		obs_data_set_bool(data, key, false);
}

/* ------------------------------------------------------------------------- */

static inline void set_json_string(json_t *json, const char *name,
				   obs_data_item_t *item)
{
	const char *val = obs_data_item_get_string(item);
	json_object_set_new(json, name, json_string(val));
}

static inline void set_json_number(json_t *json, const char *name,
				   obs_data_item_t *item)
{
	enum obs_data_number_type type = obs_data_item_numtype(item);

	if (type == OBS_DATA_NUM_INT) {
		long long val = obs_data_item_get_int(item);
		json_object_set_new(json, name, json_integer(val));
	} else {
		double val = obs_data_item_get_double(item);
		json_object_set_new(json, name, json_real(val));
	}
}

static inline void set_json_bool(json_t *json, const char *name,
				 obs_data_item_t *item)
{
	bool val = obs_data_item_get_bool(item);
	json_object_set_new(json, name, val ? json_true() : json_false());
}

static json_t *obs_data_to_json(obs_data_t *data);

static inline void set_json_obj(json_t *json, const char *name,
				obs_data_item_t *item)
{
	obs_data_t *obj = obs_data_item_get_obj(item);
	json_object_set_new(json, name, obs_data_to_json(obj));
	obs_data_release(obj);
}

static inline void set_json_array(json_t *json, const char *name,
				  obs_data_item_t *item)
{
	json_t *jarray = json_array();
	obs_data_array_t *array = obs_data_item_get_array(item);
	size_t count = obs_data_array_count(array);

	for (size_t idx = 0; idx < count; idx++) {
		obs_data_t *sub_item = obs_data_array_item(array, idx);
		json_t *jitem = obs_data_to_json(sub_item);
		json_array_append_new(jarray, jitem);
		obs_data_release(sub_item);
	}

	json_object_set_new(json, name, jarray);
	obs_data_array_release(array);
}

static json_t *obs_data_to_json(obs_data_t *data)
{
	json_t *json = json_object();
	obs_data_item_t *item = NULL;

	for (item = obs_data_first(data); item; obs_data_item_next(&item)) {
		enum obs_data_type type = obs_data_item_gettype(item);
		const char *name = get_item_name(item);

		if (!obs_data_item_has_user_value(item))
			continue;

		if (type == OBS_DATA_STRING)
			set_json_string(json, name, item);
		else if (type == OBS_DATA_NUMBER)
			set_json_number(json, name, item);
		else if (type == OBS_DATA_BOOLEAN)
			set_json_bool(json, name, item);
		else if (type == OBS_DATA_OBJECT)
			set_json_obj(json, name, item);
		else if (type == OBS_DATA_ARRAY)
			set_json_array(json, name, item);
	}

	return json;
}

/* ------------------------------------------------------------------------- */

obs_data_t *obs_data_create()
{
	struct obs_data *data = bzalloc(sizeof(struct obs_data));
	data->ref = 1;

	return data;
}

obs_data_t *obs_data_create_from_json(const char *json_string)
{
	obs_data_t *data = obs_data_create();

	json_error_t error;
	json_t *root = json_loads(json_string, JSON_REJECT_DUPLICATES, &error);

	if (root) {
		obs_data_add_json_object_data(data, root);
		json_decref(root);
	} else {
		blog(LOG_ERROR,
		     "obs-data.c: [obs_data_create_from_json] "
		     "Failed reading json string (%d): %s",
		     error.line, error.text);
		obs_data_release(data);
		data = NULL;
	}

	return data;
}

obs_data_t *obs_data_create_from_json_file(const char *json_file)
{
	char *file_data = os_quick_read_utf8_file(json_file);
	obs_data_t *data = NULL;

	if (file_data) {
		data = obs_data_create_from_json(file_data);
		bfree(file_data);
	}

	return data;
}

obs_data_t *obs_data_create_from_json_file_safe(const char *json_file,
						const char *backup_ext)
{
	obs_data_t *file_data = obs_data_create_from_json_file(json_file);
	if (!file_data && backup_ext && *backup_ext) {
		struct dstr backup_file = {0};

		dstr_copy(&backup_file, json_file);
		if (*backup_ext != '.')
			dstr_cat(&backup_file, ".");
		dstr_cat(&backup_file, backup_ext);

		if (os_file_exists(backup_file.array)) {
			blog(LOG_WARNING,
			     "obs-data.c: "
			     "[obs_data_create_from_json_file_safe] "
			     "attempting backup file");

			/* delete current file if corrupt to prevent it from
			 * being backed up again */
			os_rename(backup_file.array, json_file);

			file_data = obs_data_create_from_json_file(json_file);
		}

		dstr_free(&backup_file);
	}

	return file_data;
}

void obs_data_addref(obs_data_t *data)
{
	if (data)
		os_atomic_inc_long(&data->ref);
}

static inline void obs_data_destroy(struct obs_data *data)
{
	struct obs_data_item *item = data->first_item;

	while (item) {
		struct obs_data_item *next = item->next;
		obs_data_item_release(&item);
		item = next;
	}

	/* NOTE: don't use bfree for json text, allocated by json */
	free(data->json);
	bfree(data);
}

void obs_data_release(obs_data_t *data)
{
	if (!data)
		return;

	if (os_atomic_dec_long(&data->ref) == 0)
		obs_data_destroy(data);
}

const char *obs_data_get_json(obs_data_t *data)
{
	if (!data)
		return NULL;

	/* NOTE: don't use libobs bfree for json text */
	free(data->json);
	data->json = NULL;

	json_t *root = obs_data_to_json(data);
	data->json = json_dumps(root, JSON_PRESERVE_ORDER | JSON_INDENT(4));
	json_decref(root);

	return data->json;
}

bool obs_data_save_json(obs_data_t *data, const char *file)
{
	const char *json = obs_data_get_json(data);

	if (json && *json) {
		return os_quick_write_utf8_file(file, json, strlen(json),
						false);
	}

	return false;
}

bool obs_data_save_json_safe(obs_data_t *data, const char *file,
			     const char *temp_ext, const char *backup_ext)
{
	const char *json = obs_data_get_json(data);

	if (json && *json) {
		return os_quick_write_utf8_file_safe(
			file, json, strlen(json), false, temp_ext, backup_ext);
	}

	return false;
}

static struct obs_data_item *get_item(struct obs_data *data, const char *name)
{
	if (!data)
		return NULL;

	struct obs_data_item *item = data->first_item;

	while (item) {
		if (strcmp(get_item_name(item), name) == 0)
			return item;

		item = item->next;
	}

	return NULL;
}

static void set_item_data(struct obs_data *data, struct obs_data_item **item,
			  const char *name, const void *ptr, size_t size,
			  enum obs_data_type type, bool default_data,
			  bool autoselect_data)
{
	obs_data_item_t *new_item = NULL;

	if ((!item || !*item) && data) {
		new_item = obs_data_item_create(name, ptr, size, type,
						default_data, autoselect_data);

		obs_data_item_t *prev = obs_data_first(data);
		obs_data_item_t *next = obs_data_first(data);
		obs_data_item_next(&next);
		for (; prev && next;
		     obs_data_item_next(&prev), obs_data_item_next(&next)) {
			if (strcmp(get_item_name(next), name) > 0)
				break;
		}

		new_item->parent = data;
		if (prev && strcmp(get_item_name(prev), name) < 0) {
			prev->next = new_item;
			new_item->next = next;

		} else {
			data->first_item = new_item;
			new_item->next = prev;
		}

		if (!prev)
			data->first_item = new_item;

		obs_data_item_release(&prev);
		obs_data_item_release(&next);

	} else if (default_data) {
		obs_data_item_set_default_data(item, ptr, size, type);
	} else if (autoselect_data) {
		obs_data_item_set_autoselect_data(item, ptr, size, type);
	} else {
		obs_data_item_setdata(item, ptr, size, type);
	}
}

static inline void set_item(struct obs_data *data, obs_data_item_t **item,
			    const char *name, const void *ptr, size_t size,
			    enum obs_data_type type)
{
	obs_data_item_t *actual_item = NULL;

	if (!data && !item)
		return;

	if (!item) {
		actual_item = get_item(data, name);
		item = &actual_item;
	}

	set_item_data(data, item, name, ptr, size, type, false, false);
}

static inline void set_item_def(struct obs_data *data, obs_data_item_t **item,
				const char *name, const void *ptr, size_t size,
				enum obs_data_type type)
{
	obs_data_item_t *actual_item = NULL;

	if (!data && !item)
		return;

	if (!item) {
		actual_item = get_item(data, name);
		item = &actual_item;
	}

	if (*item && (*item)->type != type)
		return;

	set_item_data(data, item, name, ptr, size, type, true, false);
}

static inline void set_item_auto(struct obs_data *data, obs_data_item_t **item,
				 const char *name, const void *ptr, size_t size,
				 enum obs_data_type type)
{
	obs_data_item_t *actual_item = NULL;

	if (!data && !item)
		return;

	if (!item) {
		actual_item = get_item(data, name);
		item = &actual_item;
	}

	set_item_data(data, item, name, ptr, size, type, false, true);
}

static void copy_obj(struct obs_data *data, const char *name,
		     struct obs_data *obj,
		     void (*callback)(obs_data_t *, const char *, obs_data_t *))
{
	if (obj) {
		obs_data_t *new_obj = obs_data_create();
		obs_data_apply(new_obj, obj);
		callback(data, name, new_obj);
		obs_data_release(new_obj);
	}
}

static void copy_array(struct obs_data *data, const char *name,
		       struct obs_data_array *array,
		       void (*callback)(obs_data_t *, const char *,
					obs_data_array_t *))
{
	if (array) {
		obs_data_array_t *new_array = obs_data_array_create();
		da_reserve(new_array->objects, array->objects.num);

		for (size_t i = 0; i < array->objects.num; i++) {
			obs_data_t *new_obj = obs_data_create();
			obs_data_t *obj = array->objects.array[i];

			obs_data_apply(new_obj, obj);
			obs_data_array_push_back(new_array, new_obj);

			obs_data_release(new_obj);
		}

		callback(data, name, new_array);
		obs_data_array_release(new_array);
	}
}

static inline void copy_item(struct obs_data *data, struct obs_data_item *item)
{
	const char *name = get_item_name(item);
	void *ptr = get_item_data(item);

	if (item->type == OBS_DATA_OBJECT) {
		obs_data_t **obj = item->data_size ? ptr : NULL;

		if (obj)
			copy_obj(data, name, *obj, obs_data_set_obj);

	} else if (item->type == OBS_DATA_ARRAY) {
		obs_data_array_t **array = item->data_size ? ptr : NULL;

		if (array)
			copy_array(data, name, *array, obs_data_set_array);

	} else {
		if (item->data_size)
			set_item(data, NULL, name, ptr, item->data_size,
				 item->type);
	}
}

void obs_data_apply(obs_data_t *target, obs_data_t *apply_data)
{
	struct obs_data_item *item;

	if (!target || !apply_data || target == apply_data)
		return;

	item = apply_data->first_item;

	while (item) {
		copy_item(target, item);
		item = item->next;
	}
}

void obs_data_erase(obs_data_t *data, const char *name)
{
	struct obs_data_item *item = get_item(data, name);

	if (item) {
		obs_data_item_detach(item);
		obs_data_item_release(&item);
	}
}

static inline void clear_item(struct obs_data_item *item)
{
	void *ptr = get_item_data(item);
	size_t size;

	if (item->data_len) {
		if (item->type == OBS_DATA_OBJECT) {
			obs_data_t **obj = item->data_size ? ptr : NULL;

			if (obj && *obj)
				obs_data_release(*obj);

		} else if (item->type == OBS_DATA_ARRAY) {
			obs_data_array_t **array = item->data_size ? ptr : NULL;

			if (array && *array)
				obs_data_array_release(*array);
		}

		size = item->default_len + item->autoselect_size;
		if (size)
			memmove(ptr, (uint8_t *)ptr + item->data_len, size);

		item->data_size = 0;
		item->data_len = 0;
	}
}

void obs_data_clear(obs_data_t *target)
{
	struct obs_data_item *item;

	if (!target)
		return;

	item = target->first_item;

	while (item) {
		clear_item(item);
		item = item->next;
	}
}

typedef void (*set_item_t)(obs_data_t *, obs_data_item_t **, const char *,
			   const void *, size_t, enum obs_data_type);

static inline void obs_set_string(obs_data_t *data, obs_data_item_t **item,
				  const char *name, const char *val,
				  set_item_t set_item_)
{
	if (!val)
		val = "";
	set_item_(data, item, name, val, strlen(val) + 1, OBS_DATA_STRING);
}

static inline void obs_set_int(obs_data_t *data, obs_data_item_t **item,
			       const char *name, long long val,
			       set_item_t set_item_)
{
	struct obs_data_number num;
	num.type = OBS_DATA_NUM_INT;
	num.int_val = val;
	set_item_(data, item, name, &num, sizeof(struct obs_data_number),
		  OBS_DATA_NUMBER);
}

static inline void obs_set_double(obs_data_t *data, obs_data_item_t **item,
				  const char *name, double val,
				  set_item_t set_item_)
{
	struct obs_data_number num;
	num.type = OBS_DATA_NUM_DOUBLE;
	num.double_val = val;
	set_item_(data, item, name, &num, sizeof(struct obs_data_number),
		  OBS_DATA_NUMBER);
}

static inline void obs_set_bool(obs_data_t *data, obs_data_item_t **item,
				const char *name, bool val,
				set_item_t set_item_)
{
	set_item_(data, item, name, &val, sizeof(bool), OBS_DATA_BOOLEAN);
}

static inline void obs_set_obj(obs_data_t *data, obs_data_item_t **item,
			       const char *name, obs_data_t *obj,
			       set_item_t set_item_)
{
	set_item_(data, item, name, &obj, sizeof(obs_data_t *),
		  OBS_DATA_OBJECT);
}

static inline void obs_set_array(obs_data_t *data, obs_data_item_t **item,
				 const char *name, obs_data_array_t *array,
				 set_item_t set_item_)
{
	set_item_(data, item, name, &array, sizeof(obs_data_t *),
		  OBS_DATA_ARRAY);
}

static inline void obs_take_obj(obs_data_t *data, obs_data_item_t **item,
				const char *name, obs_data_t *obj,
				set_item_t set_item_)
{
	obs_set_obj(data, item, name, obj, set_item_);
	obs_data_release(obj);
}

void obs_data_set_string(obs_data_t *data, const char *name, const char *val)
{
	obs_set_string(data, NULL, name, val, set_item);
}

void obs_data_set_int(obs_data_t *data, const char *name, long long val)
{
	obs_set_int(data, NULL, name, val, set_item);
}

void obs_data_set_double(obs_data_t *data, const char *name, double val)
{
	obs_set_double(data, NULL, name, val, set_item);
}

void obs_data_set_bool(obs_data_t *data, const char *name, bool val)
{
	obs_set_bool(data, NULL, name, val, set_item);
}

void obs_data_set_obj(obs_data_t *data, const char *name, obs_data_t *obj)
{
	obs_set_obj(data, NULL, name, obj, set_item);
}

void obs_data_set_array(obs_data_t *data, const char *name,
			obs_data_array_t *array)
{
	obs_set_array(data, NULL, name, array, set_item);
}

void obs_data_set_default_string(obs_data_t *data, const char *name,
				 const char *val)
{
	obs_set_string(data, NULL, name, val, set_item_def);
}

void obs_data_set_default_int(obs_data_t *data, const char *name, long long val)
{
	obs_set_int(data, NULL, name, val, set_item_def);
}

void obs_data_set_default_double(obs_data_t *data, const char *name, double val)
{
	obs_set_double(data, NULL, name, val, set_item_def);
}

void obs_data_set_default_bool(obs_data_t *data, const char *name, bool val)
{
	obs_set_bool(data, NULL, name, val, set_item_def);
}

void obs_data_set_default_obj(obs_data_t *data, const char *name,
			      obs_data_t *obj)
{
	obs_set_obj(data, NULL, name, obj, set_item_def);
}

void obs_data_set_autoselect_string(obs_data_t *data, const char *name,
				    const char *val)
{
	obs_set_string(data, NULL, name, val, set_item_auto);
}

void obs_data_set_autoselect_int(obs_data_t *data, const char *name,
				 long long val)
{
	obs_set_int(data, NULL, name, val, set_item_auto);
}

void obs_data_set_autoselect_double(obs_data_t *data, const char *name,
				    double val)
{
	obs_set_double(data, NULL, name, val, set_item_auto);
}

void obs_data_set_autoselect_bool(obs_data_t *data, const char *name, bool val)
{
	obs_set_bool(data, NULL, name, val, set_item_auto);
}

void obs_data_set_autoselect_obj(obs_data_t *data, const char *name,
				 obs_data_t *obj)
{
	obs_set_obj(data, NULL, name, obj, set_item_auto);
}

void obs_data_set_autoselect_array(obs_data_t *data, const char *name,
				   obs_data_array_t *arr)
{
	obs_set_array(data, NULL, name, arr, set_item_auto);
}

const char *obs_data_get_string(obs_data_t *data, const char *name)
{
	return obs_data_item_get_string(get_item(data, name));
}

long long obs_data_get_int(obs_data_t *data, const char *name)
{
	return obs_data_item_get_int(get_item(data, name));
}

double obs_data_get_double(obs_data_t *data, const char *name)
{
	return obs_data_item_get_double(get_item(data, name));
}

bool obs_data_get_bool(obs_data_t *data, const char *name)
{
	return obs_data_item_get_bool(get_item(data, name));
}

obs_data_t *obs_data_get_obj(obs_data_t *data, const char *name)
{
	return obs_data_item_get_obj(get_item(data, name));
}

obs_data_array_t *obs_data_get_array(obs_data_t *data, const char *name)
{
	return obs_data_item_get_array(get_item(data, name));
}

const char *obs_data_get_default_string(obs_data_t *data, const char *name)
{
	return obs_data_item_get_default_string(get_item(data, name));
}

long long obs_data_get_default_int(obs_data_t *data, const char *name)
{
	return obs_data_item_get_default_int(get_item(data, name));
}

double obs_data_get_default_double(obs_data_t *data, const char *name)
{
	return obs_data_item_get_default_double(get_item(data, name));
}

bool obs_data_get_default_bool(obs_data_t *data, const char *name)
{
	return obs_data_item_get_default_bool(get_item(data, name));
}

obs_data_t *obs_data_get_default_obj(obs_data_t *data, const char *name)
{
	return obs_data_item_get_default_obj(get_item(data, name));
}

obs_data_array_t *obs_data_get_default_array(obs_data_t *data, const char *name)
{
	return obs_data_item_get_default_array(get_item(data, name));
}

const char *obs_data_get_autoselect_string(obs_data_t *data, const char *name)
{
	return obs_data_item_get_autoselect_string(get_item(data, name));
}

long long obs_data_get_autoselect_int(obs_data_t *data, const char *name)
{
	return obs_data_item_get_autoselect_int(get_item(data, name));
}

double obs_data_get_autoselect_double(obs_data_t *data, const char *name)
{
	return obs_data_item_get_autoselect_double(get_item(data, name));
}

bool obs_data_get_autoselect_bool(obs_data_t *data, const char *name)
{
	return obs_data_item_get_autoselect_bool(get_item(data, name));
}

obs_data_t *obs_data_get_autoselect_obj(obs_data_t *data, const char *name)
{
	return obs_data_item_get_autoselect_obj(get_item(data, name));
}

obs_data_array_t *obs_data_get_autoselect_array(obs_data_t *data,
						const char *name)
{
	return obs_data_item_get_autoselect_array(get_item(data, name));
}

obs_data_array_t *obs_data_array_create()
{
	struct obs_data_array *array = bzalloc(sizeof(struct obs_data_array));
	array->ref = 1;

	return array;
}

void obs_data_array_addref(obs_data_array_t *array)
{
	if (array)
		os_atomic_inc_long(&array->ref);
}

static inline void obs_data_array_destroy(obs_data_array_t *array)
{
	if (array) {
		for (size_t i = 0; i < array->objects.num; i++)
			obs_data_release(array->objects.array[i]);
		da_free(array->objects);
		bfree(array);
	}
}

void obs_data_array_release(obs_data_array_t *array)
{
	if (!array)
		return;

	if (os_atomic_dec_long(&array->ref) == 0)
		obs_data_array_destroy(array);
}

size_t obs_data_array_count(obs_data_array_t *array)
{
	return array ? array->objects.num : 0;
}

obs_data_t *obs_data_array_item(obs_data_array_t *array, size_t idx)
{
	obs_data_t *data;

	if (!array)
		return NULL;

	data = (idx < array->objects.num) ? array->objects.array[idx] : NULL;

	if (data)
		os_atomic_inc_long(&data->ref);
	return data;
}

size_t obs_data_array_push_back(obs_data_array_t *array, obs_data_t *obj)
{
	if (!array || !obj)
		return 0;

	os_atomic_inc_long(&obj->ref);
	return da_push_back(array->objects, &obj);
}

void obs_data_array_insert(obs_data_array_t *array, size_t idx, obs_data_t *obj)
{
	if (!array || !obj)
		return;

	os_atomic_inc_long(&obj->ref);
	da_insert(array->objects, idx, &obj);
}

void obs_data_array_push_back_array(obs_data_array_t *array,
				    obs_data_array_t *array2)
{
	if (!array || !array2)
		return;

	for (size_t i = 0; i < array2->objects.num; i++) {
		obs_data_t *obj = array2->objects.array[i];
		obs_data_addref(obj);
	}
	da_push_back_da(array->objects, array2->objects);
}

void obs_data_array_erase(obs_data_array_t *array, size_t idx)
{
	if (array) {
		obs_data_release(array->objects.array[idx]);
		da_erase(array->objects, idx);
	}
}

/* ------------------------------------------------------------------------- */
/* Item status inspection */

bool obs_data_has_user_value(obs_data_t *data, const char *name)
{
	return data && obs_data_item_has_user_value(get_item(data, name));
}

bool obs_data_has_default_value(obs_data_t *data, const char *name)
{
	return data && obs_data_item_has_default_value(get_item(data, name));
}

bool obs_data_has_autoselect_value(obs_data_t *data, const char *name)
{
	return data && obs_data_item_has_autoselect_value(get_item(data, name));
}

bool obs_data_item_has_user_value(obs_data_item_t *item)
{
	return item && item->data_size;
}

bool obs_data_item_has_default_value(obs_data_item_t *item)
{
	return item && item->default_size;
}

bool obs_data_item_has_autoselect_value(obs_data_item_t *item)
{
	return item && item->autoselect_size;
}

/* ------------------------------------------------------------------------- */
/* Clearing data values */

void obs_data_unset_user_value(obs_data_t *data, const char *name)
{
	obs_data_item_unset_user_value(get_item(data, name));
}

void obs_data_unset_default_value(obs_data_t *data, const char *name)
{
	obs_data_item_unset_default_value(get_item(data, name));
}

void obs_data_unset_autoselect_value(obs_data_t *data, const char *name)
{
	obs_data_item_unset_autoselect_value(get_item(data, name));
}

void obs_data_item_unset_user_value(obs_data_item_t *item)
{
	if (!item || !item->data_size)
		return;

	void *old_non_user_data = get_default_data_ptr(item);

	item_data_release(item);
	item->data_size = 0;
	item->data_len = 0;

	if (item->default_size || item->autoselect_size)
		move_data(item, old_non_user_data, item,
			  get_default_data_ptr(item),
			  item->default_len + item->autoselect_size);
}

void obs_data_item_unset_default_value(obs_data_item_t *item)
{
	if (!item || !item->default_size)
		return;

	void *old_autoselect_data = get_autoselect_data_ptr(item);

	item_default_data_release(item);
	item->default_size = 0;
	item->default_len = 0;

	if (item->autoselect_size)
		move_data(item, old_autoselect_data, item,
			  get_autoselect_data_ptr(item), item->autoselect_size);
}

void obs_data_item_unset_autoselect_value(obs_data_item_t *item)
{
	if (!item || !item->autoselect_size)
		return;

	item_autoselect_data_release(item);
	item->autoselect_size = 0;
}

/* ------------------------------------------------------------------------- */
/* Item iteration */

obs_data_item_t *obs_data_first(obs_data_t *data)
{
	if (!data)
		return NULL;

	if (data->first_item)
		os_atomic_inc_long(&data->first_item->ref);
	return data->first_item;
}

obs_data_item_t *obs_data_item_byname(obs_data_t *data, const char *name)
{
	if (!data)
		return NULL;

	struct obs_data_item *item = get_item(data, name);
	if (item)
		os_atomic_inc_long(&item->ref);
	return item;
}

bool obs_data_item_next(obs_data_item_t **item)
{
	if (item && *item) {
		obs_data_item_t *next = (*item)->next;
		obs_data_item_release(item);

		*item = next;

		if (next) {
			os_atomic_inc_long(&next->ref);
			return true;
		}
	}

	return false;
}

void obs_data_item_release(obs_data_item_t **item)
{
	if (item && *item) {
		long ref = os_atomic_dec_long(&(*item)->ref);
		if (!ref) {
			obs_data_item_destroy(*item);
			*item = NULL;
		}
	}
}

void obs_data_item_remove(obs_data_item_t **item)
{
	if (item && *item) {
		obs_data_item_detach(*item);
		obs_data_item_release(item);
	}
}

enum obs_data_type obs_data_item_gettype(obs_data_item_t *item)
{
	return item ? item->type : OBS_DATA_NULL;
}

enum obs_data_number_type obs_data_item_numtype(obs_data_item_t *item)
{
	struct obs_data_number *num;

	if (!item || item->type != OBS_DATA_NUMBER)
		return OBS_DATA_NUM_INVALID;

	num = get_item_data(item);
	return num->type;
}

const char *obs_data_item_get_name(obs_data_item_t *item)
{
	if (!item)
		return NULL;

	return get_item_name(item);
}

void obs_data_item_set_string(obs_data_item_t **item, const char *val)
{
	obs_set_string(NULL, item, NULL, val, set_item);
}

void obs_data_item_set_int(obs_data_item_t **item, long long val)
{
	obs_set_int(NULL, item, NULL, val, set_item);
}

void obs_data_item_set_double(obs_data_item_t **item, double val)
{
	obs_set_double(NULL, item, NULL, val, set_item);
}

void obs_data_item_set_bool(obs_data_item_t **item, bool val)
{
	obs_set_bool(NULL, item, NULL, val, set_item);
}

void obs_data_item_set_obj(obs_data_item_t **item, obs_data_t *val)
{
	obs_set_obj(NULL, item, NULL, val, set_item);
}

void obs_data_item_set_array(obs_data_item_t **item, obs_data_array_t *val)
{
	obs_set_array(NULL, item, NULL, val, set_item);
}

void obs_data_item_set_default_string(obs_data_item_t **item, const char *val)
{
	obs_set_string(NULL, item, NULL, val, set_item_def);
}

void obs_data_item_set_default_int(obs_data_item_t **item, long long val)
{
	obs_set_int(NULL, item, NULL, val, set_item_def);
}

void obs_data_item_set_default_double(obs_data_item_t **item, double val)
{
	obs_set_double(NULL, item, NULL, val, set_item_def);
}

void obs_data_item_set_default_bool(obs_data_item_t **item, bool val)
{
	obs_set_bool(NULL, item, NULL, val, set_item_def);
}

void obs_data_item_set_default_obj(obs_data_item_t **item, obs_data_t *val)
{
	obs_set_obj(NULL, item, NULL, val, set_item_def);
}

void obs_data_item_set_default_array(obs_data_item_t **item,
				     obs_data_array_t *val)
{
	obs_set_array(NULL, item, NULL, val, set_item_def);
}

void obs_data_item_set_autoselect_string(obs_data_item_t **item,
					 const char *val)
{
	obs_set_string(NULL, item, NULL, val, set_item_auto);
}

void obs_data_item_set_autoselect_int(obs_data_item_t **item, long long val)
{
	obs_set_int(NULL, item, NULL, val, set_item_auto);
}

void obs_data_item_set_autoselect_double(obs_data_item_t **item, double val)
{
	obs_set_double(NULL, item, NULL, val, set_item_auto);
}

void obs_data_item_set_autoselect_bool(obs_data_item_t **item, bool val)
{
	obs_set_bool(NULL, item, NULL, val, set_item_auto);
}

void obs_data_item_set_autoselect_obj(obs_data_item_t **item, obs_data_t *val)
{
	obs_set_obj(NULL, item, NULL, val, set_item_auto);
}

void obs_data_item_set_autoselect_array(obs_data_item_t **item,
					obs_data_array_t *val)
{
	obs_set_array(NULL, item, NULL, val, set_item_auto);
}

static inline bool item_valid(struct obs_data_item *item,
			      enum obs_data_type type)
{
	return item && item->type == type;
}

typedef void *(*get_data_t)(obs_data_item_t *);

static inline const char *data_item_get_string(obs_data_item_t *item,
					       get_data_t get_data)
{
	return item_valid(item, OBS_DATA_STRING) && get_data(item)
		       ? get_data(item)
		       : "";
}

static inline long long item_int(struct obs_data_item *item,
				 get_data_t get_data)
{
	if (item && get_data(item)) {
		struct obs_data_number *num = get_data(item);
		return (num->type == OBS_DATA_NUM_INT)
			       ? num->int_val
			       : (long long)num->double_val;
	}

	return 0;
}

static inline long long data_item_get_int(obs_data_item_t *item,
					  get_data_t get_data)
{
	return item_int(item_valid(item, OBS_DATA_NUMBER) ? item : NULL,
			get_data);
}

static inline double item_double(struct obs_data_item *item,
				 get_data_t get_data)
{
	if (item && get_data(item)) {
		struct obs_data_number *num = get_data(item);
		return (num->type == OBS_DATA_NUM_INT) ? (double)num->int_val
						       : num->double_val;
	}

	return 0.0;
}

static inline double data_item_get_double(obs_data_item_t *item,
					  get_data_t get_data)
{
	return item_double(item_valid(item, OBS_DATA_NUMBER) ? item : NULL,
			   get_data);
}

static inline bool data_item_get_bool(obs_data_item_t *item,
				      get_data_t get_data)
{
	return item_valid(item, OBS_DATA_BOOLEAN) && get_data(item)
		       ? *(bool *)get_data(item)
		       : false;
}

typedef obs_data_t *(*get_obj_t)(obs_data_item_t *);

static inline obs_data_t *data_item_get_obj(obs_data_item_t *item,
					    get_obj_t get_obj)
{
	obs_data_t *obj = item_valid(item, OBS_DATA_OBJECT) ? get_obj(item)
							    : NULL;

	if (obj)
		os_atomic_inc_long(&obj->ref);
	return obj;
}

typedef obs_data_array_t *(*get_array_t)(obs_data_item_t *);

static inline obs_data_array_t *data_item_get_array(obs_data_item_t *item,
						    get_array_t get_array)
{
	obs_data_array_t *array =
		item_valid(item, OBS_DATA_ARRAY) ? get_array(item) : NULL;

	if (array)
		os_atomic_inc_long(&array->ref);
	return array;
}

const char *obs_data_item_get_string(obs_data_item_t *item)
{
	return data_item_get_string(item, get_item_data);
}

long long obs_data_item_get_int(obs_data_item_t *item)
{
	return data_item_get_int(item, get_item_data);
}

double obs_data_item_get_double(obs_data_item_t *item)
{
	return data_item_get_double(item, get_item_data);
}

bool obs_data_item_get_bool(obs_data_item_t *item)
{
	return data_item_get_bool(item, get_item_data);
}

obs_data_t *obs_data_item_get_obj(obs_data_item_t *item)
{
	return data_item_get_obj(item, get_item_obj);
}

obs_data_array_t *obs_data_item_get_array(obs_data_item_t *item)
{
	return data_item_get_array(item, get_item_array);
}

const char *obs_data_item_get_default_string(obs_data_item_t *item)
{
	return data_item_get_string(item, get_item_default_data);
}

long long obs_data_item_get_default_int(obs_data_item_t *item)
{
	return data_item_get_int(item, get_item_default_data);
}

double obs_data_item_get_default_double(obs_data_item_t *item)
{
	return data_item_get_double(item, get_item_default_data);
}

bool obs_data_item_get_default_bool(obs_data_item_t *item)
{
	return data_item_get_bool(item, get_item_default_data);
}

obs_data_t *obs_data_item_get_default_obj(obs_data_item_t *item)
{
	return data_item_get_obj(item, get_item_default_obj);
}

obs_data_array_t *obs_data_item_get_default_array(obs_data_item_t *item)
{
	return data_item_get_array(item, get_item_default_array);
}

const char *obs_data_item_get_autoselect_string(obs_data_item_t *item)
{
	return data_item_get_string(item, get_item_autoselect_data);
}

long long obs_data_item_get_autoselect_int(obs_data_item_t *item)
{
	return data_item_get_int(item, get_item_autoselect_data);
}

double obs_data_item_get_autoselect_double(obs_data_item_t *item)
{
	return data_item_get_double(item, get_item_autoselect_data);
}

bool obs_data_item_get_autoselect_bool(obs_data_item_t *item)
{
	return data_item_get_bool(item, get_item_autoselect_data);
}

obs_data_t *obs_data_item_get_autoselect_obj(obs_data_item_t *item)
{
	return data_item_get_obj(item, get_item_autoselect_obj);
}

obs_data_array_t *obs_data_item_get_autoselect_array(obs_data_item_t *item)
{
	return data_item_get_array(item, get_item_autoselect_array);
}

/* ------------------------------------------------------------------------- */
/* Helper functions for certain structures */

typedef void (*set_obj_t)(obs_data_t *, const char *, obs_data_t *);

static inline void set_vec2(obs_data_t *data, const char *name,
			    const struct vec2 *val, set_obj_t set_obj)
{
	obs_data_t *obj = obs_data_create();
	obs_data_set_double(obj, "x", val->x);
	obs_data_set_double(obj, "y", val->y);
	set_obj(data, name, obj);
	obs_data_release(obj);
}

static inline void set_vec3(obs_data_t *data, const char *name,
			    const struct vec3 *val, set_obj_t set_obj)
{
	obs_data_t *obj = obs_data_create();
	obs_data_set_double(obj, "x", val->x);
	obs_data_set_double(obj, "y", val->y);
	obs_data_set_double(obj, "z", val->z);
	set_obj(data, name, obj);
	obs_data_release(obj);
}

static inline void set_vec4(obs_data_t *data, const char *name,
			    const struct vec4 *val, set_obj_t set_obj)
{
	obs_data_t *obj = obs_data_create();
	obs_data_set_double(obj, "x", val->x);
	obs_data_set_double(obj, "y", val->y);
	obs_data_set_double(obj, "z", val->z);
	obs_data_set_double(obj, "w", val->w);
	set_obj(data, name, obj);
	obs_data_release(obj);
}

static inline void set_quat(obs_data_t *data, const char *name,
			    const struct quat *val, set_obj_t set_obj)
{
	obs_data_t *obj = obs_data_create();
	obs_data_set_double(obj, "x", val->x);
	obs_data_set_double(obj, "y", val->y);
	obs_data_set_double(obj, "z", val->z);
	obs_data_set_double(obj, "w", val->w);
	set_obj(data, name, obj);
	obs_data_release(obj);
}

void obs_data_set_vec2(obs_data_t *data, const char *name,
		       const struct vec2 *val)
{
	set_vec2(data, name, val, obs_data_set_obj);
}

void obs_data_set_vec3(obs_data_t *data, const char *name,
		       const struct vec3 *val)
{
	set_vec3(data, name, val, obs_data_set_obj);
}

void obs_data_set_vec4(obs_data_t *data, const char *name,
		       const struct vec4 *val)
{
	set_vec4(data, name, val, obs_data_set_obj);
}

void obs_data_set_quat(obs_data_t *data, const char *name,
		       const struct quat *val)
{
	set_quat(data, name, val, obs_data_set_obj);
}

void obs_data_set_default_vec2(obs_data_t *data, const char *name,
			       const struct vec2 *val)
{
	set_vec2(data, name, val, obs_data_set_default_obj);
}

void obs_data_set_default_vec3(obs_data_t *data, const char *name,
			       const struct vec3 *val)
{
	set_vec3(data, name, val, obs_data_set_default_obj);
}

void obs_data_set_default_vec4(obs_data_t *data, const char *name,
			       const struct vec4 *val)
{
	set_vec4(data, name, val, obs_data_set_default_obj);
}

void obs_data_set_default_quat(obs_data_t *data, const char *name,
			       const struct quat *val)
{
	set_quat(data, name, val, obs_data_set_default_obj);
}

void obs_data_set_autoselect_vec2(obs_data_t *data, const char *name,
				  const struct vec2 *val)
{
	set_vec2(data, name, val, obs_data_set_autoselect_obj);
}

void obs_data_set_autoselect_vec3(obs_data_t *data, const char *name,
				  const struct vec3 *val)
{
	set_vec3(data, name, val, obs_data_set_autoselect_obj);
}

void obs_data_set_autoselect_vec4(obs_data_t *data, const char *name,
				  const struct vec4 *val)
{
	set_vec4(data, name, val, obs_data_set_autoselect_obj);
}

void obs_data_set_autoselect_quat(obs_data_t *data, const char *name,
				  const struct quat *val)
{
	set_quat(data, name, val, obs_data_set_autoselect_obj);
}

static inline void get_vec2(obs_data_t *obj, struct vec2 *val)
{
	if (!obj)
		return;

	val->x = (float)obs_data_get_double(obj, "x");
	val->y = (float)obs_data_get_double(obj, "y");
	obs_data_release(obj);
}

static inline void get_vec3(obs_data_t *obj, struct vec3 *val)
{
	if (!obj)
		return;

	val->x = (float)obs_data_get_double(obj, "x");
	val->y = (float)obs_data_get_double(obj, "y");
	val->z = (float)obs_data_get_double(obj, "z");
	obs_data_release(obj);
}

static inline void get_vec4(obs_data_t *obj, struct vec4 *val)
{
	if (!obj)
		return;

	val->x = (float)obs_data_get_double(obj, "x");
	val->y = (float)obs_data_get_double(obj, "y");
	val->z = (float)obs_data_get_double(obj, "z");
	val->w = (float)obs_data_get_double(obj, "w");
	obs_data_release(obj);
}

static inline void get_quat(obs_data_t *obj, struct quat *val)
{
	if (!obj)
		return;

	val->x = (float)obs_data_get_double(obj, "x");
	val->y = (float)obs_data_get_double(obj, "y");
	val->z = (float)obs_data_get_double(obj, "z");
	val->w = (float)obs_data_get_double(obj, "w");
	obs_data_release(obj);
}

void obs_data_get_vec2(obs_data_t *data, const char *name, struct vec2 *val)
{
	get_vec2(obs_data_get_obj(data, name), val);
}

void obs_data_get_vec3(obs_data_t *data, const char *name, struct vec3 *val)
{
	get_vec3(obs_data_get_obj(data, name), val);
}

void obs_data_get_vec4(obs_data_t *data, const char *name, struct vec4 *val)
{
	get_vec4(obs_data_get_obj(data, name), val);
}

void obs_data_get_quat(obs_data_t *data, const char *name, struct quat *val)
{
	get_quat(obs_data_get_obj(data, name), val);
}

void obs_data_get_default_vec2(obs_data_t *data, const char *name,
			       struct vec2 *val)
{
	get_vec2(obs_data_get_default_obj(data, name), val);
}

void obs_data_get_default_vec3(obs_data_t *data, const char *name,
			       struct vec3 *val)
{
	get_vec3(obs_data_get_default_obj(data, name), val);
}

void obs_data_get_default_vec4(obs_data_t *data, const char *name,
			       struct vec4 *val)
{
	get_vec4(obs_data_get_default_obj(data, name), val);
}

void obs_data_get_default_quat(obs_data_t *data, const char *name,
			       struct quat *val)
{
	get_quat(obs_data_get_default_obj(data, name), val);
}

void obs_data_get_autoselect_vec2(obs_data_t *data, const char *name,
				  struct vec2 *val)
{
	get_vec2(obs_data_get_autoselect_obj(data, name), val);
}

void obs_data_get_autoselect_vec3(obs_data_t *data, const char *name,
				  struct vec3 *val)
{
	get_vec3(obs_data_get_autoselect_obj(data, name), val);
}

void obs_data_get_autoselect_vec4(obs_data_t *data, const char *name,
				  struct vec4 *val)
{
	get_vec4(obs_data_get_autoselect_obj(data, name), val);
}

void obs_data_get_autoselect_quat(obs_data_t *data, const char *name,
				  struct quat *val)
{
	get_quat(obs_data_get_autoselect_obj(data, name), val);
}

/* ------------------------------------------------------------------------- */
/* Helper functions for media_frames_per_seconds */

static inline obs_data_t *
make_frames_per_second(struct media_frames_per_second fps, const char *option)
{
	obs_data_t *obj = obs_data_create();

	if (!option) {
		obs_data_set_double(obj, "numerator", fps.numerator);
		obs_data_set_double(obj, "denominator", fps.denominator);

	} else {
		obs_data_set_string(obj, "option", option);
	}

	return obj;
}

void obs_data_set_frames_per_second(obs_data_t *data, const char *name,
				    struct media_frames_per_second fps,
				    const char *option)
{
	obs_take_obj(data, NULL, name, make_frames_per_second(fps, option),
		     set_item);
}

void obs_data_set_default_frames_per_second(obs_data_t *data, const char *name,
					    struct media_frames_per_second fps,
					    const char *option)
{
	obs_take_obj(data, NULL, name, make_frames_per_second(fps, option),
		     set_item_def);
}

void obs_data_set_autoselect_frames_per_second(
	obs_data_t *data, const char *name, struct media_frames_per_second fps,
	const char *option)
{
	obs_take_obj(data, NULL, name, make_frames_per_second(fps, option),
		     set_item_auto);
}

static inline bool get_option(obs_data_t *data, const char **option)
{
	if (!option)
		return false;

	struct obs_data_item *opt = obs_data_item_byname(data, "option");
	if (!opt)
		return false;

	*option = obs_data_item_get_string(opt);
	obs_data_item_release(&opt);

	obs_data_release(data);

	return true;
}

#define CLAMP(x, min, max) ((x) < min ? min : ((x) > max ? max : (x)))

static inline bool get_frames_per_second(obs_data_t *data,
					 struct media_frames_per_second *fps,
					 const char **option)
{
	if (!data)
		return false;

	if (get_option(data, option))
		return true;

	if (!fps)
		goto free;

	struct obs_data_item *num = obs_data_item_byname(data, "numerator");
	struct obs_data_item *den = obs_data_item_byname(data, "denominator");
	if (!num || !den) {
		obs_data_item_release(&num);
		obs_data_item_release(&den);
		goto free;
	}

	long long num_ll = obs_data_item_get_int(num);
	long long den_ll = obs_data_item_get_int(den);

	fps->numerator = (uint32_t)CLAMP(num_ll, 0, (long long)UINT32_MAX);
	fps->denominator = (uint32_t)CLAMP(den_ll, 0, (long long)UINT32_MAX);

	obs_data_item_release(&num);
	obs_data_item_release(&den);

	obs_data_release(data);

	return media_frames_per_second_is_valid(*fps);

free:
	obs_data_release(data);
	return false;
}

bool obs_data_get_frames_per_second(obs_data_t *data, const char *name,
				    struct media_frames_per_second *fps,
				    const char **option)
{
	return get_frames_per_second(obs_data_get_obj(data, name), fps, option);
}

bool obs_data_get_default_frames_per_second(obs_data_t *data, const char *name,
					    struct media_frames_per_second *fps,
					    const char **option)
{
	return get_frames_per_second(obs_data_get_default_obj(data, name), fps,
				     option);
}

bool obs_data_get_autoselect_frames_per_second(
	obs_data_t *data, const char *name, struct media_frames_per_second *fps,
	const char **option)
{
	return get_frames_per_second(obs_data_get_autoselect_obj(data, name),
				     fps, option);
}

void obs_data_item_set_frames_per_second(obs_data_item_t **item,
					 struct media_frames_per_second fps,
					 const char *option)
{
	obs_take_obj(NULL, item, NULL, make_frames_per_second(fps, option),
		     set_item);
}

void obs_data_item_set_default_frames_per_second(
	obs_data_item_t **item, struct media_frames_per_second fps,
	const char *option)
{
	obs_take_obj(NULL, item, NULL, make_frames_per_second(fps, option),
		     set_item_def);
}

void obs_data_item_set_autoselect_frames_per_second(
	obs_data_item_t **item, struct media_frames_per_second fps,
	const char *option)
{
	obs_take_obj(NULL, item, NULL, make_frames_per_second(fps, option),
		     set_item_auto);
}

bool obs_data_item_get_frames_per_second(obs_data_item_t *item,
					 struct media_frames_per_second *fps,
					 const char **option)
{
	return get_frames_per_second(obs_data_item_get_obj(item), fps, option);
}

bool obs_data_item_get_default_frames_per_second(
	obs_data_item_t *item, struct media_frames_per_second *fps,
	const char **option)
{
	return get_frames_per_second(obs_data_item_get_default_obj(item), fps,
				     option);
}

bool obs_data_item_get_autoselect_frames_per_second(
	obs_data_item_t *item, struct media_frames_per_second *fps,
	const char **option)
{
	return get_frames_per_second(obs_data_item_get_autoselect_obj(item),
				     fps, option);
}
