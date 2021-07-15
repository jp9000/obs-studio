project(win-wasapi)

add_library(win-wasapi MODULE)
add_library(OBS::wasapi ALIAS win-wasapi)

target_sources(win-wasapi PRIVATE win-wasapi.cpp enum-wasapi.cpp
                                  enum-wasapi.hpp plugin-main.cpp)

set(MODULE_DESCRIPTION "OBS WASAPI module")

configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
               win-wasapi.rc)

target_sources(win-wasapi PRIVATE win-wasapi.rc)

target_link_libraries(win-wasapi PRIVATE OBS::libobs)

set_target_properties(win-wasapi PROPERTIES FOLDER "plugins")

setup_plugin_target(win-wasapi)
