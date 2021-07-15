project(obs-libfdk)

option(ENABLE_LIBFDK "Enable FDK AAC support" OFF)

if(NOT ENABLE_LIBFDK)
  message(STATUS "OBS: FDK AAC support not enabled")
  return()
endif()

find_package(Libfdk REQUIRED)

add_library(obs-libfdk MODULE)
add_library(OBS::libfdk ALIAS obs-libfdk)

target_sources(obs-libfdk PRIVATE obs-libfdk.c)

target_link_libraries(obs-libfdk PRIVATE OBS::libobs LibFDK::LibFDK)

set_target_properties(obs-libfdk PROPERTIES FOLDER "plugins" PREFIX "")

setup_plugin_target(obs-libfdk)
