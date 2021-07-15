# * Try to find Gio Once done this will define
#
# GIO_FOUND - system has Gio GIO_INCLUDE_DIRS - the Gio include directory
# GIO_LIBRARIES - the libraries needed to use Gio GIO_DEFINITIONS - Compiler
# switches required for using Gio

# Use pkg-config to get the directories and then use these values in the
# find_path() and find_library() calls

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(_GIO gio-2.0 gio-unix-2.0)
endif()

find_path(
  GIO_INCLUDE_DIR
  NAMES gio.h
  HINTS ${_GIO_INCLUDE_DIRS}
  PATHS /usr/include /usr/local/include /opt/local/include /sw/include
  PATH_SUFFIXES glib-2.0/gio/)

find_library(
  GIO_LIB
  NAMES ${_GIO_LIBRARIES} gio-2.0 libgio-2.0 gio-unix-2.0
  HINTS ${_GIO_LIBRARY_DIRS}
  PATHS /usr/lib /usr/local/lib /opt/local/lib /sw/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Gio REQUIRED_VARS GIO_LIB GIO_INCLUDE_DIR)
mark_as_advanced(GIO_INCLUDE_DIR GIO_LIB)

if(GIO_FOUND)
  set(GIO_INCLUDE_DIRS ${GIO_INCLUDE_DIR})
  set(GIO_LIBRARIES ${GIO_LIB})

  if(NOT TARGET GIO::GIO)
    if(IS_ABSOLUTE "${GIO_LIBRARIES}")
      add_library(GIO::GIO UNKNOWN IMPORTED)
      set_target_properties(GIO::GIO PROPERTIES IMPORTED_LOCATION
                                                "${GIO_LIBRARIES}")
    else()
      add_library(GIO::GIO INTERFACE IMPORTED)
      set_target_properties(GIO::GIO PROPERTIES IMPORTED_LIBNAME
                                                "${GIO_LIBRARIES}")
    endif()

    # Special case for gio, as both the
    set_target_properties(GIO::GIO PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                              "${_GIO_INCLUDE_DIRS}")

    target_compile_options(GIO::GIO INTERFACE ${_GIO_CFLAGS})
  endif()
endif()
