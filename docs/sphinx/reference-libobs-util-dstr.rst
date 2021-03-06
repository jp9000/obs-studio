Dynamic Strings And String Helpers
==================================

Provides string helper structures/functions (roughly equivalent to
std::string).

.. code:: cpp

   #include <util/dstr.h>


Dynamic String Structure (struct dstr)
--------------------------------------

.. type:: struct dstr
.. member:: char *dstr.array
.. member:: size_t dstr.len
.. member:: size_t dstr.capacity


General String Helper Functions
-------------------------------

.. function:: int astrcmpi(const char *str1, const char *str2)

   Case insensitive string comparison function.

----------------------

.. function:: int wstrcmpi(const wchar_t *str1, const wchar_t *str2)

   Case insensitive wide string comparison function.

----------------------

.. function:: int astrcmp_n(const char *str1, const char *str2, size_t n)

   String comparison function for a specific number of characters.

----------------------

.. function:: int wstrcmp_n(const wchar_t *str1, const wchar_t *str2, size_t n)

   Wide string comparison function for a specific number of characters.

----------------------

.. function:: int astrcmpi_n(const char *str1, const char *str2, size_t n)

   Case insensitive string comparison function for a specific number of
   characters.

----------------------

.. function:: int wstrcmpi_n(const wchar_t *str1, const wchar_t *str2, size_t n)

   Case insensitive wide string comparison function for a specific
   number of characters.

----------------------

.. function:: char *astrstri(const char *str, const char *find)

   Case insensitive version of strstr.

----------------------

.. function:: wchar_t *wstrstri(const wchar_t *str, const wchar_t *find)

   Case insensitive version of wcsstr.

----------------------

.. function:: char *strdepad(char *str)

   Removes padding characters (tab, space, CR, LF) from the front and
   end of a string.

----------------------

.. function:: wchar_t *wcsdepad(wchar_t *str)

   Removes padding characters (tab, space, CR, LF) from the front and
   end of a wide string.

----------------------

.. function:: char **strlist_split(const char *str, char split_ch, bool include_empty)

   Splits a string in to a list of multiple sub-strings.  Free with
   :c:func:`strlist_free()`.

----------------------

.. function:: void strlist_free(char **strlist)

   Frees a string list created with :c:func:`strlist_split()`.

---------------------


Dynamic String Functions
------------------------

.. function:: void dstr_init(struct dstr *dst)

   Initializes a dynamic string variable (just zeroes the variable).

   :param dst: Dynamic string to initialize

----------------------

.. function:: void dstr_init_move(struct dstr *dst, struct dstr *src)

   Moves a *src* to *dst* without copying data and zeroes *src*.

   :param dst: Destination
   :param src: Source

----------------------

.. function:: void dstr_init_move_array(struct dstr *dst, char *str)

   Sets a bmalloc-allocated string as the dynamic string without
   copying/reallocating.

   :param dst: Dynamic string to initialize
   :param str: bmalloc-allocated string

----------------------

.. function:: void dstr_init_copy(struct dstr *dst, const char *src)

   Initializes a dynamic string with a copy of a string

   :param dst: Dynamic string to initialize
   :param src: String to copy

----------------------

.. function:: void dstr_init_copy_dstr(struct dstr *dst, const struct dstr *src)

   Initializes a dynamic string with a copy of another dynamic string

   :param dst: Dynamic string to initialize
   :param src: Dynamic string to copy

----------------------

.. function:: void dstr_free(struct dstr *dst)

   Frees a dynamic string.

   :param dst: Dynamic string

----------------------

.. function:: void dstr_copy(struct dstr *dst, const char *array)

   Copies a string.

   :param dst:   Dynamic string
   :param array: String to copy

----------------------

.. function:: void dstr_copy_dstr(struct dstr *dst, const struct dstr *src)

   Copies another dynamic string.

   :param dst: Dynamic string
   :param src: Dynamic string to copy

----------------------

