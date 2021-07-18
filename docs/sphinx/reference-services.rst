Service API Reference (obs_service_t)
=====================================

Services are custom implementations of streaming services, which are
used with outputs that stream.  For example, you could have a custom
implementation for streaming to Twitch, and another for YouTube to allow
the ability to log in and use their APIs to do things such as get the
RTMP servers or control the channel.  The `libobs/obs-service.h`_ file
is the dedicated header for implementing services.

*(Author's note: the service API is incomplete as of this writing)*

.. type:: obs_service_t

   A reference-counted service object.

.. type:: obs_weak_service_t

   A weak reference to a service object.

.. code:: cpp

   #include <obs.h>


Service Definition Structure
----------------------------

.. type:: struct obs_service_info

   Service definition structure.

.. member:: const char *obs_service_info.id

   Unique string identifier for the service (required).

.. member:: const char *(*obs_service_info.get_name)(void *type_data)

   Get the translated name of the service type.

   :param  type_data:  The type_data variable of this structure
   :return:            The translated name of the service type

.. member:: void *(*obs_service_info.create)(obs_data_t *settings, obs_service_t *service)

   Creates the implementation data for the service.

   :param  settings: Settings to initialize the service with
   :param  service:   Source that this data is associated with
   :return:          The implementation data associated with this service

.. member:: void (*obs_service_info.destroy)(void *data)

   Destroys the implementation data for the service.

.. member:: void (*obs_service_info.get_defaults)(obs_data_t *settings)
            void (*obs_service_info.get_defaults2)(void *type_data, obs_data_t *settings)

   Sets the default settings for this service.

   :param  settings:  Default settings.  Call obs_data_set_default*
                      functions on this object to set default setting
                      values

.. member:: obs_properties_t *(*obs_service_info.get_properties)(void *data)
            obs_properties_t *(*obs_service_info.get_properties2)(void *data, void *type_data)

   Gets the property information of this service.

   (Optional)

   :return: The properties of the service

.. member:: void (*obs_service_info.update)(void *data, obs_data_t *settings)

   Updates the settings for this service.

   (Optional)

   :param settings: New settings for this service

.. member:: bool (*obs_service_info.initialize)(void *data, obs_output_t *output)

   Called when getting ready to start up an output, before the encoders
   and output are initialized.

   (Optional)

   :param  output: Output context to use this service with
   :return:        *true* to allow the output to start up,
                   *false* to prevent output from starting up

.. member:: const char *(*obs_service_info.get_protocol)(void *data)

   :return: The stream protocol

.. member:: const char *(*obs_service_info.get_url)(void *data)

   :return: The stream URL

.. member:: const char *(*obs_service_info.get_key)(void *data)

   :return: The stream key

.. member:: const char *(*obs_service_info.get_username)(void *data)

   (Optional)

   :return: The username

.. member:: const char *(*obs_service_info.get_password)(void *data)

   (Optional)

   :return: The password

.. member:: void (*obs_service_info.apply_encoder_settings)(void *data, obs_data_t *video_encoder_settings, obs_data_t *audio_encoder_settings)

   This function is called to apply custom encoder settings specific to
   this service.  For example, if a service requires a specific keyframe
   interval, or has a bitrate limit, the settings for the video and
   audio encoders can be optionally modified if the front-end optionally
   calls :c:func:`obs_service_apply_encoder_settings()`.

   (Optional)

   :param video_encoder_settings: The audio encoder settings to change
   :param audio_encoder_settings: The video encoder settings to change

.. member:: void *obs_service_info.type_data
            void (*obs_service_info.free_type_data)(void *type_data)

   Private data associated with this entry.  Note that this is not the
   same as the implementation data; this is used to differentiate
   between two different types if the same callbacks are used for more
   than one different type.

   (Optional)

.. member:: const char *(*obs_service_info.get_output_type)(void *data)

   (Optional)

   :return: The output type that should be used with this service


General Service Functions
-------------------------

.. function:: void obs_register_service(struct obs_service_info *info)

   Registers a service type.  Typically used in
   :c:func:`obs_module_load()` or in the program's initialization phase.

---------------------

.. function:: const char *obs_service_get_display_name(const char *id)

   Calls the :c:member:`obs_service_info.get_name` callback to get the
   translated display name of a service type.

   :param    id:            The service type string identifier
   :return:                 The translated display name of a service type

---------------------

.. function:: obs_service_t *obs_service_create(const char *id, const char *name, obs_data_t *settings, obs_data_t *hotkey_data)

   Creates a service with the specified settings.

   The "service" context is used for encoding video/audio data.  Use
   obs_service_release to release it.

   :param   id:             The service type string identifier
   :param   name:           The desired name of the service.  If this is
                            not unique, it will be made to be unique
   :param   settings:       The settings for the service, or *NULL* if
                            none
   :param   hotkey_data:    Saved hotkey data for the service, or *NULL*
                            if none
   :return:                 A reference to the newly created service, or
                            *NULL* if failed

---------------------

.. function:: void obs_service_addref(obs_service_t *service)
              void obs_service_release(obs_service_t *service)

   Adds/releases a reference to a service.  When the last reference is
   released, the service is destroyed.

---------------------

.. function:: obs_weak_service_t *obs_service_get_weak_service(obs_service_t *service)
              obs_service_t *obs_weak_service_get_service(obs_weak_service_t *weak)

   These functions are used to get a weak reference from a strong service
   reference, or a strong service reference from a weak reference.  If
   the service is destroyed, *obs_weak_service_get_service* will return
   *NULL*.

---------------------

.. function:: void obs_weak_service_addref(obs_weak_service_t *weak)
              void obs_weak_service_release(obs_weak_service_t *weak)

   Adds/releases a weak reference to a service.

---------------------

.. function:: const char *obs_service_get_name(const obs_service_t *service)

   :return: The name of the service

---------------------

.. function:: obs_data_t *obs_service_defaults(const char *id)

   :return: An incremented reference to the service's default settings

---------------------

.. function:: obs_properties_t *obs_service_properties(const obs_service_t *service)
              obs_properties_t *obs_get_service_properties(const char *id)

   Use these functions to get the properties of a service or service
   type.  Properties are optionally used (if desired) to automatically
   generate user interface widgets to allow users to update settings.

   :return: The properties list for a specific existing service.  Free
            with :c:func:`obs_properties_destroy()`

---------------------

.. function:: obs_data_t *obs_service_get_settings(const obs_service_t *service)

   :return: An incremented reference to the service's settings

---------------------

.. function:: void obs_service_update(obs_service_t *service, obs_data_t *settings)

   Updates the settings for this service context.

---------------------

.. function:: const char *obs_service_get_url(const obs_service_t *service)

  :return: The URL currently used for this service

---------------------

.. function:: const char *obs_service_get_key(const obs_service_t *service)

  :return: Stream key (if any) currently used for this service

---------------------

.. function:: const char *obs_service_get_username(const obs_service_t *service)

   :return: User name (if any) currently used for this service

---------------------

.. function:: const char *obs_service_get_password(const obs_service_t *service)

   :return: Password (if any) currently used for this service

---------------------

.. function:: void obs_service_apply_encoder_settings(obs_service_t *service, obs_data_t *video_encoder_settings, obs_data_t *audio_encoder_settings)

   Applies service-specific video encoder settings.

   :param  video_encoder_settings: Video encoder settings.  Can be *NULL*
   :param  audio_encoder_settings: Audio encoder settings.  Can be *NULL*

.. ---------------------------------------------------------------------------

.. _libobs/obs-service.h: https://github.com/jp9000/obs-studio/blob/master/libobs/obs-service.h
