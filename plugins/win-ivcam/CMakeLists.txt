option(ENABLE_IVCAM "Enable building with Realsense camera support" OFF)

if(NOT ENABLE_IVCAM)
  message(STATUS "OBS: Realsense camera plugins support not enabled")
  return()
endif()

project(win-ivcam)

find_package(RSSDK REQUIRED)

if(NOT TARGET RSS::SDK)
  message(FATAL_ERROR "RSSDK not found")
endif()

include(IDLFileHelper)

add_library(win-ivcam MODULE)
add_library(OBS::ivcam ALIAS win-ivcam)

add_library(seglibrary INTERFACE)
add_library(Intel::seglibrary ALIAS seglibrary)

add_idl_files(win-ivcam_seg_library_GENERATED_FILES seg_service/seg_service.idl)

target_sources(
  seglibrary
  INTERFACE seg_library/Dependencies.h
            seg_library/SerServer.cpp
            seg_library/SegServer.h
            seg_library/SegServerImpl.cpp
            seg_library/SegServerImpl.h
            seg_library/SegService.h
            seg_library/SegImage.cpp
            seg_library/SegImage.h
            ${win-ivcam_seg_library_GENERATED_FILES})

set(MODULE_DESCRIPTION "OBS Intel(R) RealSense(TM) module")

configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
               win-ivcam.rc)

target_sources(win-ivcam PRIVATE realsense.cpp win-ivcam.rc)

target_link_libraries(win-ivcam OBS::libobs Intel::seglibrary)

get_target_property(_SEGLIBRARY_SOURCES INTEL::seglibrary INTERFACE_SOURCES)

foreach(_SEGLIBRARY_SOURCE ${_SEGLIBRARY_SOURCES})
  get_filename_component(_EXT ${_SEGLIBRARY_SOURCE} EXT)
  if(${_EXT} STREQUAL "hpp" OR ${_EXT} STREQUAL "h")
    source_group("seg_library\\Header Files" FILES ${_SEGLIBRARY_SOURCE})
  elseif(${_EXT} STREQUAL "cpp" OR ${_EXT} STREQUAL "c")
    source_group("seg_library\\Source Files" FILES ${_SEGLIBRARY_SOURCE})
  endif()
endforeach()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  add_subdirectory(seg_service)
endif()

setup_plugin_target(win-ivcam)
