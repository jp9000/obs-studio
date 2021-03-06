#include "obs-frontend-internal.hpp"
#include <memory>

using namespace std;

static unique_ptr<obs_frontend_callbacks> c;

void obs_frontend_set_callbacks_internal(obs_frontend_callbacks *callbacks)
{
	c.reset(callbacks);
}

static inline bool callbacks_valid_(const char *func_name)
{
	if (!c) {
		blog(LOG_WARNING, "Tried to call %s with no callbacks!",
		     func_name);
		return false;
	}

	return true;
}

#define callbacks_valid() callbacks_valid_(__FUNCTION__)

static char **convert_string_list(vector<string> &strings)
{
	size_t size = 0;
	size_t string_data_offset = (strings.size() + 1) * sizeof(char *);
	uint8_t *out;
	char **ptr_list;
	char *string_data;

	size += string_data_offset;

	for (auto &str : strings)
		size += str.size() + 1;

	if (!size)
		return 0;

	out = (uint8_t *)bmalloc(size);
	ptr_list = (char **)out;
	string_data = (char *)(out + string_data_offset);

	for (auto &str : strings) {
		*ptr_list = string_data;
		memcpy(string_data, str.c_str(), str.size() + 1);

		ptr_list++;
		string_data += str.size() + 1;
	}

	*ptr_list = nullptr;
	return (char **)out;
}

/* ------------------------------------------------------------------------- */

void *obs_frontend_get_main_window(void)
{
	return !!callbacks_valid() ? c->obs_frontend_get_main_window()
				   : nullptr;
}

void *obs_frontend_get_main_window_handle(void)
{
	return !!callbacks_valid() ? c->obs_frontend_get_main_window_handle()
				   : nullptr;
}

void *obs_frontend_get_system_tray(void)
{
	return !!callbacks_valid() ? c->obs_frontend_get_system_tray()
				   : nullptr;
}

char **obs_frontend_get_scene_names(void)
{
	if (!callbacks_valid())
		return NULL;

	struct obs_frontend_source_list sources = {};
	vector<string> names;
	c->obs_frontend_get_scenes(&sources);

	for (size_t i = 0; i < sources.sources.num; i++) {
		obs_source_t *source = sources.sources.array[i];
		const char *name = obs_source_get_name(source);
		names.emplace_back(name);
	}

	obs_frontend_source_list_free(&sources);
	return convert_string_list(names);
}

void obs_frontend_get_scenes(struct obs_frontend_source_list *sources)
{
	if (callbacks_valid())
		c->obs_frontend_get_scenes(sources);
}

obs_source_t *obs_frontend_get_current_scene(void)
{
	return !!callbacks_valid() ? c->obs_frontend_get_current_scene()
				   : nullptr;
}

void obs_frontend_set_current_scene(obs_source_t *scene)
{
	if (callbacks_valid())
		c->obs_frontend_set_current_scene(scene);
}

void obs_frontend_get_transitions(struct obs_frontend_source_list *sources)
{
	if (callbacks_valid())
		c->obs_frontend_get_transitions(sources);
}

obs_source_t *obs_frontend_get_current_transition(void)
{
	return !!callbacks_valid() ? c->obs_frontend_get_current_transition()
				   : nullptr;
}

void obs_frontend_set_current_transition(obs_source_t *transition)
{
	if (callbacks_valid())
		c->obs_frontend_set_current_transition(transition);
}

int obs_frontend_get_transition_duration(void)
{
	return !!callbacks_valid() ? c->obs_frontend_get_transition_duration()
				   : 0;
}

void obs_frontend_set_transition_duration(int duration)
{
	if (callbacks_valid())
		c->obs_frontend_set_transition_duration(duration);
}

void obs_frontend_release_tbar(void)
{
	if (callbacks_valid())
		c->obs_frontend_release_tbar();
}

void obs_frontend_set_tbar_position(int position)
{
	if (callbacks_valid())
		c->obs_frontend_set_tbar_position(position);
}

char **obs_frontend_get_scene_collections(void)
{
	if (!callbacks_valid())
		return nullptr;

	vector<string> strings;
	c->obs_frontend_get_scene_collections(strings);
	return convert_string_list(strings);
}

char *obs_frontend_get_current_scene_collection(void)
{
	return !!callbacks_valid()
		       ? c->obs_frontend_get_current_scene_collection()
		       : nullptr;
}

