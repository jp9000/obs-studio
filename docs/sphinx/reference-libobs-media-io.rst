Media I/O API Reference (libobs/media-io)
=========================================

.. code:: cpp

   #include <obs.h>


Video Handler
-------------

.. type:: video_t

   Video output handler object

---------------------

.. type:: enum video_format

   Video format.  Can be one of the following values:

   - VIDEO_FORMAT_I420
   - VIDEO_FORMAT_NV12

   - VIDEO_FORMAT_YVYU
   - VIDEO_FORMAT_YUY2
   - VIDEO_FORMAT_UYVY

   - VIDEO_FORMAT_RGBA
   - VIDEO_FORMAT_BGRA
   - VIDEO_FORMAT_BGRX
   - VIDEO_FORMAT_Y800

   - VIDEO_FORMAT_I444

---------------------

.. type:: enum video_colorspace

   YUV color space.  Can be one of the following values:

   - VIDEO_CS_DEFAULT - Equivalent to VIDEO_CS_709
   - VIDEO_CS_601     - 601 color space
   - VIDEO_CS_709     - 709 color space
   - VIDEO_CS_SRGB    - sRGB color space

---------------------

.. type:: enum video_range_type

   YUV color range.

   - VIDEO_RANGE_DEFAULT - Equivalent to VIDEO_RANGE_PARTIAL
   - VIDEO_RANGE_PARTIAL - Partial range
   - VIDEO_RANGE_FULL    - Full range

---------------------

.. type:: struct video_data

   Video frame structure.

.. member:: uint8_t           *video_data.data[MAX_AV_PLANES]
.. member:: uint32_t          video_data.linesize[MAX_AV_PLANES]
.. member:: uint64_t          video_data.timestamp

---------------------

.. type:: struct video_output_info

   Video output handler information

.. member:: const char        *video_output_info.name
.. member:: enum video_format video_output_info.format
.. member:: uint32_t          video_output_info.fps_num
.. member:: uint32_t          video_output_info.fps_den
.. member:: uint32_t          video_output_info.width
.. member:: uint32_t          video_output_info.height
.. member:: size_t            video_output_info.cache_size
.. member:: enum video_colorspace video_output_info.colorspace
.. member:: enum video_range_type video_output_info.range

---------------------

.. function:: enum video_format video_format_from_fourcc(uint32_t fourcc)

   Converts a fourcc value to a video format.

   :param forcecc: Fourcc value
   :return:        Video format

---------------------

.. function:: bool video_format_get_parameters(enum video_colorspace color_space, enum video_range_type range, float matrix[16], float min_range[3], float max_range[3])

   Converts a color space/range to matrix/min/max values.

   :param color_space: Color space to convert
   :param range:       Color range to convert
   :param matrix:      Pointer to the matrix
   :param min_range:   Pointer to get the minimum range value
   :param max_range:   Pointer to get the maximum range value

---------------------

.. function:: bool video_output_connect(video_t *video, const struct video_scale_info *conversion, void (*callback)(void *param, struct video_data *frame), void *param)

   Connects a raw video callback to the video output handler.

   :param video:    Video output handler object
   :param callback: Callback to receive video data
   :param param:    Private data to pass to the callback

---------------------

.. function:: void video_output_disconnect(video_t *video, void (*callback)(void *param, struct video_data *frame), void *param)

   Disconnects a raw video callback from the video output handler.

   :param video:    Video output handler object
   :param callback: Callback
   :param param:    Private data

---------------------

.. function:: const struct video_output_info *video_output_get_info(const video_t *video)

   Gets the full video information of the video output handler.

   :param video: Video output handler object
   :return:      Video output info structure pointer

---------------------

.. function:: uint64_t video_output_get_frame_time(const video_t *video)

   Gets the frame interval of the video output handler.

   :param video: Video output handler object
   :return:      Video frame interval in nanoseconds

---------------------

.. function:: enum video_format video_output_get_format(const video_t *video)

   Gets the video format of the video output handler.

   :param video: Video output handler object
   :return:      Video format

---------------------

.. function:: uint32_t video_output_get_width(const video_t *video)
.. function:: uint32_t video_output_get_height(const video_t *video)

   Gets the width/height of the video output handler.

   :param video: Video output handler object
   :return:      Width/height

