Encoder API Reference (obs_encoder_t)
=====================================

Encoders are OBS-specific implementations of video/audio encoders, which
are used with outputs that use encoders.  x264, NVENC, Quicksync are
examples of encoder implementations.  The `libobs/obs-encoder.h`_ file
is the dedicated header for implementing encoders

.. type:: obs_encoder_t

   A reference-counted encoder object.

.. type:: obs_weak_encoder_t

   A weak reference to an encoder object.

.. code:: cpp

   #include <obs.h>


Encoder Definition Structure (obs_encoder_info)
-----------------------------------------------

.. type:: struct obs_encoder_info

   Encoder definition structure.

.. member:: const char *obs_encoder_info.id

   Unique string identifier for the encoder (required).

.. member:: enum obs_encoder_type obs_encoder_info.type

   Type of encoder.

   - **OBS_ENCODER_VIDEO** - Video encoder
   - **OBS_ENCODER_AUDIO** - Audio encoder

.. member:: const char *obs_encoder_info.codec

   The codec, in string form.  For example, "h264" for an H.264 encoder.

.. member:: const char *(*obs_encoder_info.get_name)(void *type_data)

   Get the translated name of the encoder type.

   :param  type_data:  The type_data variable of this structure
   :return:            The translated name of the encoder type

.. member:: void *(*obs_encoder_info.create)(obs_data_t *settings, obs_encoder_t *encoder)

   Creates the implementation data for the encoder.

   :param  settings: Settings to initialize the encoder with
   :param  encoder:  Encoder that this data is associated with
   :return:          The implementation data associated with this encoder

.. member:: void (*obs_encoder_info.destroy)(void *data)

   Destroys the implementation data for the encoder.

.. member:: bool (*encode)(void *data, struct encoder_frame *frame, struct encoder_packet *packet, bool *received_packet)

   Called to encode video or audio and outputs packets as they become
   available.

   :param frame:           Raw audio/video data to encode
   :param packet:          Encoder packet output, if any
   :param received_packet: Set to *true* if a packet was received,
                           *false* otherwise
   :return:                true if successful, false on critical failure

.. member:: size_t (*get_frame_size)(void *data)

   :return: An audio encoder's frame size.  For example, for AAC this
            number would be 1024

.. member:: void (*obs_encoder_info.get_defaults)(obs_data_t *settings)
            void (*obs_encoder_info.get_defaults2)(void *type_data, obs_data_t *settings)

   Sets the default settings for this encoder.

   :param  settings:  Default settings.  Call obs_data_set_default*
                      functions on this object to set default setting
                      values

.. member:: obs_properties_t *(*obs_encoder_info.get_properties)(void *data)
            obs_properties_t *(*obs_encoder_info.get_properties2)(void *data, void *type_data)

   Gets the property information of this encoder.

   (Optional)

   :return: The properties of the encoder

.. member:: void (*obs_encoder_info.update)(void *data, obs_data_t *settings)

   Updates the settings for this encoder.

   (Optional)

   :param settings: New settings for this encoder

.. member:: bool (*obs_encoder_info.get_extra_data)(void *data, uint8_t **extra_data, size_t *size)

   Returns extra data associated with this encoder (usually header).

   (Optional)

   :param  extra_data: Pointer to receive the extra data
   :param  size:       Pointer to receive the size of the extra
                       data
   :return:            true if extra data available, false
                       otherwise

.. member:: bool (*obs_encoder_info.get_sei_data)(void *data, uint8_t **sei_data, size_t *size)

   Gets the SEI data of a video encoder that has SEI data.

   (Optional)

   :param  sei_data: Pointer to receive the SEI data
   :param  size:     Pointer to receive the SEI data size
   :return:          true if SEI data available, false otherwise

.. member:: void (*obs_encoder_info.get_audio_info)(void *data, struct audio_convert_info *info)

   Returns desired audio format and sample information.  This callback
   can be used to tell the back-end that the audio data needs to be
   automatically converted to a different sample rate or audio format
   before being sent to the encoder.

   (Optional)

   :param  info: Audio format information

.. member:: void (*obs_encoder_info.get_video_info)(void *data, struct video_scale_info *info)

   Returns desired video format information.  This callback can be used
   to tell the back-end that the video data needs to be automatically
   converted to a different video format or specific size before being
   sent to the encoder.

   :param  info: Video format information

.. member:: void *obs_encoder_info.type_data
            void (*obs_encoder_info.free_type_data)(void *type_data)

   Private data associated with this entry.  Note that this is not the
   same as the implementation data; this is used to differentiate
   between two different types if the same callbacks are used for more
   than one different type.

.. member:: uint32_t obs_encoder_info.caps

   Can be 0 or a bitwise OR combination of one or more of the following
   values:

   - **OBS_ENCODER_CAP_DEPRECATED** - Encoder is deprecated


Encoder Packet Structure (encoder_packet)
-----------------------------------------

.. type:: struct encoder_packet

   Encoder packet structure.