.. function:: void dstr_ncopy(struct dstr *dst, const char *array, const size_t len)

   Copies a specific number of characters from a string.

   :param dst:   Dynamic string
   :param array: String to copy
   :param len:   Number of characters to copy

----------------------

.. function:: void dstr_ncopy_dstr(struct dstr *dst, const struct dstr *src, const size_t len)

   Copies a specific number of characters from another dynamic string.

   :param dst:   Dynamic string
   :param src:   Dynamic string to copy
   :param len:   Number of characters to copy

----------------------

.. function:: void dstr_resize(struct dstr *dst, const size_t num)

   Sets the size of the dynamic string.  If the new size is bigger than
   current size, zeroes the new characters.

   :param dst: Dynamic string
   :param num: New size

----------------------

.. function:: void dstr_reserve(struct dstr *dst, const size_t num)

   Reserves a specific number of characters in the buffer (but does not
   change the size).  Does not work if the value is smaller than the
   current reserve size.

   :param dst: Dynamic string
   :param num: New reserve size

----------------------

.. function:: bool dstr_is_empty(const struct dstr *str)

   Returns whether the dynamic string is empty.

   :param str: Dynamic string
   :return:    *true* if empty, *false* otherwise

----------------------

.. function:: void dstr_cat(struct dstr *dst, const char *array)

   Concatenates a dynamic string.

   :param dst:   Dynamic string
   :param array: String to concatenate with

----------------------

.. function:: void dstr_cat_dstr(struct dstr *dst, const struct dstr *str)

   Concatenates a dynamic string with another dynamic string.

   :param dst: Dynamic string to concatenate to
   :param str: Dynamic string to concatenate with

----------------------

.. function:: void dstr_cat_ch(struct dstr *dst, char ch)

   Concatenates a dynamic string with a single character.

   :param dst: Dynamic string to concatenate to
   :param ch:  Character to concatenate

----------------------

.. function:: void dstr_ncat(struct dstr *dst, const char *array, const size_t len)

   Concatenates a dynamic string with a specific number of characters
   from a string.

   :param dst:   Dynamic string to concatenate to
   :param array: String to concatenate with
   :param len:   Number of characters to concatenate with

----------------------

.. function:: void dstr_ncat_dstr(struct dstr *dst, const struct dstr *str, const size_t len)

   Concatenates a dynamic string with a specific number of characters
   from another dynamic string.

   :param dst: Dynamic string to concatenate to
   :param str: Dynamic string to concatenate with
   :param len: Number of characters to concatenate with

----------------------

.. function:: void dstr_insert(struct dstr *dst, const size_t idx, const char *array)

   Inserts a string in to a dynamic string at a specific index.

   :param dst:   Dynamic string to insert in to
   :param idx:   Character index to insert at
   :param array: String to insert

----------------------

.. function:: void dstr_insert_dstr(struct dstr *dst, const size_t idx, const struct dstr *str)

   Inserts another dynamic string in to a dynamic string at a specific
   index.

   :param dst:   Dynamic string to insert in to
   :param idx:   Character index to insert at
   :param str:   Dynamic string to insert

----------------------

.. function:: void dstr_insert_ch(struct dstr *dst, const size_t idx, const char ch)

   Inserts a character in to a dynamic string at a specific index.

   :param dst:   Dynamic string to insert in to
   :param idx:   Character index to insert at
   :param ch:    Character to insert

----------------------

.. function:: void dstr_remove(struct dstr *dst, const size_t idx, const size_t count)

   Removes a specific number of characters starting from a specific
   index.

   :param dst:   Dynamic string
   :param idx:   Index to start removing characters at
   :param count: Number of characters to remove

----------------------

.. function:: void dstr_printf(struct dstr *dst, const char *format, ...)
              void dstr_vprintf(struct dstr *dst, const char *format, va_list args)

   Sets a dynamic string to a formatted string.

   :param dst:    Dynamic string
   :param format: Format string

----------------------

