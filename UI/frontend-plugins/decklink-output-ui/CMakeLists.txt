project(decklink-output-ui)

if(NOT ENABLE_DECKLINK)
  return()
endif()

add_library(decklink-output-ui MODULE)
add_library(OBS::decklink-output-ui ALIAS decklink-output-ui)

if(BUILD_QT EQUAL 5)
  find_package(Qt5 COMPONENTS Widgets REQUIRED)

  target_link_libraries(decklink-output-ui PRIVATE Qt5::Widgets Qt5::GuiPrivate)

  set_target_properties(
    decklink-output-ui
    PROPERTIES AUTOMOC ON
               AUTOUIC ON
               AUTORCC ON
               AUTOUIC_SEARCH_PATHS "forms")

  target_include_directories(
    decklink-output-ui PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
                               "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")
elseif(BUILD_QT EQUAL 6)
  message(FATAL_ERROR "OBS: Building with Qt6 not supported yet.")
endif()

target_sources(
  decklink-output-ui
  PRIVATE DecklinkOutputUI.cpp
          DecklinkOutputUI.h
          decklink-ui-main.cpp
          decklink-ui-main.h
          ${CMAKE_SOURCE_DIR}/UI/combobox-ignorewheel.cpp
          ${CMAKE_SOURCE_DIR}/UI/combobox-ignorewheel.hpp
          ${CMAKE_SOURCE_DIR}/UI/double-slider.cpp
          ${CMAKE_SOURCE_DIR}/UI/double-slider.hpp
          ${CMAKE_SOURCE_DIR}/UI/properties-view.hpp
          ${CMAKE_SOURCE_DIR}/UI/properties-view.cpp
          ${CMAKE_SOURCE_DIR}/UI/properties-view.moc.hpp
          ${CMAKE_SOURCE_DIR}/UI/qt-wrappers.hpp
          ${CMAKE_SOURCE_DIR}/UI/qt-wrappers.cpp
          ${CMAKE_SOURCE_DIR}/UI/spinbox-ignorewheel.cpp
          ${CMAKE_SOURCE_DIR}/UI/spinbox-ignorewheel.hpp
          ${CMAKE_SOURCE_DIR}/UI/slider-ignorewheel.cpp
          ${CMAKE_SOURCE_DIR}/UI/slider-ignorewheel.hpp
          ${CMAKE_SOURCE_DIR}/UI/vertical-scroll-area.hpp
          ${CMAKE_SOURCE_DIR}/UI/vertical-scroll-area.cpp)

target_link_libraries(decklink-output-ui PRIVATE OBS::libobs OBS::frontend-api)

target_compile_features(decklink-output-ui PRIVATE cxx_std_17)

set_target_properties(decklink-output-ui PROPERTIES FOLDER "frontend" PREFIX "")

if(OS_WINDOWS)
  set(MODULE_DESCRIPTION "OBS Decklink Output UI")
  configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
                 decklink-output-ui.rc)

  target_sources(decklink-output-ui PRIVATE decklink-output-ui.rc)

elseif(OS_MACOS)
  find_library(COCOA Cocoa)
  mark_as_advanced(COCOA)

  target_link_libraries(decklink-output-ui PRIVATE ${COCOA})

elseif(OS_POSIX)
  find_package(X11 REQUIRED)
  target_link_libraries(decklink-output-ui PRIVATE X11::X11)
endif()

setup_plugin_target(decklink-output-ui)
