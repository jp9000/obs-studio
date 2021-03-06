# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindSndio
-------

Finds the Sndio library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``Sndio::Sndio``
  The Sndio library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Sndio_FOUND``
  True if the system has the Sndio library.
``Sndio_VERSION``
  The version of the Sndio library which was found.
``Sndio_INCLUDE_DIRS``
  Include directories needed to use Sndio.
``Sndio_LIBRARIES``
  Libraries needed to link to Sndio.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Sndio_INCLUDE_DIR``
  The directory containing ``sndio.h``.
``Sndio_LIBRARY``
  The path to the Sndio library.

#]=======================================================================]

find_path(Sndio_INCLUDE_DIR sndio.h)
find_library(Sndio_LIBRARY sndio)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sndio
  FOUND_VAR Sndio_FOUND
  REQUIRED_VARS
    Sndio_LIBRARY
    Sndio_INCLUDE_DIR
)

if(Sndio_FOUND)
  set(Sndio_LIBRARIES ${Sndio_LIBRARY})
  set(Sndio_INCLUDE_DIRS ${Sndio_INCLUDE_DIR})
endif()

if(Sndio_FOUND AND NOT TARGET Sndio::Sndio)
  add_library(Sndio::Sndio UNKNOWN IMPORTED)
  set_target_properties(Sndio::Sndio PROPERTIES
    IMPORTED_LOCATION "${Sndio_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${Sndio_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(
  Sndio_INCLUDE_DIR
  Sndio_LIBRARY
)