---------------------

.. function:: double video_output_get_frame_rate(const video_t *video)

   Gets the frame rate (as a floating point) of the video output
   handler.

   :param video: Video output handler object
   :return:      Frame rate

---------------------

.. function:: uint32_t video_output_get_skipped_frames(const video_t *video)

   Gets the skipped frame count of the video output handler.

   :param video: Video output handler object
   :return:      Skipped frame count

---------------------

.. function:: uint32_t video_output_get_total_frames(const video_t *video)

   Gets the total frames processed of the video output handler.

   :param video: Video output handler object
   :return:      Total frames processed

---------------------


Audio Handler
-------------

.. type:: audio_t

---------------------

.. type:: enum audio_format

   Audio format.  Can be one of the following values:

   - AUDIO_FORMAT_UNKNOWN
   - AUDIO_FORMAT_U8BIT
   - AUDIO_FORMAT_16BIT
   - AUDIO_FORMAT_32BIT
   - AUDIO_FORMAT_FLOAT
   - AUDIO_FORMAT_U8BIT_PLANAR
   - AUDIO_FORMAT_16BIT_PLANAR
   - AUDIO_FORMAT_32BIT_PLANAR
   - AUDIO_FORMAT_FLOAT_PLANAR

---------------------

.. type:: enum speaker_layout

   Speaker layout.  Can be one of the following values:

   - SPEAKERS_UNKNOWN
   - SPEAKERS_MONO
   - SPEAKERS_STEREO
   - SPEAKERS_2POINT1
   - SPEAKERS_QUAD
   - SPEAKERS_4POINT1
   - SPEAKERS_5POINT1
   - SPEAKERS_5POINT1_SURROUND
   - SPEAKERS_7POINT1
   - SPEAKERS_7POINT1_SURROUND
   - SPEAKERS_SURROUND

---------------------

.. type:: struct audio_data

   Audio data structure.

.. member:: uint8_t             *audio_data.data[MAX_AV_PLANES]
.. member:: uint32_t            audio_data.frames
.. member:: uint64_t            audio_data.timestamp

---------------------

.. type:: struct audio_output_data
.. member:: float               *audio_output_data.data[MAX_AUDIO_CHANNELS]

---------------------

.. type:: struct audio_output_info
.. member:: const char             *audio_output_info.name
.. member:: uint32_t               audio_output_info.samples_per_sec
.. member:: enum audio_format      audio_output_info.format
.. member:: enum speaker_layout    audio_output_info.speakers
.. member:: audio_input_callback_t audio_output_info.input_callback
.. member:: void                   *audio_output_info.input_param

---------------------

.. type:: struct audio_convert_info
.. member:: uint32_t            audio_convert_info.samples_per_sec
.. member:: enum audio_format   audio_convert_info.format
.. member:: enum speaker_layout audio_convert_info.speakers

---------------------

.. type:: typedef bool (*audio_input_callback_t)(void *param, uint64_t start_ts, uint64_t end_ts, uint64_t *new_ts, uint32_t active_mixers, struct audio_output_data *mixes)

   Audio input callback (typically used internally).

---------------------

.. function:: uint32_t get_audio_channels(enum speaker_layout speakers)

   Converts a speaker layout to its audio channel count.

   :param speakers: Speaker layout enumeration
   :return:         Channel count

---------------------

.. function:: size_t get_audio_bytes_per_channel(enum audio_format format)

   Gets the audio bytes per channel for a specific audio format.

   :param format: Audio format
   :return:       Bytes per channel

---------------------

.. function:: bool is_audio_planar(enum audio_format format)

   Returns whether the audio format is a planar format.

   :param format: Audio format
   :return:       *true* if audio is planar, *false* otherwise

---------------------

.. function:: size_t get_audio_planes(enum audio_format format, enum speaker_layout speakers)

   Gets the number of audio planes for a specific audio format and
   speaker layout.

   :param format:   Audio format
   :param speakers: Speaker layout
   :return:         Number of audio planes

---------------------

