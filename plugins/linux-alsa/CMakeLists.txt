project(linux-alsa)

option(ENABLE_ALSA "Build OBS with ALSA support" ON)
if(NOT ENABLE_ALSA)
  message(STATUS "OBS: ALSA support not enabled")
  return()
endif()

find_package(ALSA REQUIRED)

add_library(linux-alsa MODULE)
add_library(OBS::alsa ALIAS linux-alsa)

target_sources(linux-alsa PRIVATE linux-alsa.c alsa-input.c)

target_link_libraries(linux-alsa PRIVATE OBS::libobs ALSA::ALSA)

set_target_properties(linux-alsa PROPERTIES FOLDER "plugins")

setup_plugin_target(linux-alsa)
