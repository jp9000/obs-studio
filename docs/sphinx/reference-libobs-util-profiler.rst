Profiler
========

The profiler is used to get information about program performance and
efficiency.

.. type:: typedef struct profiler_snapshot profiler_snapshot_t
.. type:: typedef struct profiler_snapshot_entry profiler_snapshot_entry_t
.. type:: typedef struct profiler_name_store profiler_name_store_t
.. type:: typedef struct profiler_time_entry profiler_time_entry_t

.. code:: cpp

   #include <util/profiler.h>


Profiler Structures
-------------------

.. type:: struct profiler_time_entry
.. member:: uint64_t profiler_time_entry.time_delta
.. member:: uint64_t profiler_time_entry.count


Profiler Control Functions
--------------------------

.. function:: void profiler_start(void)

   Starts the profiler.

----------------------

.. function:: void profiler_stop(void)

   Stops the profiler.

----------------------

.. function:: void profiler_print(profiler_snapshot_t *snap)

   Creates a profiler snapshot and saves it within *snap*.

   :param snap: A profiler snapshot object

----------------------

.. function:: void profiler_print_time_between_calls(profiler_snapshot_t *snap)

   Creates a profiler snapshot of time between calls and saves it within
   *snap*.

   :param snap: A profiler snapshot object

----------------------

.. function:: void profiler_free(void)

   Frees the profiler.

----------------------


Profiling Functions
-------------------

.. function:: void profile_register_root(const char *name, uint64_t expected_time_between_calls)

   Registers a root profile node.

   :param name:                        Name of the root profile node
   :param expected_time_between_calls: The expected time between calls
                                       of the profile root node, or 0 if
                                       none.

----------------------

.. function:: void profile_start(const char *name)

   Starts a profile node.  This profile node will be a child of the last
   node that was started.

   :param name: Name of the profile node

----------------------

.. function:: void profile_end(const char *name)

   :param name: Name of the profile node

----------------------

.. function:: void profile_reenable_thread(void)

   Because :c:func:`profiler_start()` can be called in a different
   thread than the current thread, this is used to specify a point where
   it's safe to re-enable profiling in the calling thread.  Call this
   when you have looped root profile nodes and need to specify a safe
   point where the root profile node isn't active and the profiler can
   start up in the current thread again.

----------------------


Profiler Name Storage Functions
-------------------------------

.. function:: profiler_name_store_t *profiler_name_store_create(void)

   Creates a profiler name storage object.

   :return: Profiler name store object

----------------------

.. function:: void profiler_name_store_free(profiler_name_store_t *store)

   Frees a profiler name storage object.

   :param store: Profiler name storage object

----------------------

.. function:: const char *profile_store_name(profiler_name_store_t *store, const char *format, ...)

   Creates a formatted string and stores it within a profiler name
   storage object.

   :param store:  Profiler name storage object
   :param format: Formatted string
   :return:       The string created from format specifications

----------------------


Profiler Data Access Functions
------------------------------

.. function:: profiler_snapshot_t *profile_snapshot_create(void)

   Creates a profile snapshot.  Profiler snapshots are used to obtain
   data about how the active profiles performed.

   :return: A profiler snapshot object

----------------------

.. function:: void profile_snapshot_free(profiler_snapshot_t *snap)

   Frees a profiler snapshot object.

   :param snap: A profiler snapshot

----------------------

.. function:: bool profiler_snapshot_dump_csv(const profiler_snapshot_t *snap, const char *filename)

   Creates a CSV file of the profiler snapshot.

   :param snap:     A profiler snapshot
   :param filename: The path to the CSV file to save
   :return:         *true* if successfully written, *false* otherwise

----------------------

.. function:: bool profiler_snapshot_dump_csv_gz(const profiler_snapshot_t *snap, const char *filename)

   Creates a gzipped CSV file of the profiler snapshot.

   :param snap:     A profiler snapshot
   :param filename: The path to the gzipped CSV file to save
   :return:         *true* if successfully written, *false* otherwise

----------------------

.. function:: size_t profiler_snapshot_num_roots(profiler_snapshot_t *snap)

   :param snap: A profiler snapshot
   :return:     Number of root profiler nodes in the snapshot

----------------------

.. type:: typedef bool (*profiler_entry_enum_func)(void *context, profiler_snapshot_entry_t *entry)

   Profiler snapshot entry numeration callback

   :param context: Private data passed to this callback
   :param entry:   Profiler snapshot entry
   :return:        *true* to continue enumeration, *false* otherwise

