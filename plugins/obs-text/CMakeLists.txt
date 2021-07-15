project(obs-text)

add_library(obs-text MODULE)
add_library(OBS::text ALIAS obs-text)

target_link_libraries(obs-text PRIVATE OBS::libobs)

set_target_properties(obs-text PROPERTIES FOLDER "plugins")

set(MODULE_DESCRIPTION "OBS GDI+ text module")
configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
               obs-text.rc)

target_sources(obs-text PRIVATE gdiplus/obs-text.cpp obs-text.rc)

target_link_libraries(obs-text PRIVATE gdiplus)

target_compile_definitions(
  obs-text PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS
                   _CRT_NONSTDC_NO_WARNINGS)

setup_plugin_target(obs-text)