void obs_frontend_set_current_scene_collection(const char *collection)
{
	if (callbacks_valid())
		c->obs_frontend_set_current_scene_collection(collection);
}

bool obs_frontend_add_scene_collection(const char *name)
{
	return callbacks_valid() ? c->obs_frontend_add_scene_collection(name)
				 : false;
}

char **obs_frontend_get_profiles(void)
{
	if (!callbacks_valid())
		return nullptr;

	vector<string> strings;
	c->obs_frontend_get_profiles(strings);
	return convert_string_list(strings);
}

char *obs_frontend_get_current_profile(void)
{
	return !!callbacks_valid() ? c->obs_frontend_get_current_profile()
				   : nullptr;
}

void obs_frontend_set_current_profile(const char *profile)
{
	if (callbacks_valid())
		c->obs_frontend_set_current_profile(profile);
}

void obs_frontend_streaming_start(void)
{
	if (callbacks_valid())
		c->obs_frontend_streaming_start();
}

void obs_frontend_streaming_stop(void)
{
	if (callbacks_valid())
		c->obs_frontend_streaming_stop();
}

bool obs_frontend_streaming_active(void)
{
	return !!callbacks_valid() ? c->obs_frontend_streaming_active() : false;
}

void obs_frontend_recording_start(void)
{
	if (callbacks_valid())
		c->obs_frontend_recording_start();
}

void obs_frontend_recording_stop(void)
{
	if (callbacks_valid())
		c->obs_frontend_recording_stop();
}

bool obs_frontend_recording_active(void)
{
	return !!callbacks_valid() ? c->obs_frontend_recording_active() : false;
}

void obs_frontend_recording_pause(bool pause)
{
	if (!!callbacks_valid())
		c->obs_frontend_recording_pause(pause);
}

bool obs_frontend_recording_paused(void)
{
	return !!callbacks_valid() ? c->obs_frontend_recording_paused() : false;
}

void obs_frontend_replay_buffer_start(void)
{
	if (callbacks_valid())
		c->obs_frontend_replay_buffer_start();
}

void obs_frontend_replay_buffer_save(void)
{
	if (callbacks_valid())
		c->obs_frontend_replay_buffer_save();
}

void obs_frontend_replay_buffer_stop(void)
{
	if (callbacks_valid())
		c->obs_frontend_replay_buffer_stop();
}

bool obs_frontend_replay_buffer_active(void)
{
	return !!callbacks_valid() ? c->obs_frontend_replay_buffer_active()
				   : false;
}

void *obs_frontend_add_tools_menu_qaction(const char *name)
{
	return !!callbacks_valid()
		       ? c->obs_frontend_add_tools_menu_qaction(name)
		       : nullptr;
}

void obs_frontend_add_tools_menu_item(const char *name,
				      obs_frontend_cb callback,
				      void *private_data)
{
	if (callbacks_valid())
		c->obs_frontend_add_tools_menu_item(name, callback,
						    private_data);
}

void *obs_frontend_add_dock(void *dock)
{
	return !!callbacks_valid() ? c->obs_frontend_add_dock(dock) : nullptr;
}

void obs_frontend_add_event_callback(obs_frontend_event_cb callback,
				     void *private_data)
{
	if (callbacks_valid())
		c->obs_frontend_add_event_callback(callback, private_data);
}

void obs_frontend_remove_event_callback(obs_frontend_event_cb callback,
					void *private_data)
{
	if (callbacks_valid())
		c->obs_frontend_remove_event_callback(callback, private_data);
}

obs_output_t *obs_frontend_get_streaming_output(void)
{
	return !!callbacks_valid() ? c->obs_frontend_get_streaming_output()
				   : nullptr;
}

obs_output_t *obs_frontend_get_recording_output(void)
{
	return !!callbacks_valid() ? c->obs_frontend_get_recording_output()
				   : nullptr;
}

obs_output_t *obs_frontend_get_replay_buffer_output(void)
{
	return !!callbacks_valid() ? c->obs_frontend_get_replay_buffer_output()
				   : nullptr;
}

config_t *obs_frontend_get_profile_config(void)
{
	return !!callbacks_valid() ? c->obs_frontend_get_profile_config()
				   : nullptr;
}

config_t *obs_frontend_get_global_config(void)
{
	return !!callbacks_valid() ? c->obs_frontend_get_global_config()
				   : nullptr;
}

