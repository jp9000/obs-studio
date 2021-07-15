project(mac-vth264)

find_library(AVFOUNDATION AVFoundation)
find_library(COCOA Cocoa)
find_library(COREFOUNDATION CoreFoundation)
find_library(COREVIDEO CoreVideo)
find_library(VIDEOTOOLBOX VideoToolbox)
find_library(COREMEDIA CoreMedia)

mark_as_advanced(AVFOUNDATION COCOA COREFOUNDATION COREVIDEO VIDEOTOOLBOX
                 COREMEDIA)

add_library(mac-vth264 MODULE)
add_library(OBS::vth264 ALIAS mac-vth264)

target_sources(mac-vth264 PRIVATE encoder.c)

target_link_libraries(
  mac-vth264
  PRIVATE OBS::libobs
          ${AVFOUNDATION}
          ${COCOA}
          ${COREFOUNDATION}
          ${COREVIDEO}
          ${VIDEOTOOLBOX}
          ${COREMEDIA})

set_target_properties(mac-vth264 PROPERTIES FOLDER "plugins" PREFIX "")

setup_plugin_target(mac-vth264)
