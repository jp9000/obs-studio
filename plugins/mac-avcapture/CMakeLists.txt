project(mac-avcapture)

find_library(AVFOUNDATION AVFoundation)
find_library(COCOA Cocoa)
find_library(COREFOUNDATION CoreFoundation)
find_library(COREMEDIA CoreMedia)
find_library(COREVIDEO CoreVideo)
find_library(COCOA Cocoa)
find_library(COREMEDIAIO CoreMediaIO)

include_directories(${AVFOUNDATION}
                    ${COCOA}
                    ${COREFOUNDATION}
                    ${COREMEDIA}
                    ${COREVIDEO}
                    ${COREMEDIAIO}
		    ${COCOA})

set(mac-avcapture_HEADERS
	left-right.hpp
	scope-guard.hpp
	)

set(mac-avcapture_SOURCES
	av-capture.mm)

set_source_files_properties(av-capture.mm
	PROPERTIES COMPILE_FLAGS "-fobjc-arc")

add_library(mac-avcapture MODULE
	${mac-avcapture_SOURCES}
	${mac-avcapture_HEADERS})

target_link_libraries(mac-avcapture
	libobs
	${AVFOUNDATION}
	${COCOA}
	${COREFOUNDATION}
	${COREMEDIA}
	${COREVIDEO}
	${COREMEDIAIO}
	${COCOA})
set_target_properties(mac-avcapture PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(mac-avcapture data)
