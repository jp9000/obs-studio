# Once done these will be defined:
#
# LUAJIT_FOUND LUAJIT_INCLUDE_DIRS LUAJIT_LIBRARIES
#
# For use in OBS:
#
# LUAJIT_INCLUDE_DIR

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(_LIB_SUFFIX 64)
else()
  set(_LIB_SUFFIX 32)
endif()

find_path(
  LUAJIT_INCLUDE_DIR
  NAMES lua.h lualib.h
  HINTS ENV LUAJIT_PATH ${LUAJIT_PATH} ${CMAKE_SOURCE_DIR}/${LUAJIT_PATH}
        ${_LUAJIT_INCLUDE_DIRS}
  PATHS /usr/include
        /usr/local/include
        /opt/local/include
        /opt/local
        /sw/include
        ~/Library/Frameworks
        /Library/Frameworks
  PATH_SUFFIXES
    include
    luajit
    luajit/src
    include/luajit
    include/luajit/src
    luajit-2.0
    include/luajit-2.0
    luajit2.0
    include/luajit2.0
    luajit-2.1
    include/luajit-2.1
    luajit2.1
    include/luajit2.1)

find_library(
  LUAJIT_LIB
  NAMES ${_LUAJIT_LIBRARIES} luajit luajit-51 luajit-5.1 lua51
  HINTS ENV LUAJIT_PATH ${LUAJIT_PATH} ${CMAKE_SOURCE_DIR}/${LUAJIT_PATH}
        ${_LUAJIT_LIBRARY_DIRS}
  PATHS /usr/lib
        /usr/local/lib
        /opt/local/lib
        /opt/local
        /sw/lib
        ~/Library/Frameworks
        /Library/Frameworks
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
find_package_handle_standard_args(Luajit DEFAULT_MSG LUAJIT_LIB
                                  LUAJIT_INCLUDE_DIR)
mark_as_advanced(LUAJIT_INCLUDE_DIR LUAJIT_LIB)

if(LUAJIT_FOUND)
  set(LUAJIT_INCLUDE_DIRS ${LUAJIT_INCLUDE_DIR})
  set(LUAJIT_LIBRARIES ${LUAJIT_LIB})

  if(NOT TARGET Luajit::Luajit)
    if(IS_ABSOLUTE "${LUAJIT_LIBRARIES}")
      add_library(Luajit::Luajit UNKNOWN IMPORTED)
      set_target_properties(Luajit::Luajit PROPERTIES IMPORTED_LOCATION
                                                      "${LUAJIT_LIBRARIES}")
    else()
      add_library(Luajit::Luajit INTERFACE IMPORTED)
      set_target_properties(Luajit::Luajit PROPERTIES IMPORTED_LIBNAME
                                                      "${LUAJIT_LIBRARIES}")
    endif()

    set_target_properties(
      Luajit::Luajit PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                "${LUAJIT_INCLUDE_DIRS}")
  endif()
endif()
