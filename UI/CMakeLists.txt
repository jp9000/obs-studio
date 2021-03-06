if(DISABLE_UI)
	message(STATUS "UI disabled")
	return()
elseif(ENABLE_UI)
	set(FIND_MODE REQUIRED)
else()
	set(FIND_MODE QUIET)
endif()

if(BROWSER_AVAILABLE_INTERNAL)
	add_definitions(-DBROWSER_AVAILABLE)
endif()

add_subdirectory(obs-frontend-api)

# ----------------------------------------------------------------------------

project(obs)

set(DISABLE_UPDATE_MODULE TRUE CACHE BOOL "Disables building the update module")

if(NOT DEFINED TWITCH_CLIENTID OR "${TWITCH_CLIENTID}" STREQUAL "" OR
   NOT DEFINED TWITCH_HASH     OR "${TWITCH_HASH}"     STREQUAL "" OR
   NOT BROWSER_AVAILABLE_INTERNAL)
	set(TWITCH_ENABLED FALSE)
	set(TWITCH_CLIENTID "")
	set(TWITCH_HASH "0")
else()
	set(TWITCH_ENABLED TRUE)
endif()

if(NOT DEFINED RESTREAM_CLIENTID OR "${RESTREAM_CLIENTID}" STREQUAL "" OR
   NOT DEFINED RESTREAM_HASH     OR "${RESTREAM_HASH}"     STREQUAL "" OR
   NOT BROWSER_AVAILABLE_INTERNAL)
	set(RESTREAM_ENABLED FALSE)
	set(RESTREAM_CLIENTID "")
	set(RESTREAM_HASH "0")
else()
	set(RESTREAM_ENABLED TRUE)
endif()

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/ui-config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/ui-config.h")

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
set(CMAKE_AUTOMOC TRUE)

find_package(Qt5Widgets ${FIND_MODE})
find_package(Qt5Svg ${FIND_MODE})
find_package(Qt5Xml ${FIND_MODE})

find_package(FFmpeg REQUIRED COMPONENTS avcodec avutil avformat)

if(APPLE)
	find_package(Qt5MacExtras REQUIRED)
endif(APPLE)

if(NOT Qt5Widgets_FOUND)
	if (ENABLE_UI)
		message(FATAL_ERROR "Failed to find Qt5")
	else()
		message(STATUS "Qt5 not found - UI disabled")
		return()
	endif()
endif()


