# Once done these will be defined:
#
# UDEV_FOUND UDEV_INCLUDE_DIRS UDEV_LIBRARIES

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(_UDEV QUIET libudev)
endif()

find_path(
  UDEV_INCLUDE_DIR
  NAMES libudev.h
  HINTS ${_UDEV_INCLUDE_DIRS}
  PATHS /usr/include /usr/local/include /opt/local/include)

find_library(
  UDEV_LIB
  NAMES udev libudev
  HINTS ${_UDEV_LIBRARY_DIRS}
  PATHS /usr/lib /usr/local/lib /opt/local/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Udev DEFAULT_MSG UDEV_LIB UDEV_INCLUDE_DIR)
mark_as_advanced(UDEV_INCLUDE_DIR UDEV_LIB)

if(UDEV_FOUND)
  set(UDEV_INCLUDE_DIRS ${UDEV_INCLUDE_DIR})
  set(UDEV_LIBRARIES ${UDEV_LIB})

  if(NOT TARGET Udev::Udev)
    if(IS_ABSOLUTE "${UDEV_LIBRARIES}")
      add_library(Udev::Udev UNKNOWN IMPORTED)
      set_target_properties(Udev::Udev PROPERTIES IMPORTED_LOCATION
                                                  "${UDEV_LIBRARIES}")
    else()
      add_library(Udev::Udev INTERFACE IMPORTED)
      set_target_properties(Udev::Udev PROPERTIES IMPORTED_LIBNAME
                                                  "${UDEV_LIBRARIES}")
    endif()

    set_target_properties(Udev::Udev PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                                "${UDEV_INCLUDE_DIRS}")
  endif()
endif()
