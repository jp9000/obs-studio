project(mac-vth264)

find_library(AVFOUNDATION AVFoundation)
find_library(COCOA Cocoa)
find_library(COREFOUNDATION CoreFoundation)
find_library(COREVIDEO CoreVideo)
find_library(VIDEOTOOLBOX VideoToolbox)
find_library(COREMEDIA CoreMedia)

include_directories(${AVFOUNDATION}
	${COCOA}
	${COREFOUNDATION}
	${COREVIDEO}
	${VIDEOTOOLBOX}
	${COREMEDIA})

set(mac-vth264_SOURCES
	encoder.c)

add_library(mac-vth264 MODULE
	${mac-vth264_SOURCES})

target_link_libraries(mac-vth264
	libobs
	${AVFOUNDATION}
	${COCOA}
	${COREFOUNDATION}
	${COREVIDEO}
	${VIDEOTOOLBOX}
	${COREMEDIA})
set_target_properties(mac-vth264 PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(mac-vth264 data)
