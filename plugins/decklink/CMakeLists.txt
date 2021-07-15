project(decklink)

option(ENABLE_DECKLINK "Build OBS with Decklink support" ON)
if(NOT ENABLE_DECKLINK)
  message(STATUS "OBS: Decklink support not enabled")
  return()
endif()

if(OS_WINDOWS)
  include(IDLFileHelper)
  add_idl_files(win-decklink-sdk_GENERATED_FILES
                win/decklink-sdk/DeckLinkAPI.idl)
endif()

add_library(decklink MODULE)
add_library(OBS::decklink ALIAS decklink)

add_library(decklink-sdk INTERFACE)
add_library(Decklink::SDK ALIAS decklink-sdk)

target_sources(
  decklink
  PRIVATE OBSVideoFrame.cpp
          OBSVideoFrame.h
          audio-repack.c
          audio-repack.h
          audio-repack.hpp
          const.h
          decklink-device.cpp
          decklink-device.hpp
          decklink-devices.cpp
          decklink-devices.hpp
          decklink-device-discovery.cpp
          decklink-device-discovery.hpp
          decklink-device-instance.cpp
          decklink-device-instance.hpp
          decklink-device-mode.cpp
          decklink-device-mode.hpp
          decklink-output.cpp
          decklink-source.cpp
          DecklinkBase.cpp
          DecklinkBase.h
          DecklinkInput.cpp
          DecklinkInput.hpp
          DecklinkOutput.cpp
          DecklinkOutput.hpp
          platform.hpp
          plugin-main.cpp
          util.cpp
          util.hpp)

target_include_directories(decklink-sdk INTERFACE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(decklink PRIVATE OBS::libobs OBS::caption Decklink::SDK)

set_target_properties(decklink PROPERTIES FOLDER "plugins/decklink")

if(OS_WINDOWS)
  set(MODULE_DESCRIPTION "OBS DeckLink Windows module")
  configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
                 win-decklink.rc)

  target_sources(decklink PRIVATE win/platform.cpp win-decklink.rc)

  target_sources(decklink-sdk INTERFACE win/decklink-sdk/DeckLinkAPIVersion.h
                                        ${win-decklink-sdk_GENERATED_FILES})

elseif(OS_MACOS)
  find_library(COREFOUNDATION CoreFoundation)
  mark_as_advanced(COREFOUNDATION)

  target_sources(decklink PRIVATE mac/platform.cpp)

  target_sources(
    decklink-sdk
    INTERFACE mac/decklink-sdk/DeckLinkAPIDispatch.cpp
              mac/decklink-sdk/DeckLinkAPI.h
              mac/decklink-sdk/DeckLinkAPIConfiguration.h
              mac/decklink-sdk/DeckLinkAPIDeckControl.h
              mac/decklink-sdk/DeckLinkAPIDiscovery.h
              mac/decklink-sdk/DeckLinkAPIModes.h
              mac/decklink-sdk/DeckLinkAPIStreaming.h
              mac/decklink-sdk/DeckLinkAPITypes.h
              mac/decklink-sdk/DeckLinkAPIVersion.h)

  target_link_libraries(decklink PRIVATE ${COREFOUNDATION})

  target_compile_features(decklink PRIVATE cxx_auto_type)
elseif(OS_POSIX)
  target_sources(decklink PRIVATE linux/platform.cpp)

  target_sources(
    decklink-sdk
    INTERFACE linux/decklink-sdk/DeckLinkAPIDispatch.cpp
              linux/decklink-sdk/DeckLinkAPI.h
              linux/decklink-sdk/DeckLinkAPIConfiguration.h
              linux/decklink-sdk/DeckLinkAPIDeckControl.h
              linux/decklink-sdk/DeckLinkAPIDiscovery.h
              linux/decklink-sdk/DeckLinkAPIModes.h
              linux/decklink-sdk/DeckLinkAPITypes.h
              linux/decklink-sdk/DeckLinkAPIVersion.h
              linux/decklink-sdk/LinuxCOM.h)
endif()

setup_plugin_target(decklink)
setup_target_resources(decklink "obs-plugins/decklink")
