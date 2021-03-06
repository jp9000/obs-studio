Properties API Reference (obs_properties_t)
===========================================

Properties are used to enumerate available settings for a libobs object.
Typically this is used to automatically generate user interface widgets,
though can be used to enumerate available and/or valid values for
specific settings as well.

.. type:: obs_properties_t

   Properties object (not reference counted).

.. type:: obs_property_t

   A property object (not reference counted).

.. code:: cpp

   #include <obs.h>


General Functions
-----------------

.. function:: obs_properties_t *obs_properties_create(void)

   :return: A new properties object.

---------------------

.. function:: obs_properties_t *obs_properties_create_param(void *param, void (*destroy)(void *param))

   Creates a new properties object with specific private data *param*
   associated with the object, and is automatically freed with the
   object when the properties are destroyed via the *destroy* function.

   :return: A new properties object.

---------------------

.. function:: void obs_properties_destroy(obs_properties_t *props)

---------------------

.. function:: void obs_properties_set_flags(obs_properties_t *props, uint32_t flags)
              uint32_t obs_properties_get_flags(obs_properties_t *props)

   :param flags: 0 or a bitwise OR combination of one of the following
                 values:
                 
                 - OBS_PROPERTIES_DEFER_UPDATE - A hint that tells the
                   front-end to defers updating the settings until the
                   user has finished editing all properties rather than
                   immediately updating any settings

---------------------

.. function:: void obs_properties_set_param(obs_properties_t *props, void *param, void (*destroy)(void *param))
              void *obs_properties_get_param(obs_properties_t *props)

   Sets custom data associated with this properties object.  If private
   data is already associated with the object, that private data will be
   destroyed before assigning new private data to it.

---------------------

.. function:: void obs_properties_apply_settings(obs_properties_t *props, obs_data_t *settings)

   Applies settings to the properties by calling all the necessary
   modification callbacks

---------------------

.. function:: obs_properties_t *obs_properties_get_parent(obs_properties_t *props)

---------------------


Property Object Functions
-------------------------

.. function:: obs_property_t *obs_properties_add_bool(obs_properties_t *props, const char *name, const char *description)

   Adds a boolean property.

   :param    name:        Setting identifier string
   :param    description: Localized name shown to user
   :return:               The property

---------------------

.. function:: obs_property_t *obs_properties_add_int(obs_properties_t *props, const char *name, const char *description, int min, int max, int step)

   Adds an integer property.

   :param    name:        Setting identifier string
   :param    description: Localized name shown to user
   :param    min:         Minimum value
   :param    max:         Maximum value
   :param    step:        Step value
   :return:               The property

---------------------

.. function:: obs_property_t *obs_properties_add_float(obs_properties_t *props, const char *name, const char *description, double min, double max, double step)

   :param    name:        Setting identifier string
   :param    description: Localized name shown to user
   :param    min:         Minimum value
   :param    max:         Maximum value
   :param    step:        Step value
   :return:               The property

---------------------

.. function:: obs_property_t *obs_properties_add_int_slider(obs_properties_t *props, const char *name, const char *description, int min, int max, int step)

   :param    name:        Setting identifier string
   :param    description: Localized name shown to user
   :param    min:         Minimum value
   :param    max:         Maximum value
   :param    step:        Step value
   :return:               The property

---------------------

.. function:: obs_property_t *obs_properties_add_float_slider(obs_properties_t *props, const char *name, const char *description, double min, double max, double step)

   :param    name:        Setting identifier string
   :param    description: Localized name shown to user
   :param    min:         Minimum value
   :param    max:         Maximum value
   :param    step:        Step value
   :return:               The property

---------------------

.. function:: obs_property_t *obs_properties_add_text(obs_properties_t *props, const char *name, const char *description, enum obs_text_type type)

   :param    name:        Setting identifier string
   :param    description: Localized name shown to user
   :param    type:        Can be one of the following values:

                          - **OBS_TEXT_DEFAULT** - Single line of text
                          - **OBS_TEXT_PASSWORD** - Single line of text (passworded)
                          - **OBS_TEXT_MULTILINE** - Multi-line text

   :return:               The property

---------------------

.. function:: obs_property_t *obs_properties_add_path(obs_properties_t *props, const char *name, const char *description, enum obs_path_type type, const char *filter, const char *default_path)

   Adds a 'path' property.  Can be a directory or a file.

   If target is a file path, the filters should be this format, separated by
   double semi-colens, and extensions separated by space::

     "Example types 1 and 2 (*.ex1 *.ex2);;Example type 3 (*.ex3)"

   :param    name:         Setting identifier string
   :param    description:  Localized name shown to user
   :param    type:         Can be one of the following values:

                           - **OBS_PATH_FILE** - File (for reading)
                           - **OBS_PATH_FILE_SAVE** - File (for writing)
                           - **OBS_PATH_DIRECTORY** - Directory

   :param    filter:       If type is a file path, then describes the file filter
                           that the user can browse.  Items are separated via
                           double semi-colens.  If multiple file types in a
                           filter, separate with space.
   :param    default_path: The default path to start in, or *NULL*
   :return:                The property

