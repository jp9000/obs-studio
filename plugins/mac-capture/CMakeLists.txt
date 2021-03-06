project(mac-capture)

find_library(COREAUDIO CoreAudio)
find_library(AUDIOUNIT AudioUnit)
find_library(COREFOUNDATION CoreFoundation)
find_library(IOSURF IOSurface)
find_library(COCOA Cocoa)

include_directories(${COREAUDIO}
                    ${AUDIOUNIT}
                    ${COREFOUNDATION}
		    ${IOSURF}
		    ${COCOA})

set(mac-capture_HEADERS
	audio-device-enum.h
	window-utils.h)

set(mac-capture_SOURCES
	plugin-main.c
	audio-device-enum.c
	mac-audio.c
	mac-display-capture.m
	mac-window-capture.m
	window-utils.m)

add_library(mac-capture MODULE
	${mac-capture_SOURCES}
	${mac-capture_HEADERS})
target_link_libraries(mac-capture
	libobs
	${COREAUDIO}
	${AUDIOUNIT}
	${COREFOUNDATION}
	${IOSURF}
	${COCOA})
set_target_properties(mac-capture PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(mac-capture data)
