project(linux-decklink)

if(DISABLE_DECKLINK)
	message(STATUS "decklink plugin disabled")
	return()
endif()

include_directories(${CMAKE_SOURCE_DIR}/deps/libcaption)

set(linux-decklink-sdk_HEADERS
	decklink-sdk/DeckLinkAPI.h
	decklink-sdk/DeckLinkAPIConfiguration.h
	decklink-sdk/DeckLinkAPIDeckControl.h
	decklink-sdk/DeckLinkAPIDiscovery.h
	decklink-sdk/DeckLinkAPIModes.h
	decklink-sdk/DeckLinkAPITypes.h
	decklink-sdk/DeckLinkAPIVersion.h
	decklink-sdk/LinuxCOM.h
	)

set(linux-decklink-sdk_SOURCES
	decklink-sdk/DeckLinkAPIDispatch.cpp
	)

set(linux-decklink_HEADERS
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

set(linux-decklink_SOURCES
	../plugin-main.cpp
	../decklink-devices.cpp
	../decklink-source.cpp
	../decklink-output.cpp
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

add_library(linux-decklink MODULE
	${linux-decklink_SOURCES}
	${linux-decklink_HEADERS}
	${linux-decklink-sdk_HEADERS}
	${linux-decklink-sdk_SOURCES}
	)

target_link_libraries(linux-decklink
	libobs
	caption)
set_target_properties(linux-decklink PROPERTIES FOLDER "plugins/decklink")

install_obs_plugin_with_data(linux-decklink ../data)
