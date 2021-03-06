OBS Studio Frontend API
=======================

The OBS Studio frontend API is the API specific to OBS Studio itself.

.. code:: cpp

   #include <obs-frontend-api.h>


Structures/Enumerations
-----------------------

.. type:: enum obs_frontend_event

   Specifies a front-end event.  Can be one of the following values:

   - **OBS_FRONTEND_EVENT_STREAMING_STARTING**

     Triggered when streaming is starting.

   - **OBS_FRONTEND_EVENT_STREAMING_STARTED**

     Triggered when streaming has successfully started.

   - **OBS_FRONTEND_EVENT_STREAMING_STOPPING**

     Triggered when streaming is stopping.

   - **OBS_FRONTEND_EVENT_STREAMING_STOPPED**

     Triggered when streaming has fully stopped.

   - **OBS_FRONTEND_EVENT_RECORDING_STARTING**

     Triggered when recording is starting.

   - **OBS_FRONTEND_EVENT_RECORDING_STARTED**

     Triggered when recording has successfully started.

   - **OBS_FRONTEND_EVENT_RECORDING_STOPPING**

     Triggered when recording is stopping.

   - **OBS_FRONTEND_EVENT_RECORDING_STOPPED**

     Triggered when recording has fully stopped.

   - **OBS_FRONTEND_EVENT_SCENE_CHANGED**

     Triggered when the current scene has changed.

   - **OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED**

     Triggered when a scenes has been added/removed/reordered by the
     user.

   - **OBS_FRONTEND_EVENT_TRANSITION_CHANGED**

     Triggered when the current transition has changed by the user.

   - **OBS_FRONTEND_EVENT_TRANSITION_STOPPED**

     Triggered when a transition has completed.

   - **OBS_FRONTEND_EVENT_TRANSITION_LIST_CHANGED**

     Triggered when the user adds/removes transitions.

   - **OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED**

     Triggered when the user has changed the current scene collection.

   - **OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED**

     Triggered when the user has added/removed/renamed scene
     collections.

   - **OBS_FRONTEND_EVENT_PROFILE_CHANGED**

     Triggered when the user has changed the current profile.

   - **OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED**

     Triggered when the user has added/removed/renamed profiles.

   - **OBS_FRONTEND_EVENT_EXIT**

     Triggered when the program is about to exit.

   - **OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING**

     Triggered when the replay buffer is starting.

   - **OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED**

     Triggered when the replay buffer has successfully started.

   - **OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING**

     Triggered when the replay buffer is stopping.

   - **OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED**

     Triggered when the replay buffer has fully stopped.

   - **OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED**

     Triggered when the replay buffer has been saved.

   - **OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED**

     Triggered when the user has turned on studio mode.

   - **OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED**

     Triggered when the user has turned off studio mode.

   - **OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED**

     Triggered when the current preview scene has changed in studio
     mode.

   - **OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP**

     Triggered when a scene collection has been completely unloaded, and
     the program is either about to load a new scene collection, or the
     program is about to exit.

   - **OBS_FRONTEND_EVENT_FINISHED_LOADING**

     Triggered when the program has finished loading.

   - **OBS_FRONTEND_EVENT_RECORDING_PAUSED**

     Triggered when the recording has been paused.

   - **OBS_FRONTEND_EVENT_RECORDING_UNPAUSED**

     Triggered when the recording has been unpaused.

   - **OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED**

     Triggered when the virtual camera is started.

   - **OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED**

     Triggered when the virtual camera is stopped.


.. type:: struct obs_frontend_source_list

   - DARRAY(obs_source_t*) **sources**

   Example usage:

.. code:: cpp

   struct obs_frontend_source_list scenes;

   obs_frontend_get_scenes(&scenes);

   for (size_t i = 0; i < scenes.num; i++) {
           obs_source_t *source = scenes.sources.array[i];

           [...]
   }

   obs_frontend_source_list_free(&scenes);

.. type:: typedef void (*obs_frontend_cb)(void *private_data)

   Frontend tool menu callback.

.. type:: typedef void (*obs_frontend_event_cb)(enum obs_frontend_event event, void *private_data)

   Frontend event callback.

.. type:: typedef void (*obs_frontend_save_cb)(obs_data_t *save_data, bool saving, void *private_data)

   Frontend save/load callback.

.. type:: typedef bool (*obs_frontend_translate_ui_cb)(const char *text, const char **out)

   Translation callback.


Functions
---------

.. function:: void obs_frontend_source_list_free(struct obs_frontend_source_list *source_list)

   Releases sources within a source list and frees the list.

   :param source_list: Source list to free.

---------------------------------------

.. function:: void *obs_frontend_get_main_window(void)

   :return: The QMainWindow pointer to the OBS Studio window.

---------------------------------------

.. function:: void *obs_frontend_get_main_window_handle(void)

   :return: The native window handle of the OBS Studio window.

