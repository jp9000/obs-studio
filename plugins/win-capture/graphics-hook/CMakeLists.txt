project(graphics-hook)

if(NOT DEFINED ENABLE_D3D12_HOOK)
  set(ENABLE_D3D12_HOOK
      OFF
      CACHE BOOL "Enable building D3D12 hook support (requires Windows 10 SDK)"
            FORCE)
endif()

find_package(Detours REQUIRED)
find_package(Vulkan REQUIRED)

add_library(graphics-hook MODULE)
add_library(OBS::graphics-hook ALIAS graphics-hook)

target_sources(
  graphics-hook
  PRIVATE graphics-hook.c
          graphics-hook.h
          gl-capture.c
          gl-decs.h
          d3d8-capture.cpp
          d3d9-capture.cpp
          d3d9-patches.hpp
          dxgi-capture.cpp
          d3d10-capture.cpp
          d3d11-capture.cpp
          d3d12-capture.cpp
          ../funchook.c
          ../funchook.h
          ../obfuscate.c
          ../obfuscate.h
          ../graphics-hook-ver.h
          ../graphics-hook-info.h
          ../hook-helpers.h
          graphics-hook.rc)

target_include_directories(
  graphics-hook PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>")

target_link_libraries(graphics-hook PRIVATE OBS::ipc-util Detours::Detours
                                            dxguid psapi)

if(MSVC)
  target_compile_options(graphics-hook PRIVATE "$<IF:$<CONFIG:Debug>,/MTd,/MT>")
  add_target_resource(graphics-hook "$<TARGET_PDB_FILE:graphics-hook>"
                      "obs-plugins/win-capture/")
endif()

set_target_properties(
  graphics-hook
  PROPERTIES FOLDER "plugins/win-capture"
             OUTPUT_NAME
             "graphics-hook$<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,64,32>")

if(ENABLE_D3D12_HOOK)
  target_compile_definitions(graphics-hook PRIVATE COMPILE_D3D12_HOOK)
endif()

if(TARGET Vulkan::Vulkan)
  target_sources(graphics-hook PRIVATE vulkan-capture.c vulkan-capture.h)

  target_link_libraries(graphics-hook PRIVATE Vulkan::Vulkan)

  target_compile_definitions(graphics-hook PRIVATE COMPILE_VULKAN_HOOK)

  add_custom_command(
    TARGET graphics-hook
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/obs-vulkan64.json"
      "${OBS_OUTPUT_DIR}/$<CONFIGURATION>/data/obs-plugins/win-capture/obs-vulkan64.json"
    COMMAND
      "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/obs-vulkan32.json"
      "${OBS_OUTPUT_DIR}/$<CONFIGURATION>/data/obs-plugins/win-capture/obs-vulkan32.json"
    VERBATIM)
endif()

add_target_resource(graphics-hook "$<TARGET_FILE:graphics-hook>"
                    "obs-plugins/win-capture/")
