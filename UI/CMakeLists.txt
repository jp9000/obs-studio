add_subdirectory(obs-frontend-api)

project(obs)

if(NOT DEFINED TWITCH_CLIENTID
   OR "${TWITCH_CLIENTID}" STREQUAL ""
   OR NOT DEFINED TWITCH_HASH
   OR "${TWITCH_HASH}" STREQUAL ""
   OR NOT TARGET OBS::browser-panels)
  set(TWITCH_ENABLED FALSE)
  set(TWITCH_CLIENTID "")
  set(TWITCH_HASH "0")
else()
  set(TWITCH_ENABLED TRUE)
endif()

if(NOT DEFINED RESTREAM_CLIENTID
   OR "${RESTREAM_CLIENTID}" STREQUAL ""
   OR NOT DEFINED RESTREAM_HASH
   OR "${RESTREAM_HASH}" STREQUAL ""
   OR NOT TARGET OBS::browser-panels)
  set(RESTREAM_ENABLED FALSE)
  set(RESTREAM_CLIENTID "")
  set(RESTREAM_HASH "0")
else()
  set(RESTREAM_ENABLED TRUE)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ui-config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/ui-config.h)

find_package(FFmpeg REQUIRED COMPONENTS avcodec avutil avformat)
find_package(CURL REQUIRED)

add_subdirectory(frontend-plugins)
add_executable(obs)

if(BUILD_QT EQUAL 5)
  find_package(Qt5 COMPONENTS Network Widgets Svg Xml REQUIRED)

  if(OS_POSIX AND NOT OS_MACOS)
    find_package(Qt5Gui REQUIRED)

    target_link_libraries(obs PRIVATE Qt5::Gui Qt5::GuiPrivate)
  endif()

  if(NOT TARGET Qt5::Widgets)
    message(FATAL_ERROR "OBS:   Unable to find Qt5 component Widgets")
  endif()

  set_target_properties(
    obs
    PROPERTIES AUTOMOC ON
               AUTOUIC ON
               AUTORCC ON
               AUTOUIC_SEARCH_PATHS "forms;forms/source-toolbar")

  target_include_directories(
    obs PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
                "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")

  target_sources(obs PRIVATE forms/obs.qrc)

  target_link_libraries(obs PRIVATE Qt5::Widgets Qt5::Svg Qt5::Xml Qt5::Network)
elseif(BUILD_QT EQUAL 6)
  message(FATAL_ERROR "OBS: Building with Qt6 not supported yet.")
endif()

target_sources(
  obs
  PRIVATE obs-app.cpp
          obs-app.hpp
          obs-proxy-style.cpp
          obs-proxy-style.hpp
          api-interface.cpp
          auth-base.cpp
          auth-base.hpp
          crash-report.cpp
          crash-report.hpp
          display-helpers.hpp
          platform.hpp
          qt-display.cpp
          qt-display.hpp
          qt-wrappers.cpp
          qt-wrappers.hpp
          ui-validation.cpp
          ui-validation.hpp
          ${CMAKE_SOURCE_DIR}/deps/json11/json11.cpp
          ${CMAKE_SOURCE_DIR}/deps/json11/json11.hpp
          ${CMAKE_SOURCE_DIR}/deps/libff/libff/ff-util.c
          ${CMAKE_SOURCE_DIR}/deps/libff/libff/ff-util.h)

target_sources(
  obs
  PRIVATE adv-audio-control.cpp
          adv-audio-control.hpp
          audio-encoders.cpp
          audio-encoders.hpp
          balance-slider.hpp
          combobox-ignorewheel.cpp
          combobox-ignorewheel.hpp
          clickable-label.hpp
          double-slider.cpp
          double-slider.hpp
          horizontal-scroll-area.cpp
          horizontal-scroll-area.hpp
          item-widget-helpers.cpp
          item-widget-helpers.hpp
          context-bar-controls.cpp
          context-bar-controls.hpp
          expand-checkbox.hpp
          focus-list.cpp
          focus-list.hpp
          hotkey-edit.cpp
          hotkey-edit.hpp
          locked-checkbox.cpp
          locked-checkbox.hpp
          log-viewer.cpp
          log-viewer.hpp
          media-controls.cpp
          media-controls.hpp
          media-slider.cpp
          media-slider.hpp
          menu-button.cpp
          menu-button.hpp
          mute-checkbox.hpp
          properties-view.cpp
          properties-view.hpp
          properties-view.moc.hpp
          record-button.cpp
          record-button.hpp
          remote-text.cpp
          remote-text.hpp
          scene-tree.cpp
          scene-tree.hpp
          screenshot-obj.hpp
          slider-absoluteset-style.cpp
          slider-absoluteset-style.hpp
          slider-ignorewheel.cpp
          slider-ignorewheel.hpp
          source-label.cpp
          source-label.hpp
          spinbox-ignorewheel.cpp
          spinbox-ignorewheel.hpp
          source-tree.cpp
          source-tree.hpp
          url-push-button.cpp
          url-push-button.hpp
          undo-stack-obs.cpp
          undo-stack-obs.hpp
          volume-control.cpp
          volume-control.hpp
          vertical-scroll-area.cpp
          vertical-scroll-area.hpp
          visibility-checkbox.cpp
          visibility-checkbox.hpp
          visibility-item-widget.cpp
          visibility-item-widget.hpp)