---------------------------------------

.. function:: char **obs_frontend_get_scene_names(void)

   :return: The scene name list, ending with NULL.  The list is stored
            within one contiguous segment of memory, so freeing the
            returned pointer with :c:func:`bfree()` will free the entire
            list.

---------------------------------------

.. function:: void obs_frontend_get_scenes(struct obs_frontend_source_list *sources)

   :param sources: Pointer to a :c:type:`obs_frontend_source_list`
                   structure to receive the list of
                   reference-incremented scenes.  Release with
                   :c:func:`obs_frontend_source_list_free`.

---------------------------------------

.. function:: obs_source_t *obs_frontend_get_current_scene(void)

   :return: A new reference to the currently active scene.

---------------------------------------

.. function:: void obs_frontend_set_current_scene(obs_source_t *scene)

   :param scene: The scene to set as the current scene.

---------------------------------------

.. function:: void obs_frontend_get_transitions(struct obs_frontend_source_list *sources)

   :param sources: Pointer to a :c:type:`obs_frontend_source_list`
                   structure to receive the list of
                   reference-incremented transitions.  Release with
                   :c:func:`obs_frontend_source_list_free`.

---------------------------------------

.. function:: obs_source_t *obs_frontend_get_current_transition(void)

   :return: A new reference to the currently active transition.

---------------------------------------

.. function:: void obs_frontend_set_current_transition(obs_source_t *transition)

   :param transition: The transition to set as the current transition.

---------------------------------------

.. function:: int obs_frontend_get_transition_duration(void)

   :return: The transition duration (in milliseconds) currently set in the UI.

---------------------------------------

.. function:: void obs_frontend_set_transition_duration(int duration)

   :param duration: Desired transition duration (in milliseconds)

---------------------------------------

.. function:: char **obs_frontend_get_scene_collections(void)

   :return: The list of profile names, ending with NULL.  The list is
            stored within one contiguous segment of memory, so freeing
            the returned pointer with :c:func:`bfree()` will free the
            entire list.

---------------------------------------

.. function:: char *obs_frontend_get_current_scene_collection(void)

   :return: A new pointer to the current scene collection name.  Free
            with :c:func:`bfree()`.

---------------------------------------

.. function:: void obs_frontend_set_current_scene_collection(const char *collection)

   :param profile: Name of the scene collection to activate.

---------------------------------------

.. function:: char **obs_frontend_get_profiles(void)

   :return: The list of profile names, ending with NULL.  The list is
            stored within one contiguous segment of memory, so freeing
            the returned pointer with :c:func:`bfree()` will free the
            entire list.

---------------------------------------

.. function:: char *obs_frontend_get_current_profile(void)

   :return: A new pointer to the current profile name.  Free with
            :c:func:`bfree()`.

---------------------------------------

.. function:: void obs_frontend_set_current_profile(const char *profile)

   :param profile: Name of the profile to activate.

---------------------------------------

.. function:: void obs_frontend_add_event_callback(obs_frontend_event_cb callback, void *private_data)

   Adds a callback that will be called when a frontend event occurs.
   See :c:type:`obs_frontend_event` on what sort of events can be
   triggered.

   :param callback:     Callback to use when a frontend event occurs.
   :param private_data: Private data associated with the callback.

---------------------------------------

.. function:: void obs_frontend_remove_event_callback(obs_frontend_event_cb callback, void *private_data)

   Removes an event callback.

   :param callback:     Callback to remove.
   :param private_data: Private data associated with the callback.

---------------------------------------

.. function:: void obs_frontend_add_save_callback(obs_frontend_save_cb callback, void *private_data)

   Adds a callback that will be called when the current scene collection
   is being saved/loaded.

   :param callback:     Callback to use when saving/loading a scene
                        collection.
   :param private_data: Private data associated with the callback.

---------------------------------------

.. function:: void obs_frontend_remove_save_callback(obs_frontend_save_cb callback, void *private_data)

   Removes a save/load callback.

   :param callback:     Callback to remove.
   :param private_data: Private data associated with the callback.

---------------------------------------

.. function:: void obs_frontend_add_preload_callback(obs_frontend_save_cb callback, void *private_data)

   Adds a callback that will be called right before a scene collection
   is loaded.  Useful if you

   :param callback:     Callback to use when pre-loading.
   :param private_data: Private data associated with the callback.

---------------------------------------

.. function:: void obs_frontend_remove_preload_callback(obs_frontend_save_cb callback, void *private_data)

   Removes a pre-load callback.

   :param callback:     Callback to remove.
   :param private_data: Private data associated with the callback.

---------------------------------------

.. function:: void obs_frontend_push_ui_translation(obs_frontend_translate_ui_cb translate)

   Pushes a UI translation callback.  This allows a front-end plugin to
   intercept when Qt is automatically generating translating data.
   Typically this is just called with obs_module_get_string.

   :param translate: The translation callback to use.

---------------------------------------