void obs_frontend_open_projector(const char *type, int monitor,
				 const char *geometry, const char *name)
{
	if (callbacks_valid())
		c->obs_frontend_open_projector(type, monitor, geometry, name);
}

void obs_frontend_save(void)
{
	if (callbacks_valid())
		c->obs_frontend_save();
}

void obs_frontend_defer_save_begin(void)
{
	if (callbacks_valid())
		c->obs_frontend_defer_save_begin();
}

void obs_frontend_defer_save_end(void)
{
	if (callbacks_valid())
		c->obs_frontend_defer_save_end();
}

void obs_frontend_add_save_callback(obs_frontend_save_cb callback,
				    void *private_data)
{
	if (callbacks_valid())
		c->obs_frontend_add_save_callback(callback, private_data);
}

void obs_frontend_remove_save_callback(obs_frontend_save_cb callback,
				       void *private_data)
{
	if (callbacks_valid())
		c->obs_frontend_remove_save_callback(callback, private_data);
}

void obs_frontend_add_preload_callback(obs_frontend_save_cb callback,
				       void *private_data)
{
	if (callbacks_valid())
		c->obs_frontend_add_preload_callback(callback, private_data);
}

void obs_frontend_remove_preload_callback(obs_frontend_save_cb callback,
					  void *private_data)
{
	if (callbacks_valid())
		c->obs_frontend_remove_preload_callback(callback, private_data);
}

void obs_frontend_push_ui_translation(obs_frontend_translate_ui_cb translate)
{
	if (callbacks_valid())
		c->obs_frontend_push_ui_translation(translate);
}

void obs_frontend_pop_ui_translation(void)
{
	if (callbacks_valid())
		c->obs_frontend_pop_ui_translation();
}

void obs_frontend_set_streaming_service(obs_service_t *service)
{
	if (callbacks_valid())
		c->obs_frontend_set_streaming_service(service);
}

obs_service_t *obs_frontend_get_streaming_service(void)
{
	return !!callbacks_valid() ? c->obs_frontend_get_streaming_service()
				   : nullptr;
}

void obs_frontend_save_streaming_service(void)
{
	if (callbacks_valid())
		c->obs_frontend_save_streaming_service();
}

bool obs_frontend_preview_program_mode_active(void)
{
	return !!callbacks_valid()
		       ? c->obs_frontend_preview_program_mode_active()
		       : false;
}

void obs_frontend_set_preview_program_mode(bool enable)
{
	if (callbacks_valid())
		c->obs_frontend_set_preview_program_mode(enable);
}

void obs_frontend_preview_program_trigger_transition(void)
{
	if (callbacks_valid())
		c->obs_frontend_preview_program_trigger_transition();
}

void obs_frontend_set_preview_enabled(bool enable)
{
	if (callbacks_valid())
		c->obs_frontend_set_preview_enabled(enable);
}

bool obs_frontend_preview_enabled(void)
{
	return !!callbacks_valid() ? c->obs_frontend_preview_enabled() : false;
}

obs_source_t *obs_frontend_get_current_preview_scene(void)
{
	return !!callbacks_valid() ? c->obs_frontend_get_current_preview_scene()
				   : nullptr;
}

void obs_frontend_set_current_preview_scene(obs_source_t *scene)
{
	if (callbacks_valid())
		c->obs_frontend_set_current_preview_scene(scene);
}

void obs_frontend_take_screenshot(void)
{
	if (callbacks_valid())
		c->obs_frontend_take_screenshot();
}

void obs_frontend_take_source_screenshot(obs_source_t *source)
{
	if (callbacks_valid())
		c->obs_frontend_take_source_screenshot(source);
}

obs_output_t *obs_frontend_get_virtualcam_output(void)
{
	return !!callbacks_valid() ? c->obs_frontend_get_virtualcam_output()
				   : nullptr;
}

void obs_frontend_start_virtualcam(void)
{
	if (callbacks_valid())
		c->obs_frontend_start_virtualcam();
}

void obs_frontend_stop_virtualcam(void)
{
	if (callbacks_valid())
		c->obs_frontend_stop_virtualcam();
}

bool obs_frontend_virtualcam_active(void)
{
	return !!callbacks_valid() ? c->obs_frontend_virtualcam_active()
				   : false;
}

void obs_frontend_reset_video(void)
{
	if (callbacks_valid())
		c->obs_frontend_reset_video();
}