target_sources(
  obs
  PRIVATE window-basic-about.cpp
          window-basic-about.hpp
          window-basic-auto-config.cpp
          window-basic-auto-config.hpp
          window-basic-auto-config-test.cpp
          window-basic-adv-audio.cpp
          window-basic-adv-audio.hpp
          window-basic-filters.cpp
          window-basic-filters.hpp
          window-basic-interaction.cpp
          window-basic-interaction.hpp
          window-basic-main.cpp
          window-basic-main.hpp
          window-basic-main-browser.cpp
          window-basic-main-dropfiles.cpp
          window-basic-main-icons.cpp
          window-basic-main-outputs.cpp
          window-basic-main-outputs.hpp
          window-basic-main-profiles.cpp
          window-basic-main-scene-collections.cpp
          window-basic-main-screenshot.cpp
          window-basic-main-transitions.cpp
          window-basic-preview.cpp
          window-basic-properties.cpp
          window-basic-properties.hpp
          window-basic-settings.cpp
          window-basic-settings.hpp
          window-basic-settings-stream.cpp
          window-basic-source-select.cpp
          window-basic-source-select.hpp
          window-basic-stats.cpp
          window-basic-stats.hpp
          window-basic-status-bar.cpp
          window-basic-status-bar.hpp
          window-basic-transform.cpp
          window-basic-transform.hpp
          window-basic-preview.hpp
          window-dock.cpp
          window-dock.hpp
          window-importer.cpp
          window-importer.hpp
          window-log-reply.hpp
          window-main.hpp
          window-missing-files.cpp
          window-missing-files.hpp
          window-namedialog.cpp
          window-namedialog.hpp
          window-log-reply.cpp
          window-projector.cpp
          window-projector.hpp
          window-remux.cpp
          window-remux.hpp)

target_sources(
  obs
  PRIVATE importers/importers.cpp importers/importers.hpp importers/classic.cpp
          importers/sl.cpp importers/studio.cpp importers/xsplit.cpp)

target_compile_features(obs PRIVATE cxx_std_17)

target_include_directories(obs PRIVATE ${CMAKE_SOURCE_DIR}/deps/json11
                                       ${CMAKE_SOURCE_DIR}/deps/libff)

target_link_libraries(
  obs PRIVATE CURL::libcurl FFmpeg::avcodec FFmpeg::avutil FFmpeg::avformat
              OBS::libobs OBS::frontend-api)

set_target_properties(obs PROPERTIES FOLDER "frontend")

if(TARGET OBS::browser-panels)
  get_target_property(_PANEL_INCLUDE_DIRECTORY OBS::browser-panels
                      INTERFACE_INCLUDE_DIRECTORIES)
  target_include_directories(obs PRIVATE ${_PANEL_INCLUDE_DIRECTORY})

  target_compile_definitions(obs PRIVATE BROWSER_AVAILABLE)

  target_sources(
    obs
    PRIVATE obf.c
            obf.h
            auth-oauth.cpp
            auth-oauth.hpp
            window-dock-browser.cpp
            window-dock-browser.hpp
            window-extra-browsers.cpp
            window-extra-browsers.hpp)

  if(TWITCH_ENABLED)
    target_sources(obs PRIVATE auth-twitch.cpp auth-twitch.hpp)
  endif()

  if(RESTREAM_ENABLED)
    target_sources(obs PRIVATE auth-restream.cpp auth-restream.hpp)
  endif()
