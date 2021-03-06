Source API Reference (obs_source_t)
===================================

Sources are used to render video and/or audio on stream.  Things such as
capturing displays/games/audio, playing a video, showing an image, or
playing audio.  Sources can also be used to implement audio and video
filters as well as transitions.  The `libobs/obs-source.h`_ file is the
dedicated header for implementing sources.

.. type:: obs_source_t

   A reference-counted video/audio input source.

.. type:: obs_weak_source_t

   A weak reference to an video/audio input source.

.. code:: cpp

   #include <obs.h>


Source Definition Structure (obs_source_info)
---------------------------------------------

.. type:: struct obs_source_info

   Source definition structure.

.. member:: const char *obs_source_info.id

   Unique string identifier for the source (required).

.. member:: enum obs_source_type obs_source_info.type

   Type of source.

   - **OBS_SOURCE_TYPE_INPUT**      - Video/Audio Input
   - **OBS_SOURCE_TYPE_FILTER**     - Filter
   - **OBS_SOURCE_TYPE_TRANSITION** - Transition

.. member:: uint32_t obs_source_info.output_flags

   Source output capability flags (required).

   (Author's note: This should be renamed to "capability_flags")

   A bitwise OR combination of one or more of the following values:

   - **OBS_SOURCE_VIDEO** - Source has video
  
     Unless SOURCE_ASYNC_VIDEO is specified, the source must include the
     :c:member:`obs_source_info.video_render` callback in the source
     definition structure.

   - **OBS_SOURCE_AUDIO** - Source has audio
  
     Use the :c:func:`obs_source_output_audio()` function to pass raw
     audio data, which will be automatically converted and uploaded.  If
     used with OBS_SOURCE_ASYNC_VIDEO, audio will automatically be
     synced up to the video output based upon their mutual timestamps.

   - **OBS_SOURCE_ASYNC** - Video is asynchronous (use
     OBS_SOURCE_ASYNC_VIDEO instead to automatically combine this flag
     with the OBS_SOURCE_VIDEO flag).

   - **OBS_SOURCE_ASYNC_VIDEO** - Source passes raw video data via RAM

     Use the :c:func:`obs_source_output_video()` function to pass raw
     video data, which will be automatically drawn at a timing relative
     to the provided timestamp.
     
     If audio is also present on the source, the audio will
     automatically be synced to the video based upon their mutual
     timestamps.

   - **OBS_SOURCE_CUSTOM_DRAW** - Source uses custom graphics calls,
     rather than just rendering a single texture.

     This capability flag must be used if the source does not use
     :c:func:`obs_source_draw()` to render a single texture.

     This capability flag is an important hint to turn off a specific
     optimization that allows the first effect filter in the filter
     chain to render the source directly with that effect filter.  The
     optimization does not work if there are custom graphics calls, and
     the source must be rendered to a texture first before being sent to
     the first filter in the filter chain.

     (Author's note: Ironically, not many sources render with that
     optimization.  I should have made it so that the optimization isn't
     used by default, and a flag should have been used to turn on the
     optimization -- not turn it off).

   - **OBS_SOURCE_INTERACTION** - Source can be interacted with by the
     user.

     When this is used, the source will receive interaction events if
     these callbacks are provided:
     :c:member:`obs_source_info.mouse_click`,
     :c:member:`obs_source_info.mouse_move`,
     :c:member:`obs_source_info.mouse_wheel`,
     :c:member:`obs_source_info.focus`, and
     :c:member:`obs_source_info.key_click`.

   - **OBS_SOURCE_COMPOSITE** - Source composites child sources

     When used, specifies that the source composites one or more child
     sources.  Scenes and transitions are examples of sources that
     contain and render child sources.
     
     Sources that render sub-sources must implement the audio_render
     callback in order to perform custom audio mixing of child sources.

     This capability flag is always set for transitions.

   - **OBS_SOURCE_DO_NOT_DUPLICATE** - Source should not be fully
     duplicated.

     When this is used, specifies that the source should not be fully
     duplicated, and should prefer to duplicate via holding references
     rather than full duplication.

     When functions such as :c:func:`obs_source_duplicate()` or
     :c:func:`obs_scene_duplicate()` are called, sources or child
     sources with this flag will never be fully duplicated, and will
     instead only be referenced.

     An example of the type of sources that should not be fully
     duplicated are video devices, browsers, and video/audio captures,
     as they will either not function correctly or will cause
     performance or resource issues when duplicated.

   - **OBS_SOURCE_DEPRECATED** - Source is deprecated and should not be
     used.

   - **OBS_SOURCE_DO_NOT_SELF_MONITOR** - Audio of this source should
     not allow monitoring if the current monitoring device is the same
     device being captured by the source.

     This flag is used as a hint to the back-end to prevent the source
     from creating an audio feedback loop.  This is primarily only used
     with desktop audio capture sources.

   - **OBS_SOURCE_CAP_DISABLED** - This source type has been disabled
     and should not be shown as a type of source the user can add.

   - **OBS_SOURCE_CAP_OBSOLETE** - This source type is obsolete and
     should not be shown as a type of source the user can add.
     Identical to *OBS_SOURCE_CAP_DISABLED*.  Meant to be used when a
     source has changed in some way (mostly defaults/properties), but
     you want to avoid breaking older configurations.  Basically solves
     the problem of "I want to change the defaults of a source but I
     don't want to break people's configurations"

   - **OBS_SOURCE_CONTROLLABLE_MEDIA** - This source has media that can
     be controlled

.. member:: const char *(*obs_source_info.get_name)(void *type_data)

   Get the translated name of the source type.

   :param  type_data:  The type_data variable of this structure
   :return:            The translated name of the source type

.. member:: void *(*obs_source_info.create)(obs_data_t *settings, obs_source_t *source)

   Creates the implementation data for the source.

   :param  settings: Settings to initialize the source with
   :param  source:   Source that this data is associated with
   :return:          The implementation data associated with this source

.. member:: void (*obs_source_info.destroy)(void *data)

   Destroys the implementation data for the source.

   Async sources must not call obs_source_output_video after returning
   from destroy.

.. member:: uint32_t (*obs_source_info.get_width)(void *data)
	    uint32_t (*obs_source_info.get_height)(void *data);

   Returns the width/height of the source.  These callbacks are required
   if this is a video source and is synchronous.

   (Author's note: These should really be consolidated in to one
   function, not two)

   :return: The width/height of the video

.. member:: void (*obs_source_info.get_defaults)(obs_data_t *settings)
            void (*obs_source_info.get_defaults2)(void *type_data, obs_data_t *settings)

   Sets the default settings for this source.

   :param  settings:  Default settings.  Call obs_data_set_default*
                      functions on this object to set default setting
                      values

.. member:: obs_properties_t *(*obs_source_info.get_properties)(void *data)
            obs_properties_t *(*obs_source_info.get_properties2)(void *data, void *type_data)

   Gets the property information of this source.

   (Optional)

   :return: The properties of the source

.. member:: void (*obs_source_info.update)(void *data, obs_data_t *settings)

   Updates the settings for this source.

   (Optional)

   :param settings: New settings for this source

.. member:: void (*obs_source_info.activate)(void *data)

   Called when the source has been activated in the main view (visible
   on stream/recording).

   (Optional)

.. member:: void (*obs_source_info.deactivate)(void *data)

   Called when the source has been deactivated from the main view (no
   longer visible on stream/recording).

   (Optional)

.. member:: void (*obs_source_info.show)(void *data)

   Called when the source is visible on any display and/or on the main
   view.

   (Optional)

.. member:: void (*obs_source_info.hide)(void *data)

   Called when the source is no longer visible on any display and/or on
   the main view.

   (Optional)

.. member:: void (*obs_source_info.video_tick)(void *data, float seconds)

   Called each video frame with the time elapsed.

   (Optional)

   :param  seconds: Seconds elapsed since the last frame

.. member:: void (*obs_source_info.video_render)(void *data, gs_effect_t *effect)

   Called when rendering the source with the graphics subsystem.

   If this is an input/transition source, this is called to draw the
   source texture with the graphics subsystem.

   If this is a filter source, it wraps source draw calls (for example
   applying a custom effect with custom parameters to a source).  In
   this case, it's highly recommended to use the
   :c:func:`obs_source_process_filter_begin()` and
   :c:func:`obs_source_process_filter_end()` functions to automatically
   handle effect-based filter processing.  However, you can implement
   custom draw handling as desired as well.

   If the source output capability flags do not include
   OBS_SOURCE_CUSTOM_DRAW, the source must use
   :c:func:`obs_source_draw()` to render the source's texture.

   :param effect: This parameter is no longer used.  Instead, call
                  :c:func:`obs_source_draw()`

.. member:: struct obs_source_frame *(*obs_source_info.filter_video)(void *data, struct obs_source_frame *frame)

   Called to filter raw async video data.  This function is only used
   with asynchronous video filters.

   :param  frame: Video frame to filter
   :return:       New video frame data.  This can defer video data to
                  be drawn later if time is needed for processing

.. member:: struct obs_audio_data *(*obs_source_info.filter_audio)(void *data, struct obs_audio_data *audio)

   Called to filter raw audio data.  This function is only used with
   audio filters.

   :param  audio: Audio data to filter
   :return:       Modified or new audio data.  You can directly modify
                  the data passed and return it, or you can defer audio
                  data for later if time is needed for processing.  If
                  you are returning new data, that data must exist until
                  the next call to the
                  :c:member:`obs_source_info.filter_audio` callback or
                  until the filter is removed/destroyed

.. member:: void (*obs_source_info.enum_active_sources)(void *data, obs_source_enum_proc_t enum_callback, void *param)

   Called to enumerate all active sources being used within this
   source.  If the source has children that render audio/video it must
   implement this callback.  Only used with sources that have the
   OBS_SOURCE_COMPOSITE output capability flag.

   :param  enum_callback: Enumeration callback
   :param  param:         User data to pass to callback

.. member:: void (*obs_source_info.save)(void *data, obs_data_t *settings)

   Called when saving custom data for a source.  This is a separate
   function because sometimes a source needs to know when it is being
   saved so it doesn't always have to update the current settings until
   a certain point.

   (Optional)

   :param  settings: Settings object to save data to

.. member:: void (*obs_source_info.load)(void *data, obs_data_t *settings)

   Called when loading custom data from saved source data.  This is
   called after all the loading sources have actually been created,
   allowing the ability to reference other sources if desired.

   (Optional)

   :param  settings: Settings object to load data from

.. member:: void (*obs_source_info.mouse_click)(void *data, const struct obs_mouse_event *event, int32_t type, bool mouse_up, uint32_t click_count)

   Called when interacting with a source and a mouse-down or mouse-up
   occurs.  Only used with sources that have the OBS_SOURCE_INTERACTION
   output capability flag.

   (Optional)

   :param event:       Mouse event properties
   :param type:        Mouse button pushed
   :param mouse_up:    Mouse event type (true if mouse-up)
   :param click_count: Mouse click count (1 for single click, etc.)

.. member:: void (*obs_source_info.mouse_move)(void *data, const struct obs_mouse_event *event, bool mouse_leave)

   Called when interacting with a source and a mouse-move occurs.  Only
   used with sources that have the OBS_SOURCE_INTERACTION output
   capability flag.

   (Optional)

   :param event:       Mouse event properties
   :param mouse_leave: Mouse leave state (true if mouse left source)

.. member:: void (*obs_source_info.mouse_wheel)(void *data, const struct obs_mouse_event *event, int x_delta, int y_delta)

   Called when interacting with a source and a mouse-wheel occurs.  Only
   used with sources that have the OBS_SOURCE_INTERACTION output
   capability flag.

   (Optional)

   :param event:       Mouse event properties
   :param x_delta:     Movement delta in the horizontal direction
   :param y_delta:     Movement delta in the vertical direction


.. member:: void (*obs_source_info.focus)(void *data, bool focus)

   Called when interacting with a source and gain focus/lost focus event
   occurs.  Only used with sources that have the OBS_SOURCE_INTERACTION
   output capability flag.

   (Optional)

   :param focus:       Focus state (true if focus gained)

.. member:: void (*obs_source_info.key_click)(void *data, const struct obs_key_event *event, bool key_up)

   Called when interacting with a source and a key-up or key-down
   occurs.  Only used with sources that have the OBS_SOURCE_INTERACTION
   output capability flag.

   (Optional)

   :param event:       Key event properties
   :param focus:       Key event type (true if mouse-up)

.. member:: void (*obs_source_info.filter_remove)(void *data, obs_source_t *source)

   Called when the filter is removed from a source.

   (Optional)

   :param  data:   Filter data
   :param  source: Source that the filter being removed from

.. member:: void *obs_source_info.type_data
            void (*obs_source_info.free_type_data)(void *type_data)

   Private data associated with this entry.  Note that this is not the
   same as the implementation data; this is used to differentiate
   between two different types if the same callbacks are used for more
   than one different type.

.. member:: bool (*obs_source_info.audio_render)(void *data, uint64_t *ts_out, struct obs_source_audio_mix *audio_output, uint32_t mixers, size_t channels, size_t sample_rate)

   Called to render audio of composite sources.  Only used with sources
   that have the OBS_SOURCE_COMPOSITE output capability flag.

.. member:: void (*obs_source_info.enum_all_sources)(void *data, obs_source_enum_proc_t enum_callback, void *param)

   Called to enumerate all active and inactive sources being used
   within this source.  If this callback isn't implemented,
   enum_active_sources will be called instead.  Only used with sources
   that have the OBS_SOURCE_COMPOSITE output capability flag.

   This is typically used if a source can have inactive child sources.

   :param  enum_callback: Enumeration callback
   :param  param:         User data to pass to callback

.. member:: void (*obs_source_info.transition_start)(void *data)
            void (*obs_source_info.transition_stop)(void *data)

   Called on transition sources when the transition starts/stops.

   (Optional)

.. member:: enum obs_icon_type obs_source_info.icon_type

   Icon used for the source.

   - **OBS_ICON_TYPE_UNKNOWN**         - Unknown
   - **OBS_ICON_TYPE_IMAGE**           - Image
   - **OBS_ICON_TYPE_COLOR**           - Color
   - **OBS_ICON_TYPE_SLIDESHOW**       - Slideshow
   - **OBS_ICON_TYPE_AUDIO_INPUT**     - Audio Input
   - **OBS_ICON_TYPE_AUDIO_OUTPUT**    - Audio Output
   - **OBS_ICON_TYPE_DESKTOP_CAPTURE** - Desktop Capture
   - **OBS_ICON_TYPE_WINDOW_CAPTURE**  - Window Capture
   - **OBS_ICON_TYPE_GAME_CAPTURE**    - Game Capture
   - **OBS_ICON_TYPE_CAMERA**          - Camera
   - **OBS_ICON_TYPE_TEXT**            - Text
   - **OBS_ICON_TYPE_MEDIA**           - Media
   - **OBS_ICON_TYPE_BROWSER**         - Browser
   - **OBS_ICON_TYPE_CUSTOM**          - Custom (not implemented yet)

.. member:: void (*obs_source_info.media_play_pause)(void *data, bool pause)

   Called to pause or play media.

.. member:: void (*obs_source_info.media_restart)(void *data)

   Called to restart the media.

.. member:: void (*obs_source_info.media_stop)(void *data)

   Called to stop the media.

.. member:: void (*obs_source_info.media_next)(void *data)

   Called to go to the next media.

.. member:: void (*obs_source_info.media_previous)(void *data)

   Called to go to the previous media.

.. member:: int64_t (*obs_source_info.media_get_duration)(void *data)

   Called to get the media duration.

.. member:: int64_t (*obs_source_info.media_get_time)(void *data)

   Called to get the current time of the media.

.. member:: void (*obs_source_info.media_set_time)(void *data, int64_t miliseconds)

   Called to set the media time.

.. member:: enum obs_media_state (*obs_source_info.media_get_state)(void *data)

   Called to get the state of the media.

   - **OBS_MEDIA_STATE_NONE**      - None
   - **OBS_MEDIA_STATE_PLAYING**   - Playing
   - **OBS_MEDIA_STATE_OPENING**   - Opening
   - **OBS_MEDIA_STATE_BUFFERING** - Buffering
   - **OBS_MEDIA_STATE_PAUSED**    - Paused
   - **OBS_MEDIA_STATE_STOPPED**   - Stopped
   - **OBS_MEDIA_STATE_ENDED**     - Ended
   - **OBS_MEDIA_STATE_ERROR**     - Error


.. _source_signal_handler_reference:

Source Signals
--------------

**destroy** (ptr *source*)

   This signal is called when the source is about to be destroyed.  Do
   not increment any references when using this signal.

**remove** (ptr source)

   Called when the :c:func:`obs_source_remove()` function is called on
   the source.

**save** (ptr source)

   Called when the source is being saved.

**load** (ptr source)

   Called when the source is being loaded.

**activate** (ptr source)

   Called when the source has been activated in the main view (visible
   on stream/recording).

**deactivate** (ptr source)

   Called when the source has been deactivated from the main view (no
   longer visible on stream/recording).

**show** (ptr source)

   Called when the source is visible on any display and/or on the main
   view.

**hide** (ptr source)

   Called when the source is no longer visible on any display and/or on
   the main view.

**mute** (ptr source, bool muted)

   Called when the source is muted/unmuted.

**push_to_mute_changed** (ptr source, bool enabled)

   Called when push-to-mute has been enabled/disabled.

**push_to_mute_delay** (ptr source, int delay)

   Called when the push-to-mute delay value has changed.

**push_to_talk_changed** (ptr source, bool enabled)

   Called when push-to-talk has been enabled/disabled.

**push_to_talk_delay** (ptr source, int delay)

   Called when the push-to-talk delay value has changed.

**enable** (ptr source, bool enabled)

   Called when the source has been disabled/enabled.

**rename** (ptr source, string new_name, string prev_name)

   Called when the source has been renamed.

**volume** (ptr source, in out float volume)

   Called when the volume of the source has changed.

**update_properties** (ptr source)

   Called when the properties of the source have been updated.

**update_flags** (ptr source, int flags)

   Called when the flags of the source have been changed.

**audio_sync** (ptr source, int out int offset)

   Called when the audio sync offset has changed.

**audio_mixers** (ptr source, in out int mixers)

   Called when the audio mixers have changed.

**filter_add** (ptr source, ptr filter)

   Called when a filter has been added to the source.

**filter_remove** (ptr source, ptr filter)

   Called when a filter has been removed from the source.

**reorder_filters** (ptr source)

   Called when filters have been reordered.

**transition_start** (ptr source)

   Called when a transition is starting.

**transition_video_stop** (ptr source)

   Called when a transition's video transitioning has stopped.

**transition_stop** (ptr source)

   Called when a transition has stopped.

**media_started**

   Called when media has started.

**media_ended**

   Called when media has ended.

**media_pause**

   Called when media has been paused.

**media_play**

   Called when media starts playing.

**media_restart**

   Called when the playing of media has been restarted.

**media_stopped**

   Called when the playing of media has been stopped.

**media_next**

   Called when the media source switches to the next media.

**media_previous**

   Called when the media source switches to the previous media.

General Source Functions
------------------------

.. function:: void obs_register_source(struct obs_source_info *info)

   Registers a source type.  Typically used in
   :c:func:`obs_module_load()` or in the program's initialization phase.

---------------------

.. function:: const char *obs_source_get_display_name(const char *id)

   Calls the :c:member:`obs_source_info.get_name` callback to get the
   translated display name of a source type.

   :param    id:            The source type string identifier
   :return:                 The translated display name of a source type

---------------------

.. function:: obs_source_t *obs_source_create(const char *id, const char *name, obs_data_t *settings, obs_data_t *hotkey_data)

   Creates a source of the specified type with the specified settings.
  
   The "source" context is used for anything related to presenting
   or modifying video/audio.  Use obs_source_release to release it.

   :param   id:             The source type string identifier
   :param   name:           The desired name of the source.  If this is
                            not unique, it will be made to be unique
   :param   settings:       The settings for the source, or *NULL* if
                            none
   :param   hotkey_data:    Saved hotkey data for the source, or *NULL*
                            if none
   :return:                 A reference to the newly created source, or
                            *NULL* if failed

---------------------

.. function:: obs_source_t *obs_source_create_private(const char *id, const char *name, obs_data_t *settings)

   Creates a 'private' source which is not enumerated by
   :c:func:`obs_enum_sources()`, and is not saved by
   :c:func:`obs_save_sources()`.

   Author's Note: The existence of this function is a result of design
   flaw: the front-end should control saving/loading of sources, and
   functions like :c:func:`obs_enum_sources()` and
   :c:func:`obs_save_sources()` should not exist in the back-end.

   :param   id:             The source type string identifier
   :param   name:           The desired name of the source.  For private
                            sources, this does not have to be unique,
                            and can additionally be *NULL* if desired
   :param   settings:       The settings for the source, or *NULL* if
                            none
   :return:                 A reference to the newly created source, or
                            *NULL* if failed

---------------------

.. function:: obs_source_t *obs_source_duplicate(obs_source_t *source, const char *desired_name, bool create_private)

   Duplicates a source.  If the source has the
   OBS_SOURCE_DO_NOT_DUPLICATE output flag set, this only returns a
   new reference to the same source.

   :param source:         The source to duplicate
   :param desired_name:   The desired name of the new source.  If this is
                          not a private source and the name is not unique,
                          it will be made to be unique
   :param create_private: If *true*, the new source will be a private
                          source if fully duplicated
   :return:               A new source reference

---------------------

.. function:: void obs_source_addref(obs_source_t *source)
              void obs_source_release(obs_source_t *source)

   Adds/releases a reference to a source.  When the last reference is
   released, the source is destroyed.

---------------------

.. function:: obs_weak_source_t *obs_source_get_weak_source(obs_source_t *source)
              obs_source_t *obs_weak_source_get_source(obs_weak_source_t *weak)

   These functions are used to get a weak reference from a strong source
   reference, or a strong source reference from a weak reference.  If
   the source is destroyed, *obs_weak_source_get_source* will return
   *NULL*.

---------------------

.. function:: void obs_weak_source_addref(obs_weak_source_t *weak)
              void obs_weak_source_release(obs_weak_source_t *weak)

   Adds/releases a weak reference to a source.

---------------------

.. function:: void obs_source_remove(obs_source_t *source)

   Notifies all reference holders of the source (via
   :c:func:`obs_source_removed()`) that the source should be released.

---------------------

.. function:: bool obs_source_removed(const obs_source_t *source)

   :return: *true* if the source should be released

---------------------

.. function:: uint32_t obs_source_get_output_flags(const obs_source_t *source)
              uint32_t obs_get_source_output_flags(const char *id)

   :return: Capability flags of a source

   Author's Note: "Output flags" is poor wording in retrospect; this
   should have been named "Capability flags", and the OBS_SOURCE_*
   macros should really be OBS_SOURCE_CAP_* macros instead.

   See :c:member:`obs_source_info.output_flags` for more information.

---------------------

.. function:: obs_data_t *obs_get_source_defaults(const char *id)

   Calls :c:member:`obs_source_info.get_defaults` to get the defaults
   settings of the source type.

   :return: The default settings for a source type

---------------------

.. function:: obs_properties_t *obs_source_properties(const obs_source_t *source)
              obs_properties_t *obs_get_source_properties(const char *id)

   Use these functions to get the properties of a source or source type.
   Properties are optionally used (if desired) to automatically generate
   user interface widgets to allow users to update settings.

   :return: The properties list for a specific existing source.  Free with
            :c:func:`obs_properties_destroy()`

---------------------

.. function:: bool obs_source_configurable(const obs_source_t *source)
              bool obs_is_source_configurable(const char *id)

   :return: *true* if the the source has custom properties, *false*
            otherwise

---------------------

.. function:: void obs_source_update(obs_source_t *source, obs_data_t *settings)

   Updates the settings for a source and calls the
   :c:member:`obs_source_info.update` callback of the source.  If the
   source is a video source, the :c:member:`obs_source_info.update` will
   be not be called immediately; instead, it will be deferred to the
   video thread to prevent threading issues.

---------------------

.. function:: void obs_source_video_render(obs_source_t *source)

   Renders a video source.  This will call the
   :c:member:`obs_source_info.video_render` callback of the source.

---------------------

.. function:: uint32_t obs_source_get_width(obs_source_t *source)
              uint32_t obs_source_get_height(obs_source_t *source)

   Calls the :c:member:`obs_source_info.get_width` or
   :c:member:`obs_source_info.get_height` of the source to get its width
   and/or height.

   Author's Note: These functions should be consolidated in to a single
   function/callback rather than having a function for both width and
   height.

   :return: The width or height of the source

---------------------

.. function:: obs_data_t *obs_source_get_settings(const obs_source_t *source)

   :return: The settings string for a source.  The reference counter of the
            returned settings data is incremented, so
            :c:func:`obs_data_release()` must be called when the
            settings are no longer used

---------------------

.. function:: const char *obs_source_get_name(const obs_source_t *source)

   :return: The name of the source

---------------------

.. function:: void obs_source_set_name(obs_source_t *source, const char *name)

   Sets the name of a source.  If the source is not private and the name
   is not unique, it will automatically be given a unique name.

---------------------

.. function:: enum obs_source_type obs_source_get_type(const obs_source_t *source)

   :return: | OBS_SOURCE_TYPE_INPUT for inputs
            | OBS_SOURCE_TYPE_FILTER for filters
            | OBS_SOURCE_TYPE_TRANSITION for transitions
            | OBS_SOURCE_TYPE_SCENE for scenes

---------------------

.. function:: const char *obs_source_get_id(const obs_source_t *source)

   :return: The source's type identifier string

---------------------

.. function:: signal_handler_t *obs_source_get_signal_handler(const obs_source_t *source)

   :return: The source's signal handler

   See the :ref:`source_signal_handler_reference` for more information
   on signals that are available for sources.

---------------------

.. function:: proc_handler_t *obs_source_get_proc_handler(const obs_source_t *source)

   :return: The procedure handler for a source

---------------------

.. function:: void obs_source_set_volume(obs_source_t *source, float volume)
              float obs_source_get_volume(const obs_source_t *source)

   Sets/gets the user volume for a source that has audio output.

---------------------

.. function:: bool obs_source_muted(const obs_source_t *source)
              void obs_source_set_muted(obs_source_t *source, bool muted)

   Sets/gets whether the source's audio is muted.

---------------------

.. function:: bool obs_source_push_to_mute_enabled(const obs_source_t *source)
              void obs_source_enable_push_to_mute(obs_source_t *source, bool enabled)

   Sets/gets whether push-to-mute is enabled.

---------------------

.. function:: uint64_t obs_source_get_push_to_mute_delay(const obs_source_t *source)
              void obs_source_set_push_to_mute_delay(obs_source_t *source, uint64_t delay)

   Sets/gets the push-to-mute delay.

---------------------

.. function:: bool obs_source_push_to_talk_enabled(const obs_source_t *source)
              void obs_source_enable_push_to_talk(obs_source_t *source, bool enabled)

   Sets/gets whether push-to-talk is enabled.

---------------------

.. function:: uint64_t obs_source_get_push_to_talk_delay(const obs_source_t *source)
              void obs_source_set_push_to_talk_delay(obs_source_t *source, uint64_t delay)

   Sets/gets the push-to-talk delay.

---------------------

.. function:: void obs_source_set_sync_offset(obs_source_t *source, int64_t offset)
              int64_t obs_source_get_sync_offset(const obs_source_t *source)

   Sets/gets the audio sync offset (in nanoseconds) for a source.

---------------------

.. function:: void obs_source_enum_active_sources(obs_source_t *source, obs_source_enum_proc_t enum_callback, void *param)
              void obs_source_enum_active_tree(obs_source_t *source, obs_source_enum_proc_t enum_callback, void *param)

   Enumerates active child sources or source tree used by this source.

   Relevant data types used with this function:

.. code:: cpp

   typedef void (*obs_source_enum_proc_t)(obs_source_t *parent,
                   obs_source_t *child, void *param);

---------------------

.. function:: bool obs_source_active(const obs_source_t *source)

   :return: *true* if active, *false* if not.  A source is only
            considered active if it's being shown on the final mix

---------------------

.. function:: bool obs_source_showing(const obs_source_t *source)

   :return: *true* if showing, *false* if not.  A source is considered
            showing if it's being displayed anywhere at all, whether on
            a display context or on the final output

---------------------

.. function:: void obs_source_inc_showing(obs_source_t *source)
              void obs_source_dec_showing(obs_source_t *source)

   Increments/decrements a source's "showing" state.  Typically used
   when drawing a source on a display manually.

---------------------

.. function:: void obs_source_set_flags(obs_source_t *source, uint32_t flags)
              uint32_t obs_source_get_flags(const obs_source_t *source)

   :param flags: OBS_SOURCE_FLAG_FORCE_MONO Forces audio to mono

---------------------

.. function:: void obs_source_set_audio_mixers(obs_source_t *source, uint32_t mixers)
              uint32_t obs_source_get_audio_mixers(const obs_source_t *source)

   Sets/gets the audio mixer channels that a source outputs to
   (depending on what bits are set).  Audio mixers allow filtering
   specific using multiple audio encoders to mix different sources
   together depending on what mixer channel they're set to.

   For example, to output to mixer 1 and 3, you would perform a bitwise
   OR on bits 0 and 2:  (1<<0) | (1<<2), or 0x5.

---------------------

.. function:: void obs_source_enum_filters(obs_source_t *source, obs_source_enum_proc_t callback, void *param)

   Enumerates active filters on a source.

   Relevant data types used with this function:

.. code:: cpp

   typedef void (*obs_source_enum_proc_t)(obs_source_t *parent,
                   obs_source_t *child, void *param);

---------------------

.. function:: obs_source_t *obs_source_get_filter_by_name(obs_source_t *source, const char *name)

   :return: The desired filter, or *NULL* if not found.  The reference
            of the filter is incremented

---------------------

.. function:: void obs_source_copy_filters(obs_source_t *dst, obs_source_t *src)

   Copies filters from the source to the destination.  If filters by the
   same name already exist in the destination source, the newer filters
   will be given unique names.

---------------------

.. function:: bool obs_source_enabled(const obs_source_t *source)
              void obs_source_set_enabled(obs_source_t *source, bool enabled)

   Enables/disables a source, or returns the enabled state.

---------------------

.. function:: void obs_source_add_audio_capture_callback(obs_source_t *source, obs_source_audio_capture_t callback, void *param)
              void obs_source_remove_audio_capture_callback(obs_source_t *source, obs_source_audio_capture_t callback, void *param)

   Adds/removes an audio capture callback for a source.  This allows the
   ability to get the raw audio data of a source as it comes in.

   Relevant data types used with this function:

.. code:: cpp

   typedef void (*obs_source_audio_capture_t)(void *param, obs_source_t *source,
                   const struct audio_data *audio_data, bool muted);

---------------------

.. function:: void obs_source_set_deinterlace_mode(obs_source_t *source, enum obs_deinterlace_mode mode)
              enum obs_deinterlace_mode obs_source_get_deinterlace_mode(const obs_source_t *source)

   Sets/gets the deinterlace mode.

   :param mode:   | OBS_DEINTERLACE_MODE_DISABLE    - Disables deinterlacing
                  | OBS_DEINTERLACE_MODE_DISCARD    - Discard
                  | OBS_DEINTERLACE_MODE_RETRO      - Retro
                  | OBS_DEINTERLACE_MODE_BLEND      - Blend
                  | OBS_DEINTERLACE_MODE_BLEND_2X   - Blend 2x
                  | OBS_DEINTERLACE_MODE_LINEAR     - Linear
                  | OBS_DEINTERLACE_MODE_LINEAR_2X  - Linear 2x
                  | OBS_DEINTERLACE_MODE_YADIF      - Yadif
                  | OBS_DEINTERLACE_MODE_YADIF_2X   - Yadif 2x


---------------------

.. function:: void obs_source_set_deinterlace_field_order(obs_source_t *source, enum obs_deinterlace_field_order order)
              enum obs_deinterlace_field_order obs_source_get_deinterlace_field_order(const obs_source_t *source)

   Sets/gets the deinterlace field order.

   :param order: | OBS_DEINTERLACE_FIELD_ORDER_TOP - Start from top
                 | OBS_DEINTERLACE_FIELD_ORDER_BOTTOM - Start from bottom

---------------------

.. function:: obs_data_t *obs_source_get_private_settings(obs_source_t *item)

   Gets private front-end settings data.  This data is saved/loaded
   automatically.  Returns an incremented reference.

---------------------

.. function:: void obs_source_send_mouse_click(obs_source_t *source, const struct obs_mouse_event *event, int32_t type, bool mouse_up, uint32_t click_count)

   Used for interacting with sources: sends a mouse down/up event to a
   source.

---------------------

.. function:: void obs_source_send_mouse_move(obs_source_t *source, const struct obs_mouse_event *event, bool mouse_leave)

   Used for interacting with sources: sends a mouse move event to a
   source.

---------------------

.. function:: void obs_source_send_mouse_wheel(obs_source_t *source, const struct obs_mouse_event *event, int x_delta, int y_delta)

   Used for interacting with sources:  sends a mouse wheel event to a
   source.

---------------------

.. function:: void obs_source_send_focus(obs_source_t *source, bool focus)

   Used for interacting with sources:  sends a got-focus or lost-focus
   event to a source.

---------------------

.. function:: void obs_source_send_key_click(obs_source_t *source, const struct obs_key_event *event, bool key_up)

   Used for interacting with sources:  sends a key up/down event to a
   source.

---------------------


Functions used by sources
-------------------------

.. function:: void obs_source_draw_set_color_matrix(const struct matrix4 *color_matrix, const struct vec3 *color_range_min, const struct vec3 *color_range_max)

   Helper function to set the color matrix information when drawing the
   source.
  
   :param  color_matrix:    The color matrix.  Assigns to the 'color_matrix'
                            effect variable.
   :param  color_range_min: The minimum color range.  Assigns to the
                            'color_range_min' effect variable.  If NULL,
                            {0.0f, 0.0f, 0.0f} is used.
   :param  color_range_max: The maximum color range.  Assigns to the
                            'color_range_max' effect variable.  If NULL,
                            {1.0f, 1.0f, 1.0f} is used.

---------------------

.. function:: void obs_source_draw(gs_texture_t *image, int x, int y, uint32_t cx, uint32_t cy, bool flip)

   Helper function to draw sprites for a source (synchronous video).
  
   :param  image:  The sprite texture to draw.  Assigns to the 'image' variable
                   of the current effect.
   :param  x:      X position of the sprite.
   :param  y:      Y position of the sprite.
   :param  cx:     Width of the sprite.  If 0, uses the texture width.
   :param  cy:     Height of the sprite.  If 0, uses the texture height.
   :param  flip:   Specifies whether to flip the image vertically.

---------------------

.. function:: void obs_source_output_video(obs_source_t *source, const struct obs_source_frame *frame)

   Outputs asynchronous video data.  Set to NULL to deactivate the texture.

   Relevant data types used with this function:

.. code:: cpp

   enum video_format {
           VIDEO_FORMAT_NONE,
   
           /* planar 420 format */
           VIDEO_FORMAT_I420, /* three-plane */
           VIDEO_FORMAT_NV12, /* two-plane, luma and packed chroma */
   
           /* packed 422 formats */
           VIDEO_FORMAT_YVYU,
           VIDEO_FORMAT_YUY2, /* YUYV */
           VIDEO_FORMAT_UYVY,
   
           /* packed uncompressed formats */
           VIDEO_FORMAT_RGBA,
           VIDEO_FORMAT_BGRA,
           VIDEO_FORMAT_BGRX,
           VIDEO_FORMAT_Y800, /* grayscale */
   
           /* planar 4:4:4 */
           VIDEO_FORMAT_I444,
   };

   struct obs_source_frame {
           uint8_t             *data[MAX_AV_PLANES];
           uint32_t            linesize[MAX_AV_PLANES];
           uint32_t            width;
           uint32_t            height;
           uint64_t            timestamp;
   
           enum video_format   format;
           float               color_matrix[16];
           bool                full_range;
           float               color_range_min[3];
           float               color_range_max[3];
           bool                flip;
   };

---------------------

.. function:: void obs_source_set_async_rotation(obs_source_t *source, long rotation)

   Allows the ability to set rotation (0, 90, 180, -90, 270) for an
   async video source.  The rotation will be automatically applied to
   the source.

---------------------

.. function:: void obs_source_preload_video(obs_source_t *source, const struct obs_source_frame *frame)

   Preloads a video frame to ensure a frame is ready for playback as
   soon as video playback starts.

---------------------

.. function:: void obs_source_show_preloaded_video(obs_source_t *source)

   Shows any preloaded video frame.
   
---------------------

.. function:: void obs_source_output_audio(obs_source_t *source, const struct obs_source_audio *audio)

   Outputs audio data.

---------------------

.. function:: void obs_source_update_properties(obs_source_t *source)

   Signal an update to any currently used properties.

---------------------

.. function:: bool obs_source_add_active_child(obs_source_t *parent, obs_source_t *child)

   Adds an active child source.  Must be called by parent sources on child
   sources when the child is added and active.  This ensures that the source is
   properly activated if the parent is active.
  
   :return: *true* if source can be added, *false* if it causes recursion

---------------------

.. function:: void obs_source_remove_active_child(obs_source_t *parent, obs_source_t *child)

   Removes an active child source.  Must be called by parent sources on child
   sources when the child is removed or inactive.  This ensures that the source
   is properly deactivated if the parent is no longer active.

---------------------


Filters
-------

.. function:: obs_source_t *obs_filter_get_parent(const obs_source_t *filter)

   If the source is a filter, returns the parent source of the filter.
   The parent source is the source being filtered.
   
   Only guaranteed to be valid inside of the video_render, filter_audio,
   filter_video, and filter_remove callbacks.

---------------------

.. function:: obs_source_t *obs_filter_get_target(const obs_source_t *filter)

   If the source is a filter, returns the target source of the filter.
   The target source is the next source in the filter chain.
   
   Only guaranteed to be valid inside of the video_render, filter_audio,
   filter_video, and filter_remove callbacks.

---------------------

.. function:: void obs_source_default_render(obs_source_t *source)

   Can be used by filters to directly render a non-async parent source
   without any filter processing.

---------------------

.. function:: void obs_source_filter_add(obs_source_t *source, obs_source_t *filter)
              void obs_source_filter_remove(obs_source_t *source, obs_source_t *filter)

   Adds/removes a filter to/from a source.

---------------------

.. function:: void obs_source_filter_set_order(obs_source_t *source, obs_source_t *filter, enum obs_order_movement movement)

   Modifies the order of a specific filter.

   :param movement: | Can be one of the following:
                    | OBS_ORDER_MOVE_UP
                    | OBS_ORDER_MOVE_DOWN
                    | OBS_ORDER_MOVE_TOP
                    | OBS_ORDER_MOVE_BOTTOM

---------------------


Functions used by filters
-------------------------

.. function:: bool obs_source_process_filter_begin(obs_source_t *filter, enum gs_color_format format, enum obs_allow_direct_render allow_direct)

   Default RGB filter handler for generic effect filters.  Processes the
   filter chain and renders them to texture if needed, then the filter is
   drawn with.
  
   After calling this, set your parameters for the effect, then call
   obs_source_process_filter_end to draw the filter.
  
   :return: *true* if filtering should continue, *false* if the filter
            is bypassed for whatever reason

---------------------

.. function:: void obs_source_process_filter_end(obs_source_t *filter, gs_effect_t *effect, uint32_t width, uint32_t height)

   Draws the filter using the effect's "Draw" technique.
  
   Before calling this function, first call obs_source_process_filter_begin and
   then set the effect parameters, and then call this function to finalize the
   filter.

---------------------

.. function:: void obs_source_process_filter_tech_end(obs_source_t *filter, gs_effect_t *effect, uint32_t width, uint32_t height, const char *tech_name)

   Draws the filter with a specific technique in the effect.
  
   Before calling this function, first call obs_source_process_filter_begin and
   then set the effect parameters, and then call this function to finalize the
   filter.

---------------------

.. function:: void obs_source_skip_video_filter(obs_source_t *filter)

   Skips the filter if the filter is invalid and cannot be rendered.

---------------------


.. _transitions:

Transitions
-----------

.. function:: obs_source_t *obs_transition_get_source(obs_source_t *transition, enum obs_transition_target target)

   :param target: | OBS_TRANSITION_SOURCE_A - Source being transitioned from, or the current source if not transitioning
                  | OBS_TRANSITION_SOURCE_B - Source being transitioned to
   :return:       An incremented reference to the source or destination
                  sources of the transition

---------------------

.. function:: void obs_transition_clear(obs_source_t *transition)

   Clears the transition.

---------------------

.. function:: obs_source_t *obs_transition_get_active_source(obs_source_t *transition)

   :return: An incremented reference to the currently active source of
            the transition

---------------------

.. function:: bool obs_transition_start(obs_source_t *transition, enum obs_transition_mode mode, uint32_t duration_ms, obs_source_t *dest)

   Starts the transition with the desired destination source.

   :param mode:        Currently only OBS_TRANSITION_MODE_AUTO
   :param duration_ms: Duration in milliseconds.  If the transition has
                       a fixed duration set by
                       :c:func:`obs_transition_enable_fixed`, this
                       parameter will have no effect
   :param dest:        The destination source to transition to

---------------------

.. function:: void obs_transition_set_size(obs_source_t *transition, uint32_t cx, uint32_t cy)
              void obs_transition_get_size(const obs_source_t *transition, uint32_t *cx, uint32_t *cy)

   Sets/gets the dimensions of the transition.

---------------------

.. function:: void obs_transition_set_scale_type(obs_source_t *transition, enum obs_transition_scale_type type)
              enum obs_transition_scale_type obs_transition_get_scale_type( const obs_source_t *transition)

   Sets/gets the scale type for sources within the transition.

   :param type: | OBS_TRANSITION_SCALE_MAX_ONLY - Scale to aspect ratio, but only to the maximum size of each source
                | OBS_TRANSITION_SCALE_ASPECT   - Always scale the sources, but keep aspect ratio
                | OBS_TRANSITION_SCALE_STRETCH  - Scale and stretch the sources to the size of the transition

---------------------

.. function:: void obs_transition_set_alignment(obs_source_t *transition, uint32_t alignment)
              uint32_t obs_transition_get_alignment(const obs_source_t *transition)

   Sets/gets the alignment used to draw the two sources within
   transition the transition.

   :param alignment: | Can be any bitwise OR combination of:
                     | OBS_ALIGN_CENTER
                     | OBS_ALIGN_LEFT
                     | OBS_ALIGN_RIGHT
                     | OBS_ALIGN_TOP
                     | OBS_ALIGN_BOTTOM

---------------------


Functions used by transitions
-----------------------------

.. function:: void obs_transition_enable_fixed(obs_source_t *transition, bool enable, uint32_t duration_ms)
              bool obs_transition_fixed(obs_source_t *transition)

   Sets/gets whether the transition uses a fixed duration.  Useful for
   certain types of transitions such as stingers.  If this is set, the 
   *duration_ms* parameter of :c:func:`obs_transition_start()` has no
   effect.

---------------------

.. function:: float obs_transition_get_time(obs_source_t *transition)

   :return: The current transition time value (0.0f..1.0f)

---------------------

.. function:: void obs_transition_video_render(obs_source_t *transition, obs_transition_video_render_callback_t callback)

   Helper function used for rendering transitions.  This function will
   render two distinct textures for source A and source B of the
   transition, allowing the ability to blend them together with a pixel
   shader in a desired manner.

   The *a* and *b* parameters of *callback* are automatically rendered
   textures of source A and source B, *t* is the time value
   (0.0f..1.0f), *cx* and *cy* are the current dimensions of the
   transition, and *data* is the implementation's private data.

   Relevant data types used with this function:

.. code:: cpp

   typedef void (*obs_transition_video_render_callback_t)(void *data,
                   gs_texture_t *a, gs_texture_t *b, float t,
                   uint32_t cx, uint32_t cy);

---------------------

.. function::  bool obs_transition_audio_render(obs_source_t *transition, uint64_t *ts_out, struct obs_source_audio_mix *audio, uint32_t mixers, size_t channels, size_t sample_rate, obs_transition_audio_mix_callback_t mix_a_callback, obs_transition_audio_mix_callback_t mix_b_callback)

   Helper function used for transitioning audio.  Typically you'd call
   this in the obs_source_info.audio_render callback with its
   parameters, and use the mix_a_callback and mix_b_callback to
   determine the the audio fading of source A and source B.

   Relevant data types used with this function:

.. code:: cpp

   typedef float (*obs_transition_audio_mix_callback_t)(void *data, float t);

---------------------

.. function:: void obs_transition_swap_begin(obs_source_t *tr_dest, obs_source_t *tr_source)
              void obs_transition_swap_end(obs_source_t *tr_dest, obs_source_t *tr_source)

   Swaps two transitions.  Call obs_transition_swap_begin, swap the
   source, then call obs_transition_swap_end when complete.  This allows
   the ability to seamlessly swap two different transitions without it
   affecting the output.
   
   For example, if a transition is assigned to output channel 0, you'd
   call obs_transition_swap_begin, then you'd call obs_set_output_source
   with the new transition, then call
   :c:func:`obs_transition_swap_begin()`.

.. ---------------------------------------------------------------------------

.. _libobs/obs-source.h: https://github.com/jp9000/obs-studio/blob/master/libobs/obs-source.h