.. function:: void dstr_catf(struct dstr *dst, const char *format, ...)
              void dstr_vcatf(struct dstr *dst, const char *format, va_list args)

   Concatenates a dynamic string with a formatted string.

   :param dst:    Dynamic string
   :param format: Format string

----------------------

.. function:: const char *dstr_find_i(const struct dstr *str, const char *find)

   Finds a string within a dynamic string, case insensitive.

   :param str:  Dynamic string
   :param find: String to find
   :return:     Pointer to the first occurrence, or *NULL* if not found

----------------------

.. function:: const char *dstr_find(const struct dstr *str, const char *find)

   Finds a string within a dynamic string.

   :param str:  Dynamic string
   :param find: String to find
   :return:     Pointer to the first occurrence, or *NULL* if not found

----------------------

.. function:: void dstr_replace(struct dstr *str, const char *find, const char *replace)

   Replaces all occurrences of *find* with *replace*.

   :param str:     Dynamic string
   :param find:    String to find
   :param replace: Replacement string

----------------------

.. function:: int dstr_cmp(const struct dstr *str1, const char *str2)

   Compares with a string.

   :param str1: Dynamic string
   :param str2: String to compare
   :return:     0 if equal, nonzero otherwise

----------------------

.. function:: int dstr_cmpi(const struct dstr *str1, const char *str2)

   Compares with a string, case-insensitive.

   :param str1: Dynamic string
   :param str2: String to compare
   :return:     0 if equal, nonzero otherwise

----------------------

.. function:: int dstr_ncmp(const struct dstr *str1, const char *str2, const size_t n)

   Compares a specific number of characters.

   :param str1: Dynamic string
   :param str2: String to compare
   :param n:    Number of characters to compare
   :return:     0 if equal, nonzero otherwise

----------------------

.. function:: int dstr_ncmpi(const struct dstr *str1, const char *str2, const size_t n)

   Compares a specific number of characters, case-insensitive.

   :param str1: Dynamic string
   :param str2: String to compare
   :param n:    Number of characters to compare
   :return:     0 if equal, nonzero otherwise

----------------------

.. function:: void dstr_depad(struct dstr *dst)

   Removes all padding characters (tabs, spaces, CR, LF) from the front
   and ends of a dynamic string.

   :param dst: Dynamic string

----------------------

.. function:: void dstr_left(struct dstr *dst, const struct dstr *str, const size_t pos)

   Copies a certain number of characters from the left side of one
   dynamic string in to another.

   :param dst:   Destination
   :param str:   Source
   :param pos:   Number of characters

----------------------

.. function:: void dstr_mid(struct dstr *dst, const struct dstr *str, const size_t start, const size_t count)

   Copies a certain number of characters from the middle of one dynamic
   string in to another.

   :param dst:   Destination
   :param str:   Source
   :param start: Starting index within *str*
   :param count: Number of characters to copy

----------------------

.. function:: void dstr_right(struct dstr *dst, const struct dstr *str, const size_t pos)

   Copies a certain number of characters from the right of one dynamic
   string in to another.

   :param dst:   Destination
   :param str:   Source
   :param pos:   Index of *str* to copy from

----------------------

.. function:: char dstr_end(const struct dstr *str)

   :param str: Dynamic string
   :return:    The last character of a dynamic string

----------------------

.. function:: void dstr_from_wcs(struct dstr *dst, const wchar_t *wstr)

   Copies a wide string in to a dynamic string and converts it to UTF-8.

   :param dst:  Dynamic string
   :param wstr: Wide string

----------------------

.. function:: wchar_t *dstr_to_wcs(const struct dstr *str)

   Converts a dynamic array to a wide string.  Free with
   :c:func:`bfree()`.

   :param str: Dynamic string
   :return:    Wide string allocation.  Free with :c:func:`bfree()`

----------------------

.. function:: void dstr_to_upper(struct dstr *str)

   Converts all characters within a dynamic array to uppercase.

   :param str: Dynamic string

----------------------

.. function:: void dstr_to_lower(struct dstr *str)

   Converts all characters within a dynamic array to lowercase.

   :param str: Dynamic string
