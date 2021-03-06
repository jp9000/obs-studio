project(text-freetype2)

if(DISABLE_FREETYPE)
	message(STATUS "Freetype text plugin disabled")
	return()
endif()

find_package(Freetype QUIET)
if(NOT FREETYPE_FOUND AND ENABLE_FREETYPE)
	message(FATAL_ERROR "Freetype library not found but set as enabled")
elseif(NOT FREETYPE_FOUND)
	message(STATUS "Freetype library not found, Freetype text plugin disabled")
	return()
endif()

if(WIN32)
	set(MODULE_DESCRIPTION "OBS Freetype text module")
	configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in text-freetype2.rc)
	set(text-freetype2_PLATFORM_SOURCES
		find-font.c
		find-font-windows.c
		text-freetype2.rc)
elseif(APPLE)
	find_package(Iconv QUIET)
	if(NOT ICONV_FOUND AND ENABLE_FREETYPE)
		message(FATAL_ERROR "Iconv library not found but plugin set as enabled")
	elseif(NOT ICONV_FOUND)
		message(STATUS "Iconv library not found, Freetype text plugin disabled")
		return()
	endif()

	find_library(COCOA Cocoa)

	set(text-freetype2_PLATFORM_SOURCES
		find-font.c
		find-font-cocoa.m
		find-font-iconv.c)

	include_directories(${COCOA}
		${ICONV_INCLUDE_DIRS})

	set(text-freetype2_PLATFORM_DEPS
		${COCOA}
		${ICONV_LIBRARIES})
else()
	find_package(Fontconfig QUIET)
	if(NOT FONTCONFIG_FOUND AND ENABLE_FREETYPE)
		message(FATAL_ERROR "fontconfig not found but plugin set as enabled")
	elseif(NOT FONTCONFIG_FOUND)
		message(STATUS "fontconfig not found, Freetype text plugin disabled")
		return()
	endif()

	set(text-freetype2_PLATFORM_SOURCES
		find-font-unix.c)

	include_directories(${FONTCONFIG_INCLUDE_DIRS})
endif()

include_directories(${FREETYPE_INCLUDE_DIRS})

set(text-freetype2_SOURCES
	find-font.h
	obs-convenience.c
	text-functionality.c
	text-freetype2.c
	obs-convenience.h
	text-freetype2.h)

add_library(text-freetype2 MODULE
	${text-freetype2_PLATFORM_SOURCES}
	${text-freetype2_SOURCES})
target_link_libraries(text-freetype2
	libobs
	${text-freetype2_PLATFORM_DEPS}
	${FREETYPE_LIBRARIES})
set_target_properties(text-freetype2 PROPERTIES FOLDER "plugins")

if(NOT WIN32)
	if(FONTCONFIG_FOUND)
		target_link_libraries(text-freetype2 ${FONTCONFIG_LIBRARIES})
	endif()
endif()

if(APPLE AND ICONV_FOUND)
	target_link_libraries(text-freetype2 ${ICONV_LIBRARIES})
endif()

install_obs_plugin_with_data(text-freetype2 data)
