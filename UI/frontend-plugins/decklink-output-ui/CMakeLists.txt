project(decklink-output-ui)

if(APPLE)
	find_library(COCOA Cocoa)
	include_directories(${COCOA})
endif()

if(UNIX AND NOT APPLE)
	find_package(X11 REQUIRED)
	link_libraries(${X11_LIBRARIES})
	include_directories(${X11_INCLUDE_DIR})

	find_package(Qt5X11Extras REQUIRED)
endif()

set(decklink-ouput-ui_HEADERS
	${decklink-ouput-ui_HEADERS}
	../../qt-wrappers.hpp
	../../properties-view.hpp
	../../properties-view.moc.hpp
	../../vertical-scroll-area.hpp
	../../double-slider.hpp
	../../slider-ignorewheel.hpp
	../../combobox-ignorewheel.hpp
	../../spinbox-ignorewheel.hpp
	./DecklinkOutputUI.h
	decklink-ui-main.h
	)
set(decklink-ouput-ui_SOURCES
	${decklink-ouput-ui_SOURCES}
	../../qt-wrappers.cpp
	../../properties-view.cpp
	../../vertical-scroll-area.cpp
	../../double-slider.cpp
	../../slider-ignorewheel.cpp
	../../combobox-ignorewheel.cpp
	../../spinbox-ignorewheel.cpp
	./DecklinkOutputUI.cpp
	decklink-ui-main.cpp
	)
set(decklink-ouput-ui_UI
	${decklink-ouput-ui_UI}
	forms/output.ui
	)

if(WIN32)
	set(MODULE_DESCRIPTION "OBS DeckLink Output UI")
	configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in decklink-ouput-ui.rc)
	list(APPEND decklink-ouput-ui_SOURCES
		decklink-ouput-ui.rc)
endif()

if(APPLE)
	set(decklink-ouput-ui_PLATFORM_LIBS
		${COCOA})
endif()

qt5_wrap_ui(decklink-ouput-ui_UI_HEADERS
	${decklink-ouput-ui_UI})

add_library(decklink-ouput-ui MODULE
	${decklink-ouput-ui_HEADERS}
	${decklink-ouput-ui_SOURCES}
	${decklink-ouput-ui_UI_HEADERS}
	)
target_link_libraries(decklink-ouput-ui
	${frontend-tools_PLATFORM_LIBS}
	obs-frontend-api
	Qt5::Widgets
	libobs)

if(UNIX AND NOT APPLE)
	target_link_libraries(decklink-ouput-ui
		Qt5::X11Extras)
endif()

set_target_properties(decklink-ouput-ui PROPERTIES FOLDER "frontend")

install_obs_plugin_with_data(decklink-ouput-ui data)