---------------------

.. function:: obs_property_t *obs_properties_add_list(obs_properties_t *props, const char *name, const char *description, enum obs_combo_type type, enum obs_combo_format format)

   Adds an integer/string/floating point item list.  This would be
   implemented as a combo box in user interface.

   :param    name:        Setting identifier string
   :param    description: Localized name shown to user
   :param    type:        Can be one of the following values:

                          - **OBS_COMBO_TYPE_EDITABLE** - Can be edited.
                            Only used with string lists.
                          - **OBS_COMBO_TYPE_LIST** - Not editable.

   :param    format:      Can be one of the following values:

                          - **OBS_COMBO_FORMAT_INT** - Integer list
                          - **OBS_COMBO_FORMAT_FLOAT** - Floating point
                            list
                          - **OBS_COMBO_FORMAT_STRING** - String list

   :return:               The property

   Important Related Functions:

   - :c:func:`obs_property_list_add_string`
   - :c:func:`obs_property_list_add_int`
   - :c:func:`obs_property_list_add_float`
   - :c:func:`obs_property_list_insert_string`
   - :c:func:`obs_property_list_insert_int`
   - :c:func:`obs_property_list_insert_float`
   - :c:func:`obs_property_list_item_remove`
   - :c:func:`obs_property_list_clear`

---------------------

.. function:: obs_property_t *obs_properties_add_color(obs_properties_t *props, const char *name, const char *description)

   Adds a color property without alpha.

   :param    name:        Setting identifier string
   :param    description: Localized name shown to user
   :return:               The property

---------------------

.. function:: obs_property_t *obs_properties_add_color_alpha(obs_properties_t *props, const char *name, const char *description)

   Adds a color property with alpha.

   :param    name:        Setting identifier string
   :param    description: Localized name shown to user
   :return:               The property

---------------------

.. function:: obs_property_t *obs_properties_add_button(obs_properties_t *props, const char *name, const char *text, obs_property_clicked_t callback)

   Adds a button property.  This property does not actually store any
   settings; it's used to implement a button in user interface if the
   properties are used to generate user interface.

   :param    name:        Setting identifier string
   :param    description: Localized name shown to user
   :return:               The property

   Relevant data types used with this function:

.. code:: cpp

   typedef bool (*obs_property_clicked_t)(obs_properties_t *props,
                   obs_property_t *property, void *data);

---------------------

.. function:: obs_property_t *obs_properties_add_font(obs_properties_t *props, const char *name, const char *description)

   Adds a font property.

   :param    name:        Setting identifier string
   :param    description: Localized name shown to user
   :return:               The property

---------------------

.. function:: obs_property_t *obs_properties_add_editable_list(obs_properties_t *props, const char *name, const char *description, enum obs_editable_list_type type, const char *filter, const char *default_path)

   Adds a list in which the user can add/insert/remove items.

   :param    name:         Setting identifier string
   :param    description:  Localized name shown to user
   :param    type:         Can be one of the following values:
                          
                           - **OBS_EDITABLE_LIST_TYPE_STRINGS** - An
                             editable list of strings.
                           - **OBS_EDITABLE_LIST_TYPE_FILES** - An
                             editable list of files.
                           - **OBS_EDITABLE_LIST_TYPE_FILES_AND_URLS** -
                             An editable list of files and URLs.

   :param    filter:       File filter to use if a file list
   :param    default_path: Default path if a file list
   :return:                The property

---------------------

.. function:: obs_property_t *obs_properties_add_frame_rate(obs_properties_t *props, const char *name, const char *description)

   Adds a frame rate property.

   :param    name:        Setting identifier string
   :param    description: Localized name shown to user
   :return:               The property

   Important Related Functions:

   - :c:func:`obs_property_frame_rate_option_add`
   - :c:func:`obs_property_frame_rate_fps_range_add`
   - :c:func:`obs_property_frame_rate_option_insert`
   - :c:func:`obs_property_frame_rate_fps_range_insert`

---------------------