.. member:: uint8_t               *encoder_packet.data

   Packet data.

.. member:: size_t                encoder_packet.size

   Packet size.

.. member:: int64_t               encoder_packet.pts
            int64_t               encoder_packet.dts

   Packet presentation and decode timestamps.

.. member:: int32_t               encoder_packet.timebase_num
            int32_t               encoder_packet.timebase_den

   Packet time base.

.. member:: enum obs_encoder_type encoder_packet.type

   Can be one of the following values:

   - **OBS_ENCODER_VIDEO** - Video data
   - **OBS_ENCODER_AUDIO** - Audio data

.. member:: bool                  encoder_packet.keyframe

   Packet is a keyframe.

.. member:: int64_t               encoder_packet.dts_usec

   The DTS in microseconds.

   (This should not be set by the encoder implementation)

.. member:: int64_t               encoder_packet.sys_dts_usec

   The system time of this packet in microseconds.

   (This should not be set by the encoder implementation)

.. member:: int                   encoder_packet.priority

   Packet priority.  This is no longer used.

   (This should not be set by the encoder implementation)

.. member:: int                   encoder_packet.drop_priority

   Packet drop priority.

   If this packet needs to be dropped, the next packet must be of this
   priority or higher to continue transmission.

   (This should not be set by the encoder implementation)

.. member:: size_t                encoder_packet.track_idx

   Audio track index.

   (This should not be set by the encoder implementation)

.. member:: obs_encoder_t         *encoder_packet.encoder

   Encoder object associated with this packet.

   (This should not be set by the encoder implementation)


Raw Frame Data Structure (encoder_frame)
----------------------------------------

.. type:: struct encoder_frame

   Raw frame data structure.

.. member:: uint8_t *encoder_frame.data[MAX_AV_PLANES]

   Raw video/audio data.

.. member:: uint32_t encoder_frame.linesize[MAX_AV_PLANES]

   Line size of each plane.

.. member:: uint32_t encoder_frame.frames

   Number of audio frames (if audio).

.. member:: int64_t encoder_frame.pts

   Presentation timestamp.


General Encoder Functions
-------------------------

.. function:: void obs_register_encoder(struct obs_encoder_info *info)

   Registers an encoder type.  Typically used in
   :c:func:`obs_module_load()` or in the program's initialization phase.

---------------------

.. function:: const char *obs_encoder_get_display_name(const char *id)

   Calls the :c:member:`obs_encoder_info.get_name` callback to get the
   translated display name of an encoder type.

   :param    id:            The encoder type string identifier
   :return:                 The translated display name of an encoder type

---------------------

.. function:: obs_encoder_t *obs_video_encoder_create(const char *id, const char *name, obs_data_t *settings, obs_data_t *hotkey_data)

   Creates a video encoder with the specified settings.
  
   The "encoder" context is used for encoding video/audio data.  Use
   obs_encoder_release to release it.

   :param   id:             The encoder type string identifier
   :param   name:           The desired name of the encoder.  If this is
                            not unique, it will be made to be unique
   :param   settings:       The settings for the encoder, or *NULL* if
                            none
   :param   hotkey_data:    Saved hotkey data for the encoder, or *NULL*
                            if none
   :return:                 A reference to the newly created encoder, or
                            *NULL* if failed

---------------------

.. function:: obs_encoder_t *obs_audio_encoder_create(const char *id, const char *name, obs_data_t *settings, size_t mixer_idx, obs_data_t *hotkey_data)

   Creates an audio encoder with the specified settings.
  
   The "encoder" context is used for encoding video/audio data.  Use
   :c:func:`obs_encoder_release()` to release it.

   :param   id:             The encoder type string identifier
   :param   name:           The desired name of the encoder.  If this is
                            not unique, it will be made to be unique
   :param   settings:       The settings for the encoder, or *NULL* if
                            none
   :param   mixer_idx:      The audio mixer index this audio encoder
                            will capture audio from
   :param   hotkey_data:    Saved hotkey data for the encoder, or *NULL*
                            if none
   :return:                 A reference to the newly created encoder, or
                            *NULL* if failed

---------------------

.. function:: void obs_encoder_addref(obs_encoder_t *encoder)
              void obs_encoder_release(obs_encoder_t *encoder)

   Adds/releases a reference to an encoder.  When the last reference is
   released, the encoder is destroyed.

---------------------

.. function:: obs_weak_encoder_t *obs_encoder_get_weak_encoder(obs_encoder_t *encoder)
              obs_encoder_t *obs_weak_encoder_get_encoder(obs_weak_encoder_t *weak)

   These functions are used to get a weak reference from a strong encoder
   reference, or a strong encoder reference from a weak reference.  If
   the encoder is destroyed, *obs_weak_encoder_get_encoder* will return
   *NULL*.

---------------------

.. function:: void obs_weak_encoder_addref(obs_weak_encoder_t *weak)
              void obs_weak_encoder_release(obs_weak_encoder_t *weak)

   Adds/releases a weak reference to an encoder.

