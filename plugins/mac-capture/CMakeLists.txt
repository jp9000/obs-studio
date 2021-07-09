project(mac-capture)

find_library(COREAUDIO CoreAudio)
find_library(AUDIOUNIT AudioUnit)
find_library(COREFOUNDATION CoreFoundation)
find_library(IOSURF IOSurface)
find_library(COCOA Cocoa)

add_library(mac-capture MODULE)
add_library(OBS::capture ALIAS mac-capture)

target_sources(
  mac-capture
  PRIVATE plugin-main.c
          audio-device-enum.c
          audio-device-enum.h
          mac-audio.c
          mac-display-capture.m
          mac-window-capture.m
          window-utils.m
          window-utils.h)

target_link_libraries(mac-capture PRIVATE OBS::libobs ${COREAUDIO} ${AUDIOUNIT}
                                          ${COREFOUNDATION} ${IOSURF} ${COCOA})

set_target_properties(mac-capture PROPERTIES FOLDER "plugins" PREFIX "")

setup_plugin_target(mac-capture)
