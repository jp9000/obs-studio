Platform Helpers
================

These functions/structures/types are used to perform actions that
typically don't have shared functions across different operating systems
and platforms.

.. code:: cpp

   #include <util/platform.h>


File Functions
--------------

.. function:: FILE *os_wfopen(const wchar_t *path, const char *mode)

   Opens a file with a wide string path.

----------------------

.. function:: FILE *os_fopen(const char *path, const char *mode)

   Opens a file with a UTF8 string path.

----------------------

.. function:: int64_t os_fgetsize(FILE *file)

   Returns a file's size.

----------------------

.. function:: int os_stat(const char *file, struct stat *st)

   Equivalent to the posix *stat* function.

----------------------

.. function:: int os_fseeki64(FILE *file, int64_t offset, int origin)

   Equivalent to fseek.

----------------------

.. function:: int64_t os_ftelli64(FILE *file)

   Gets the current file position.

----------------------

.. function:: size_t os_fread_utf8(FILE *file, char **pstr)

   Reads a UTF8 encoded file and allocates a pointer to the UTF8 string.

----------------------

.. function:: char *os_quick_read_utf8_file(const char *path)

   Reads a UTF8 encoded file and returns an allocated pointer to the
   string.

----------------------

.. function:: bool os_quick_write_utf8_file(const char *path, const char *str, size_t len, bool marker)

   Writes a UTF8 encoded file.

----------------------

.. function:: bool os_quick_write_utf8_file_safe(const char *path, const char *str, size_t len, bool marker, const char *temp_ext, const char *backup_ext)

   Writes a UTF8 encoded file with overwrite corruption prevention.

----------------------

.. function:: int64_t os_get_file_size(const char *path)

   Gets a file's size.

----------------------

.. function:: int64_t os_get_free_space(const char *path)

   Gets free space of a specific file path.

---------------------


String Conversion Functions
---------------------------

.. function:: size_t os_utf8_to_wcs(const char *str, size_t len, wchar_t *dst, size_t dst_size)

   Converts a UTF8 string to a wide string.

----------------------

.. function:: size_t os_wcs_to_utf8(const wchar_t *str, size_t len, char *dst, size_t dst_size)

   Converts a wide string to a UTF8 string.

----------------------

.. function:: size_t os_utf8_to_wcs_ptr(const char *str, size_t len, wchar_t **pstr)

   Gets an bmalloc-allocated wide string converted from a UTF8 string.

----------------------

.. function:: size_t os_wcs_to_utf8_ptr(const wchar_t *str, size_t len, char **pstr)

   Gets an bmalloc-allocated UTF8 string converted from a wide string.

---------------------


Number/String Conversion Functions
----------------------------------

.. function:: double os_strtod(const char *str)

   Converts a string to a double.

----------------------

.. function:: int os_dtostr(double value, char *dst, size_t size)

   Converts a double to a string.

---------------------


Dynamic Link Library Functions
------------------------------

These functions are roughly equivalent to dlopen/dlsym/dlclose.

.. function:: void *os_dlopen(const char *path)

   Opens a dynamic library.

----------------------

.. function:: void *os_dlsym(void *module, const char *func)

   Returns a symbol from a dynamic library.

----------------------

.. function:: void os_dlclose(void *module)

   Closes a dynamic library.

---------------------

.. function:: bool os_is_obs_plugin(const char *path)

   Returns true if the path is a dynamic library that looks like an OBS plugin.

   Currently only needed on Windows for performance reasons.

---------------------


CPU Usage Functions
-------------------

.. function:: os_cpu_usage_info_t *os_cpu_usage_info_start(void)

   Creates a CPU usage information object.

----------------------

.. function:: double              os_cpu_usage_info_query(os_cpu_usage_info_t *info)

   Queries the current CPU usage.

----------------------

.. function:: void                os_cpu_usage_info_destroy(os_cpu_usage_info_t *info)

   Destroys a CPU usage information object.

---------------------


Sleep/Time Functions
--------------------

.. function:: bool os_sleepto_ns(uint64_t time_target)

   Sleeps to a specific time with high precision, in nanoseconds.

---------------------

.. function:: void os_sleep_ms(uint32_t duration)

   Sleeps for a specific number of milliseconds.

---------------------

.. function:: uint64_t os_gettime_ns(void)

   Gets the current high-precision system time, in nanoseconds.

---------------------

Other Path/File Functions
-------------------------

.. function:: int os_get_config_path(char *dst, size_t size, const char *name)
              char *os_get_config_path_ptr(const char *name)

   Gets the user-specific application configuration data path.

---------------------

.. function:: int os_get_program_data_path(char *dst, size_t size, const char *name)
              char *os_get_program_data_path_ptr(const char *name)

   Gets the application configuration data path.

---------------------

