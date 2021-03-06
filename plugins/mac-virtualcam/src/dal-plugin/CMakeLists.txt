# Universal build for Apple Silicon
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")

project(mac-dal-plugin)

find_library(AVFOUNDATION AVFoundation)
find_library(COCOA Cocoa)
find_library(COREFOUNDATION CoreFoundation)
find_library(COREMEDIA CoreMedia)
find_library(COREVIDEO CoreVideo)
find_library(COCOA Cocoa)
find_library(COREMEDIAIO CoreMediaIO)
find_library(IOSURFACE IOSurface)
find_library(IOKIT IOKit)

# Possible we could remove osme of these
include_directories(${AVFOUNDATION}
					${COCOA}
					${COREFOUNDATION}
					${COREMEDIA}
					${COREVIDEO}
					${COREMEDIAIO}
					${COCOA}
					${IOSURFACE}
					./
					../common)

set(mac-dal-plugin_HEADERS
	Defines.h
	Logging.h
	OBSDALPlugInInterface.h
    OBSDALObjectStore.h
	OBSDALPlugIn.h
	OBSDALDevice.h
    OBSDALStream.h
	CMSampleBufferUtils.h
	OBSDALMachClient.h
	../common/MachProtocol.h)

set(mac-dal-plugin_SOURCES
	OBSDALPlugInMain.mm
	OBSDALPlugInInterface.mm
	OBSDALObjectStore.mm
	OBSDALPlugIn.mm
	OBSDALDevice.mm
    OBSDALStream.mm
	CMSampleBufferUtils.mm
	OBSDALMachClient.mm)

add_library(mac-dal-plugin MODULE
	${mac-dal-plugin_SOURCES}
	${mac-dal-plugin_HEADERS})

set_target_properties(mac-dal-plugin PROPERTIES
	FOLDER "plugins"
	BUNDLE TRUE
	OUTPUT_NAME "obs-mac-virtualcam"
	COMPILE_FLAGS "-std=gnu++14 -stdlib=libc++ -fobjc-arc -fobjc-weak")

if (XCODE)
	set(TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/Debug")
else (XCODE)
	set(TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}")
endif (XCODE)

target_link_libraries(mac-dal-plugin
	${AVFOUNDATION}
	${COCOA}
	${COREFOUNDATION}
	${COREMEDIA}
	${COREVIDEO}
	${COREMEDIAIO}
	${IOSURFACE}
	${IOKIT})

add_custom_command(TARGET mac-dal-plugin
	POST_BUILD
	COMMAND rm -rf ${TARGET_DIR}/obs-mac-virtualcam.plugin || true
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${TARGET_DIR}/obs-mac-virtualcam.bundle ${TARGET_DIR}/obs-mac-virtualcam.plugin
	COMMENT "Rename bundle to plugin"
)

# Note: Xcode seems to run a command `builtin-infoPlistUtility` to generate the Info.plist, but I'm
# not sure where to find that binary. If we had access to it, the command would look something like:
# builtin-infoPlistUtility ${PROJECT_SOURCE_DIR}/../common/CoreMediaIO/DeviceAbstractionLayer/Devices/Sample/PlugIn/SampleVCam-Info.plist -producttype com.apple.product-type.bundle -expandbuildsettings -platform macosx -o mac-virtualcam.bundle/Contents/Info.plist
# Instead, just copy in one that was already generated from Xcode.
add_custom_command(TARGET mac-dal-plugin
	POST_BUILD
	COMMAND cp ${PROJECT_SOURCE_DIR}/Info.plist ${TARGET_DIR}/obs-mac-virtualcam.plugin/Contents/Info.plist
    COMMAND mkdir ${TARGET_DIR}/obs-mac-virtualcam.plugin/Contents/Resources
    COMMAND cp ${PROJECT_SOURCE_DIR}/placeholder.png ${TARGET_DIR}/obs-mac-virtualcam.plugin/Contents/Resources/placeholder.png
	COMMAND /usr/bin/plutil -insert CFBundleVersion -string "${OBS_VERSION}" ${TARGET_DIR}/obs-mac-virtualcam.plugin/Contents/Info.plist
	COMMAND /usr/bin/plutil -insert CFBundleShortVersionString -string "${OBS_VERSION}" ${TARGET_DIR}/obs-mac-virtualcam.plugin/Contents/Info.plist
	DEPENDS {PROJECT_SOURCE_DIR}/Info.plist
	COMMENT "Copy in Info.plist"
)

add_custom_command(TARGET mac-dal-plugin
	POST_BUILD
	COMMAND /usr/bin/codesign --force --deep --sign - --timestamp=none ${TARGET_DIR}/obs-mac-virtualcam.plugin
	COMMENT "Codesign plugin"
)

add_custom_command(TARGET mac-dal-plugin
	POST_BUILD
	COMMAND rm -rf "${OBS_OUTPUT_DIR}/$<CONFIGURATION>/data/obs-mac-virtualcam.plugin" || true
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${TARGET_DIR}/obs-mac-virtualcam.plugin "${OBS_OUTPUT_DIR}/$<CONFIGURATION>/data/obs-mac-virtualcam.plugin"
	COMMENT "Copy plugin to destination"
)
