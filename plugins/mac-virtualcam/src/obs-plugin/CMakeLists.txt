project(mac-virtualcam)

find_library(AVFOUNDATION AVFoundation)
find_library(APPKIT AppKit)
find_library(COCOA Cocoa)
find_library(COREFOUNDATION CoreFoundation)
find_library(COREMEDIA CoreMedia)
find_library(COREVIDEO CoreVideo)
find_library(COCOA Cocoa)
find_library(COREMEDIAIO CoreMediaIO)
find_library(IOSURFACE IOSurface)
find_library(IOKIT IOKit)

include_directories(${AVFOUNDATION}
					${APPKIT}
					${COCOA}
					${COREFOUNDATION}
					${COREMEDIA}
					${COREVIDEO}
					${COREMEDIAIO}
					${COCOA}
					${IOSURFACE}
					"${CMAKE_SOURCE_DIR}/UI/obs-frontend-api"
					../common)

set(mac-virtualcam_HEADERS
	Defines.h
	OBSDALMachServer.h
	../common/MachProtocol.h)

set(mac-virtualcam_SOURCES
	plugin-main.mm
	OBSDALMachServer.mm)

add_library(mac-virtualcam MODULE
	${mac-virtualcam_SOURCES}
	${mac-virtualcam_HEADERS})

target_link_libraries(mac-virtualcam
	libobs
	obs-frontend-api
	Qt5::Core
	Qt5::Widgets
	${AVFOUNDATION}
	${APPKIT}
	${COCOA}
	${COREFOUNDATION}
	${COREMEDIA}
	${COREVIDEO}
	${COREMEDIAIO}
	${IOSURFACE}
	${IOKIT})

set_target_properties(mac-virtualcam PROPERTIES
	FOLDER "plugins"
	COMPILE_FLAGS "-std=gnu++14 -stdlib=libc++ -fobjc-arc -fobjc-weak"
)

install_obs_plugin_with_data(mac-virtualcam data)