---------------------

.. function:: void obs_encoder_set_name(obs_encoder_t *encoder, const char *name)

   Sets the name of an encoder.  If the encoder is not private and the
   name is not unique, it will automatically be given a unique name.

---------------------

.. function:: const char *obs_encoder_get_name(const obs_encoder_t *encoder)

   :return: The name of the encoder

---------------------

.. function:: const char *obs_encoder_get_codec(const obs_encoder_t *encoder)
              const char *obs_get_encoder_codec(const char *id)

   :return: The codec identifier of the encoder

---------------------

.. function:: enum obs_encoder_type obs_encoder_get_type(const obs_encoder_t *encoder)
              enum obs_encoder_type obs_get_encoder_type(const char *id)

   :return: The encoder type: OBS_ENCODER_VIDEO or OBS_ENCODER_AUDIO

---------------------

.. function:: void obs_encoder_set_scaled_size(obs_encoder_t *encoder, uint32_t width, uint32_t height)

   Sets the scaled resolution for a video encoder.  Set width and height to 0
   to disable scaling.  If the encoder is active, this function will trigger
   a warning, and do nothing.

---------------------

.. function:: bool obs_encoder_scaling_enabled(const obs_encoder_t *encoder)

   :return: *true* if pre-encode (CPU) scaling enabled, *false*
            otherwise.

---------------------

.. function:: uint32_t obs_encoder_get_width(const obs_encoder_t *encoder)
              uint32_t obs_encoder_get_height(const obs_encoder_t *encoder)

   :return: The width/height of a video encoder's encoded image

---------------------

.. function:: uint32_t obs_encoder_get_sample_rate(const obs_encoder_t *encoder)

   :return: The sample rate of an audio encoder's audio data

---------------------

.. function:: void obs_encoder_set_preferred_video_format(obs_encoder_t *encoder, enum video_format format)
              enum video_format obs_encoder_get_preferred_video_format(const obs_encoder_t *encoder)


   Sets the preferred video format for a video encoder.  If the encoder can use
   the format specified, it will force a conversion to that format if the
   obs output format does not match the preferred format.
  
   If the format is set to VIDEO_FORMAT_NONE, will revert to the default
   functionality of converting only when absolutely necessary.

---------------------

.. function:: obs_data_t *obs_encoder_defaults(const char *id)
              obs_data_t *obs_encoder_get_defaults(const obs_encoder_t *encoder)

   :return: An incremented reference to the encoder's default settings

---------------------

.. function:: obs_properties_t *obs_encoder_properties(const obs_encoder_t *encoder)
              obs_properties_t *obs_get_encoder_properties(const char *id)

   Use these functions to get the properties of an encoder or encoder
   type.  Properties are optionally used (if desired) to automatically
   generate user interface widgets to allow users to update settings.

   :return: The properties list for a specific existing encoder.  Free
            with :c:func:`obs_properties_destroy()`

---------------------

.. function:: void obs_encoder_update(obs_encoder_t *encoder, obs_data_t *settings)

   Updates the settings for this encoder context.

---------------------

.. function:: obs_data_t *obs_encoder_get_settings(const obs_encoder_t *encoder)

   :return: An incremented reference to the encoder's settings

---------------------

.. function:: signal_handler_t *obs_encoder_get_signal_handler(const obs_encoder_t *encoder)

   :return: The signal handler of the encoder

---------------------

.. function:: proc_handler_t *obs_encoder_get_proc_handler(const obs_encoder_t *encoder)

   :return: The procedure handler of the encoder

---------------------

.. function:: bool obs_encoder_get_extra_data(const obs_encoder_t *encoder, uint8_t **extra_data, size_t *size)

   Gets extra data (headers) associated with this encoder.

   :return: *true* if successful, *false* if no extra data associated
            with this encoder

---------------------

.. function:: void obs_encoder_set_video(obs_encoder_t *encoder, video_t *video)
              void obs_encoder_set_audio(obs_encoder_t *encoder, audio_t *audio)

   Sets the video/audio handler to use with this video/audio encoder.
   This is used to capture the raw video/audio data.

---------------------

.. function:: video_t *obs_encoder_video(const obs_encoder_t *encoder)
              audio_t *obs_encoder_audio(const obs_encoder_t *encoder)

   :return: The video/audio handler associated with this encoder, or
            *NULL* if none or not a matching encoder type

---------------------

.. function:: bool obs_encoder_active(const obs_encoder_t *encoder)

   :return: *true* if the encoder is active, *false* otherwise

---------------------


Functions used by encoders
--------------------------

.. function:: void obs_encoder_packet_ref(struct encoder_packet *dst, struct encoder_packet *src)
              void obs_encoder_packet_release(struct encoder_packet *packet)

   Adds or releases a reference to an encoder packet.

.. ---------------------------------------------------------------------------

.. _libobs/obs-encoder.h: https://github.com/jp9000/obs-studio/blob/master/libobs/obs-encoder.h
