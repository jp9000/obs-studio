Output API Reference (obs_output_t)
===================================

Outputs allow the ability to output the currently rendering audio/video.
Streaming and recording are two common examples of outputs, but not the
only types of outputs.  Outputs can receive the raw data or receive
encoded data.  The `libobs/obs-output.h`_ file is the dedicated header
for implementing outputs

.. type:: obs_output_t

   A reference-counted output object.

.. type:: obs_weak_output_t

   A weak reference to an output object.

.. code:: cpp

   #include <obs.h>


Output Definition Structure (obs_output_info)
---------------------------------------------

.. type:: struct obs_output_info

   Output definition structure.

.. member:: const char *obs_output_info.id

   Unique string identifier for the source (required).

.. member:: uint32_t obs_output_info.flags

   Output capability flags (required).

   (Author's note: This should be renamed to "capability_flags")

   A bitwise OR combination of one or more of the following values:

   - **OBS_OUTPUT_VIDEO** - Can output video.

   - **OBS_OUTPUT_AUDIO** - Can output audio.

   - **OBS_OUTPUT_AV** - Combines OBS_OUTPUT_VIDEO and OBS_OUTPUT_AUDIO.

   - **OBS_OUTPUT_ENCODED** - Output is encoded.

     When this capability flag is used, the output must have encoders
     assigned to it via the :c:func:`obs_output_set_video_encoder()`
     and/or :c:func:`obs_output_set_audio_encoder()` functions in order
     to be started.

   - **OBS_OUTPUT_SERVICE** - Output requires a service object.

     When this capability flag is used, the output must have a service
     assigned to it via the :c:func:`obs_output_set_service()` function
     in order to be started.

     This is usually used with live streaming outputs that stream to
     specific services.

   - **OBS_OUTPUT_MULTI_TRACK** - Output supports multiple audio tracks.

     When this capability flag is used, specifies that this output
     supports multiple encoded audio tracks simultaneously.

   - **OBS_OUTPUT_CAN_PAUSE** - Output supports pausing.

     When this capability flag is used, the output supports pausing.
     When an output is paused, raw or encoded audio/video data will be
     halted when paused down to the exact point to the closest video
     frame.  Audio data will be correctly truncated down to the exact
     audio sample according to that video frame timing.

.. member:: const char *(*obs_output_info.get_name)(void *type_data)

   Get the translated name of the output type.

   :param  type_data:  The type_data variable of this structure
   :return:            The translated name of the output type

.. member:: void *(*obs_output_info.create)(obs_data_t *settings, obs_output_t *output)

   Creates the implementation data for the output.

   :param  settings: Settings to initialize the output with
   :param  output:   Output that this data is associated with
   :return:          The implementation data associated with this output

.. member:: void (*obs_output_info.destroy)(void *data)

   Destroys the implementation data for the output.

.. member:: bool (*obs_output_info.start)(void *data)

   Starts the output.  If needed, this function can spawn a thread,
   return *true* immediately, and then signal for failure later.

   :return: *true* if successful or deferring to a signal to indicate
            failure, *false* on failure to start

.. member:: void (*obs_output_info.stop)(void *data, uint64_t ts)

   Requests an output to stop at a specified time.  The *ts* parameter
   indicates when the stop should occur.  Output will actually stop when
   either the :c:func:`obs_output_end_data_capture()` or
   :c:func:`obs_output_signal_stop()` functions are called.  If *ts* is
   0, an immediate stop was requested.

   :param ts: The timestamp to stop.  If 0, the output should attempt to
              stop immediately rather than wait for any more data to
              process

.. member:: void (*obs_output_info.raw_video)(void *data, struct video_data *frame)

   This is called when the output receives raw video data.  Only applies
   to outputs that are not encoded.

   :param frame: The raw video frame

.. member:: void (*obs_output_info.raw_audio)(void *data, struct audio_data *frames)

   This is called when the output receives raw audio data.  Only applies
   to outputs that are not encoded.

   **This callback must be used with single-track raw outputs.**

   :param frames: The raw audio frames

.. member:: void (*obs_output_info.raw_audio2)(void *data, size_t idx, struct audio_data *frames)

   This is called when the output receives raw audio data.  Only applies
   to outputs that are not encoded.

   **This callback must be used with multi-track raw outputs.**

   :param idx:    The audio track index
   :param frames: The raw audio frames

.. member:: void (*obs_output_info.encoded_packet)(void *data, struct encoder_packet *packet)

   This is called when the output receives encoded video/audio data.
   Only applies to outputs that are encoded.  Packets will always be
   given in monotonic timestamp order.

   :param packet: The video or audio packet.  If NULL, an encoder error
                  occurred, and the output should call
                  :c:func:`obs_output_signal_stop()` with the error code
                  **OBS_OUTPUT_ENCODE_ERROR**.

.. member:: void (*obs_output_info.update)(void *data, obs_data_t *settings)

   Updates the settings for this output.

   (Optional)

   :param settings: New settings for this output

.. member:: void (*obs_output_info.get_defaults)(obs_data_t *settings)
            void (*obs_output_info.get_defaults2)(void *type_data, obs_data_t *settings)

   Sets the default settings for this output.

   (Optional)

   :param  settings:  Default settings.  Call obs_data_set_default*
                      functions on this object to set default setting
                      values

.. member:: obs_properties_t *(*obs_output_info.get_properties)(void *data)
            obs_properties_t *(*obs_output_info.get_properties2)(void *data, void *type_data)

   Gets the property information of this output.

   (Optional)

   :return: The properties of the output

.. member:: void (*obs_output_info.unused1)(void *data)

   This callback is no longer used.

.. member:: uint64_t (*obs_output_info.get_total_bytes)(void *data)

   Returns the number of total bytes processed by this output.

   (Optional)

   :return: Total bytes processed by this output since it started

.. member:: int (*obs_output_info.get_dropped_frames)(void *data)

   Returns the number of dropped frames.

   (Optional)

   :return: Number of dropped frames due to network congestion by this
            output since it started

.. member:: void *obs_output_info.type_data
            void (*obs_output_info.free_type_data)(void *type_data)

   Private data associated with this entry.  Note that this is not the
   same as the implementation data; this is used to differentiate
   between two different types if the same callbacks are used for more
   than one different type.

   (Optional)

.. member:: float (*obs_output_info.get_congestion)(void *data)

   This function is used to indicate how currently congested the output
   is.  Useful for visualizing how much data is backed up on streaming
   outputs.

   (Optional)

   :return: Current congestion value (0.0f..1.0f)

.. member:: int (*obs_output_info.get_connect_time_ms)(void *data)

   This function is used to determine how many milliseconds it took to
   connect to its current server.

   (Optional)

   :return: Milliseconds it took to connect to its current server

.. member:: const char *obs_output_info.encoded_video_codecs
            const char *obs_output_info.encoded_audio_codecs

   This variable specifies which codecs are supported by an encoded
   output, separated by semicolon.

   (Optional, though recommended)

.. _output_signal_handler_reference:

Output Signals
--------------

**start** (ptr output)

   Called when the output starts.

**stop** (ptr output, int code)

   Called when the output stops.

   :Parameters: - **code** - Can be one of the following values:

                  | OBS_OUTPUT_SUCCESS        - Successfully stopped
                  | OBS_OUTPUT_BAD_PATH       - The specified path was invalid
                  | OBS_OUTPUT_CONNECT_FAILED - Failed to connect to a server
                  | OBS_OUTPUT_INVALID_STREAM - Invalid stream path
                  | OBS_OUTPUT_ERROR          - Generic error
                  | OBS_OUTPUT_DISCONNECTED   - Unexpectedly disconnected
                  | OBS_OUTPUT_UNSUPPORTED    - The settings, video/audio format, or codecs are unsupported by this output
                  | OBS_OUTPUT_NO_SPACE       - Ran out of disk space
                  | OBS_OUTPUT_ENCODE_ERROR   - Encoder error

**pause** (ptr output)

   Called when the output has been paused.

**unpause** (ptr output)

   Called when the output has been unpaused.

**starting** (ptr output)

   Called when the output is starting.

**stopping** (ptr output)

   Called when the output is stopping.

**activate** (ptr output)

   Called when the output activates (starts capturing data).

**deactivate** (ptr output)

   Called when the output deactivates (stops capturing data).

**reconnect** (ptr output)

   Called when the output is reconnecting.

**reconnect_success** (ptr output)

   Called when the output has successfully reconnected.

General Output Functions
------------------------

.. function:: void obs_register_output(struct obs_output_info *info)

   Registers an output type.  Typically used in
   :c:func:`obs_module_load()` or in the program's initialization phase.

---------------------

.. function:: const char *obs_output_get_display_name(const char *id)

   Calls the :c:member:`obs_output_info.get_name` callback to get the
   translated display name of an output type.

   :param    id:            The output type string identifier
   :return:                 The translated display name of an output type

---------------------

.. function:: obs_output_t *obs_output_create(const char *id, const char *name, obs_data_t *settings, obs_data_t *hotkey_data)

   Creates an output with the specified settings.
  
   The "output" context is used for anything related to outputting the
   final video/audio mix (E.g. streaming or recording).  Use
   obs_output_release to release it.

   :param   id:             The output type string identifier
   :param   name:           The desired name of the output.  If this is
                            not unique, it will be made to be unique
   :param   settings:       The settings for the output, or *NULL* if
                            none
   :param   hotkey_data:    Saved hotkey data for the output, or *NULL*
                            if none
   :return:                 A reference to the newly created output, or
                            *NULL* if failed

---------------------

.. function:: void obs_output_addref(obs_output_t *output)
              void obs_output_release(obs_output_t *output)

   Adds/releases a reference to an output.  When the last reference is
   released, the output is destroyed.

---------------------

.. function:: obs_weak_output_t *obs_output_get_weak_output(obs_output_t *output)
              obs_output_t *obs_weak_output_get_output(obs_weak_output_t *weak)

   These functions are used to get a weak reference from a strong output
   reference, or a strong output reference from a weak reference.  If
   the output is destroyed, *obs_weak_output_get_output* will return
   *NULL*.

---------------------

.. function:: void obs_weak_output_addref(obs_weak_output_t *weak)
              void obs_weak_output_release(obs_weak_output_t *weak)

   Adds/releases a weak reference to an output.

---------------------

.. function:: const char *obs_output_get_name(const obs_output_t *output)

   :return: The name of the output

---------------------

.. function:: bool obs_output_start(obs_output_t *output)

   Starts the output.

   :return: *true* if output successfully started, *false* otherwise.  If
            the output failed to start,
            :c:func:`obs_output_get_last_error()` may contain a specific
            error string related to the reason

---------------------

.. function:: void obs_output_stop(obs_output_t *output)

   Requests the output to stop.  The output will wait until all data is
   sent up until the time the call was made, then when the output has
   successfully stopped, it will send the "stop" signal.  See
   :ref:`output_signal_handler_reference` for more information on output
   signals.

---------------------

.. function:: void obs_output_set_delay(obs_output_t *output, uint32_t delay_sec, uint32_t flags)

   Sets the current output delay, in seconds (if the output supports delay)
  
   If delay is currently active, it will set the delay value, but will not
   affect the current delay, it will only affect the next time the output is
   activated.

   :param delay_sec: Amount to delay the output, in seconds
   :param flags:      | Can be 0 or a combination of one of the following values:
                      | OBS_OUTPUT_DELAY_PRESERVE - On reconnection, start where it left of on reconnection.  Note however that this option will consume extra memory to continually increase delay while waiting to reconnect

---------------------

.. function:: uint32_t obs_output_get_delay(const obs_output_t *output)

   Gets the currently set delay value, in seconds.

---------------------

.. function:: uint32_t obs_output_get_active_delay(const obs_output_t *output)

   If delay is active, gets the currently active delay value, in
   seconds.  The active delay can increase if the
   OBS_OUTPUT_DELAY_PRESERVE flag was set when setting a delay.

---------------------

.. function:: void obs_output_force_stop(obs_output_t *output)

   Attempts to get the output to stop immediately without waiting for
   data to send.

---------------------

.. function:: bool obs_output_active(const obs_output_t *output)

   :return: *true* if the output is currently active, *false* otherwise

---------------------

.. function:: obs_data_t *obs_output_defaults(const char *id)

   :return: An incremented reference to the output's default settings

---------------------

.. function:: obs_properties_t *obs_output_properties(const obs_output_t *output)
              obs_properties_t *obs_get_output_properties(const char *id)

   Use these functions to get the properties of an output or output
   type.  Properties are optionally used (if desired) to automatically
   generate user interface widgets to allow users to update settings.

   :return: The properties list for a specific existing output.  Free
            with :c:func:`obs_properties_destroy()`

---------------------

.. function:: void obs_output_update(obs_output_t *output, obs_data_t *settings)

   Updates the settings for this output context.

---------------------

.. function:: bool obs_output_can_pause(const obs_output_t *output)

   :return: *true* if the output can be paused, *false* otherwise

---------------------

.. function:: bool obs_output_pause(obs_output_t *output, bool pause)

   Pause an output (if supported by the output).

   :return: *true* if the output was paused successfully, *false*
            otherwise

---------------------

.. function:: bool obs_output_paused(const obs_output_t *output)

   :return: *true* if the output is paused, *false* otherwise

---------------------

.. function:: obs_data_t *obs_output_get_settings(const obs_output_t *output)

   :return: An incremented reference to the output's settings

---------------------

.. function:: signal_handler_t *obs_output_get_signal_handler(const obs_output_t *output)

   :return: The signal handler of the output

---------------------

.. function:: proc_handler_t *obs_output_get_proc_handler(const obs_output_t *output)

   :return: The procedure handler of the output

---------------------

.. function:: void obs_output_set_media(obs_output_t *output, video_t *video, audio_t *audio)

   Sets the current video/audio handlers for the output (typically
   :c:func:`obs_get_video()` and :c:func:`obs_get_audio()`).  Only used
   with raw outputs so they can catch the raw video/audio frames.

---------------------

.. function:: video_t *obs_output_video(const obs_output_t *output)
              audio_t *obs_output_audio(const obs_output_t *output)

   Gets the current video/audio handlers for the output.

---------------------

.. function:: void obs_output_set_mixer(obs_output_t *output, size_t mixer_idx)
              size_t obs_output_get_mixer(const obs_output_t *output)

   Sets/gets the current audio mixer for non-encoded outputs.  For
   multi-track outputs, this would be the equivalent of setting the mask
   only for the specified mixer index.

---------------------

.. function:: void obs_output_set_mixers(obs_output_t *output, size_t mixers)
              size_t obs_output_get_mixers(const obs_output_t *output)

   Sets/gets the current audio mixers (via mask) for non-encoded
   multi-track outputs.  If used with single-track outputs, the
   single-track output will use either the first set mixer track in the
   bitmask, or the first track if none is set in the bitmask.

---------------------

.. function:: void obs_output_set_video_encoder(obs_output_t *output, obs_encoder_t *encoder)
              void obs_output_set_audio_encoder(obs_output_t *output, obs_encoder_t *encoder, size_t idx)

   Sets the video/audio encoders for an encoded output.

   :param encoder: The video/audio encoder
   :param idx:     The audio encoder index if the output supports
                   multiple audio streams at once

---------------------

.. function:: obs_encoder_t *obs_output_get_video_encoder(const obs_output_t *output)
              obs_encoder_t *obs_output_get_audio_encoder(const obs_output_t *output, size_t idx)

   Gets the video/audio encoders for an encoded output.

   :param idx:     The audio encoder index if the output supports
                   multiple audio streams at once
   :return:        The video/audio encoder.  The reference is not
                   incremented

---------------------

.. function:: void obs_output_set_service(obs_output_t *output, obs_service_t *service)
              obs_service_t *obs_output_get_service(const obs_output_t *output)

   Sets/gets the service for outputs that require services (such as RTMP
   outputs).  *obs_output_get_service* does not return an incremented
   reference.

---------------------

.. function:: void obs_output_set_reconnect_settings(obs_output_t *output, int retry_count, int retry_sec);

   Sets the auto-reconnect settings for outputs that support it.  The
   retry time will double on each retry to prevent overloading services.

   :param retry_count: Maximum retry count.  Set to 0 to disable
                       reconnecting
   :param retry_sec:   Starting retry wait duration, in seconds

---------------------

.. function:: uint64_t obs_output_get_total_bytes(const obs_output_t *output)

   :return: Total bytes sent/processed

---------------------

.. function:: int obs_output_get_frames_dropped(const obs_output_t *output)

   :return: Number of frames that were dropped due to network congestion

---------------------

.. function:: int obs_output_get_total_frames(const obs_output_t *output)

   :return: Total frames sent/processed

---------------------

.. function:: void obs_output_set_preferred_size(obs_output_t *output, uint32_t width, uint32_t height)

   Sets the preferred scaled resolution for this output.  Set width and height
   to 0 to disable scaling.
  
   If this output uses an encoder, it will call obs_encoder_set_scaled_size on
   the encoder before the stream is started.  If the encoder is already active,
   then this function will trigger a warning and do nothing.

---------------------

.. function:: uint32_t obs_output_get_width(const obs_output_t *output)
              uint32_t obs_output_get_height(const obs_output_t *output)

   :return: The width/height of the output

---------------------

.. function:: float obs_output_get_congestion(obs_output_t *output)

   :return: The congestion value.  This value is used to visualize the
            current congestion of a network output.  For example, if
            there is no congestion, the value will be 0.0f, if it's
            fully congested, the value will be 1.0f

---------------------

.. function:: int obs_output_get_connect_time_ms(obs_output_t *output)

   :return: How long the output took to connect to a server, in
            milliseconds

---------------------

.. function:: bool obs_output_reconnecting(const obs_output_t *output)

   :return: *true* if the output is currently reconnecting to a server,
            *false* otherwise

---------------------

.. function:: const char *obs_output_get_supported_video_codecs(const obs_output_t *output)
              const char *obs_output_get_supported_audio_codecs(const obs_output_t *output)

   :return: Supported video/audio codecs of an encoded output, separated
            by semicolen

---------------------

.. function:: uint32_t obs_output_get_flags(const obs_output_t *output)
              uint32_t obs_get_output_flags(const char *id)

   :return: The output capability flags

---------------------

Functions used by outputs
-------------------------

.. function:: void obs_output_set_last_error(obs_output_t *output, const char *message)
              const char *obs_output_get_last_error(obs_output_t *output)

   Sets/gets the translated error message that is presented to a user in
   case of disconnection, inability to connect, etc.

---------------------

.. function:: void obs_output_set_video_conversion(obs_output_t *output, const struct video_scale_info *conversion)

   Optionally sets the video conversion information.  Only used by raw
   outputs.

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
   
   enum video_colorspace {
           VIDEO_CS_DEFAULT,
           VIDEO_CS_601,
           VIDEO_CS_709,
           VIDEO_CS_SRGB,
   };
   
   enum video_range_type {
           VIDEO_RANGE_DEFAULT,
           VIDEO_RANGE_PARTIAL,
           VIDEO_RANGE_FULL
   };
   
   struct video_scale_info {
           enum video_format     format;
           uint32_t              width;
           uint32_t              height;
           enum video_range_type range;
           enum video_colorspace colorspace;
   };

---------------------

.. function:: void obs_output_set_audio_conversion(obs_output_t *output, const struct audio_convert_info *conversion)

   Optionally sets the audio conversion information.  Only used by raw
   outputs.

   Relevant data types used with this function:

.. code:: cpp

   enum audio_format {
           AUDIO_FORMAT_UNKNOWN,
   
           AUDIO_FORMAT_U8BIT,
           AUDIO_FORMAT_16BIT,
           AUDIO_FORMAT_32BIT,
           AUDIO_FORMAT_FLOAT,
   
           AUDIO_FORMAT_U8BIT_PLANAR,
           AUDIO_FORMAT_16BIT_PLANAR,
           AUDIO_FORMAT_32BIT_PLANAR,
           AUDIO_FORMAT_FLOAT_PLANAR,
   };
   
   enum speaker_layout {
           SPEAKERS_UNKNOWN,
           SPEAKERS_MONO,
           SPEAKERS_STEREO,
           SPEAKERS_2POINT1,
           SPEAKERS_QUAD,
           SPEAKERS_4POINT1,
           SPEAKERS_5POINT1,
           SPEAKERS_5POINT1_SURROUND,
           SPEAKERS_7POINT1,
           SPEAKERS_7POINT1_SURROUND,
           SPEAKERS_SURROUND,
   };
   
   struct audio_convert_info {
           uint32_t            samples_per_sec;
           enum audio_format   format;
           enum speaker_layout speakers;
   };

---------------------

.. function:: bool obs_output_can_begin_data_capture(const obs_output_t *output, uint32_t flags)

   Determines whether video/audio capture (encoded or raw) is able to
   start.  Call this before initializing any output data to ensure that
   the output can start.

   :param flags: Set to 0 to initialize both audio/video, otherwise a
                 bitwise OR combination of OBS_OUTPUT_VIDEO and/or
                 OBS_OUTPUT_AUDIO
   :return:      *true* if data capture can begin

---------------------

.. function:: bool obs_output_initialize_encoders(obs_output_t *output, uint32_t flags)

   Initializes any encoders/services associated with the output.  This
   must be called for encoded outputs before calling
   :c:func:`obs_output_begin_data_capture()`.

   :param flags: Set to 0 to initialize both audio/video, otherwise a
                 bitwise OR combination of OBS_OUTPUT_VIDEO and/or
                 OBS_OUTPUT_AUDIO
   :return:      *true* if successful, *false* otherwise

---------------------

.. function:: bool obs_output_begin_data_capture(obs_output_t *output, uint32_t flags)

   Begins data capture from raw media or encoders.  This is typically
   when the output actually activates (starts) internally.  Video/audio
   data will start being sent to the callbacks of the output.

   :param flags: Set to 0 to initialize both audio/video, otherwise a
                 bitwise OR combination of OBS_OUTPUT_VIDEO and/or
                 OBS_OUTPUT_AUDIO
   :return:      *true* if successful, *false* otherwise.  Typically the
                 return value does not need to be checked if
                 :c:func:`obs_output_can_begin_data_capture()` was
                 called

---------------------

.. function:: void obs_output_end_data_capture(obs_output_t *output)

   Ends data capture of an output.  This is typically when the output
   actually intentionally deactivates (stops).  Video/audio data will
   stop being sent to the callbacks of the output.  The output will
   trigger the "stop" signal with the OBS_OUTPUT_SUCCESS code to
   indicate that the output has stopped successfully.  See
   :ref:`output_signal_handler_reference` for more information on output
   signals.

---------------------

.. function:: void obs_output_signal_stop(obs_output_t *output, int code)

   Ends data capture of an output with an output code, indicating that
   the output stopped unexpectedly.  This is typically used if for
   example the server was disconnected for some reason, or if there was
   an error saving to file.  The output will trigger the "stop" signal
   with the the desired code to indicate that the output has stopped
   successfully.  See :ref:`output_signal_handler_reference` for more
   information on output signals.

   :c:func:`obs_output_set_last_error()` may be used in conjunction with
   these error codes to optionally relay more detailed error information
   to the user

   :param code: | Can be one of the following values:
                | OBS_OUTPUT_SUCCESS        - Successfully stopped
                | OBS_OUTPUT_BAD_PATH       - The specified path was invalid
                | OBS_OUTPUT_CONNECT_FAILED - Failed to connect to a server
                | OBS_OUTPUT_INVALID_STREAM - Invalid stream path
                | OBS_OUTPUT_ERROR          - Generic error
                | OBS_OUTPUT_DISCONNECTED   - Unexpectedly disconnected
                | OBS_OUTPUT_UNSUPPORTED    - The settings, video/audio format, or codecs are unsupported by this output
                | OBS_OUTPUT_NO_SPACE       - Ran out of disk space

---------------------

.. function:: uint64_t obs_output_get_pause_offset(obs_output_t *output)

   Returns the current pause offset of the output.  Used with raw
   outputs to calculate system timestamps when using calculated
   timestamps (see FFmpeg output for an example).

.. ---------------------------------------------------------------------------

.. _libobs/obs-output.h: https://github.com/jp9000/obs-studio/blob/master/libobs/obs-output.h
