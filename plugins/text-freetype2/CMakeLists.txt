project(text-freetype2)

option(ENABLE_FREETYPE "Enable FreeType text plugin" ON)

if(NOT ENABLE_FREETYPE)
  message(STATUS "OBS: FreeType 2 text support not enabled")
  return()
endif()

find_package(Freetype REQUIRED)

add_library(text-freetype2 MODULE)
add_library(OBS::text-freetype2 ALIAS text-freetype2)

target_sources(
  text-freetype2 PRIVATE find-font.h obs-convenience.c text-functionality.c
                         text-freetype2.c obs-convenience.h text-freetype2.h)

target_link_libraries(text-freetype2 PRIVATE OBS::libobs Freetype::Freetype)

set_target_properties(text-freetype2 PROPERTIES FOLDER "plugins" PREFIX "")

if(OS_WINDOWS)
  set(MODULE_DESCRIPTION "OBS FreeType text module")
  configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
                 text-freetype2.rc)

  target_sources(text-freetype2 PRIVATE find-font.c find-font-windows.c
                                        text-freetype2.rc)

elseif(OS_MACOS)
  find_package(Iconv REQUIRED)
  find_library(COCOA Cocoa)
  mark_as_advanced(COCOA)

  target_sources(text-freetype2 PRIVATE find-font.c find-font-cocoa.m
                                        find-font-iconv.c)

  target_link_libraries(text-freetype2 PRIVATE Iconv::Iconv ${COCOA})

elseif(OS_POSIX)
  find_package(Fontconfig REQUIRED)

  target_sources(text-freetype2 PRIVATE find-font-unix.c)

  target_link_libraries(text-freetype2 PRIVATE Fontconfig::Fontconfig)
endif()

setup_plugin_target(text-freetype2)