.. function:: bool os_file_exists(const char *path)

   Returns true if a file/directory exists, false otherwise.

---------------------

.. function:: size_t os_get_abs_path(const char *path, char *abspath, size_t size)
              char *os_get_abs_path_ptr(const char *path)

   Converts a relative path to an absolute path.

---------------------

.. function:: const char *os_get_path_extension(const char *path)

   Returns the extension portion of a path string.

---------------------

.. type:: typedef struct os_dir os_dir_t

   A directory object.

.. type:: struct os_dirent

   A directory entry record.

.. member:: char os_dirent.d_name[256]

   The directory entry name.

.. member:: bool os_dirent.directory

   *true* if the entry is a directory.

---------------------

.. function:: os_dir_t *os_opendir(const char *path)

   Opens a directory object to enumerate files within the directory.

---------------------

.. function:: struct os_dirent *os_readdir(os_dir_t *dir)

   Returns the linked list of directory entries.

---------------------

.. function:: void os_closedir(os_dir_t *dir)

   Closes a directory object.

---------------------

.. type:: struct os_globent

   A glob entry.
   
.. member:: char *os_globent.path

   The full path to the glob entry.

.. member:: bool os_globent.directory

   *true* if the glob entry is a directory, *false* otherwise.

.. type:: struct os_glob_info

   A glob object.

.. member:: size_t             os_glob_info.gl_pathc

   Number of glob entries.

.. member:: struct os_globent *os_glob_info.gl_pathv

   Array of glob entries.

.. type:: typedef struct os_glob_info os_glob_t

---------------------

.. function:: int os_glob(const char *pattern, int flags, os_glob_t **pglob)

   Enumerates files based upon a glob string.

---------------------

.. function:: void os_globfree(os_glob_t *pglob)

   Frees a glob object.

---------------------

.. function:: int os_unlink(const char *path)

   Deletes a file.

---------------------

.. function:: int os_rmdir(const char *path)

   Deletes a directory.

---------------------

.. function:: char *os_getcwd(char *path, size_t size)

   Returns a new bmalloc-allocated path to the current working
   directory.

---------------------

.. function:: int os_chdir(const char *path)

   Changes the current working directory.

---------------------

.. function:: int os_mkdir(const char *path)

   Creates a directory.

---------------------

.. function:: int os_mkdirs(const char *path)

   Creates a full directory path if it doesn't exist.

---------------------

.. function:: int os_rename(const char *old_path, const char *new_path)

   Renames a file.

---------------------

.. function:: int os_copyfile(const char *file_in, const char *file_out)

   Copies a file.

---------------------

.. function:: int os_safe_replace(const char *target_path, const char *from_path, const char *backup_path)

   Safely replaces a file.

---------------------

.. function:: char *os_generate_formatted_filename(const char *extension, bool space, const char *format)

   Returns a new bmalloc-allocated filename generated from specific
   formatting.

---------------------


Sleep-Inhibition Functions
--------------------------

These functions/types are used to inhibit the computer from going to
sleep.

.. type:: struct os_inhibit_info
.. type:: typedef struct os_inhibit_info os_inhibit_t

---------------------

.. function:: os_inhibit_t *os_inhibit_sleep_create(const char *reason)

   Creates a sleep inhibition object.

---------------------

.. function:: bool os_inhibit_sleep_set_active(os_inhibit_t *info, bool active)

   Activates/deactivates a sleep inhibition object.

---------------------

.. function:: void os_inhibit_sleep_destroy(os_inhibit_t *info)

   Destroys a sleep inhibition object.  If the sleep inhibition object
   was active, it will be deactivated.

---------------------


Other Functions
---------------

.. function:: void os_breakpoint(void)

   Triggers a debugger breakpoint (or crashes the program if no debugger
   present).

---------------------

.. function:: int os_get_physical_cores(void)

   Returns the number of physical cores available.

---------------------

.. function:: int os_get_logical_cores(void)

   Returns the number of logical cores available.

---------------------

.. function:: uint64_t os_get_sys_free_size(void)

   Returns the amount of memory available.

---------------------

.. type:: struct os_proc_memory_usage

   Memory usage structure.

.. member:: uint64_t os_proc_memory_usage.resident_size

   Resident size.

.. member:: uint64_t os_proc_memory_usage.virtual_size

   Virtual size.

.. type:: typedef struct os_proc_memory_usage os_proc_memory_usage_t

---------------------

.. function:: bool os_get_proc_memory_usage(os_proc_memory_usage_t *usage)

   Gets memory usage of the current process.

---------------------

.. function:: uint64_t os_get_proc_resident_size(void)

   Returns the resident memory size of the current process.

---------------------

.. function:: uint64_t os_get_proc_virtual_size(void)

   Returns the virtual memory size of the current process.
