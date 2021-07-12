project(libobs-d3d11)

add_library(libobs-d3d11 MODULE)
add_library(OBS::libobs-d3d11 ALIAS libobs-d3d11)

target_sources(
  libobs-d3d11
  PRIVATE d3d11-indexbuffer.cpp
          d3d11-samplerstate.cpp
          d3d11-shader.cpp
          d3d11-shaderprocessor.cpp
          d3d11-shaderprocessor.hpp
          d3d11-stagesurf.cpp
          d3d11-subsystem.cpp
          d3d11-subsystem.hpp
          d3d11-texture2d.cpp
          d3d11-texture3d.cpp
          d3d11-vertexbuffer.cpp
          d3d11-duplicator.cpp
          d3d11-rebuild.cpp
          d3d11-zstencilbuffer.cpp
          intel-nv12-support.hpp)

set(MODULE_DESCRIPTION "OBS Library D3D11 wrapper")
configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
               libobs-d3d11.rc)

target_include_directories(libobs-d3d11 PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_sources(libobs-d3d11 PRIVATE libobs-d3d11.rc)

target_compile_definitions(
  libobs-d3d11 PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS
                       _CRT_NONSTDC_NO_WARNINGS)

if(NOT DEFINED GPU_PRIORITY_VAL
   OR "x${GPU_PRIORITY_VAL}x" STREQUAL "xx"
   OR "${GPU_PRIORITY_VAL}" STREQUAL "0")
  target_compile_definitions(libobs-d3d11 PRIVATE USE_GPU_PRIORITY=FALSE
                                                  GPU_PRIORITY_VAL=0)
else()
  target_compile_definitions(
    libobs-d3d11 PRIVATE USE_GPU_PRIORITY=TRUE
                         GPU_PRIORITY_VAL=${GPU_PRIORITY_VAL})
endif()

target_link_libraries(libobs-d3d11 PRIVATE OBS::libobs d3d9 d3d11 dxgi)

set_target_properties(
  libobs-d3d11
  PROPERTIES OUTPUT_NAME libobs-d3d11
             FOLDER "core"
             PREFIX "")

setup_binary_target(libobs-d3d11)
