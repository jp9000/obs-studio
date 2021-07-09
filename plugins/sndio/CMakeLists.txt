project(sndio)

option(ENABLE_SNDIO "Build OBS with sndio support" OFF)
if(NOT ENABLE_SNDIO)
  message(STATUS "OBS: sndio support not enabled")
  return()
endif()

find_package(Sndio REQUIRED)

add_library(sndio MODULE)
add_library(OBS::sndio ALIAS sndio)

target_sources(sndio PRIVATE sndio.c sndio-input.c)

target_link_libraries(sndio PRIVATE OBS::libobs Sndio::Sndio)

set_target_properties(sndio PROPERTIES FOLDER "plugins")

setup_plugin_target(sndio)
