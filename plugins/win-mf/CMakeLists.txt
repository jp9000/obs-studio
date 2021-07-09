option(ENABLE_WINMF "Enable building with deprecated win-mf plugin" OFF)
mark_as_advanced(ENABLE_WINMF)

project(win-mf)

add_library(win-mf MODULE)
add_library(OBS::win-mf ALIAS win-mf)

set(MODULE_DESCRIPTION "OBS Windows Media Foundations H.264/AAC encoder")
configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
               win-mf.rc)

target_sources(win-mf PRIVATE win-mf.rc)

if(ENABLE_WINMF)
  target_sources(
    win-mf
    PRIVATE mf-plugin.cpp
            mf-aac.cpp
            mf-aac-encoder.cpp
            mf-aac-encoder.hpp
            mf-common.cpp
            mf-common.hpp
            mf-encoder-descriptor.cpp
            mf-encoder-descriptor.hpp
            mf-h264.cpp
            mf-h264-encoder.cpp
            mf-h264-encoder.hpp)

  target_link_libraries(
    win-mf
    PRIVATE d3d9
            dxva2
            uuid
            mfplat
            mfuuid
            mf
            wmcodecdspuuid)

  target_compile_definitions(win-mf PRIVATE ENABLE_WINMF)
else()
  target_sources(win-mf PRIVATE mf-plugin.cpp win-mf.rc)
endif()

target_link_libraries(win-mf PRIVATE OBS::libobs)

set_target_properties(win-mf PROPERTIES FOLDER "plugins")

setup_plugin_target(win-mf)