.. function:: size_t get_audio_size(enum audio_format format, enum speaker_layout speakers, uint32_t frames)

   Gets the audio block size for a specific frame out with the given
   format and speaker layout.

   :param format:   Audio format
   :param speakers: Speaker layout
   :param frames:   Audio frame count
   :return:         Audio block size

---------------------

.. function:: uint64_t audio_frames_to_ns(size_t sample_rate, uint64_t frames)

   Helper function to convert a specific number of audio frames to
   nanoseconds based upon its sample rate.

   :param sample_rate: Sample rate
   :param frames:      Frame count
   :return:            Nanoseconds

---------------------

.. function:: uint64_t ns_to_audio_frames(size_t sample_rate, uint64_t ns)

   Helper function to convert a specific number of nanoseconds to audio
   frame count based upon its sample rate.

   :param sample_rate: Sample rate
   :param ns:          Nanoseconds
   :return:            Frame count

---------------------

.. type:: typedef void (*audio_output_callback_t)(void *param, size_t mix_idx, struct audio_data *data)

   Audio output callback.  Typically used internally.

---------------------

.. function:: bool audio_output_connect(audio_t *audio, size_t mix_idx, const struct audio_convert_info *conversion, audio_output_callback_t callback, void *param)

   Connects a raw audio callback to the audio output handler.
   Optionally allows audio conversion if necessary.

   :param audio:      Audio output handler object
   :param mix_idx:    Mix index to get raw audio from
   :param conversion: Audio conversion information, or *NULL* for no
                      conversion
   :param callback:   Raw audio callback
   :param param:      Private data to pass to the callback

---------------------

.. function:: void audio_output_disconnect(audio_t *audio, size_t mix_idx, audio_output_callback_t callback, void *param)

   Disconnects a raw audio callback from the audio output handler.

   :param audio:      Audio output handler object
   :param mix_idx:    Mix index to get raw audio from
   :param callback:   Raw audio callback
   :param param:      Private data to pass to the callback

---------------------

.. function:: size_t audio_output_get_block_size(const audio_t *audio)

   Gets the audio block size of an audio output handler.

   :param audio: Audio output handler object
   :return:      Audio block size

---------------------

.. function:: size_t audio_output_get_planes(const audio_t *audio)

   Gets the plane count of an audio output handler.

   :param audio: Audio output handler object
   :return:      Audio plane count

---------------------

.. function:: size_t audio_output_get_channels(const audio_t *audio)

   Gets the channel count of an audio output handler.

   :param audio: Audio output handler object
   :return:      Audio channel count

---------------------

.. function:: uint32_t audio_output_get_sample_rate(const audio_t *audio)

   Gets the sample rate of an audio output handler.

   :param audio: Audio output handler object
   :return:      Audio sample rate

---------------------

.. function:: const struct audio_output_info *audio_output_get_info(const audio_t *audio)

   Gets all audio information for an audio output handler.

   :param audio: Audio output handler object
   :return:      Pointer to audio output information structure

---------------------


Resampler
---------

FFmpeg wrapper to resample audio.

.. type:: typedef struct audio_resampler audio_resampler_t

---------------------

.. type:: struct resample_info
.. member:: uint32_t            resample_info.samples_per_sec
.. member:: enum audio_format   resample_info.format
.. member:: enum speaker_layout resample_info.speakers

---------------------

.. function:: audio_resampler_t *audio_resampler_create(const struct resample_info *dst, const struct resample_info *src)

   Creates an audio resampler.

   :param dst: Destination audio information
   :param src: Source audio information
   :return:    Audio resampler object

---------------------

.. function:: void audio_resampler_destroy(audio_resampler_t *resampler)

   Destroys an audio resampler.

   :param resampler: Audio resampler object

---------------------

.. function:: bool audio_resampler_resample(audio_resampler_t *resampler, uint8_t *output[], uint32_t *out_frames, uint64_t *ts_offset, const uint8_t *const input[], uint32_t in_frames)

   Resamples audio frames.

   :param resampler:   Audio resampler object
   :param output:      Pointer to receive converted audio frames
   :param out_frames:  Pointer to receive converted audio frame count
   :param ts_offset:   Pointer to receive timestamp offset (in
                       nanoseconds)
   :param const input: Input frames to convert
   :param in_frames:   Input frame count