.. function:: obs_property_t *obs_properties_add_group(obs_properties_t *props, const char *name, const char *description, enum obs_group_type type, obs_properties_t *group)

   Adds a property group.

   :param    name:        Setting identifier string
   :param    description: Localized name shown to user
   :param    type:        Can be one of the following values:

                          - **OBS_GROUP_NORMAL** - A normal group with just a name and content.
                          - **OBS_GROUP_CHECKABLE** - A checkable group with a checkbox, name and content.

   :param    group:       Group to add

   :return:               The property

   Important Related Functions:

   - :c:func:`obs_property_group_type`
   - :c:func:`obs_property_group_content`
   - :c:func:`obs_properties_get_parent`

---------------------


Property Enumeration Functions
------------------------------

.. function:: obs_property_t *obs_properties_first(obs_properties_t *props)

   :return: The first property in the properties object.

---------------------

.. function:: obs_property_t *obs_properties_get(obs_properties_t *props, const char *property)

   :param property: The name of the property to get
   :return:         A specific property or *NULL* if not found

---------------------

.. function:: bool                   obs_property_next(obs_property_t **p)

   :param p: Pointer to the pointer of the next property
   :return: *true* if successful, *false* if no more properties

---------------------

.. function:: const char *           obs_property_name(obs_property_t *p)

   :return: The setting identifier string of the property

   *(Author's Note: "name" was a bad name to use here.  Should have been
   "setting")*

---------------------

