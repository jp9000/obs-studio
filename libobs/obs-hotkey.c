/******************************************************************************
    Copyright (C) 2014-2015 by Ruwen Hahn <palana@stunned.de>

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

#include <inttypes.h>

#include "obs-internal.h"

static inline bool lock(void)
{
	if (!obs)
		return false;

	pthread_mutex_lock(&obs->hotkeys.mutex);
	return true;
}

static inline void unlock(void)
{
	pthread_mutex_unlock(&obs->hotkeys.mutex);
}

obs_hotkey_id obs_hotkey_get_id(const obs_hotkey_t *key)
{
	return key->id;
}

const char *obs_hotkey_get_name(const obs_hotkey_t *key)
{
	return key->name;
}

const char *obs_hotkey_get_description(const obs_hotkey_t *key)
{
	return key->description;
}

obs_hotkey_registerer_t obs_hotkey_get_registerer_type(const obs_hotkey_t *key)
{
	return key->registerer_type;
}

void *obs_hotkey_get_registerer(const obs_hotkey_t *key)
{
	return key->registerer;
}

obs_hotkey_id obs_hotkey_get_pair_partner_id(const obs_hotkey_t *key)
{
	return key->pair_partner_id;
}

obs_key_combination_t
obs_hotkey_binding_get_key_combination(obs_hotkey_binding_t *binding)
{
	return binding->key;
}

obs_hotkey_id obs_hotkey_binding_get_hotkey_id(obs_hotkey_binding_t *binding)
{
	return binding->hotkey_id;
}

obs_hotkey_t *obs_hotkey_binding_get_hotkey(obs_hotkey_binding_t *binding)
{
	return binding->hotkey;
}

static inline bool find_id(obs_hotkey_id id, size_t *idx);
void obs_hotkey_set_name(obs_hotkey_id id, const char *name)
{
	size_t idx;

	if (!find_id(id, &idx))
		return;

	obs_hotkey_t *hotkey = &obs->hotkeys.hotkeys.array[idx];
	bfree(hotkey->name);
	hotkey->name = bstrdup(name);
}

void obs_hotkey_set_description(obs_hotkey_id id, const char *desc)
{
	size_t idx;

	if (!find_id(id, &idx))
		return;

	obs_hotkey_t *hotkey = &obs->hotkeys.hotkeys.array[idx];
	bfree(hotkey->description);
	hotkey->description = bstrdup(desc);
}

static inline bool find_pair_id(obs_hotkey_pair_id id, size_t *idx);
void obs_hotkey_pair_set_names(obs_hotkey_pair_id id, const char *name0,
			       const char *name1)
{
	size_t idx;
	obs_hotkey_pair_t pair;

	if (!find_pair_id(id, &idx))
		return;

	pair = obs->hotkeys.hotkey_pairs.array[idx];

	obs_hotkey_set_name(pair.id[0], name0);
	obs_hotkey_set_name(pair.id[1], name1);
}

void obs_hotkey_pair_set_descriptions(obs_hotkey_pair_id id, const char *desc0,
				      const char *desc1)
{
	size_t idx;
	obs_hotkey_pair_t pair;

	if (!find_pair_id(id, &idx))
		return;

	pair = obs->hotkeys.hotkey_pairs.array[idx];

	obs_hotkey_set_description(pair.id[0], desc0);
	obs_hotkey_set_description(pair.id[1], desc1);
}

static void hotkey_signal(const char *signal, obs_hotkey_t *hotkey)
{
	calldata_t data;
	calldata_init(&data);
	calldata_set_ptr(&data, "key", hotkey);

	signal_handler_signal(obs->hotkeys.signals, signal, &data);

	calldata_free(&data);
}

static inline void fixup_pointers(void);
static inline void load_bindings(obs_hotkey_t *hotkey, obs_data_array_t *data);

static inline void context_add_hotkey(struct obs_context_data *context,
				      obs_hotkey_id id)
{
	da_push_back(context->hotkeys, &id);
}

static inline obs_hotkey_id
obs_hotkey_register_internal(obs_hotkey_registerer_t type, void *registerer,
			     struct obs_context_data *context, const char *name,
			     const char *description, obs_hotkey_func func,
			     void *data)
{
	if ((obs->hotkeys.next_id + 1) == OBS_INVALID_HOTKEY_ID)
		blog(LOG_WARNING, "obs-hotkey: Available hotkey ids exhausted");

	obs_hotkey_t *base_addr = obs->hotkeys.hotkeys.array;
	obs_hotkey_id result = obs->hotkeys.next_id++;
	obs_hotkey_t *hotkey = da_push_back_new(obs->hotkeys.hotkeys);

	hotkey->id = result;
	hotkey->name = bstrdup(name);
	hotkey->description = bstrdup(description);
	hotkey->func = func;
	hotkey->data = data;
	hotkey->registerer_type = type;
	hotkey->registerer = registerer;
	hotkey->pair_partner_id = OBS_INVALID_HOTKEY_PAIR_ID;

	if (context) {
		obs_data_array_t *data =
			obs_data_get_array(context->hotkey_data, name);
		load_bindings(hotkey, data);
		obs_data_array_release(data);

		context_add_hotkey(context, result);
	}

	if (base_addr != obs->hotkeys.hotkeys.array)
		fixup_pointers();

	hotkey_signal("hotkey_register", hotkey);

	return result;
}

obs_hotkey_id obs_hotkey_register_frontend(const char *name,
					   const char *description,
					   obs_hotkey_func func, void *data)
{
	if (!lock())
		return OBS_INVALID_HOTKEY_ID;

	obs_hotkey_id id = obs_hotkey_register_internal(
		OBS_HOTKEY_REGISTERER_FRONTEND, NULL, NULL, name, description,
		func, data);

	unlock();
	return id;
}

obs_hotkey_id obs_hotkey_register_encoder(obs_encoder_t *encoder,
					  const char *name,
					  const char *description,
					  obs_hotkey_func func, void *data)
{
	if (!encoder || !lock())
		return OBS_INVALID_HOTKEY_ID;

	obs_hotkey_id id = obs_hotkey_register_internal(
		OBS_HOTKEY_REGISTERER_ENCODER,
		obs_encoder_get_weak_encoder(encoder), &encoder->context, name,
		description, func, data);

	unlock();
	return id;
}

obs_hotkey_id obs_hotkey_register_output(obs_output_t *output, const char *name,
					 const char *description,
					 obs_hotkey_func func, void *data)
{
	if (!output || !lock())
		return OBS_INVALID_HOTKEY_ID;

	obs_hotkey_id id = obs_hotkey_register_internal(
		OBS_HOTKEY_REGISTERER_OUTPUT,
		obs_output_get_weak_output(output), &output->context, name,
		description, func, data);

	unlock();
	return id;
}

obs_hotkey_id obs_hotkey_register_service(obs_service_t *service,
					  const char *name,
					  const char *description,
					  obs_hotkey_func func, void *data)
{
	if (!service || !lock())
		return OBS_INVALID_HOTKEY_ID;

	obs_hotkey_id id = obs_hotkey_register_internal(
		OBS_HOTKEY_REGISTERER_SERVICE,
		obs_service_get_weak_service(service), &service->context, name,
		description, func, data);

	unlock();
	return id;
}

obs_hotkey_id obs_hotkey_register_source(obs_source_t *source, const char *name,
					 const char *description,
					 obs_hotkey_func func, void *data)
{
	if (!source || source->context.private || !lock())
		return OBS_INVALID_HOTKEY_ID;

	obs_hotkey_id id = obs_hotkey_register_internal(
		OBS_HOTKEY_REGISTERER_SOURCE,
		obs_source_get_weak_source(source), &source->context, name,
		description, func, data);

	unlock();
	return id;
}

static inline void fixup_pair_pointers(void);

static obs_hotkey_pair_t *create_hotkey_pair(struct obs_context_data *context,
					     obs_hotkey_active_func func0,
					     obs_hotkey_active_func func1,
					     void *data0, void *data1)
{
	if ((obs->hotkeys.next_pair_id + 1) == OBS_INVALID_HOTKEY_PAIR_ID)
		blog(LOG_WARNING, "obs-hotkey: Available hotkey pair ids "
				  "exhausted");

	obs_hotkey_pair_t *base_addr = obs->hotkeys.hotkey_pairs.array;
	obs_hotkey_pair_t *pair = da_push_back_new(obs->hotkeys.hotkey_pairs);

	pair->pair_id = obs->hotkeys.next_pair_id++;
	pair->func[0] = func0;
	pair->func[1] = func1;
	pair->id[0] = OBS_INVALID_HOTKEY_ID;
	pair->id[1] = OBS_INVALID_HOTKEY_ID;
	pair->data[0] = data0;
	pair->data[1] = data1;

	if (context)
		da_push_back(context->hotkey_pairs, &pair->pair_id);

	if (base_addr != obs->hotkeys.hotkey_pairs.array)
		fixup_pair_pointers();

	return pair;
}

static void obs_hotkey_pair_first_func(void *data, obs_hotkey_id id,
				       obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);

	obs_hotkey_pair_t *pair = data;
	if (pair->pressed1)
		return;

	if (pair->pressed0 && !pressed)
		pair->pressed0 = false;
	else if (pair->func[0](pair->data[0], pair->pair_id, hotkey, pressed))
		pair->pressed0 = pressed;
}

static void obs_hotkey_pair_second_func(void *data, obs_hotkey_id id,
					obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);

	obs_hotkey_pair_t *pair = data;
	if (pair->pressed0)
		return;

	if (pair->pressed1 && !pressed)
		pair->pressed1 = false;
	else if (pair->func[1](pair->data[1], pair->pair_id, hotkey, pressed))
		pair->pressed1 = pressed;
}

static inline bool find_id(obs_hotkey_id id, size_t *idx);
static obs_hotkey_pair_id register_hotkey_pair_internal(
	obs_hotkey_registerer_t type, void *registerer,
	void *(*weak_ref)(void *), struct obs_context_data *context,
	const char *name0, const char *description0, const char *name1,
	const char *description1, obs_hotkey_active_func func0,
	obs_hotkey_active_func func1, void *data0, void *data1)
{

	if (!lock())
		return OBS_INVALID_HOTKEY_PAIR_ID;

	obs_hotkey_pair_t *pair =
		create_hotkey_pair(context, func0, func1, data0, data1);

	pair->id[0] = obs_hotkey_register_internal(type, weak_ref(registerer),
						   context, name0, description0,
						   obs_hotkey_pair_first_func,
						   pair);

	pair->id[1] = obs_hotkey_register_internal(type, weak_ref(registerer),
						   context, name1, description1,
						   obs_hotkey_pair_second_func,
						   pair);

	size_t idx;
	if (find_id(pair->id[0], &idx))
		obs->hotkeys.hotkeys.array[idx].pair_partner_id = pair->id[1];

	if (find_id(pair->id[1], &idx))
		obs->hotkeys.hotkeys.array[idx].pair_partner_id = pair->id[0];

	obs_hotkey_pair_id id = pair->pair_id;

	unlock();
	return id;
}

static inline void *obs_id_(void *id_)
{
	return id_;
}

obs_hotkey_pair_id obs_hotkey_pair_register_frontend(
	const char *name0, const char *description0, const char *name1,
	const char *description1, obs_hotkey_active_func func0,
	obs_hotkey_active_func func1, void *data0, void *data1)
{
	return register_hotkey_pair_internal(OBS_HOTKEY_REGISTERER_FRONTEND,
					     NULL, obs_id_, NULL, name0,
					     description0, name1, description1,
					     func0, func1, data0, data1);
}

static inline void *weak_encoder_ref(void *ref)
{
	return obs_encoder_get_weak_encoder(ref);
}

obs_hotkey_pair_id obs_hotkey_pair_register_encoder(
	obs_encoder_t *encoder, const char *name0, const char *description0,
	const char *name1, const char *description1,
	obs_hotkey_active_func func0, obs_hotkey_active_func func1, void *data0,
	void *data1)
{
	if (!encoder)
		return OBS_INVALID_HOTKEY_PAIR_ID;
	return register_hotkey_pair_internal(OBS_HOTKEY_REGISTERER_ENCODER,
					     encoder, weak_encoder_ref,
					     &encoder->context, name0,
					     description0, name1, description1,
					     func0, func1, data0, data1);
}

static inline void *weak_output_ref(void *ref)
{
	return obs_output_get_weak_output(ref);
}

obs_hotkey_pair_id obs_hotkey_pair_register_output(
	obs_output_t *output, const char *name0, const char *description0,
	const char *name1, const char *description1,
	obs_hotkey_active_func func0, obs_hotkey_active_func func1, void *data0,
	void *data1)
{
	if (!output)
		return OBS_INVALID_HOTKEY_PAIR_ID;
	return register_hotkey_pair_internal(OBS_HOTKEY_REGISTERER_OUTPUT,
					     output, weak_output_ref,
					     &output->context, name0,
					     description0, name1, description1,
					     func0, func1, data0, data1);
}

static inline void *weak_service_ref(void *ref)
{
	return obs_service_get_weak_service(ref);
}

obs_hotkey_pair_id obs_hotkey_pair_register_service(
	obs_service_t *service, const char *name0, const char *description0,
	const char *name1, const char *description1,
	obs_hotkey_active_func func0, obs_hotkey_active_func func1, void *data0,
	void *data1)
{
	if (!service)
		return OBS_INVALID_HOTKEY_PAIR_ID;
	return register_hotkey_pair_internal(OBS_HOTKEY_REGISTERER_SERVICE,
					     service, weak_service_ref,
					     &service->context, name0,
					     description0, name1, description1,
					     func0, func1, data0, data1);
}

static inline void *weak_source_ref(void *ref)
{
	return obs_source_get_weak_source(ref);
}

obs_hotkey_pair_id obs_hotkey_pair_register_source(
	obs_source_t *source, const char *name0, const char *description0,
	const char *name1, const char *description1,
	obs_hotkey_active_func func0, obs_hotkey_active_func func1, void *data0,
	void *data1)
{
	if (!source)
		return OBS_INVALID_HOTKEY_PAIR_ID;
	return register_hotkey_pair_internal(OBS_HOTKEY_REGISTERER_SOURCE,
					     source, weak_source_ref,
					     &source->context, name0,
					     description0, name1, description1,
					     func0, func1, data0, data1);
}

typedef bool (*obs_hotkey_internal_enum_func)(void *data, size_t idx,
					      obs_hotkey_t *hotkey);

static inline void enum_hotkeys(obs_hotkey_internal_enum_func func, void *data)
{
	const size_t num = obs->hotkeys.hotkeys.num;
	obs_hotkey_t *array = obs->hotkeys.hotkeys.array;
	for (size_t i = 0; i < num; i++) {
		if (!func(data, i, &array[i]))
			break;
	}
}

typedef bool (*obs_hotkey_pair_internal_enum_func)(size_t idx,
						   obs_hotkey_pair_t *pair,
						   void *data);

static inline void enum_hotkey_pairs(obs_hotkey_pair_internal_enum_func func,
				     void *data)
{
	const size_t num = obs->hotkeys.hotkey_pairs.num;
	obs_hotkey_pair_t *array = obs->hotkeys.hotkey_pairs.array;
	for (size_t i = 0; i < num; i++) {
		if (!func(i, &array[i], data))
			break;
	}
}

typedef bool (*obs_hotkey_binding_internal_enum_func)(
	void *data, size_t idx, obs_hotkey_binding_t *binding);

static inline void enum_bindings(obs_hotkey_binding_internal_enum_func func,
				 void *data)
{
	const size_t num = obs->hotkeys.bindings.num;
	obs_hotkey_binding_t *array = obs->hotkeys.bindings.array;
	for (size_t i = 0; i < num; i++) {
		if (!func(data, i, &array[i]))
			break;
	}
}

struct obs_hotkey_internal_find_forward {
	obs_hotkey_id id;
	bool found;
	size_t idx;
};

static inline bool find_id_helper(void *data, size_t idx, obs_hotkey_t *hotkey)
{
	struct obs_hotkey_internal_find_forward *find = data;
	if (hotkey->id != find->id)
		return true;

	find->idx = idx;
	find->found = true;
	return false;
}

static inline bool find_id(obs_hotkey_id id, size_t *idx)
{
	struct obs_hotkey_internal_find_forward find = {id, false, 0};
	enum_hotkeys(find_id_helper, &find);
	*idx = find.idx;
	return find.found;
}

static inline bool pointer_fixup_func(void *data, size_t idx,
				      obs_hotkey_binding_t *binding)
{
	UNUSED_PARAMETER(idx);
	UNUSED_PARAMETER(data);

	size_t idx_;
	if (!find_id(binding->hotkey_id, &idx_)) {
		bcrash("obs-hotkey: Could not find hotkey id '%" PRIuMAX "' "
		       "for binding '%s' (modifiers 0x%x)",
		       (uintmax_t)binding->hotkey_id,
		       obs_key_to_name(binding->key.key),
		       binding->key.modifiers);
		binding->hotkey = NULL;
		return true;
	}

	binding->hotkey = &obs->hotkeys.hotkeys.array[idx_];

	return true;
}

static inline void fixup_pointers(void)
{
	enum_bindings(pointer_fixup_func, NULL);
}

struct obs_hotkey_internal_find_pair_forward {
	obs_hotkey_pair_id id;
	bool found;
	size_t idx;
};

static inline bool find_pair_id_helper(size_t idx, obs_hotkey_pair_t *pair,
				       void *data)
{
	struct obs_hotkey_internal_find_pair_forward *find = data;
	if (pair->pair_id != find->id)
		return true;

	find->idx = idx;
	find->found = true;
	return false;
}

static inline bool find_pair_id(obs_hotkey_pair_id id, size_t *idx)
{
	struct obs_hotkey_internal_find_pair_forward find = {id, false, 0};
	enum_hotkey_pairs(find_pair_id_helper, &find);
	*idx = find.idx;
	return find.found;
}

static inline bool pair_pointer_fixup_func(size_t idx, obs_hotkey_pair_t *pair,
					   void *data)
{
	UNUSED_PARAMETER(idx);
	UNUSED_PARAMETER(data);

	if (find_id(pair->id[0], &idx))
		obs->hotkeys.hotkeys.array[idx].data = pair;

	if (find_id(pair->id[1], &idx))
		obs->hotkeys.hotkeys.array[idx].data = pair;

	return true;
}

static inline void fixup_pair_pointers(void)
{
	enum_hotkey_pairs(pair_pointer_fixup_func, NULL);
}

static inline void enum_context_hotkeys(struct obs_context_data *context,
					obs_hotkey_internal_enum_func func,
					void *data)
{
	const size_t num = context->hotkeys.num;
	const obs_hotkey_id *array = context->hotkeys.array;
	obs_hotkey_t *hotkey_array = obs->hotkeys.hotkeys.array;
	for (size_t i = 0; i < num; i++) {
		size_t idx;
		if (!find_id(array[i], &idx))
			continue;

		if (!func(data, idx, &hotkey_array[idx]))
			break;
	}
}

static inline void load_modifier(uint32_t *modifiers, obs_data_t *data,
				 const char *name, uint32_t flag)
{
	if (obs_data_get_bool(data, name))
		*modifiers |= flag;
}

static inline void create_binding(obs_hotkey_t *hotkey,
				  obs_key_combination_t combo)
{
	obs_hotkey_binding_t *binding = da_push_back_new(obs->hotkeys.bindings);
	if (!binding)
		return;

	binding->key = combo;
	binding->hotkey_id = hotkey->id;
	binding->hotkey = hotkey;
}

static inline void load_binding(obs_hotkey_t *hotkey, obs_data_t *data)
{
	if (!hotkey || !data)
		return;

	obs_key_combination_t combo = {0};
	uint32_t *modifiers = &combo.modifiers;
	load_modifier(modifiers, data, "shift", INTERACT_SHIFT_KEY);
	load_modifier(modifiers, data, "control", INTERACT_CONTROL_KEY);
	load_modifier(modifiers, data, "alt", INTERACT_ALT_KEY);
	load_modifier(modifiers, data, "command", INTERACT_COMMAND_KEY);

	combo.key = obs_key_from_name(obs_data_get_string(data, "key"));
	if (!modifiers &&
	    (combo.key == OBS_KEY_NONE || combo.key >= OBS_KEY_LAST_VALUE))
		return;

	create_binding(hotkey, combo);
}

static inline void load_bindings(obs_hotkey_t *hotkey, obs_data_array_t *data)
{
	const size_t count = obs_data_array_count(data);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(data, i);
		load_binding(hotkey, item);
		obs_data_release(item);
	}

	hotkey_signal("hotkey_bindings_changed", hotkey);
}

static inline void remove_bindings(obs_hotkey_id id);

void obs_hotkey_load_bindings(obs_hotkey_id id,
			      obs_key_combination_t *combinations, size_t num)
{
	size_t idx;

	if (!lock())
		return;

	if (find_id(id, &idx)) {
		obs_hotkey_t *hotkey = &obs->hotkeys.hotkeys.array[idx];
		remove_bindings(id);
		for (size_t i = 0; i < num; i++)
			create_binding(hotkey, combinations[i]);

		hotkey_signal("hotkey_bindings_changed", hotkey);
	}
	unlock();
}

void obs_hotkey_load(obs_hotkey_id id, obs_data_array_t *data)
{
	size_t idx;

	if (!lock())
		return;

	if (find_id(id, &idx)) {
		remove_bindings(id);
		load_bindings(&obs->hotkeys.hotkeys.array[idx], data);
	}
	unlock();
}

static inline bool enum_load_bindings(void *data, size_t idx,
				      obs_hotkey_t *hotkey)
{
	UNUSED_PARAMETER(idx);

	obs_data_array_t *hotkey_data = obs_data_get_array(data, hotkey->name);
	if (!hotkey_data)
		return true;

	load_bindings(hotkey, hotkey_data);
	obs_data_array_release(hotkey_data);
	return true;
}

void obs_hotkeys_load_encoder(obs_encoder_t *encoder, obs_data_t *hotkeys)
{
	if (!encoder || !hotkeys)
		return;
	if (!lock())
		return;

	enum_context_hotkeys(&encoder->context, enum_load_bindings, hotkeys);
	unlock();
}

void obs_hotkeys_load_output(obs_output_t *output, obs_data_t *hotkeys)
{
	if (!output || !hotkeys)
		return;
	if (!lock())
		return;

	enum_context_hotkeys(&output->context, enum_load_bindings, hotkeys);
	unlock();
}

void obs_hotkeys_load_service(obs_service_t *service, obs_data_t *hotkeys)
{
	if (!service || !hotkeys)
		return;
	if (!lock())
		return;

	enum_context_hotkeys(&service->context, enum_load_bindings, hotkeys);
	unlock();
}

void obs_hotkeys_load_source(obs_source_t *source, obs_data_t *hotkeys)
{
	if (!source || !hotkeys)
		return;
	if (!lock())
		return;

	enum_context_hotkeys(&source->context, enum_load_bindings, hotkeys);
	unlock();
}

void obs_hotkey_pair_load(obs_hotkey_pair_id id, obs_data_array_t *data0,
			  obs_data_array_t *data1)
{
	if ((!data0 && !data1) || !lock())
		return;

	size_t idx;
	if (!find_pair_id(id, &idx))
		goto unlock;

	obs_hotkey_pair_t *pair = &obs->hotkeys.hotkey_pairs.array[idx];

	if (find_id(pair->id[0], &idx)) {
		remove_bindings(pair->id[0]);
		load_bindings(&obs->hotkeys.hotkeys.array[idx], data0);
	}
	if (find_id(pair->id[1], &idx)) {
		remove_bindings(pair->id[1]);
		load_bindings(&obs->hotkeys.hotkeys.array[idx], data1);
	}

unlock:
	unlock();
}

static inline void save_modifier(uint32_t modifiers, obs_data_t *data,
				 const char *name, uint32_t flag)
{
	if ((modifiers & flag) == flag)
		obs_data_set_bool(data, name, true);
}

struct save_bindings_helper_t {
	obs_data_array_t *array;
	obs_hotkey_t *hotkey;
};

static inline bool save_bindings_helper(void *data, size_t idx,
					obs_hotkey_binding_t *binding)
{
	UNUSED_PARAMETER(idx);
	struct save_bindings_helper_t *h = data;

	if (h->hotkey->id != binding->hotkey_id)
		return true;

	obs_data_t *hotkey = obs_data_create();

	uint32_t modifiers = binding->key.modifiers;
	save_modifier(modifiers, hotkey, "shift", INTERACT_SHIFT_KEY);
	save_modifier(modifiers, hotkey, "control", INTERACT_CONTROL_KEY);
	save_modifier(modifiers, hotkey, "alt", INTERACT_ALT_KEY);
	save_modifier(modifiers, hotkey, "command", INTERACT_COMMAND_KEY);

	obs_data_set_string(hotkey, "key", obs_key_to_name(binding->key.key));

	obs_data_array_push_back(h->array, hotkey);

	obs_data_release(hotkey);

	return true;
}

static inline obs_data_array_t *save_hotkey(obs_hotkey_t *hotkey)
{
	obs_data_array_t *data = obs_data_array_create();

	struct save_bindings_helper_t arg = {data, hotkey};
	enum_bindings(save_bindings_helper, &arg);

	return data;
}

obs_data_array_t *obs_hotkey_save(obs_hotkey_id id)
{
	size_t idx;
	obs_data_array_t *result = NULL;

	if (!lock())
		return result;

	if (find_id(id, &idx))
		result = save_hotkey(&obs->hotkeys.hotkeys.array[idx]);
	unlock();

	return result;
}

void obs_hotkey_pair_save(obs_hotkey_pair_id id, obs_data_array_t **p_data0,
			  obs_data_array_t **p_data1)
{
	if ((!p_data0 && !p_data1) || !lock())
		return;

	size_t idx;
	if (!find_pair_id(id, &idx))
		goto unlock;

	obs_hotkey_pair_t *pair = &obs->hotkeys.hotkey_pairs.array[idx];

	if (p_data0 && find_id(pair->id[0], &idx)) {
		*p_data0 = save_hotkey(&obs->hotkeys.hotkeys.array[idx]);
	}
	if (p_data1 && find_id(pair->id[1], &idx)) {
		*p_data1 = save_hotkey(&obs->hotkeys.hotkeys.array[idx]);
	}

unlock:
	unlock();
}

static inline bool enum_save_hotkey(void *data, size_t idx,
				    obs_hotkey_t *hotkey)
{
	UNUSED_PARAMETER(idx);

	obs_data_array_t *hotkey_data = save_hotkey(hotkey);
	obs_data_set_array(data, hotkey->name, hotkey_data);
	obs_data_array_release(hotkey_data);
	return true;
}

static inline obs_data_t *save_context_hotkeys(struct obs_context_data *context)
{
	if (!context->hotkeys.num)
		return NULL;

	obs_data_t *result = obs_data_create();
	enum_context_hotkeys(context, enum_save_hotkey, result);
	return result;
}

obs_data_t *obs_hotkeys_save_encoder(obs_encoder_t *encoder)
{
	obs_data_t *result = NULL;

	if (!lock())
		return result;

	result = save_context_hotkeys(&encoder->context);
	unlock();

	return result;
}

obs_data_t *obs_hotkeys_save_output(obs_output_t *output)
{
	obs_data_t *result = NULL;

	if (!lock())
		return result;

	result = save_context_hotkeys(&output->context);
	unlock();

	return result;
}

obs_data_t *obs_hotkeys_save_service(obs_service_t *service)
{
	obs_data_t *result = NULL;

	if (!lock())
		return result;

	result = save_context_hotkeys(&service->context);
	unlock();

	return result;
}

obs_data_t *obs_hotkeys_save_source(obs_source_t *source)
{
	obs_data_t *result = NULL;

	if (!lock())
		return result;

	result = save_context_hotkeys(&source->context);
	unlock();

	return result;
}

struct binding_find_data {
	obs_hotkey_id id;
	size_t *idx;
	bool found;
};

static inline bool binding_finder(void *data, size_t idx,
				  obs_hotkey_binding_t *binding)
{
	struct binding_find_data *find = data;
	if (binding->hotkey_id != find->id)
		return true;

	*find->idx = idx;
	find->found = true;
	return false;
}

static inline bool find_binding(obs_hotkey_id id, size_t *idx)
{
	struct binding_find_data data = {id, idx, false};
	enum_bindings(binding_finder, &data);
	return data.found;
}

static inline void release_pressed_binding(obs_hotkey_binding_t *binding);

static inline void remove_bindings(obs_hotkey_id id)
{
	size_t idx;
	while (find_binding(id, &idx)) {
		obs_hotkey_binding_t *binding =
			&obs->hotkeys.bindings.array[idx];

		if (binding->pressed)
			release_pressed_binding(binding);

		da_erase(obs->hotkeys.bindings, idx);
	}
}

static void release_registerer(obs_hotkey_t *hotkey)
{
	switch (hotkey->registerer_type) {
	case OBS_HOTKEY_REGISTERER_FRONTEND:
		break;

	case OBS_HOTKEY_REGISTERER_ENCODER:
		obs_weak_encoder_release(hotkey->registerer);
		break;

	case OBS_HOTKEY_REGISTERER_OUTPUT:
		obs_weak_output_release(hotkey->registerer);
		break;

	case OBS_HOTKEY_REGISTERER_SERVICE:
		obs_weak_service_release(hotkey->registerer);
		break;

	case OBS_HOTKEY_REGISTERER_SOURCE:
		obs_weak_source_release(hotkey->registerer);
		break;
	}

	hotkey->registerer = NULL;
}

static inline bool unregister_hotkey(obs_hotkey_id id)
{
	if (id >= obs->hotkeys.next_id)
		return false;

	size_t idx;
	if (!find_id(id, &idx))
		return false;

	obs_hotkey_t *hotkey = &obs->hotkeys.hotkeys.array[idx];

	hotkey_signal("hotkey_unregister", hotkey);

	release_registerer(hotkey);

	bfree(hotkey->name);
	bfree(hotkey->description);

	if (hotkey->registerer_type == OBS_HOTKEY_REGISTERER_SOURCE)
		obs_weak_source_release(hotkey->registerer);

	da_erase(obs->hotkeys.hotkeys, idx);
	remove_bindings(id);

	return obs->hotkeys.hotkeys.num >= idx;
}

static inline bool unregister_hotkey_pair(obs_hotkey_pair_id id)
{
	if (id >= obs->hotkeys.next_pair_id)
		return false;

	size_t idx;
	if (!find_pair_id(id, &idx))
		return false;

	obs_hotkey_pair_t *pair = &obs->hotkeys.hotkey_pairs.array[idx];

	bool need_fixup = unregister_hotkey(pair->id[0]);
	need_fixup = unregister_hotkey(pair->id[1]) || need_fixup;
	if (need_fixup)
		fixup_pointers();

	da_erase(obs->hotkeys.hotkey_pairs, idx);
	return obs->hotkeys.hotkey_pairs.num >= idx;
}

void obs_hotkey_unregister(obs_hotkey_id id)
{
	if (!lock())
		return;
	if (unregister_hotkey(id))
		fixup_pointers();
	unlock();
}

void obs_hotkey_pair_unregister(obs_hotkey_pair_id id)
{
	if (!lock())
		return;

	if (unregister_hotkey_pair(id))
		fixup_pair_pointers();

	unlock();
}

static void context_release_hotkeys(struct obs_context_data *context)
{
	if (!context->hotkeys.num)
		goto cleanup;

	bool need_fixup = false;
	for (size_t i = 0; i < context->hotkeys.num; i++)
		need_fixup = unregister_hotkey(context->hotkeys.array[i]) ||
			     need_fixup;

	if (need_fixup)
		fixup_pointers();

cleanup:
	da_free(context->hotkeys);
}

static void context_release_hotkey_pairs(struct obs_context_data *context)
{
	if (!context->hotkey_pairs.num)
		goto cleanup;

	bool need_fixup = false;
	for (size_t i = 0; i < context->hotkey_pairs.num; i++)
		need_fixup = unregister_hotkey_pair(
				     context->hotkey_pairs.array[i]) ||
			     need_fixup;

	if (need_fixup)
		fixup_pair_pointers();

cleanup:
	da_free(context->hotkey_pairs);
}

void obs_hotkeys_context_release(struct obs_context_data *context)
{
	if (!lock())
		return;

	context_release_hotkeys(context);
	context_release_hotkey_pairs(context);

	obs_data_release(context->hotkey_data);
	unlock();
}

void obs_hotkeys_free(void)
{
	const size_t num = obs->hotkeys.hotkeys.num;
	obs_hotkey_t *hotkeys = obs->hotkeys.hotkeys.array;
	for (size_t i = 0; i < num; i++) {
		bfree(hotkeys[i].name);
		bfree(hotkeys[i].description);

		release_registerer(&hotkeys[i]);
	}
	da_free(obs->hotkeys.bindings);
	da_free(obs->hotkeys.hotkeys);
	da_free(obs->hotkeys.hotkey_pairs);

	for (size_t i = 0; i < OBS_KEY_LAST_VALUE; i++) {
		if (obs->hotkeys.translations[i]) {
			bfree(obs->hotkeys.translations[i]);
			obs->hotkeys.translations[i] = NULL;
		}
	}
}

struct obs_hotkey_internal_enum_forward {
	obs_hotkey_enum_func func;
	void *data;
};

static inline bool enum_hotkey(void *data, size_t idx, obs_hotkey_t *hotkey)
{
	UNUSED_PARAMETER(idx);

	struct obs_hotkey_internal_enum_forward *forward = data;
	return forward->func(forward->data, hotkey->id, hotkey);
}

void obs_enum_hotkeys(obs_hotkey_enum_func func, void *data)
{
	struct obs_hotkey_internal_enum_forward forwarder = {func, data};
	if (!lock())
		return;

	enum_hotkeys(enum_hotkey, &forwarder);
	unlock();
}

void obs_enum_hotkey_bindings(obs_hotkey_binding_enum_func func, void *data)
{
	if (!lock())
		return;

	enum_bindings(func, data);
	unlock();
}

static inline bool modifiers_match(obs_hotkey_binding_t *binding,
				   uint32_t modifiers_, bool strict_modifiers)
{
	uint32_t modifiers = binding->key.modifiers;
	return !modifiers ||
	       (!strict_modifiers && (modifiers & modifiers_) == modifiers) ||
	       (strict_modifiers && modifiers == modifiers_);
}

static inline bool is_pressed(obs_key_t key)
{
	return obs_hotkeys_platform_is_pressed(obs->hotkeys.platform_context,
					       key);
}

static inline void press_released_binding(obs_hotkey_binding_t *binding)
{
	binding->pressed = true;

	obs_hotkey_t *hotkey = binding->hotkey;
	if (hotkey->pressed++)
		return;

	if (!obs->hotkeys.reroute_hotkeys)
		hotkey->func(hotkey->data, hotkey->id, hotkey, true);
	else if (obs->hotkeys.router_func)
		obs->hotkeys.router_func(obs->hotkeys.router_func_data,
					 hotkey->id, true);
}

static inline void release_pressed_binding(obs_hotkey_binding_t *binding)
{
	binding->pressed = false;

	obs_hotkey_t *hotkey = binding->hotkey;
	if (--hotkey->pressed)
		return;

	if (!obs->hotkeys.reroute_hotkeys)
		hotkey->func(hotkey->data, hotkey->id, hotkey, false);
	else if (obs->hotkeys.router_func)
		obs->hotkeys.router_func(obs->hotkeys.router_func_data,
					 hotkey->id, false);
}

static inline void handle_binding(obs_hotkey_binding_t *binding,
				  uint32_t modifiers, bool no_press,
				  bool strict_modifiers, bool *pressed)
{
	bool modifiers_match_ =
		modifiers_match(binding, modifiers, strict_modifiers);
	bool modifiers_only = binding->key.key == OBS_KEY_NONE;

	if (!binding->key.modifiers)
		binding->modifiers_match = true;

	if (modifiers_only)
		pressed = &modifiers_only;

	if (!binding->key.modifiers && modifiers_only)
		goto reset;

	if ((!binding->modifiers_match && !modifiers_only) || !modifiers_match_)
		goto reset;

	if ((pressed && !*pressed) ||
	    (!pressed && !is_pressed(binding->key.key)))
		goto reset;

	if (binding->pressed || no_press)
		return;

	press_released_binding(binding);
	return;

reset:
	binding->modifiers_match = modifiers_match_;
	if (!binding->pressed)
		return;

	release_pressed_binding(binding);
}

struct obs_hotkey_internal_inject {
	obs_key_combination_t hotkey;
	bool pressed;
	bool strict_modifiers;
};

static inline bool inject_hotkey(void *data, size_t idx,
				 obs_hotkey_binding_t *binding)
{
	UNUSED_PARAMETER(idx);
	struct obs_hotkey_internal_inject *event = data;

	if (modifiers_match(binding, event->hotkey.modifiers,
			    event->strict_modifiers)) {
		bool pressed = binding->key.key == event->hotkey.key &&
			       event->pressed;
		handle_binding(binding, event->hotkey.modifiers, false,
			       event->strict_modifiers, &pressed);
	}

	return true;
}

void obs_hotkey_inject_event(obs_key_combination_t hotkey, bool pressed)
{
	if (!lock())
		return;

	struct obs_hotkey_internal_inject event = {
		{hotkey.modifiers, hotkey.key},
		pressed,
		obs->hotkeys.strict_modifiers,
	};
	enum_bindings(inject_hotkey, &event);
	unlock();
}

void obs_hotkey_enable_background_press(bool enable)
{
	if (!lock())
		return;

	obs->hotkeys.thread_disable_press = !enable;
	unlock();
}

void obs_hotkey_enable_strict_modifiers(bool enable)
{
	if (!lock())
		return;

	obs->hotkeys.strict_modifiers = enable;
	unlock();
}

struct obs_query_hotkeys_helper {
	uint32_t modifiers;
	bool no_press;
	bool strict_modifiers;
};

static inline bool query_hotkey(void *data, size_t idx,
				obs_hotkey_binding_t *binding)
{
	UNUSED_PARAMETER(idx);

	struct obs_query_hotkeys_helper *param =
		(struct obs_query_hotkeys_helper *)data;
	handle_binding(binding, param->modifiers, param->no_press,
		       param->strict_modifiers, NULL);

	return true;
}

static inline void query_hotkeys()
{
	uint32_t modifiers = 0;
	if (is_pressed(OBS_KEY_SHIFT))
		modifiers |= INTERACT_SHIFT_KEY;
	if (is_pressed(OBS_KEY_CONTROL))
		modifiers |= INTERACT_CONTROL_KEY;
	if (is_pressed(OBS_KEY_ALT))
		modifiers |= INTERACT_ALT_KEY;
	if (is_pressed(OBS_KEY_META))
		modifiers |= INTERACT_COMMAND_KEY;

	struct obs_query_hotkeys_helper param = {
		modifiers,
		obs->hotkeys.thread_disable_press,
		obs->hotkeys.strict_modifiers,
	};
	enum_bindings(query_hotkey, &param);
}

#define NBSP "\xC2\xA0"

void *obs_hotkey_thread(void *arg)
{
	UNUSED_PARAMETER(arg);

	os_set_thread_name("libobs: hotkey thread");

	const char *hotkey_thread_name =
		profile_store_name(obs_get_profiler_name_store(),
				   "obs_hotkey_thread(%g" NBSP "ms)", 25.);
	profile_register_root(hotkey_thread_name, (uint64_t)25000000);

	while (os_event_timedwait(obs->hotkeys.stop_event, 25) == ETIMEDOUT) {
		if (!lock())
			continue;

		profile_start(hotkey_thread_name);
		query_hotkeys();
		profile_end(hotkey_thread_name);

		unlock();

		profile_reenable_thread();
	}
	return NULL;
}

void obs_hotkey_trigger_routed_callback(obs_hotkey_id id, bool pressed)
{
	if (!lock())
		return;

	if (!obs->hotkeys.reroute_hotkeys)
		goto unlock;

	size_t idx;
	if (!find_id(id, &idx))
		goto unlock;

	obs_hotkey_t *hotkey = &obs->hotkeys.hotkeys.array[idx];
	hotkey->func(hotkey->data, id, hotkey, pressed);

unlock:
	unlock();
}

void obs_hotkey_set_callback_routing_func(obs_hotkey_callback_router_func func,
					  void *data)
{
	if (!lock())
		return;

	obs->hotkeys.router_func = func;
	obs->hotkeys.router_func_data = data;
	unlock();
}

void obs_hotkey_enable_callback_rerouting(bool enable)
{
	if (!lock())
		return;

	obs->hotkeys.reroute_hotkeys = enable;
	unlock();
}

static void obs_set_key_translation(obs_key_t key, const char *translation)
{
	bfree(obs->hotkeys.translations[key]);
	obs->hotkeys.translations[key] = NULL;

	if (translation)
		obs->hotkeys.translations[key] = bstrdup(translation);
}

void obs_hotkeys_set_translations_s(
	struct obs_hotkeys_translations *translations, size_t size)
{
#define ADD_TRANSLATION(key_name, var_name) \
	if (t.var_name)                     \
		obs_set_key_translation(key_name, t.var_name);

	struct obs_hotkeys_translations t = {0};
	struct dstr numpad = {0};
	struct dstr mouse = {0};
	struct dstr button = {0};

	if (!translations) {
		return;
	}

	memcpy(&t, translations, (size < sizeof(t)) ? size : sizeof(t));

	ADD_TRANSLATION(OBS_KEY_INSERT, insert);
	ADD_TRANSLATION(OBS_KEY_DELETE, del);
	ADD_TRANSLATION(OBS_KEY_HOME, home);
	ADD_TRANSLATION(OBS_KEY_END, end);
	ADD_TRANSLATION(OBS_KEY_PAGEUP, page_up);
	ADD_TRANSLATION(OBS_KEY_PAGEDOWN, page_down);
	ADD_TRANSLATION(OBS_KEY_NUMLOCK, num_lock);
	ADD_TRANSLATION(OBS_KEY_SCROLLLOCK, scroll_lock);
	ADD_TRANSLATION(OBS_KEY_CAPSLOCK, caps_lock);
	ADD_TRANSLATION(OBS_KEY_BACKSPACE, backspace);
	ADD_TRANSLATION(OBS_KEY_TAB, tab);
	ADD_TRANSLATION(OBS_KEY_PRINT, print);
	ADD_TRANSLATION(OBS_KEY_PAUSE, pause);
	ADD_TRANSLATION(OBS_KEY_SHIFT, shift);
	ADD_TRANSLATION(OBS_KEY_ALT, alt);
	ADD_TRANSLATION(OBS_KEY_CONTROL, control);
	ADD_TRANSLATION(OBS_KEY_META, meta);
	ADD_TRANSLATION(OBS_KEY_MENU, menu);
	ADD_TRANSLATION(OBS_KEY_SPACE, space);
	ADD_TRANSLATION(OBS_KEY_ESCAPE, escape);
#ifdef __APPLE__
	const char *numpad_str = t.apple_keypad_num;
	ADD_TRANSLATION(OBS_KEY_NUMSLASH, apple_keypad_divide);
	ADD_TRANSLATION(OBS_KEY_NUMASTERISK, apple_keypad_multiply);
	ADD_TRANSLATION(OBS_KEY_NUMMINUS, apple_keypad_minus);
	ADD_TRANSLATION(OBS_KEY_NUMPLUS, apple_keypad_plus);
	ADD_TRANSLATION(OBS_KEY_NUMPERIOD, apple_keypad_decimal);
	ADD_TRANSLATION(OBS_KEY_NUMEQUAL, apple_keypad_equal);
#else
	const char *numpad_str = t.numpad_num;
	ADD_TRANSLATION(OBS_KEY_NUMSLASH, numpad_divide);
	ADD_TRANSLATION(OBS_KEY_NUMASTERISK, numpad_multiply);
	ADD_TRANSLATION(OBS_KEY_NUMMINUS, numpad_minus);
	ADD_TRANSLATION(OBS_KEY_NUMPLUS, numpad_plus);
	ADD_TRANSLATION(OBS_KEY_NUMPERIOD, numpad_decimal);
#endif

	if (numpad_str) {
		dstr_copy(&numpad, numpad_str);
		dstr_depad(&numpad);

		if (dstr_find(&numpad, "%1") == NULL) {
			dstr_cat(&numpad, " %1");
		}

#define ADD_NUMPAD_NUM(idx)                \
	dstr_copy_dstr(&button, &numpad);  \
	dstr_replace(&button, "%1", #idx); \
	obs_set_key_translation(OBS_KEY_NUM##idx, button.array)

		ADD_NUMPAD_NUM(0);
		ADD_NUMPAD_NUM(1);
		ADD_NUMPAD_NUM(2);
		ADD_NUMPAD_NUM(3);
		ADD_NUMPAD_NUM(4);
		ADD_NUMPAD_NUM(5);
		ADD_NUMPAD_NUM(6);
		ADD_NUMPAD_NUM(7);
		ADD_NUMPAD_NUM(8);
		ADD_NUMPAD_NUM(9);
	}

	if (t.mouse_num) {
		dstr_copy(&mouse, t.mouse_num);
		dstr_depad(&mouse);

		if (dstr_find(&mouse, "%1") == NULL) {
			dstr_cat(&mouse, " %1");
		}

#define ADD_MOUSE_NUM(idx)                 \
	dstr_copy_dstr(&button, &mouse);   \
	dstr_replace(&button, "%1", #idx); \
	obs_set_key_translation(OBS_KEY_MOUSE##idx, button.array)

		ADD_MOUSE_NUM(1);
		ADD_MOUSE_NUM(2);
		ADD_MOUSE_NUM(3);
		ADD_MOUSE_NUM(4);
		ADD_MOUSE_NUM(5);
		ADD_MOUSE_NUM(6);
		ADD_MOUSE_NUM(7);
		ADD_MOUSE_NUM(8);
		ADD_MOUSE_NUM(9);
		ADD_MOUSE_NUM(10);
		ADD_MOUSE_NUM(11);
		ADD_MOUSE_NUM(12);
		ADD_MOUSE_NUM(13);
		ADD_MOUSE_NUM(14);
		ADD_MOUSE_NUM(15);
		ADD_MOUSE_NUM(16);
		ADD_MOUSE_NUM(17);
		ADD_MOUSE_NUM(18);
		ADD_MOUSE_NUM(19);
		ADD_MOUSE_NUM(20);
		ADD_MOUSE_NUM(21);
		ADD_MOUSE_NUM(22);
		ADD_MOUSE_NUM(23);
		ADD_MOUSE_NUM(24);
		ADD_MOUSE_NUM(25);
		ADD_MOUSE_NUM(26);
		ADD_MOUSE_NUM(27);
		ADD_MOUSE_NUM(28);
		ADD_MOUSE_NUM(29);
	}

	dstr_free(&numpad);
	dstr_free(&mouse);
	dstr_free(&button);
}

const char *obs_get_hotkey_translation(obs_key_t key, const char *def)
{
	if (key == OBS_KEY_NONE) {
		return NULL;
	}

	return obs->hotkeys.translations[key] ? obs->hotkeys.translations[key]
					      : def;
}

void obs_hotkey_update_atomic(obs_hotkey_atomic_update_func func, void *data)
{
	if (!lock())
		return;

	func(data);

	unlock();
}

void obs_hotkeys_set_audio_hotkeys_translations(const char *mute,
						const char *unmute,
						const char *push_to_mute,
						const char *push_to_talk)
{
#define SET_T(n)               \
	bfree(obs->hotkeys.n); \
	obs->hotkeys.n = bstrdup(n)
	SET_T(mute);
	SET_T(unmute);
	SET_T(push_to_mute);
	SET_T(push_to_talk);
#undef SET_T
}

void obs_hotkeys_set_sceneitem_hotkeys_translations(const char *show,
						    const char *hide)
{
#define SET_T(n)                           \
	bfree(obs->hotkeys.sceneitem_##n); \
	obs->hotkeys.sceneitem_##n = bstrdup(n)
	SET_T(show);
	SET_T(hide);
#undef SET_T
}
