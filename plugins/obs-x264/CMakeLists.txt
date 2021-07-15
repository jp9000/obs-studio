project(obs-x264)

find_package(Libx264 REQUIRED)

add_library(obs-x264 MODULE)
add_library(OBS::x264 ALIAS obs-x264)
add_library(obs-x264-util INTERFACE)
add_executable(obs-x264-test)

target_sources(obs-x264-util INTERFACE obs-x264-options.h obs-x264-options.c)

target_link_libraries(obs-x264-util INTERFACE OBS::libobs)

target_sources(obs-x264-test PRIVATE obs-x264-test.c)

target_link_libraries(obs-x264-test PRIVATE obs-x264-util)

target_sources(obs-x264 PRIVATE obs-x264.c obs-x264-plugin-main.c)

target_link_libraries(obs-x264 PRIVATE LIBX264::LIBX264 obs-x264-util)

set_target_properties(obs-x264 PROPERTIES FOLDER "plugins" PREFIX "")

if(OS_WINDOWS)
  set(MODULE_DESCRIPTION "OBS x264 encoder")
  configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
                 obs-x264.rc)

  target_sources(obs-x264 PRIVATE obs-x264.rc)

endif()

set_target_properties(obs-x264-test PROPERTIES FOLDER "plugins")
add_test(NAME obs-x264-test COMMAND obs-x264-test)

setup_plugin_target(obs-x264)