include_directories(${FFMPEG_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(SYSTEM "obs-frontend-api")
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/libobs")
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/deps/libff")
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/deps/json11")
if(BROWSER_AVAILABLE_INTERNAL)
	include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/plugins/obs-browser/panel")
endif()

find_package(Libcurl REQUIRED)
include_directories(${LIBCURL_INCLUDE_DIRS})
add_definitions(${LIBCURL_DEFINITIONS})

if(WIN32)
	include_directories(${BLAKE2_INCLUDE_DIR})

	set(obs_PLATFORM_SOURCES
		platform-windows.cpp
		win-update/update-window.cpp
		win-update/win-update.cpp
		win-update/win-update-helpers.cpp
		${obs-studio_BINARY_DIR}/obs.rc)
	set(obs_PLATFORM_HEADERS
		win-update/update-window.hpp
		win-update/win-update.hpp
		win-update/win-update-helpers.hpp)
	set(obs_PLATFORM_LIBRARIES
		crypt32
		blake2)

	if(CMAKE_SIZEOF_VOID_P EQUAL 4)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE")
	endif()
elseif(APPLE)
	set(obs_PLATFORM_SOURCES
		platform-osx.mm)

	find_package(AppKit REQUIRED)
	set(obs_PLATFORM_LIBRARIES ${APPKIT_LIBRARIES})

	add_definitions(-fobjc-arc)

	option(ENABLE_SPARKLE_UPDATER "Enables updates via the Sparkle framework (don't forget to update the Info.plist for your .app)" OFF)
	if(ENABLE_SPARKLE_UPDATER)
		find_library(SPARKLE Sparkle)
		include_directories(${SPARKLE})
		set(obs_PLATFORM_SOURCES
			${obs_PLATFORM_SOURCES}
			sparkle-updater.mm)
		set(obs_PLATFORM_LIBRARIES
			${obs_PLATFORM_LIBRARIES}
			${SPARKLE})
		add_definitions(-DUPDATE_SPARKLE=1)
	endif()

elseif(UNIX)
        find_package(Qt5X11Extras REQUIRED)

	set(obs_PLATFORM_SOURCES
		platform-x11.cpp)

        set(obs_PLATFORM_LIBRARIES
                Qt5::X11Extras)

	if("${CMAKE_SYSTEM_NAME}" MATCHES "FreeBSD")
		list(APPEND obs_PLATFORM_LIBRARIES
			procstat)
	endif()
endif()

if(BROWSER_AVAILABLE_INTERNAL)
	list(APPEND obs_PLATFORM_SOURCES
		obf.c
		auth-oauth.cpp
		window-dock-browser.cpp
		window-extra-browsers.cpp
		)
	list(APPEND obs_PLATFORM_HEADERS
		obf.h
		auth-oauth.hpp
		window-dock-browser.hpp
		window-extra-browsers.hpp
		)

	if(TWITCH_ENABLED)
		list(APPEND obs_PLATFORM_SOURCES
			auth-twitch.cpp
			)
		list(APPEND obs_PLATFORM_HEADERS
			auth-twitch.hpp
			)
	endif()

	if(RESTREAM_ENABLED)
		list(APPEND obs_PLATFORM_SOURCES
			auth-restream.cpp
			)
		list(APPEND obs_PLATFORM_HEADERS
			auth-restream.hpp
			)
	endif()
endif()

set(obs_libffutil_SOURCES
	../deps/libff/libff/ff-util.c
	)
set(obs_libffutil_HEADERS
	../deps/libff/libff/ff-util.h
	)

if(MSVC)
	set_source_files_properties(
		../deps/libff/libff/ff-util.c
		PROPERTIES COMPILE_FLAGS -Dinline=__inline
		)
	set(obs_PLATFORM_LIBRARIES
		${obs_PLATFORM_LIBRARIES}
		w32-pthreads)
endif()

set(obs_SOURCES
	${obs_PLATFORM_SOURCES}
	${obs_libffutil_SOURCES}
	../deps/json11/json11.cpp
	obs-app.cpp
	window-dock.cpp
	api-interface.cpp
	window-basic-main.cpp
	window-basic-stats.cpp
	window-basic-filters.cpp
	window-basic-settings.cpp
	window-basic-interaction.cpp
	window-basic-properties.cpp
	window-basic-auto-config.cpp
	window-basic-main-outputs.cpp
	window-basic-source-select.cpp
	window-basic-settings-stream.cpp
	window-basic-main-screenshot.cpp
	window-basic-auto-config-test.cpp
	window-basic-main-scene-collections.cpp
	window-basic-main-transitions.cpp
	window-basic-main-dropfiles.cpp
	window-basic-main-profiles.cpp
	window-basic-main-browser.cpp
	window-basic-main-icons.cpp
	window-basic-status-bar.cpp
	window-basic-adv-audio.cpp
	window-basic-transform.cpp
	window-basic-preview.cpp
	window-basic-about.cpp
	window-importer.cpp
	media-controls.cpp
	window-namedialog.cpp
	window-log-reply.cpp
	window-projector.cpp
	window-remux.cpp
	window-missing-files.cpp
	auth-base.cpp
	source-tree.cpp
	scene-tree.cpp
	properties-view.cpp
	focus-list.cpp
	menu-button.cpp
	double-slider.cpp
	slider-ignorewheel.cpp
	combobox-ignorewheel.cpp
	spinbox-ignorewheel.cpp
	record-button.cpp
	ui-validation.cpp
	url-push-button.cpp
	volume-control.cpp
	adv-audio-control.cpp
	item-widget-helpers.cpp
	context-bar-controls.cpp
	horizontal-scroll-area.cpp
	vertical-scroll-area.cpp
	visibility-item-widget.cpp
	slider-absoluteset-style.cpp
	qt-display.cpp
	crash-report.cpp
	hotkey-edit.cpp
	source-label.cpp
	remote-text.cpp
	audio-encoders.cpp
	qt-wrappers.cpp
	log-viewer.cpp
	obs-proxy-style.cpp
	locked-checkbox.cpp
	visibility-checkbox.cpp
	media-slider.cpp)

set(obs_HEADERS
	${obs_PLATFORM_HEADERS}
	${obs_libffutil_HEADERS}
	../deps/json11/json11.hpp
	obs-app.hpp
	platform.hpp
	window-dock.hpp
	window-main.hpp
	window-basic-main.hpp
	window-basic-stats.hpp
	window-basic-filters.hpp
	window-basic-settings.hpp
	window-basic-interaction.hpp
	window-basic-properties.hpp
	window-basic-auto-config.hpp
	window-basic-main-outputs.hpp
	window-basic-source-select.hpp
	window-basic-about.hpp
	window-basic-status-bar.hpp
	window-basic-adv-audio.hpp
	window-basic-transform.hpp
	window-basic-preview.hpp
	window-importer.hpp
	media-controls.hpp
	window-namedialog.hpp
	window-log-reply.hpp
	window-projector.hpp
	window-remux.hpp
	window-missing-files.hpp
	auth-base.hpp
	source-tree.hpp
	scene-tree.hpp
	properties-view.hpp
	properties-view.moc.hpp
	display-helpers.hpp
	balance-slider.hpp
	double-slider.hpp
	slider-ignorewheel.hpp
	combobox-ignorewheel.hpp
	spinbox-ignorewheel.hpp
	focus-list.hpp
	menu-button.hpp
	mute-checkbox.hpp
	record-button.hpp
	ui-validation.hpp
	screenshot-obj.hpp
	url-push-button.hpp
	volume-control.hpp
	adv-audio-control.hpp
	item-widget-helpers.hpp
	visibility-checkbox.hpp
	context-bar-controls.hpp
	locked-checkbox.hpp
	horizontal-scroll-area.hpp
	expand-checkbox.hpp
	vertical-scroll-area.hpp
	visibility-item-widget.hpp
	slider-absoluteset-style.hpp
	qt-display.hpp
	crash-report.hpp
	hotkey-edit.hpp
	source-label.hpp
	remote-text.hpp
	audio-encoders.hpp
	qt-wrappers.hpp
	clickable-label.hpp
	log-viewer.hpp
	obs-proxy-style.hpp
	obs-proxy-style.hpp
	media-slider.hpp)

set(obs_importers_HEADERS
	importers/importers.hpp)

set(obs_importers_SOURCES
	importers/importers.cpp
	importers/classic.cpp
	importers/sl.cpp
	importers/studio.cpp
	importers/xsplit.cpp)

source_group("importers\\Source Files" FILES ${obs_importers_SOURCES})
source_group("importers\\Header Files" FILES ${obs_importers_HEADERS})

set(obs_UI
	forms/source-toolbar/browser-source-toolbar.ui
	forms/source-toolbar/device-select-toolbar.ui
	forms/source-toolbar/game-capture-toolbar.ui
	forms/source-toolbar/image-source-toolbar.ui
	forms/source-toolbar/color-source-toolbar.ui
	forms/source-toolbar/text-source-toolbar.ui
	forms/source-toolbar/media-controls.ui
	forms/AutoConfigStartPage.ui
	forms/AutoConfigVideoPage.ui
	forms/AutoConfigStreamPage.ui
	forms/AutoConfigTestPage.ui
	forms/ColorSelect.ui
	forms/OBSLogReply.ui
	forms/OBSBasic.ui
	forms/OBSBasicTransform.ui
	forms/OBSBasicFilters.ui
	forms/OBSBasicSettings.ui
	forms/OBSBasicSourceSelect.ui
	forms/OBSBasicInteraction.ui
	forms/OBSExtraBrowsers.ui
	forms/OBSUpdate.ui
	forms/OBSRemux.ui
	forms/OBSImporter.ui
	forms/OBSMissingFiles.ui
	forms/OBSAbout.ui)

set(obs_QRC
	forms/obs.qrc)

qt5_wrap_ui(obs_UI_HEADERS ${obs_UI})
qt5_add_resources(obs_QRC_SOURCES ${obs_QRC})

add_executable(obs WIN32
	obs.manifest
	${obs_SOURCES}
	${obs_HEADERS}
	${obs_importers_SOURCES}
	${obs_importers_HEADERS}
	${obs_UI_HEADERS}
	${obs_QRC_SOURCES})

if(WIN32)
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(_output_suffix "64")
	else()
		set(_output_suffix "32")
	endif()

	set_target_properties(obs
		PROPERTIES
			OUTPUT_NAME "obs${_output_suffix}")
endif()

if (ENABLE_WAYLAND)
	find_package(Qt5Gui REQUIRED)
	include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})

	set(obs_PLATFORM_LIBRARIES
		${obs_PLATFORM_LIBRARIES}
		Qt5::Gui
		Qt5::GuiPrivate)
endif()

target_link_libraries(obs
	libobs
	Qt5::Widgets
	Qt5::Svg
	Qt5::Xml
	obs-frontend-api
	${FFMPEG_LIBRARIES}
	${LIBCURL_LIBRARIES}
	${obs_PLATFORM_LIBRARIES})

if (APPLE)
	target_link_libraries(obs
			Qt5::MacExtras)
endif()
set_target_properties(obs PROPERTIES FOLDER "frontend")

define_graphic_modules(obs)

install_obs_core(obs)
install_obs_data(obs data obs-studio)
install_obs_data_file(obs ../AUTHORS obs-studio/authors)

if (UNIX AND UNIX_STRUCTURE AND NOT APPLE)
	add_subdirectory(xdg-data)
endif()

add_subdirectory(frontend-plugins)
if(WIN32)
	add_subdirectory(win-update/updater)
endif()
