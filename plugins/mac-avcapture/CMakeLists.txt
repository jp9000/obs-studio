project(mac-avcapture)

find_library(AVFOUNDATION AVFoundation)
find_library(COCOA Cocoa)
find_library(COREFOUNDATION CoreFoundation)
find_library(COREMEDIA CoreMedia)
find_library(COREVIDEO CoreVideo)
find_library(COREMEDIAIO CoreMediaIO)

mark_as_advanced(AVFOUNDATION COCOA COREFOUNDATION COREMEDIA COREMEDIAIO
                 COREVIDEO)

add_library(mac-avcapture MODULE)
add_library(OBS::avcapture ALIAS mac-avcapture)

target_sources(mac-avcapture PRIVATE av-capture.mm left-right.hpp
                                     scope-guard.hpp)

target_compile_features(mac-avcapture PRIVATE cxx_std_11)

set_source_files_properties(av-capture.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")

target_link_libraries(
  mac-avcapture
  PRIVATE OBS::libobs
          ${AVFOUNDATION}
          ${COCOA}
          ${COREFOUNDATION}
          ${COREMEDIA}
          ${COREVIDEO}
          ${COREMEDIAIO})

set_target_properties(mac-avcapture PROPERTIES FOLDER "plugins" PREFIX "")

setup_plugin_target(mac-avcapture)