endif()

if(OS_WINDOWS)
  set_target_properties(obs PROPERTIES WIN32_EXECUTABLE ON OUTPUT_NAME
                                                           "obs${_ARCH_SUFFIX}")

  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/obs.rc.in
                 ${CMAKE_BINARY_DIR}/obs.rc)

  target_sources(
    obs
    PRIVATE obs.manifest
            platform-windows.cpp
            win-update/update-window.cpp
            win-update/update-window.hpp
            win-update/win-update.cpp
            win-update/win-update.hpp
            win-update/win-update-helpers.cpp
            win-update/win-update-helpers.hpp
            ${CMAKE_BINARY_DIR}/obs.rc)

  target_link_libraries(obs PRIVATE crypt32 OBS::blake2)

  target_compile_features(obs PRIVATE cxx_std_17)

  target_compile_definitions(
    obs PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS
                _CRT_NONSTDC_NO_WARNINGS)

  if(MSVC)
    target_link_libraries(obs PRIVATE OBS::w32-pthreads)

    set_source_files_properties(
      ${CMAKE_CURRENT_SOURCE_DIR}../deps/libff/libff/ff-util.c
      PROPERTIES COMPILE_FLAGS -Dinline=__inline)
  endif()

  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_link_options(obs PRIVATE "/LARGEADDRESSAWARE")
  endif()

  add_subdirectory(win-update/updater)

elseif(OS_MACOS)
  set_target_properties(
    obs
    PROPERTIES OUTPUT_NAME ${OBS_BUNDLE_NAME}
               MACOSX_BUNDLE ON
               MACOSX_BUNDLE_INFO_PLIST
               ${CMAKE_SOURCE_DIR}/cmake/bundle/macOS/Info.plist.in)

  if(XCODE)
    set_target_properties(
      obs
      PROPERTIES XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER
                 "${MACOSX_BUNDLE_GUI_IDENTIFIER}"
                 XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon
                 XCODE_ATTRIBUTE_PRODUCT_NAME "OBS")

    set(APP_ICON_TARGET ${CMAKE_SOURCE_DIR}/cmake/bundle/macOS/Assets.xcassets)

    target_sources(obs PRIVATE ${APP_ICON_TARGET})
    set_source_files_properties(${APP_ICON_TARGET}
                                PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  else()
    set(APP_ICON_TARGET ${CMAKE_SOURCE_DIR}/cmake/bundle/macOS/AppIcon.iconset)
    set(APP_ICON_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/AppIcon.icns)

    add_custom_command(
      OUTPUT ${APP_ICON_OUTPUT} COMMAND iconutil -c icns "${APP_ICON_TARGET}"
                                        -o "${APP_ICON_OUTPUT}")

    set(MACOSX_BUNDLE_ICON_FILE AppIcon.icns)
    target_sources(obs PRIVATE ${APP_ICON_OUTPUT}
                               ${CMAKE_CURRENT_SOURCE_DIR}/../AUTHORS)

    set_source_files_properties(${APP_ICON_OUTPUT}
                                PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  endif()

  find_library(APPKIT Appkit)
  mark_as_advanced(APPKIT)

  target_link_libraries(obs PRIVATE ${APPKIT})

  if(ENABLE_SPARKLE_UPDATER)
    find_library(SPARKLE Sparkle)
    mark_as_advanced(SPARKLE)

    target_sources(obs PRIVATE sparkle-updater.mm)
    target_compile_definitions(obs PRIVATE ENABLE_SPARKLE_UPDATER)

    target_link_libraries(obs PRIVATE ${SPARKLE})
  endif()

  target_sources(obs PRIVATE platform-osx.mm)

  set_source_files_properties(platform-osx.mm PROPERTIES COMPILE_FLAGS
                                                         "-fobjc-arc")

elseif(OS_POSIX)
  target_sources(obs PRIVATE platform-x11.cpp)

  if(NOT LINUX_PORTABLE)
    add_subdirectory(xdg-data)
  endif()

  if(OS_FREEBSD)
    target_link_libraries(obs PRIVATE procstat)
  endif()
endif()

define_graphic_modules(obs)
setup_obs_app(obs)
setup_target_resources(obs obs-studio)
add_target_resource(obs ${CMAKE_CURRENT_SOURCE_DIR}/../AUTHORS
                    obs-studio/authors)
