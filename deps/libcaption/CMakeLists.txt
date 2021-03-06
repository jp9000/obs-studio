cmake_minimum_required(VERSION 2.8.12)
project(libcaption)
add_definitions(-D__STDC_CONSTANT_MACROS)
if (WIN32)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if (UNIX AND NOT APPLE)
  add_compile_options("-fPIC")
endif()

# Don't need to prefix local includes with "caption/*"
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/caption)

set(CAPTION_SOURCES
  src/utf8.c
  src/srt.c
  src/scc.c
  src/mpeg.c
  src/cea708.c
  src/xds.c
  src/caption.c
  src/eia608_charmap.c
  src/eia608_from_utf8.c
  src/eia608.c
)

set(CAPTION_HEADERS
  caption/utf8.h
  caption/scc.h
  caption/mpeg.h
  caption/cea708.h
  caption/eia608.h
  caption/caption.h
  caption/eia608_charmap.h
)

add_library(caption STATIC ${CAPTION_SOURCES})
set_target_properties(caption PROPERTIES
	FOLDER "deps"
	POSITION_INDEPENDENT_CODE ON)