----------------------

.. function:: void profiler_snapshot_enumerate_roots(profiler_snapshot_t *snap, profiler_entry_enum_func func, void *context)

   Enumerates root profile nodes.

   :param snap:    A profiler snapshot
   :param func:    Enumeration callback
   :param context: Private data to pass to the callback

----------------------

.. type:: typedef bool (*profiler_name_filter_func)(void *data, const char *name, bool *remove)

   Callback used to determine what profile nodes are removed/filtered.

   :param data:    Private data passed to this callback
   :param name:    Profile node name to be filtered
   :param remove:  Used to determined whether the node should be removed
                   or not
   :return:        *true* to continue enumeration, *false* otherwise

----------------------

.. function:: void profiler_snapshot_filter_roots(profiler_snapshot_t *snap, profiler_name_filter_func func, void *data)

   Removes/filters profile roots based upon their names.

   :param snap: A profiler snapshot
   :param func: Enumeration callback to filter with
   :param data: Private data to pass to the callback

----------------------

.. function:: size_t profiler_snapshot_num_children(profiler_snapshot_entry_t *entry)

   :param entry: A profiler snapshot entry
   :return:      Number of children for the entry

----------------------

.. function:: void profiler_snapshot_enumerate_children(profiler_snapshot_entry_t *entry, profiler_entry_enum_func func, void *context)

   Enumerates child entries of a profiler snapshot entry.

   :param entry:   A profiler snapshot entry
   :param func:    Enumeration callback
   :param context: Private data passed to the callback

----------------------

.. function:: const char *profiler_snapshot_entry_name(profiler_snapshot_entry_t *entry)

   :param entry: A profiler snapshot entry
   :return:      The name of the profiler snapshot entry

----------------------

.. function:: profiler_time_entries_t *profiler_snapshot_entry_times(profiler_snapshot_entry_t *entry)

   Gets the time entries for a snapshot entry.

   :param entry: A profiler snapshot entry
   :return:      An array of profiler time entries

----------------------

.. function:: uint64_t profiler_snapshot_entry_min_time(profiler_snapshot_entry_t *entry)

   Gets the minimum time for a profiler snapshot entry.

   :param entry: A profiler snapshot entry
   :return:      The minimum time value for the snapshot entry

----------------------

.. function:: uint64_t profiler_snapshot_entry_max_time(profiler_snapshot_entry_t *entry)

   Gets the maximum time for a profiler snapshot entry.

   :param entry: A profiler snapshot entry
   :return:      The maximum time value for the snapshot entry

----------------------

.. function:: uint64_t profiler_snapshot_entry_overall_count(profiler_snapshot_entry_t *entry)

   Gets the overall count for a profiler snapshot entry.

   :param entry: A profiler snapshot entry
   :return:      The overall count value for the snapshot entry

----------------------

.. function:: profiler_time_entries_t *profiler_snapshot_entry_times_between_calls(profiler_snapshot_entry_t *entry)

   Gets an array of time between calls for a profiler snapshot entry.

   :param entry: A profiler snapshot entry
   :return:      An array of profiler time entries

----------------------

.. function:: uint64_t profiler_snapshot_entry_expected_time_between_calls(profiler_snapshot_entry_t *entry)

   Gets the expected time between calls for a profiler snapshot entry.

   :param entry: A profiler snapshot entry
   :return:      The expected time between calls for the snapshot entry,
                 or 0 if not set

----------------------

.. function:: uint64_t profiler_snapshot_entry_min_time_between_calls(profiler_snapshot_entry_t *entry)

   Gets the minimum time seen between calls for a profiler snapshot entry.

   :param entry: A profiler snapshot entry
   :return:      The minimum time seen between calls for the snapshot entry

----------------------

.. function:: uint64_t profiler_snapshot_entry_max_time_between_calls(profiler_snapshot_entry_t *entry)

   Gets the maximum time seen between calls for a profiler snapshot entry.

   :param entry: A profiler snapshot entry
   :return:      The maximum time seen between calls for the snapshot entry

----------------------

.. function:: uint64_t profiler_snapshot_entry_overall_between_calls_count(profiler_snapshot_entry_t *entry)

   Gets the overall time between calls for a profiler snapshot entry.

   :param entry: A profiler snapshot entry
   :return:      The overall time between calls for the snapshot entry
