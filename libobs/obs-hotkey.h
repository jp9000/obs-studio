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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t obs_hotkey_id;
typedef size_t obs_hotkey_pair_id;

#ifndef SWIG
#define OBS_INVALID_HOTKEY_ID (~(obs_hotkey_id)0)
#define OBS_INVALID_HOTKEY_PAIR_ID (~(obs_hotkey_pair_id)0)
#else
const size_t OBS_INVALID_HOTKEY_ID = (size_t)-1;
const size_t OBS_INVALID_HOTKEY_PAIR_ID = (size_t)-1;
#endif

#define XINPUT_MOUSE_LEN 33

enum obs_key {
#define OBS_HOTKEY(x) x,
#include "obs-hotkeys.h"
#undef OBS_HOTKEY
	OBS_KEY_LAST_VALUE //not an actual key
};
typedef enum obs_key obs_key_t;

struct obs_key_combination {
	uint32_t modifiers;
	obs_key_t key;
};
typedef struct obs_key_combination obs_key_combination_t;

typedef struct obs_hotkey obs_hotkey_t;
typedef struct obs_hotkey_binding obs_hotkey_binding_t;

enum obs_hotkey_registerer_type {
	OBS_HOTKEY_REGISTERER_FRONTEND,
	OBS_HOTKEY_REGISTERER_SOURCE,
	OBS_HOTKEY_REGISTERER_OUTPUT,
	OBS_HOTKEY_REGISTERER_ENCODER,
	OBS_HOTKEY_REGISTERER_SERVICE,
};
typedef enum obs_hotkey_registerer_type obs_hotkey_registerer_t;

/* getter functions */

EXPORT obs_hotkey_id obs_hotkey_get_id(const obs_hotkey_t *key);
EXPORT const char *obs_hotkey_get_name(const obs_hotkey_t *key);
EXPORT const char *obs_hotkey_get_description(const obs_hotkey_t *key);
EXPORT obs_hotkey_registerer_t
obs_hotkey_get_registerer_type(const obs_hotkey_t *key);
EXPORT void *obs_hotkey_get_registerer(const obs_hotkey_t *key);
EXPORT obs_hotkey_id obs_hotkey_get_pair_partner_id(const obs_hotkey_t *key);

EXPORT obs_key_combination_t
obs_hotkey_binding_get_key_combination(obs_hotkey_binding_t *binding);
EXPORT obs_hotkey_id
obs_hotkey_binding_get_hotkey_id(obs_hotkey_binding_t *binding);
EXPORT obs_hotkey_t *
obs_hotkey_binding_get_hotkey(obs_hotkey_binding_t *binding);

/* setter functions */

EXPORT void obs_hotkey_set_name(obs_hotkey_id id, const char *name);
EXPORT void obs_hotkey_set_description(obs_hotkey_id id, const char *desc);
EXPORT void obs_hotkey_pair_set_names(obs_hotkey_pair_id id, const char *name0,
				      const char *name1);
EXPORT void obs_hotkey_pair_set_descriptions(obs_hotkey_pair_id id,
					     const char *desc0,
					     const char *desc1);

#ifndef SWIG
struct obs_hotkeys_translations {
	const char *insert;
	const char *del;
	const char *home;
	const char *end;
	const char *page_up;
	const char *page_down;
	const char *num_lock;
	const char *scroll_lock;
	const char *caps_lock;
	const char *backspace;
	const char *tab;
	const char *print;
	const char *pause;
	const char *left;
	const char *right;
	const char *up;
	const char *down;
	const char *shift;
	const char *alt;
	const char *control;
	const char *meta; /* windows/super key */
	const char *menu;
	const char *space;
	const char *numpad_num; /* For example, "Numpad %1" */
	const char *numpad_divide;
	const char *numpad_multiply;
	const char *numpad_minus;
	const char *numpad_plus;
	const char *numpad_decimal;
	const char *apple_keypad_num; /* For example, "%1 (Keypad)" */
	const char *apple_keypad_divide;
	const char *apple_keypad_multiply;
	const char *apple_keypad_minus;
	const char *apple_keypad_plus;
	const char *apple_keypad_decimal;
	const char *apple_keypad_equal;
	const char *mouse_num; /* For example, "Mouse %1" */
	const char *escape;
};

