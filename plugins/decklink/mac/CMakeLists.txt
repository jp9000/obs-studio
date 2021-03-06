project(mac-decklink)

if(DISABLE_DECKLINK)
	message(STATUS "decklink plugin disabled")
	return()
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}")

find_library(COREFOUNDATION CoreFoundation)

include_directories(${CMAKE_SOURCE_DIR}/deps/libcaption)

set(mac-decklink-sdk_HEADERS
	decklink-sdk/DeckLinkAPI.h
	decklink-sdk/DeckLinkAPIConfiguration.h
	decklink-sdk/DeckLinkAPIDeckControl.h
	decklink-sdk/DeckLinkAPIDiscovery.h
	decklink-sdk/DeckLinkAPIModes.h
	decklink-sdk/DeckLinkAPIStreaming.h
	decklink-sdk/DeckLinkAPITypes.h
	decklink-sdk/DeckLinkAPIVersion.h)

set(mac-decklink-sdk_SOURCES
	decklink-sdk/DeckLinkAPIDispatch.cpp
	)

set(mac-decklink_HEADERS
	../decklink-devices.hpp
	../const.h
	../DecklinkOutput.hpp
	../platform.hpp
	../DecklinkInput.hpp
	../DecklinkBase.h
	../decklink-device-instance.hpp
	../decklink-device-discovery.hpp
	../decklink-device.hpp
	../decklink-device-mode.hpp
	../audio-repack.h
	../audio-repack.hpp
	../util.hpp
	../OBSVideoFrame.h
	)

set(mac-decklink_SOURCES
	../plugin-main.cpp
	../decklink-devices.cpp
	../decklink-output.cpp
	../decklink-source.cpp
	../DecklinkOutput.cpp
	../DecklinkInput.cpp
	../DecklinkBase.cpp
	../decklink-device-instance.cpp
	../decklink-device-discovery.cpp
	../decklink-device.cpp
	../decklink-device-mode.cpp
	../audio-repack.c
	platform.cpp
	../util.cpp
	../OBSVideoFrame.cpp
	)

list(APPEND decklink_HEADERS ${decklink_UI_HEADERS})

include_directories(
	${COREFOUNDATION}
)

list(APPEND mac-decklink_HEADERS ${decklink_UI_HEADERS})

add_library(mac-decklink MODULE
	${mac-decklink_SOURCES}
	${mac-decklink_HEADERS}
	${mac-decklink-sdk_HEADERS}
	${mac-decklink-sdk_SOURCES}
	)

target_link_libraries(mac-decklink
	libobs
	${COREFOUNDATION}
	caption)
set_target_properties(mac-decklink PROPERTIES FOLDER "plugins/decklink")

install_obs_plugin_with_data(mac-decklink ../data)
