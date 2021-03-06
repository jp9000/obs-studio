#pragma once

#include "obs-frontend-api.h"

#include <vector>
#include <string>

struct obs_frontend_callbacks {
	virtual ~obs_frontend_callbacks() {}
	virtual void *obs_frontend_get_main_window(void) = 0;
	virtual void *obs_frontend_get_main_window_handle(void) = 0;
	virtual void *obs_frontend_get_system_tray(void) = 0;

	virtual void
	obs_frontend_get_scenes(struct obs_frontend_source_list *sources) = 0;
	virtual obs_source_t *obs_frontend_get_current_scene(void) = 0;
	virtual void obs_frontend_set_current_scene(obs_source_t *scene) = 0;

	virtual void obs_frontend_get_transitions(
		struct obs_frontend_source_list *sources) = 0;
	virtual obs_source_t *obs_frontend_get_current_transition(void) = 0;
	virtual void
	obs_frontend_set_current_transition(obs_source_t *transition) = 0;
	virtual int obs_frontend_get_transition_duration(void) = 0;
	virtual void obs_frontend_set_transition_duration(int duration) = 0;
	virtual void obs_frontend_release_tbar(void) = 0;
	virtual void obs_frontend_set_tbar_position(int position) = 0;

	virtual void obs_frontend_get_scene_collections(
		std::vector<std::string> &strings) = 0;
	virtual char *obs_frontend_get_current_scene_collection(void) = 0;
	virtual void
	obs_frontend_set_current_scene_collection(const char *collection) = 0;
	virtual bool obs_frontend_add_scene_collection(const char *name) = 0;

	virtual void
	obs_frontend_get_profiles(std::vector<std::string> &strings) = 0;
	virtual char *obs_frontend_get_current_profile(void) = 0;
	virtual void obs_frontend_set_current_profile(const char *profile) = 0;

	virtual void obs_frontend_streaming_start(void) = 0;
	virtual void obs_frontend_streaming_stop(void) = 0;
	virtual bool obs_frontend_streaming_active(void) = 0;

	virtual void obs_frontend_recording_start(void) = 0;
	virtual void obs_frontend_recording_stop(void) = 0;
	virtual bool obs_frontend_recording_active(void) = 0;
	virtual void obs_frontend_recording_pause(bool pause) = 0;
	virtual bool obs_frontend_recording_paused(void) = 0;

	virtual void obs_frontend_replay_buffer_start(void) = 0;
	virtual void obs_frontend_replay_buffer_save(void) = 0;
	virtual void obs_frontend_replay_buffer_stop(void) = 0;
	virtual bool obs_frontend_replay_buffer_active(void) = 0;

	virtual void *obs_frontend_add_tools_menu_qaction(const char *name) = 0;
	virtual void obs_frontend_add_tools_menu_item(const char *name,
						      obs_frontend_cb callback,
						      void *private_data) = 0;

	virtual void *obs_frontend_add_dock(void *dock) = 0;

	virtual void
	obs_frontend_add_event_callback(obs_frontend_event_cb callback,
					void *private_data) = 0;
	virtual void
	obs_frontend_remove_event_callback(obs_frontend_event_cb callback,
					   void *private_data) = 0;

	virtual obs_output_t *obs_frontend_get_streaming_output(void) = 0;
	virtual obs_output_t *obs_frontend_get_recording_output(void) = 0;
	virtual obs_output_t *obs_frontend_get_replay_buffer_output(void) = 0;

	virtual config_t *obs_frontend_get_profile_config(void) = 0;
	virtual config_t *obs_frontend_get_global_config(void) = 0;

	virtual void obs_frontend_open_projector(const char *type, int monitor,
						 const char *geometry,
						 const char *name) = 0;
	virtual void obs_frontend_save(void) = 0;
	virtual void obs_frontend_defer_save_begin(void) = 0;
	virtual void obs_frontend_defer_save_end(void) = 0;
	virtual void
	obs_frontend_add_save_callback(obs_frontend_save_cb callback,
				       void *private_data) = 0;
	virtual void
	obs_frontend_remove_save_callback(obs_frontend_save_cb callback,
					  void *private_data) = 0;

	virtual void
	obs_frontend_add_preload_callback(obs_frontend_save_cb callback,
					  void *private_data) = 0;
	virtual void
	obs_frontend_remove_preload_callback(obs_frontend_save_cb callback,
					     void *private_data) = 0;

	virtual void obs_frontend_push_ui_translation(
		obs_frontend_translate_ui_cb translate) = 0;
	virtual void obs_frontend_pop_ui_translation(void) = 0;

	virtual void
	obs_frontend_set_streaming_service(obs_service_t *service) = 0;
	virtual obs_service_t *obs_frontend_get_streaming_service(void) = 0;
	virtual void obs_frontend_save_streaming_service() = 0;

	virtual bool obs_frontend_preview_program_mode_active(void) = 0;
	virtual void obs_frontend_set_preview_program_mode(bool enable) = 0;
	virtual void obs_frontend_preview_program_trigger_transition(void) = 0;

	virtual bool obs_frontend_preview_enabled(void) = 0;
	virtual void obs_frontend_set_preview_enabled(bool enable) = 0;

	virtual obs_source_t *obs_frontend_get_current_preview_scene(void) = 0;
	virtual void
	obs_frontend_set_current_preview_scene(obs_source_t *scene) = 0;

	virtual void on_load(obs_data_t *settings) = 0;
	virtual void on_preload(obs_data_t *settings) = 0;
	virtual void on_save(obs_data_t *settings) = 0;
	virtual void on_event(enum obs_frontend_event event) = 0;

	virtual void obs_frontend_take_screenshot() = 0;
	virtual void
	obs_frontend_take_source_screenshot(obs_source_t *source) = 0;

	virtual obs_output_t *obs_frontend_get_virtualcam_output(void) = 0;
	virtual void obs_frontend_start_virtualcam(void) = 0;
	virtual void obs_frontend_stop_virtualcam(void) = 0;
	virtual bool obs_frontend_virtualcam_active(void) = 0;

	virtual void obs_frontend_reset_video(void) = 0;
};

EXPORT void
obs_frontend_set_callbacks_internal(obs_frontend_callbacks *callbacks);