/* This function is an optional way to provide translations for specific keys
 * that may not have translations.  If the operating system can provide
 * translations for these keys, it will use the operating system's translation
 * over these translations.  If no translations are specified, it will use
 * the default English translations for that specific operating system. */
EXPORT void
obs_hotkeys_set_translations_s(struct obs_hotkeys_translations *translations,
			       size_t size);
#endif

#define obs_hotkeys_set_translations(translations) \
	obs_hotkeys_set_translations_s(            \
		translations, sizeof(struct obs_hotkeys_translations))

EXPORT void
obs_hotkeys_set_audio_hotkeys_translations(const char *mute, const char *unmute,
					   const char *push_to_mute,
					   const char *push_to_talk);

EXPORT void obs_hotkeys_set_sceneitem_hotkeys_translations(const char *show,
							   const char *hide);

/* registering hotkeys (giving hotkeys a name and a function) */

typedef void (*obs_hotkey_func)(void *data, obs_hotkey_id id,
				obs_hotkey_t *hotkey, bool pressed);

EXPORT obs_hotkey_id obs_hotkey_register_frontend(const char *name,
						  const char *description,
						  obs_hotkey_func func,
						  void *data);

EXPORT obs_hotkey_id obs_hotkey_register_encoder(obs_encoder_t *encoder,
						 const char *name,
						 const char *description,
						 obs_hotkey_func func,
						 void *data);

EXPORT obs_hotkey_id obs_hotkey_register_output(obs_output_t *output,
						const char *name,
						const char *description,
						obs_hotkey_func func,
						void *data);

EXPORT obs_hotkey_id obs_hotkey_register_service(obs_service_t *service,
						 const char *name,
						 const char *description,
						 obs_hotkey_func func,
						 void *data);

EXPORT obs_hotkey_id obs_hotkey_register_source(obs_source_t *source,
						const char *name,
						const char *description,
						obs_hotkey_func func,
						void *data);

typedef bool (*obs_hotkey_active_func)(void *data, obs_hotkey_pair_id id,
				       obs_hotkey_t *hotkey, bool pressed);

EXPORT obs_hotkey_pair_id obs_hotkey_pair_register_frontend(
	const char *name0, const char *description0, const char *name1,
	const char *description1, obs_hotkey_active_func func0,
	obs_hotkey_active_func func1, void *data0, void *data1);

EXPORT obs_hotkey_pair_id obs_hotkey_pair_register_encoder(
	obs_encoder_t *encoder, const char *name0, const char *description0,
	const char *name1, const char *description1,
	obs_hotkey_active_func func0, obs_hotkey_active_func func1, void *data0,
	void *data1);

EXPORT obs_hotkey_pair_id obs_hotkey_pair_register_output(
	obs_output_t *output, const char *name0, const char *description0,
	const char *name1, const char *description1,
	obs_hotkey_active_func func0, obs_hotkey_active_func func1, void *data0,
	void *data1);

EXPORT obs_hotkey_pair_id obs_hotkey_pair_register_service(
	obs_service_t *service, const char *name0, const char *description0,
	const char *name1, const char *description1,
	obs_hotkey_active_func func0, obs_hotkey_active_func func1, void *data0,
	void *data1);

EXPORT obs_hotkey_pair_id obs_hotkey_pair_register_source(
	obs_source_t *source, const char *name0, const char *description0,
	const char *name1, const char *description1,
	obs_hotkey_active_func func0, obs_hotkey_active_func func1, void *data0,
	void *data1);

EXPORT void obs_hotkey_unregister(obs_hotkey_id id);

