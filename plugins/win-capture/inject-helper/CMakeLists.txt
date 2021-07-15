project(inject-helper)

add_executable(inject-helper)

target_sources(
  inject-helper PRIVATE inject-helper.c ../inject-library.c ../inject-library.h
                        ../obfuscate.c ../obfuscate.h)

target_include_directories(
  inject-helper PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>")

if(MSVC)
  target_compile_options(inject-helper PRIVATE "$<IF:$<CONFIG:Debug>,/MTd,/MT>")
  add_target_resource(inject-helper "$<TARGET_PDB_FILE:inject-helper>"
                      "obs-plugins/win-capture/")
endif()

set_target_properties(
  inject-helper
  PROPERTIES FOLDER "plugins/win-capture"
             OUTPUT_NAME
             "inject-helper$<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,64,32>")

add_target_resource(inject-helper "$<TARGET_FILE:inject-helper>"
                    "obs-plugins/win-capture/")
