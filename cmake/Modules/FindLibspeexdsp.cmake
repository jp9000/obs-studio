# Once done these will be defined:
#
# LIBSPEEXDSP_FOUND LIBSPEEXDSP_INCLUDE_DIRS LIBSPEEXDSP_LIBRARIES
#
# For use in OBS:
#
# SPEEXDSP_INCLUDE_DIR

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(_SPEEXDSP QUIET speexdsp libspeexdsp)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(_lib_suffix 64)
else()
  set(_lib_suffix 32)
endif()

find_path(
  SPEEXDSP_INCLUDE_DIR
  NAMES speex/speex_preprocess.h
  HINTS ENV SPEEX_PATH ${SPEEX_PATH} ${CMAKE_SOURCE_DIR}/${SPEEX_PATH}
        ${_SPEEXDSP_INCLUDE_DIRS}
  PATHS /usr/include /usr/local/include /opt/local/include /sw/include
  PATH_SUFFIXES include)

find_library(
  SPEEXDSP_LIB
  NAMES ${_SPEEXDSP_LIBRARIES} speexdsp libspeexdsp
  HINTS ENV SPEEX_PATH ${SPEEX_PATH} ${CMAKE_SOURCE_DIR}/${SPEEX_PATH}
        ${_SPEEXDSP_LIBRARY_DIRS}
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
find_package_handle_standard_args(Libspeexdsp DEFAULT_MSG SPEEXDSP_LIB
                                  SPEEXDSP_INCLUDE_DIR)
mark_as_advanced(SPEEXDSP_INCLUDE_DIR SPEEXDSP_LIB)

if(LIBSPEEXDSP_FOUND)
  set(LIBSPEEXDSP_INCLUDE_DIRS ${SPEEXDSP_INCLUDE_DIR})
  set(LIBSPEEXDSP_LIBRARIES ${SPEEXDSP_LIB})

  if(NOT TARGET LibspeexDSP::LibspeexDSP)
    if(IS_ABSOLUTE "${LIBSPEEXDSP_LIBRARIES}")
      add_library(LibspeexDSP::LibspeexDSP UNKNOWN IMPORTED)
      set_target_properties(
        LibspeexDSP::LibspeexDSP PROPERTIES IMPORTED_LOCATION
                                            "${LIBSPEEXDSP_LIBRARIES}")
    else()
      add_library(LibspeexDSP::LibspeexDSP INTERFACE IMPORTED)
      set_target_properties(
        LibspeexDSP::LibspeexDSP PROPERTIES IMPORTED_LIBNAME
                                            "${LIBSPEEXDSP_LIBRARIES}")
    endif()

    set_target_properties(
      LibspeexDSP::LibspeexDSP PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                          "${LIBSPEEXDSP_INCLUDE_DIRS}")
  endif()
endif()
