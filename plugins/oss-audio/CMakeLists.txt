project(oss-audio)

option(ENABLE_OSS "Enable building with OSS audio support" ON)

if(NOT ENABLE_OSS)
  message(STATUS "OBS: OSS audio support not enabled")
  return()
endif()

find_package(OSS REQUIRED)

add_library(oss-audio MODULE)
add_library(OBS::oss-audio ALIAS oss-audio)

target_sources(oss-audio PRIVATE oss-audio.c oss-input.c)

target_link_libraries(oss-audio PRIVATE OBS::libobs OSS::OSS)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/oss-platform.h.in oss-platform.h)

target_include_directories(oss-audio PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_sources(oss-audio PRIVATE oss-platform.h)

set_target_properties(oss-audio PROPERTIES FOLDER "plugins")

setup_plugin_target(oss-audio)
