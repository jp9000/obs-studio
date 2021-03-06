Frontends
=========

Initialization and Shutdown
---------------------------

To initialize libobs, you must call :c:func:`obs_startup()`,
:c:func:`obs_reset_video()`, and then :c:func:`obs_reset_audio()`.
After that, modules typically should be loaded.

You can load individual modules manually by calling
:c:func:`obs_open_module()`.  After loading, the
:c:func:`obs_open_module()` function, you must then call
:c:func:`obs_init_module()` to initialize the module.

You can load modules automatically via two functions:
:c:func:`obs_add_module_path()` and :c:func:`obs_load_all_modules()`.

After all plugin modules have been loaded, call
:c:func:`obs_post_load_modules()`.

Certain modules may optionally use a configuration storage directory,
which is set as a parameter to :c:func:`obs_startup()`.

When it's time to shut down the frontend, make sure to release all
references to any objects, free any data, and then call
:c:func:`obs_shutdown()`.  If for some reason any libobs objects have
not been released, they will be destroyed automatically and a warning
will be logged.

To detect if any general memory allocations have not been freed, call
the :c:func:`bnum_allocs()` to get the number of allocations remaining.
If the number remaining is above 0, there are memory leaks.

See :ref:`obs_init_shutdown_reference` for more information.


Reconfiguring Video
-------------------

Any time after initialization, video settings can be reconfigured by
calling :c:func:`obs_reset_video()` as long as no outputs are active.
Audio was originally intended to have this capability as well, but
currently is not able to be reset once initialized; libobs must be fully
shutdown in order to reconfigure audio settings.


Displays
--------

Displays as the name implies are used for display/preview panes.  To use
displays, you must have a native window handle or identifier to draw on.

First you must call :c:func:`obs_display_create()` to initialize the
display, then you must assign a draw callback with
:c:func:`obs_display_add_draw_callback()`.  If you need to remove a draw
callback, call :c:func:`obs_display_remove_draw_callback()` similarly.

When drawing, to draw the main preview window (if any), call
:c:func:`obs_render_main_texture()`.  If you need to render a specific
source on a secondary display, you can increment its "showing" state
with :c:func:`obs_source_inc_showing()` while it's showing in the
secondary display, draw it with :c:func:`obs_source_video_render()` in
the draw callback, then when it's no longer showing in the secondary
display, call :c:func:`obs_source_dec_showing()`.

If the display needs to be resized, call :c:func:`obs_display_resize()`.

If the display needs a custom background color other than black, call
:c:func:`obs_display_set_background_color()`.

If the display needs to be temporarily disabled, call
:c:func:`obs_display_set_enabled()` to disable, and
:c:func:`obs_display_enabled()` to get its enabled/disabled state.

Then call :c:func:`obs_display_destroy()` to destroy the display when
it's no longer needed.

*(Important note: do not use more than one display widget within the
hierarchy of the same base window; this will cause presentation stalls
on Macs.)*

For an example of how displays are used with Qt, see
`UI/qt-display.hpp`_ and `UI/qt-display.cpp`_.

See :ref:`display_reference` for more information.


Saving/Loading Objects and Object Management
--------------------------------------------

The frontend is generally expected to manage its own objects, however
for sources, there are some helper functions to allow easier
saving/loading all sources: :c:func:`obs_save_sources()` and
:c:func:`obs_load_sources()`.  With those functions, all sources that
aren't private will automatically be saved and loaded.  You can also
save/load individual sources manually by using
:c:func:`obs_save_source()` and :c:func:`obs_load_source()`.

*(Author's note: I should not have written those helper functions; the
downside is I had to add "private" sources that aren't saveable via the*
:c:func:`obs_source_create_private()` *function.  Just one of the many
minor design flaws that can occur during long-term development.)*

For outputs, encoders, and services, there are no helper functions, so
usually you'd get their settings individually and save them as json.
(See :c:func:`obs_output_get_settings()`).  You don't have to save each
object to different files individually; you'd save multiple objects
together in a bigger :c:type:`obs_data_t` object, then save that via
:c:func:`obs_data_save_json_safe()`, then load everything again via
:c:func:`obs_data_create_from_json_file_safe()`.


Signals
-------

The core, as well as scenes and sources, have a set of standard signals
that are used to determine when something happens or changes.

Typically the most important signals are the
:ref:`output_signal_handler_reference`:  the **start**, **stop**,
**starting**, **stopping**, **reconnect**, **reconnect_success**
signals in particular.

Most other signals for scenes/sources are optional if you are the only
thing controlling their state.  However, it's generally recommended to
watch most signals when possible for consistency.  See
:ref:`source_signal_handler_reference` and :ref:`scene_signal_reference`
for more information.

