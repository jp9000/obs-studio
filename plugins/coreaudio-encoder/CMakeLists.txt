project(coreaudio-encoder)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  option(ENABLE_COREAUDIO_ENCODER
         "Enable building with CoreAudio encoder (Windows)" ON)
  if(NOT ENABLE_COREAUDIO_ENCODER)
    message(STATUS "OBS: CoreAudio encoder not enabled.")
    return()
  endif()
endif()

add_library(coreaudio-encoder MODULE)
add_library(OBS::coreaudio-encoder ALIAS coreaudio-encoder)

target_sources(coreaudio-encoder PRIVATE encoder.cpp)

set_target_properties(
  coreaudio-encoder
  PROPERTIES OUTPUT_NAME "coreaudio-encoder"
             FOLDER "plugins"
             PREFIX "")

target_compile_features(coreaudio-encoder PRIVATE cxx_std_11)

target_link_libraries(coreaudio-encoder PRIVATE OBS::libobs)

if(OS_MACOS)
  find_library(COREFOUNDATION CoreFoundation)
  find_library(COREAUDIO CoreAudio)
  find_library(AUDIOTOOLBOX AudioToolbox)

  mark_as_advanced(AUDIOTOOLBOX COREAUDIO COREFOUNDATION)

  target_link_libraries(coreaudio-encoder PRIVATE ${COREFOUNDATION}
                                                  ${COREAUDIO} ${AUDIOTOOLBOX})

elseif(OS_WINDOWS)
  if(MINGW)
    set_source_files_properties(encoder.cpp PROPERTIES COMPILE_FLAGS
                                                       "-Wno-multichar")
  endif()

  target_compile_definitions(coreaudio-encoder PRIVATE UNICODE _UNICODE)

  set(MODULE_DESCRIPTION "OBS CoreAudio encoder")
  configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
                 coreaudio-encoder.rc)

  target_sources(coreaudio-encoder PRIVATE coreaudio-encoder.rc
                                           windows-imports.h)
endif()

setup_plugin_target(coreaudio-encoder)
