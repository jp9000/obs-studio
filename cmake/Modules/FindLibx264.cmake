# Once done these will be defined:
#
# LIBX264_FOUND LIBX264_INCLUDE_DIRS LIBX264_LIBRARIES
#
# For use in OBS:
#
# X264_INCLUDE_DIR

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(_X264 QUIET x264)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(_lib_suffix 64)
else()
  set(_lib_suffix 32)
endif()

find_path(
  X264_INCLUDE_DIR
  NAMES x264.h
  HINTS ENV X264_PATH ${X264_PATH} ${CMAKE_SOURCE_DIR}/${X264_PATH}
        ${_X264_INCLUDE_DIRS}
  PATHS /usr/include /usr/local/include /opt/local/include /sw/include
  PATH_SUFFIXES include)

find_library(
  X264_LIB
  NAMES ${_X264_LIBRARIES} x264 libx264
  HINTS ENV X264_PATH ${X264_PATH} ${CMAKE_SOURCE_DIR}/${X264_PATH}
        ${_X264_LIBRARY_DIRS}
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

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libx264 DEFAULT_MSG X264_LIB X264_INCLUDE_DIR)
mark_as_advanced(X264_INCLUDE_DIR X264_LIB)

if(LIBX264_FOUND)
  set(LIBX264_INCLUDE_DIRS ${X264_INCLUDE_DIR})
  set(LIBX264_LIBRARIES ${X264_LIB})

  if(NOT TARGET LIBX264::LIBX264)
    if(IS_ABSOLUTE "${LIBX264_LIBRARIES}")
      add_library(LIBX264::LIBX264 UNKNOWN IMPORTED)
      set_target_properties(LIBX264::LIBX264 PROPERTIES IMPORTED_LOCATION
                                                        "${LIBX264_LIBRARIES}")
    else()
      add_library(LIBX264::LIBX264 INTERFACE IMPORTED)
      set_target_properties(LIBX264::LIBX264 PROPERTIES IMPORTED_LIBNAME
                                                        "${LIBX264_LIBRARIES}")
    endif()

    set_target_properties(
      LIBX264::LIBX264 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                  "${LIBX264_INCLUDE_DIRS}")
  endif()
endif()
