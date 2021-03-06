Python/Lua Scripting
====================

Scripting (21.0+) adds support for Python 3 and Luajit 2 (which is
roughly equivalent to Lua 5.2), allowing the ability to quickly extend,
add features, or automate the program without needing to build a native
plugin module.

Scripting can be accessed in OBS Studio via the Tools menu -> Scripts
option, which will bring up the scripting dialog.  Scripts can be added,
removed, and reloaded in real time while the program is running.

**NOTE:** On windows, currently only Python 3.6 is supported.  To use
Python on windows, you must download and install Python 3.6.x "x86-64"
for OBS 64bit (64bit is the default), or Python 3.6.x "x86" if using OBS
32bit.  Then, in the scripting dialog, you must set the path to the
Python 3.6.x install in the "Python Settings" tab.

All API bindings are provided through the **obspython** module in
Python, and the **obslua** module in Lua.

Certain functions have been changed/replaced in order to provide script
callbacks, see :ref:`other_script_differences` for more information.

**WARNING:** Because bindings to the entire API are provided, it is
possible to leak memory or crash the program with an improperly-written
script.  Please exercise caution when making scripts and check the
memory leak counter in your log file to make sure scripts you write
aren't leaking memory.  **Please treat the API bindings as though you
were writing a C program:  read the documentation for functions you use,
and release/destroy objects you reference or create via the API.**


Script Function Exports
-----------------------

There are a number of global functions that scripts can optionally
provide:

.. py:function:: script_load(settings)

   Called on script startup with specific settings associated with the
   script.  The *settings* parameter provided is not typically used for
   settings that are set by the user; instead the parameter is used for
   any extra internal settings data that may be used in the script.

   :param settings: Settings associated with the script.

.. py:function:: script_unload()

   Called when the script is being unloaded.

.. py:function:: script_save(settings)

   Called when the script is being saved.  This is not necessary for
   settings that are set by the user; instead this is used for any
   extra internal settings data that may be used in the script.

   :param settings: Settings associated with the script.

.. py:function:: script_defaults(settings)

   Called to set default settings (if any) associated with the script.
   You would typically call :ref:`obs_data_default_funcs` for the
   on the settings in order to set its default values.

   :param settings: Settings associated with the script.

.. py:function:: script_update(settings)

   Called when the script's settings (if any) have been changed by the
   user.

   :param settings: Settings associated with the script.

.. py:function:: script_properties()

   Called to define user properties associated with the script.  These
   properties are used to define how to show settings properties to a
   user.

   :return: obs_properties_t object created via
            :c:func:`obs_properties_create()`.

.. py:function:: script_tick(seconds)

   Called every frame in case per-frame processing is needed.  If a
   timer is needed, please use :ref:`scripting_timers` instead, as
   timers are more efficient if all that's needed is basic timer
   functionality.  Using this function in Python is not recommended due
   to the global interpreter lock of Python.

   :param seconds: Seconds passed since previous frame.


Getting the Current Script's Path
---------------------------------

There is a function you can use to get the current script's path.  This
function is automatically implemented in to each script before the
script is loaded, and is part of the script's namespace, not
obslua/obspython:

.. py:function:: script_path()

   :return: The path to the script.


.. _scripting_timers:

Script Timers
-------------

Script timers provide an efficient means of providing timer callbacks
without necessarily having to lock scripts/interpreters every frame.
(These functions are part of the obspython/obslua modules/namespaces).

.. py:function:: timer_add(callback, milliseconds)

    Adds an timer callback which triggers every *millseconds*.

.. py:function:: timer_remove(callback)

    Removes a timer callback.  (Note: You can also use
    :py:func:`remove_current_callback()` to terminate the timer from the
    timer callback)


Script Sources (Lua Only)
-------------------------

It is possible to register sources in Lua.  To do so, create a table,
and define its keys the same way you would define an
:c:type:`obs_source_info` structure:

.. code:: lua

    local info = {}
    info.id = "my_source_id"
    info.type = obslua.OBS_SOURCE_TYPE_INPUT
    info.output_flags = obslua.OBS_SOURCE_VIDEO

    info.get_name = function()
            return "My Source"
    end

    info.create = function(settings, source)
            -- typically source data would be stored as a table
            local my_source_data = {}

            [...]

            return my_source_data
    end

    info.video_render = function(my_source_data, effect)
            [...]
    end

    info.get_width = function(my_source_data)
            [...]

            -- assuming the source data contains a 'width' key
            return my_source_data.width
    end

    info.get_height = function(my_source_data)
            [...]

            -- assuming the source data contains a 'height' key
            return my_source_data.height
    end

    -- register the source
    obs_register_source(info)


