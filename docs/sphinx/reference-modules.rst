Module API Reference
====================

Modules add custom functionality to libobs: typically
:ref:`plugins_sources`, :ref:`plugins_outputs`, :ref:`plugins_encoders`,
and :ref:`plugins_services`.

.. type:: obs_module_t

   A module object (not reference counted).

.. code:: cpp

   #include <obs-module.h>


Module Macros
-------------

These macros are used within custom plugin modules.

.. function:: OBS_DECLARE_MODULE()

   Declares a libobs module.  Exports important core module functions
   related to the module itself, OBS version, etc.

---------------------

.. function:: OBS_MODULE_USE_DEFAULT_LOCALE(module_name, default_locale)

   Helper macro that uses the standard ini file format for localization.
   Automatically initializes and destroys localization data, and
   automatically provides module externs such as
   :c:func:`obs_module_text()` to be able to get a localized string with
   little effort.

---------------------

Module Exports
--------------

These are functions that plugin modules can optionally export in order
to communicate with libobs and front-ends.

.. function:: bool obs_module_load(void)

   Required: Called when the module is loaded.  Implement this function
   to load all the sources/encoders/outputs/services for your module, or
   anything else that may need loading.
  
   :return:          Return true to continue loading the module, otherwise
                     false to indicate failure and unload the module

---------------------

.. function:: void obs_module_unload(void)

   Optional: Called when the module is unloaded.

---------------------

.. function:: void obs_module_post_load(void)

   Optional: Called when all modules have finished loading.

---------------------

.. function:: void obs_module_set_locale(const char *locale)

   Called to set the locale language and load the locale data for the
   module.

---------------------

.. function:: void obs_module_free_locale(void)

   Called on module destruction to free locale data.

---------------------

.. function:: const char *obs_module_name(void)

   (Optional)
   
   :return: The full name of the module

---------------------

.. function:: const char *obs_module_description(void)

   (Optional)

   :return: A description of the module

---------------------


Module Externs
--------------

These functions are externs that are usable throughout the module.

.. function:: const char *obs_module_text(const char *lookup_string)

   :return: A localized string

---------------------

.. function:: bool obs_module_get_string(const char *lookup_string, const char **translated_string)

   Helper function for looking up locale.
   
   :return: *true* if text found, otherwise *false*

---------------------

.. function:: obs_module_t *obs_current_module(void)

   :return: The current module

---------------------

.. function:: char *obs_module_file(const char *file)

   Returns the location to a module data file associated with the
   current module.  Free with :c:func:`bfree()` when complete.
   
   Equivalent to:

.. code:: cpp

      obs_find_module_file(obs_current_module(), file);

---------------------

.. function:: char *obs_module_config_path(const char *file)

   Returns the location to a module config file associated with the
   current module.  Free with :c:func:`bfree()` when complete.  Will
   return NULL if configuration directory is not set.
   
   Equivalent to:

.. code:: cpp

      obs_module_get_config_path(obs_current_module(), file);

---------------------


Frontend Module Functions
--------------------------

These are functions used by frontends to load and get information about
plugin modules.

.. function:: int obs_open_module(obs_module_t **module, const char *path, const char *data_path)

   Opens a plugin module directly from a specific path.
  
   If the module already exists then the function will return successful, and
   the module parameter will be given the pointer to the existing
   module.
  
   This does not initialize the module, it only loads the module image.  To
   initialize the module, call :c:func:`obs_init_module()`.
  
   :param  module:    The pointer to the created module
   :param  path:      Specifies the path to the module library file.  If the
                      extension is not specified, it will use the extension
                      appropriate to the operating system
   :param  data_path: Specifies the path to the directory where the module's
                      data files are stored (or *NULL* if none)
   :returns:          | MODULE_SUCCESS          - Successful
                      | MODULE_ERROR            - A generic error occurred
                      | MODULE_FILE_NOT_FOUND   - The module was not found
                      | MODULE_MISSING_EXPORTS  - Required exports are missing
                      | MODULE_INCOMPATIBLE_VER - Incompatible version

---------------------

.. function:: bool obs_init_module(obs_module_t *module)

   Initializes the module, which calls its obs_module_load export.
   
   :return: *true* if the module was loaded successfully

---------------------

.. function:: void obs_log_loaded_modules(void)

   Logs loaded modules.

---------------------

.. function:: const char *obs_get_module_file_name(obs_module_t *module)

   :return: The module file name

---------------------

.. function:: const char *obs_get_module_name(obs_module_t *module)

   :return: The module full name (or *NULL* if none)

---------------------

.. function:: void obs_get_module_author(obs_module_t *module)

   :return: The module author(s)

---------------------

.. function:: const char *obs_get_module_description(obs_module_t *module)

   :return: The module description

---------------------

.. function:: const char *obs_get_module_binary_path(obs_module_t *module)

   :return: The module binary path

---------------------

.. function:: const char *obs_get_module_data_path(obs_module_t *module)

   :return: The module data path

---------------------

.. function:: void obs_add_module_path(const char *bin, const char *data)

   Adds a module search path to be used with obs_find_modules.  If the search
   path strings contain %module%, that text will be replaced with the module
   name when used.
  
   :param  bin:  Specifies the module's binary directory search path
   :param  data: Specifies the module's data directory search path

---------------------

.. function:: void obs_load_all_modules(void)

   Automatically loads all modules from module paths (convenience function).

---------------------

.. function:: void obs_post_load_modules(void)

   Notifies modules that all modules have been loaded.

---------------------

.. function:: void obs_find_modules(obs_find_module_callback_t callback, void *param)

   Finds all modules within the search paths added by
   :c:func:`obs_add_module_path()`.

   Relevant data types used with this function:

.. code:: cpp

   struct obs_module_info {
           const char *bin_path;
           const char *data_path;
   };

   typedef void (*obs_find_module_callback_t)(void *param,
                   const struct obs_module_info *info);

---------------------

.. function:: void obs_enum_modules(obs_enum_module_callback_t callback, void *param)

   Enumerates all loaded modules.

   Relevant data types used with this function:

.. code:: cpp

   typedef void (*obs_enum_module_callback_t)(void *param, obs_module_t *module);

---------------------

.. function:: char *obs_find_module_file(obs_module_t *module, const char *file)

   Returns the location of a plugin module data file.
  
   Note:   Modules should use obs_module_file function defined in obs-module.h
           as a more elegant means of getting their files without having to
           specify the module parameter.
  
   :param  module: The module associated with the file to locate
   :param  file:   The file to locate
   :return:        Path string, or NULL if not found.  Use bfree to free string

---------------------

.. function:: char *obs_module_get_config_path(obs_module_t *module, const char *file)

   Returns the path of a plugin module config file (whether it exists or not).
  
   Note:   Modules should use obs_module_config_path function defined in
           obs-module.h as a more elegant means of getting their files without
           having to specify the module parameter.
  
   :param  module: The module associated with the path
   :param  file:   The file to get a path to
   :return:        Path string, or NULL if not found.  Use bfree to free string

---------------------

.. function:: void *obs_get_module_lib(obs_module_t *module)

   Returns library file of specified module.

   :param  module: The module where to find library file.
   :return:        Pointer to module library.