.. function:: const char *           obs_property_description(obs_property_t *p)

   :return: The actual localized display name of the property

   *(Author's note: This one should have been the "name")*

---------------------

.. function:: const char *           obs_property_long_description(obs_property_t *p)

   :return: A detailed description of what the setting is used for.
            Usually used with things like tooltips.

---------------------

.. function:: enum obs_property_type obs_property_get_type(obs_property_t *p)

   :return: One of the following values:

            - OBS_PROPERTY_INVALID
            - OBS_PROPERTY_BOOL
            - OBS_PROPERTY_INT
            - OBS_PROPERTY_FLOAT
            - OBS_PROPERTY_TEXT
            - OBS_PROPERTY_PATH
            - OBS_PROPERTY_LIST
            - OBS_PROPERTY_COLOR
            - OBS_PROPERTY_BUTTON
            - OBS_PROPERTY_FONT
            - OBS_PROPERTY_EDITABLE_LIST
            - OBS_PROPERTY_FRAME_RATE
            - OBS_PROPERTY_GROUP

---------------------

.. function:: bool                   obs_property_enabled(obs_property_t *p)

---------------------

.. function:: bool                   obs_property_visible(obs_property_t *p)

---------------------

.. function:: int                    obs_property_int_min(obs_property_t *p)

---------------------

.. function:: int                    obs_property_int_max(obs_property_t *p)

---------------------

.. function:: int                    obs_property_int_step(obs_property_t *p)

---------------------

.. function:: enum obs_number_type   obs_property_int_type(obs_property_t *p)

---------------------

.. function:: double                 obs_property_float_min(obs_property_t *p)

---------------------

.. function:: double                 obs_property_float_max(obs_property_t *p)

---------------------

.. function:: double                 obs_property_float_step(obs_property_t *p)

---------------------

.. function:: enum obs_number_type   obs_property_float_type(obs_property_t *p)

---------------------

.. function:: enum obs_text_type     obs_property_text_type(obs_property_t *p)

---------------------

.. function:: enum obs_path_type     obs_property_path_type(obs_property_t *p)

---------------------

.. function:: const char *           obs_property_path_filter(obs_property_t *p)

---------------------

.. function:: const char *           obs_property_path_default_path(obs_property_t *p)

---------------------

.. function:: enum obs_combo_type    obs_property_list_type(obs_property_t *p)

---------------------

.. function:: enum obs_combo_format  obs_property_list_format(obs_property_t *p)

---------------------

.. function:: bool obs_property_list_item_disabled(obs_property_t *p, size_t idx)

---------------------

.. function:: size_t      obs_property_list_item_count(obs_property_t *p)

---------------------

.. function:: const char *obs_property_list_item_name(obs_property_t *p, size_t idx)

---------------------

.. function:: const char *obs_property_list_item_string(obs_property_t *p, size_t idx)

---------------------

.. function:: long long   obs_property_list_item_int(obs_property_t *p, size_t idx)

---------------------

.. function:: double      obs_property_list_item_float(obs_property_t *p, size_t idx)

---------------------

.. function:: enum obs_editable_list_type obs_property_editable_list_type(obs_property_t *p)

---------------------

.. function:: const char *obs_property_editable_list_filter(obs_property_t *p)

---------------------

.. function:: const char *obs_property_editable_list_default_path(obs_property_t *p)

---------------------

.. function:: size_t      obs_property_frame_rate_options_count(obs_property_t *p)

---------------------

.. function:: const char *obs_property_frame_rate_option_name(obs_property_t *p, size_t idx)

---------------------

.. function:: const char *obs_property_frame_rate_option_description( obs_property_t *p, size_t idx)

---------------------

.. function:: size_t      obs_property_frame_rate_fps_ranges_count(obs_property_t *p)

---------------------

.. function:: struct media_frames_per_second obs_property_frame_rate_fps_range_min( obs_property_t *p, size_t idx)

---------------------

.. function:: struct media_frames_per_second obs_property_frame_rate_fps_range_max( obs_property_t *p, size_t idx)

---------------------

.. function:: enum obs_group_type obs_property_group_type(obs_property_t *p)

  :return: One of the following values:

            - OBS_COMBO_INVALID
            - OBS_GROUP_NORMAL
            - OBS_GROUP_CHECKABLE

---------------------

.. function:: obs_properties_t *obs_property_group_content(obs_property_t *p)

---------------------


Property Modification Functions
-------------------------------

.. function:: void obs_property_set_modified_callback(obs_property_t *p, obs_property_modified_t modified)
              void obs_property_set_modified_callback2(obs_property_t *p, obs_property_modified2_t modified2, void *priv)

   Allows the ability to change the properties depending on what
   settings are used by the user.

   Relevant data types used with these functions:

.. code:: cpp

   typedef bool (*obs_property_modified_t)(obs_properties_t *props,
                   obs_property_t *property, obs_data_t *settings);
   typedef bool (*obs_property_modified2_t)(void *priv,
                   obs_properties_t *props, obs_property_t *property,
                   obs_data_t *settings);

---------------------

.. function:: bool obs_property_modified(obs_property_t *p, obs_data_t *settings)

---------------------

.. function:: bool obs_property_button_clicked(obs_property_t *p, void *obj)

---------------------

.. function:: void obs_property_set_visible(obs_property_t *p, bool visible)

---------------------

.. function:: void obs_property_set_enabled(obs_property_t *p, bool enabled)

---------------------

.. function:: void obs_property_set_description(obs_property_t *p, const char *description)

   Sets the displayed localized name of the property, shown to the user.

---------------------

.. function:: void obs_property_set_long_description(obs_property_t *p, const char *long_description)

   Sets the localized long description of the property, usually shown to
   a user via tooltip.

---------------------

.. function:: void obs_property_int_set_limits(obs_property_t *p, int min, int max, int step)

---------------------

.. function:: void obs_property_float_set_limits(obs_property_t *p, double min, double max, double step)

---------------------

.. function:: void obs_property_list_clear(obs_property_t *p)

---------------------

.. function:: size_t obs_property_list_add_string(obs_property_t *p, const char *name, const char *val)

   Adds a string to a string list.

---------------------

.. function:: size_t obs_property_list_add_int(obs_property_t *p, const char *name, long long val)

   Adds an integer to a integer list.

---------------------

.. function:: size_t obs_property_list_add_float(obs_property_t *p, const char *name, double val)

   Adds a floating point to a floating point list.

---------------------

.. function:: void obs_property_list_insert_string(obs_property_t *p, size_t idx, const char *name, const char *val)

   Inserts a string in to a string list.

---------------------

.. function:: void obs_property_list_insert_int(obs_property_t *p, size_t idx, const char *name, long long val)

   Inserts an integer in to an integer list.

---------------------

.. function:: void obs_property_list_insert_float(obs_property_t *p, size_t idx, const char *name, double val)

   Inserts a floating point in to a floating point list.

---------------------

.. function:: void obs_property_list_item_disable(obs_property_t *p, size_t idx, bool disabled)

---------------------

.. function:: void obs_property_list_item_remove(obs_property_t *p, size_t idx)

---------------------

.. function:: size_t obs_property_frame_rate_option_add(obs_property_t *p, const char *name, const char *description)

---------------------

.. function:: size_t obs_property_frame_rate_fps_range_add(obs_property_t *p, struct media_frames_per_second min, struct media_frames_per_second max)

---------------------

.. function:: void obs_property_frame_rate_clear(obs_property_t *p)

---------------------

.. function:: void obs_property_frame_rate_options_clear(obs_property_t *p)

---------------------

.. function:: void obs_property_frame_rate_fps_ranges_clear(obs_property_t *p)

---------------------

.. function:: void obs_property_frame_rate_option_insert(obs_property_t *p, size_t idx, const char *name, const char *description)

---------------------

.. function:: void obs_property_frame_rate_fps_range_insert(obs_property_t *p, size_t idx, struct media_frames_per_second min, struct media_frames_per_second max)
