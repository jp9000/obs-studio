project(get-graphics-offsets)

add_executable(get-graphics-offsets)

target_sources(
  get-graphics-offsets
  PRIVATE get-graphics-offsets.c
          get-graphics-offsets.h
          dxgi-offsets.cpp
          d3d8-offsets.cpp
          d3d9-offsets.cpp
          ../graphics-hook-info.h
          ../hook-helpers.h)

target_include_directories(
  get-graphics-offsets
  PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>")

target_link_libraries(get-graphics-offsets d3d9.lib dxgi.lib d3d11.lib)

if(MSVC)
  target_compile_options(get-graphics-offsets
                         PRIVATE "$<IF:$<CONFIG:Debug>,/MTd,/MT>")
  add_target_resource(get-graphics-offsets "$<TARGET_PDB_FILE:get-graphics-offsets>"
                      "obs-plugins/win-capture/")
endif()

set_target_properties(get-graphics-offsets PROPERTIES FOLDER
                                                      "plugins/win-capture")
set_target_properties(
  get-graphics-offsets
  PROPERTIES OUTPUT_NAME
             "get-graphics-offsets$<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,64,32>"
)

add_target_resource(get-graphics-offsets "$<TARGET_FILE:get-graphics-offsets>"
                    "obs-plugins/win-capture/")
