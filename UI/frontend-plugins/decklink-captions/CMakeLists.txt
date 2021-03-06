project(decklink-captions)

if(APPLE)
    find_library(COCOA Cocoa)
    include_directories(${COCOA})
endif()

if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    link_libraries(${X11_LIBRARIES})
    include_directories(${X11_INCLUDE_DIR})
endif()

set(decklink-captions_HEADERS
        decklink-captions.h
        )
set(decklink-captions_SOURCES
        decklink-captions.cpp
        )
set(decklink-captions_UI
        forms/captions.ui
        )

if(APPLE)
    set(decklink-captions_PLATFORM_LIBS
            ${COCOA})
endif()

qt5_wrap_ui(decklink-captions_UI_HEADERS
        ${decklink-captions_UI})

add_library(decklink-captions MODULE
        ${decklink-captions_HEADERS}
        ${decklink-captions_SOURCES}
        ${decklink-captions_UI_HEADERS}
        )
target_link_libraries(decklink-captions
        ${frontend-tools_PLATFORM_LIBS}
        obs-frontend-api
        Qt5::Widgets
        libobs)
set_target_properties(decklink-captions PROPERTIES FOLDER "plugins/decklink")

install_obs_plugin_with_data(decklink-captions data)