.. function:: void obs_frontend_pop_ui_translation(void)

   Pops the current UI translation callback.

---------------------------------------

.. function:: void obs_frontend_streaming_start(void)

   Starts streaming.

---------------------------------------

.. function:: void obs_frontend_streaming_stop(void)

   Stops streaming.

---------------------------------------

.. function:: bool obs_frontend_streaming_active(void)

   :return: *true* if streaming active, *false* otherwise.

---------------------------------------

.. function:: void obs_frontend_recording_start(void)

   Starts recording.

---------------------------------------

.. function:: void obs_frontend_recording_stop(void)

   Stops recording.

---------------------------------------

.. function:: bool obs_frontend_recording_active(void)

   :return: *true* if recording active, *false* otherwise.

---------------------------------------

.. function:: void obs_frontend_recording_pause(bool pause)

   :pause: *true* to pause recording, *false* to unpause.

---------------------------------------

.. function:: bool obs_frontend_recording_paused(void)

   :return: *true* if recording paused, *false* otherwise.

---------------------------------------

.. function:: void obs_frontend_replay_buffer_start(void)

   Starts replay buffer.

---------------------------------------

.. function:: void obs_frontend_replay_buffer_stop(void)

   Stops replay buffer.

---------------------------------------

.. function:: void obs_frontend_replay_buffer_save(void)

   Saves a replay if the replay buffer is active.

---------------------------------------

.. function:: bool obs_frontend_replay_buffer_active(void)

   :return: *true* if replay buffer active, *false* otherwise.

---------------------------------------

.. function:: void obs_frontend_open_projector(const char *type, int monitor, const char *geometry, const char *name)

   :param type:     "Preview", "Source", "Scene", "StudioProgram", or "Multiview" (case insensitive).
   :param monitor:  Monitor to open the projector on. If -1, opens a window.
   :param geometry: If *monitor* is -1, size and position of the projector window. Encoded in Base64 using Qt's geometry encoding.
   :param name:     If *type* is "Source" or "Scene", name of the source or scene to be displayed.

---------------------------------------

.. function:: void obs_frontend_save(void)

   Saves the current scene collection.

---------------------------------------

.. function:: obs_output_t *obs_frontend_get_streaming_output(void)

   :return: A new reference to the current streaming output.

---------------------------------------

.. function:: obs_output_t *obs_frontend_get_recording_output(void)

   :return: A new reference to the current srecording output.

---------------------------------------

.. function:: obs_output_t *obs_frontend_get_replay_buffer_output(void)

   :return: A new reference to the current replay buffer output.

---------------------------------------

.. function:: void obs_frontend_set_streaming_service(obs_service_t *service)

   Sets the current streaming service to stream with.

   :param service: The streaming service to set.

---------------------------------------

.. function:: obs_service_t *obs_frontend_get_streaming_service(void)

   :return: A new reference to the current streaming service object.

---------------------------------------

.. function:: void obs_frontend_save_streaming_service(void)

   Saves the current streaming service data.

---------------------------------------

.. function:: bool obs_frontend_preview_program_mode_active(void)

   :return: *true* if studio mode is active, *false* otherwise.

---------------------------------------

.. function:: void obs_frontend_set_preview_program_mode(bool enable)

   Activates/deactivates studio mode.

   :param enable: *true* to activate studio mode, *false* to deactivate
                  studio mode.

---------------------------------------

.. function:: void obs_frontend_preview_program_trigger_transition(void)

   Triggers a preview-to-program transition if studio mode is active.

---------------------------------------

.. function:: obs_source_t *obs_frontend_get_current_preview_scene(void)

   :return: A new reference to the current preview scene if studio mode
            is active, or the current scene if studio mode is not
            active.

---------------------------------------

.. function:: void obs_frontend_set_current_preview_scene(obs_source_t *scene)

   Sets the current preview scene in studio mode, or the currently
   active scene if not in studio mode.

   :param scene: The scene to set as the current preview.

---------------------------------------

.. function:: void *obs_frontend_take_screenshot(void)

   Takes a screenshot of the main OBS output.

---------------------------------------

.. function:: void *obs_frontend_take_source_screenshot(obs_source_t *source)

   Takes a screenshot of the specified source.

   :param source: The source to take screenshot of.

---------------------------------------

.. function:: obs_output_t *obs_frontend_get_virtualcam_output(void)

   :return: A new reference to the current virtual camera output.

---------------------------------------

.. function:: void obs_frontend_start_virtualcam(void)

   Starts the virtual camera.

---------------------------------------

.. function:: void obs_frontend_stop_virtualcam(void)

   Stops the virtual camera.

---------------------------------------

.. function:: bool obs_frontend_virtualcam_active(void)

   :return: *true* if virtual camera is active, *false* otherwise.

---------------------------------------

.. function:: void obs_frontend_reset_video(void)

   Reloads the UI canvas and resets libobs video with latest data from profile.