EXPORT void obs_hotkey_pair_unregister(obs_hotkey_pair_id id);

/* loading hotkeys (associating a hotkey with a physical key and modifiers) */

EXPORT void obs_hotkey_load_bindings(obs_hotkey_id id,
				     obs_key_combination_t *combinations,
				     size_t num);

EXPORT void obs_hotkey_load(obs_hotkey_id id, obs_data_array_t *data);

EXPORT void obs_hotkeys_load_encoder(obs_encoder_t *encoder,
				     obs_data_t *hotkeys);

EXPORT void obs_hotkeys_load_output(obs_output_t *output, obs_data_t *hotkeys);

EXPORT void obs_hotkeys_load_service(obs_service_t *service,
				     obs_data_t *hotkeys);

EXPORT void obs_hotkeys_load_source(obs_source_t *source, obs_data_t *hotkeys);

EXPORT void obs_hotkey_pair_load(obs_hotkey_pair_id id, obs_data_array_t *data0,
				 obs_data_array_t *data1);

EXPORT obs_data_array_t *obs_hotkey_save(obs_hotkey_id id);

EXPORT void obs_hotkey_pair_save(obs_hotkey_pair_id id,
				 obs_data_array_t **p_data0,
				 obs_data_array_t **p_data1);

EXPORT obs_data_t *obs_hotkeys_save_encoder(obs_encoder_t *encoder);

EXPORT obs_data_t *obs_hotkeys_save_output(obs_output_t *output);

EXPORT obs_data_t *obs_hotkeys_save_service(obs_service_t *service);

EXPORT obs_data_t *obs_hotkeys_save_source(obs_source_t *source);

/* enumerating hotkeys */

typedef bool (*obs_hotkey_enum_func)(void *data, obs_hotkey_id id,
				     obs_hotkey_t *key);

EXPORT void obs_enum_hotkeys(obs_hotkey_enum_func func, void *data);

/* enumerating bindings */

typedef bool (*obs_hotkey_binding_enum_func)(void *data, size_t idx,
					     obs_hotkey_binding_t *binding);

EXPORT void obs_enum_hotkey_bindings(obs_hotkey_binding_enum_func func,
				     void *data);

/* hotkey event control */

EXPORT void obs_hotkey_inject_event(obs_key_combination_t hotkey, bool pressed);

EXPORT void obs_hotkey_enable_background_press(bool enable);

EXPORT void obs_hotkey_enable_strict_modifiers(bool enable);

/* hotkey callback routing (trigger callbacks through e.g. a UI thread) */

typedef void (*obs_hotkey_callback_router_func)(void *data, obs_hotkey_id id,
						bool pressed);

EXPORT void
obs_hotkey_set_callback_routing_func(obs_hotkey_callback_router_func func,
				     void *data);

EXPORT void obs_hotkey_trigger_routed_callback(obs_hotkey_id id, bool pressed);

/* hotkey callbacks won't be processed if callback rerouting is enabled and no
 * router func is set */
EXPORT void obs_hotkey_enable_callback_rerouting(bool enable);

/* misc */

typedef void (*obs_hotkey_atomic_update_func)(void *);
EXPORT void obs_hotkey_update_atomic(obs_hotkey_atomic_update_func func,
				     void *data);

struct dstr;
EXPORT void obs_key_to_str(obs_key_t key, struct dstr *str);
EXPORT void obs_key_combination_to_str(obs_key_combination_t key,
				       struct dstr *str);

EXPORT obs_key_t obs_key_from_virtual_key(int code);
EXPORT int obs_key_to_virtual_key(obs_key_t key);

EXPORT const char *obs_key_to_name(obs_key_t key);
EXPORT obs_key_t obs_key_from_name(const char *name);

static inline bool obs_key_combination_is_empty(obs_key_combination_t combo)
{
	return !combo.modifiers && combo.key == OBS_KEY_NONE;
}

#ifdef __cplusplus
}
#endif
