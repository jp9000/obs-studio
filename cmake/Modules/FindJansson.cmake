# Once done these will be defined:
#
# JANSSON_FOUND JANSSON_INCLUDE_DIRS JANSSON_LIBRARIES JANSSON_VERSION

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(_JANSSON QUIET jansson)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(_lib_suffix 64)
else()
  set(_lib_suffix 32)
endif()

find_path(
  Jansson_INCLUDE_DIR
  NAMES jansson.h
  HINTS ENV JANSSON_PATH ${JANSSON_PATH} ${CMAKE_SOURCE_DIR}/${JANSSON_PATH}
        ${_JANSSON_INCLUDE_DIRS}
  PATHS /usr/include /usr/local/include /opt/local/include /sw/include)

find_library(
  Jansson_LIB
  NAMES ${_JANSSON_LIBRARIES} jansson libjansson
  HINTS ENV JANSSON_PATH ${JANSSON_PATH} ${CMAKE_SOURCE_DIR}/${JANSSON_PATH}
        ${_JANSSON_LIBRARY_DIRS}
  PATHS /usr/lib /usr/local/lib /opt/local/lib /sw/lib
  PATH_SUFFIXES
    lib${_lib_suffix}
    lib
    libs${_lib_suffix}
    libs
    bin${_lib_suffix}
    bin
    ../lib${_lib_suffix}
    ../lib
    ../libs${_lib_suffix}
    ../libs
    ../bin${_lib_suffix}
    ../bin)

if(JANSSON_VERSION)
  set(_JANSSON_VERSION_STRING "${JANSSON_VERSION}")
elseif(_JANSSON_FOUND AND _JANSSON_VERSION)
  set(_JANSSON_VERSION_STRING "${_JANSSON_VERSION}")
elseif(EXISTS "${Jansson_INCLUDE_DIR}/jansson.h")
  file(STRINGS "${Jansson_INCLUDE_DIR}/jansson.h" _jansson_version_parse
       REGEX "#define[ \t]+JANSSON_VERSION[ \t]+.+")
  string(REGEX REPLACE ".*#define[ \t]+JANSSON_VERSION[ \t]+\"(.+)\".*" "\\1"
                       _JANSSON_VERSION_STRING "${_jansson_version_parse}")
else()
  if(NOT Jansson_FIND_QUIETLY)
    message(WARNING "Failed to find Jansson version")
  endif()
  set(_JANSSON_VERSION_STRING "unknown")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  Jansson
  FOUND_VAR JANSSON_FOUND
  REQUIRED_VARS Jansson_LIB Jansson_INCLUDE_DIR
  VERSION_VAR _JANSSON_VERSION_STRING)
mark_as_advanced(Jansson_INCLUDE_DIR Jansson_LIB)

if(JANSSON_FOUND)
  set(JANSSON_INCLUDE_DIRS ${Jansson_INCLUDE_DIR})
  set(JANSSON_LIBRARIES ${Jansson_LIB})
  set(JANSSON_VERSION ${_JANSSON_VERSION_STRING})

  if(NOT TARGET Jansson::Jansson)
    if(IS_ABSOLUTE "${JANSSON_LIBRARIES}")
      add_library(Jansson::Jansson UNKNOWN IMPORTED GLOBAL)
      set_target_properties(Jansson::Jansson PROPERTIES IMPORTED_LOCATION
                                                        "${JANSSON_LIBRARIES}")
    else()
      add_library(Jansson::Jansson INTERFACE IMPORTED GLOBAL)
      set_target_properties(Jansson::Jansson PROPERTIES IMPORTED_LIBNAME
                                                        "${JANSSON_LIBRARIES}")
    endif()

    set_target_properties(
      Jansson::Jansson PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                  "${JANSSON_INCLUDE_DIRS}")
  endif()
endif()