For example, let's say you wanted to connect a callback to the **stop**
signal of an output.  The **stop** signal has two parameters:  *output*
and *code*.  A callback for this signal would typically look something
like this:

.. code:: cpp

   static void output_stopped(void *my_data, calldata_t *cd)
   {
           obs_output_t *output = calldata_ptr(cd, "output");
           int code = calldata_int(cd, "code");

           [...]
   }

*(Note that callbacks are not thread-safe.)*

Then to connect it to the **stop** signal, you use the
:c:func:`signal_handler_connect()` with the callback.  In this case for
example:

.. code:: cpp

   signal_handler_t *handler = obs_output_get_signal_handler(output);
   signal_handler_connect(handler, "stop", output_stopped);


.. _displaying_sources:

Displaying Sources
------------------

Sources are displayed on stream/recording via :ref:`output_channels`
with the :c:func:`obs_set_output_source()` function.  There are 64
channels that you can assign sources to, which will draw on top of each
other in ascending index order.  Typically, a normal source shouldn't be
directly assigned with this function; you would use a scene or a
transition containing scenes.

To draw one or more sources together with a specific transform applied
to them, scenes are used.  To create a scene, you call
:c:func:`obs_scene_create()`.  Child sources are referenced using scene
items, and then specific transforms are applied to those scene items.
Scene items are not sources but containers for sources; the same source
can be referenced by multiple scene items within the same scene, or can
be referenced in multiple scenes.  To create a scene item that
references a source, you call :c:func:`obs_scene_add()`, which returns a
new reference to a scene item.

To change the transform of a scene item, you typically would call a
function like :c:func:`obs_sceneitem_set_pos()` to change its position,
:c:func:`obs_sceneitem_set_rot()` to change its rotation, or
:c:func:`obs_sceneitem_set_scale()` to change its scaling.  Scene items
can also force scaling in to a custom size constraint referred to as a
"bounding box"; a bounding box will force the source to be drawn at a
specific size and with specific scaling constraint within that size.  To
use a bounding box, you call the
:c:func:`obs_sceneitem_set_bounds_type()`,
:c:func:`obs_sceneitem_set_bounds()`, and
:c:func:`obs_sceneitem_set_bounds_alignment()`.  Though the easiest way
to handle everything related to transforms is to use the
:c:func:`obs_sceneitem_set_info()` and
:c:func:`obs_sceneitem_get_info()` functions.  See
:ref:`scene_item_reference` for all the functions related to scene
items.

Usually, a smooth transition between multiple scenes is required.  To do
this, transitions are used.  To create a transition, you use
:c:func:`obs_source_create()` or :c:func:`obs_source_create_private()`
like any other source.  Then, to activate a transition, you call
:c:func:`obs_transition_start()`.  When the transition is not active and
is only displaying one source, it performs a pass-through to the current
displaying source.  See :ref:`transitions` for more functions related to
using transitions.

The recommended way to set up your structure is to have a transition as
the source that is used as the main output source, then your scene as a
child of the transition, then your sources as children in the scene.
When you need to switch to a new scene, simply call
:c:func:`obs_transition_start()`.


Outputs, Encoders, and Services
-------------------------------

Outputs, encoders, and services are all used together, and managed a bit
differently than sources.  There currently is no global function to
save/load them, that must be accomplished manually for now via their
settings if needed.

Encoders are used with outputs that expect encoded data (which is almost
all typical outputs), such as standard file recording or streaming.

Services are used with outputs to a stream; the `RTMP output`_ is the
quintessential example of this.

Here's an example of how an output would be used with encoders and
services:

.. code:: cpp

   obs_encoder_set_video(my_h264_encoder, obs_get_video());
   obs_encoder_set_audio(my_aac_encoder, obs_get_audio());
   obs_output_set_video_encoder(my_output, my_h264_encoder);
   obs_output_set_audio_encoder(my_output, my_aac_encoder);
   obs_output_set_service(my_output, my_service); /* if a stream */
   obs_output_start(my_output);

Once the output has started successfully, it automatically starts
capturing the video and/or audio from the current video/audio output
(i.e. any sources that are assigned to the :ref:`output_channels`).

If the output fails to start up, it will send the **stop** signal with
an error code in the *code* parameter, possibly accompanied by a
translated error message stored that can be obtained via the
:c:func:`obs_output_get_last_error()` function.

.. --------------------------------------------------------------------

.. _RTMP Output: https://github.com/jp9000/obs-studio/blob/master/plugins/obs-outputs/rtmp-stream.c
.. _UI/qt-display.hpp: https://github.com/jp9000/obs-studio/blob/master/UI/qt-display.hpp
.. _UI/qt-display.cpp: https://github.com/jp9000/obs-studio/blob/master/UI/qt-display.cpp