.. _other_script_differences:

Other Differences From the C API
--------------------------------

Certain functions are implemented differently from the C API due to how
callbacks work.  (These functions are part of the obspython/obslua
modules/namespaces).

.. py:function:: obs_enum_sources()

   Enumerates all sources.

   :return: An array of reference-incremented sources.  Release with
            :py:func:`source_list_release()`.

.. py:function:: obs_scene_enum_items(scene)

   Enumerates scene items within a scene.

   :param scene: obs_scene_t object to enumerate items from.
   :return:      List of scene items.  Release with
                 :py:func:`sceneitem_list_release()`.

.. py:function:: obs_add_main_render_callback(callback)

   **Lua only:** Adds a primary output render callback.  This callback
   has no parameters.

   :param callback: Render callback.  Use
                    :py:func:`obs_remove_main_render_callback()` or
                    :py:func:`remove_current_callback()` to remove the
                    callback.

.. py:function:: obs_remove_main_render_callback(callback)

   **Lua only:** Removes a primary output render callback.

   :param callback: Render callback.

.. py:function:: signal_handler_connect(handler, signal, callback)

   Adds a callback to a specific signal on a signal handler.  This
   callback has one parameter:  the calldata_t object.

   :param handler:  A signal_handler_t object.
   :param signal:   The signal on the signal handler (string)
   :param callback: The callback to connect to the signal.  Use
                    :py:func:`signal_handler_disconnect()` or
                    :py:func:`remove_current_callback()` to remove the
                    callback.

.. py:function:: signal_handler_disconnect(handler, signal, callback)

   Removes a callback from a specific signal of a signal handler.

   :param handler:  A signal_handler_t object.
   :param signal:   The signal on the signal handler (string)
   :param callback: The callback to disconnect from the signal.

.. py:function:: signal_handler_connect_global(handler, callback)

   Adds a global callback to a signal handler.  This callback has two
   parameters:  the first parameter is the signal string, and the second
   parameter is the calldata_t object.

   :param handler:  A signal_handler_t object.
   :param callback: The callback to connect.  Use
                    :py:func:`signal_handler_disconnect_global()` or
                    :py:func:`remove_current_callback()` to remove the
                    callback.

.. py:function:: signal_handler_disconnect_global(handler, callback)

   Removes a global callback from a signal handler.

   :param handler:  A signal_handler_t object.
   :param callback: The callback to disconnect.

.. py:function:: obs_hotkey_register_frontend(name, description, callback)

   Adds a frontend hotkey.  The callback takes one parameter: a boolean
   'pressed' parameter.

   :param name:        Unique name identifier string of the hotkey.
   :param description: Hotkey description shown to the user.
   :param callback:    Callback for the hotkey.  Use
                       :py:func:`obs_hotkey_unregister()` or
                       :py:func:`remove_current_callback()` to remove
                       the callback.

.. py:function:: obs_hotkey_unregister(callback)

   Unregisters the hotkey associated with the specified callback.

   :param callback: Callback of the hotkey to unregister.

.. py:function:: obs_properties_add_button(properties, setting_name, text, callback)

   Adds a button property to an obs_properties_t object.  The callback
   takes two parameters:  the first parameter is the obs_properties_t
   object, and the second parameter is the obs_property_t for the
   button.

   :param properties:   An obs_properties_t object.
   :param setting_name: A setting identifier string.
   :param text:         Button text.
   :param callback:     Button callback.  This callback is automatically
                        cleaned up.

.. py:function:: remove_current_callback()

   Removes the current callback being executed.  Does nothing if not
   within a callback.

.. py:function:: source_list_release(source_list)

   Releases the references of a source list.

   :param source_list: Array of sources to release.


.. py:function:: sceneitem_list_release(item_list)

   Releases the references of a scene item list.

   :param item_list: Array of scene items to release.

.. py:function:: calldata_source(calldata, name)

   Casts a pointer parameter of a calldata_t object to an obs_source_t
   object.

   :param calldata: A calldata_t object.
   :param name:     Name of the parameter.
   :return:         A borrowed reference to an obs_source_t object.

.. py:function:: calldata_sceneitem(calldata, name)

   Casts a pointer parameter of a calldata_t object to an
   obs_sceneitem_t object.

   :param calldata: A calldata_t object.
   :param name:     Name of the parameter.
   :return:         A borrowed reference to an obs_sceneitem_t object.
