# - Try to find Gio
# Once done this will define
#
#  GIO_FOUND - system has Gio
#  GIO_INCLUDE_DIRS - the Gio include directory
#  GIO_LIBRARIES - the libraries needed to use Gio
#  GIO_DEFINITIONS - Compiler switches required for using Gio

# Use pkg-config to get the directories and then use these values
# in the find_path() and find_library() calls
find_package(PkgConfig)
pkg_check_modules(PC_GIO gio-2.0)

set(GIO_DEFINITIONS ${PC_GIO_CFLAGS})

find_path(GIO_INCLUDE_DIRS gio.h PATHS ${PC_GIO_INCLUDEDIR} ${PC_GIO_INCLUDE_DIRS} PATH_SUFFIXES glib-2.0/gio/)
find_library(GIO_LIBRARIES NAMES gio-2.0 libgio-2.0 PATHS ${PC_GIO_LIBDIR} ${PC_GIO_LIBRARY_DIRS})
mark_as_advanced(GIO_INCLUDE_DIRS GIO_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Gio REQUIRED_VARS GIO_LIBRARIES GIO_INCLUDE_DIRS)

if(GIO_FOUND)
  set(HAVE_DBUS "1")
else()
  set(HAVE_DBUS "0")
endif()
