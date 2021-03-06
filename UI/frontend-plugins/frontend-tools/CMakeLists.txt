project(frontend-tools)

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

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/deps/obs-scripting")

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/frontend-tools-config.h.in"
	"${CMAKE_BINARY_DIR}/config/frontend-tools-config.h")

set(frontend-tools_HEADERS
	${frontend-tools_HEADERS}
	"${CMAKE_BINARY_DIR}/config/frontend-tools-config.h"
	auto-scene-switcher.hpp
	output-timer.hpp
	tool-helpers.hpp
	../../properties-view.hpp
	../../properties-view.moc.hpp
	../../horizontal-scroll-area.hpp
	../../vertical-scroll-area.hpp
	../../double-slider.hpp
	../../slider-ignorewheel.hpp
	../../combobox-ignorewheel.hpp
	../../spinbox-ignorewheel.hpp
	../../qt-wrappers.hpp
	)
set(frontend-tools_SOURCES
	${frontend-tools_SOURCES}
	auto-scene-switcher.cpp
	frontend-tools.c
	output-timer.cpp
	../../properties-view.cpp
	../../horizontal-scroll-area.cpp
	../../vertical-scroll-area.cpp
	../../double-slider.cpp
	../../slider-ignorewheel.cpp
	../../combobox-ignorewheel.cpp
	../../spinbox-ignorewheel.cpp
	../../qt-wrappers.cpp
	)
set(frontend-tools_UI
	${frontend-tools_UI}
	forms/auto-scene-switcher.ui
	forms/output-timer.ui
	)

if(SCRIPTING_ENABLED)
	set(frontend-tools_HEADERS
		${frontend-tools_HEADERS}
		scripts.hpp
		)
	set(frontend-tools_SOURCES
		${frontend-tools_SOURCES}
		scripts.cpp
		)
	set(frontend-tools_UI
		${frontend-tools_UI}
		forms/scripts.ui
		)
	set(EXTRA_LIBS
		${EXTRA_LIBS}
		obs-scripting
		)
endif()

if(WIN32)
	set(MODULE_DESCRIPTION "OBS Frontend Tools")
	configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in frontend-tools.rc)
	set(frontend-tools_PLATFORM_SOURCES
		auto-scene-switcher-win.cpp
		frontend-tools.rc)

	set(frontend-tools_PLATFORM_SOURCES
		${frontend-tools_PLATFORM_SOURCES}
		captions.cpp
		captions-handler.cpp
		captions-mssapi.cpp
		captions-mssapi-stream.cpp)
	set(frontend-tools_PLATFORM_HEADERS
		captions.hpp
		captions-handler.hpp
		captions-mssapi.hpp
		captions-mssapi-stream.hpp)
	set(frontend-tools_PLATFORM_UI
		forms/captions.ui)
elseif(APPLE)
	set(frontend-tools_PLATFORM_SOURCES
		auto-scene-switcher-osx.mm)
	set_source_files_properties(auto-scene-switcher-osx.mm
		PROPERTIES COMPILE_FLAGS "-fobjc-arc")

	set(frontend-tools_PLATFORM_LIBS
		${COCOA})
else()
	set(frontend-tools_PLATFORM_SOURCES
		auto-scene-switcher-nix.cpp)
endif()

qt5_wrap_ui(frontend-tools_UI_HEADERS
	${frontend-tools_UI}
	${frontend-tools_PLATFORM_UI})

add_library(frontend-tools MODULE
	${frontend-tools_HEADERS}
	${frontend-tools_SOURCES}
	${frontend-tools_UI_HEADERS}
	${frontend-tools_PLATFORM_SOURCES}
	${frontend-tools_PLATFORM_HEADERS}
	)
target_link_libraries(frontend-tools
	${frontend-tools_PLATFORM_LIBS}
	${EXTRA_LIBS}
	obs-frontend-api
	Qt5::Widgets
	libobs)

if(UNIX AND NOT APPLE)
	target_link_libraries(frontend-tools
		Qt5::X11Extras)
endif()

set_target_properties(frontend-tools PROPERTIES FOLDER "frontend")

install_obs_plugin_with_data(frontend-tools data)
