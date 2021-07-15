project(decklink-captions)

if(NOT ENABLE_DECKLINK)
  return()
endif()

add_library(decklink-captions MODULE)
add_library(OBS::decklink-captions ALIAS decklink-captions)

if(BUILD_QT EQUAL 5)
  find_package(Qt5 COMPONENTS Widgets REQUIRED)
  target_link_libraries(decklink-captions PRIVATE Qt5::Widgets)

  set_target_properties(
    decklink-captions
    PROPERTIES AUTOMOC ON
               AUTOUIC ON
               AUTORCC ON
               AUTOUIC_SEARCH_PATHS "forms")

  target_include_directories(
    decklink-captions PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
                              "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")
elseif(BUILD_QT EQUAL 6)
  message(FATAL_ERROR "OBS: Building with Qt6 not supported yet.")
endif()

target_compile_features(decklink-captions PRIVATE cxx_std_17)

target_sources(decklink-captions PRIVATE decklink-captions.cpp
                                         decklink-captions.h)

target_link_libraries(decklink-captions PRIVATE OBS::frontend-api OBS::libobs)

if(OS_MACOS)
  find_library(COCOA Cocoa)
  mark_as_advanced(COCOA)
  target_link_libraries(decklink-captions PRIVATE ${COCOA})

elseif(OS_POSIX)
  find_package(X11 REQUIRED)
  mark_as_advanced(X11)
  target_link_libraries(decklink-captions PRIVATE X11::X11)
endif()

set_target_properties(decklink-captions PROPERTIES FOLDER "plugins/decklink"
                                                   PREFIX "")

setup_plugin_target(decklink-captions)
