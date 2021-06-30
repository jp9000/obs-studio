project(libcaption)

add_library(caption STATIC)
add_library(OBS::caption ALIAS caption)

target_sources(
  caption
  PRIVATE src/caption.c
          src/utf8.c
          caption/utf8.h
          src/srt.c
          src/scc.c
          caption/scc.h
          src/mpeg.c
          caption/mpeg.h
          src/cea708.c
          caption/cea708.h
          src/xds.c
          src/eia608.c
          caption/eia608.h
          src/eia608_from_utf8.c
          src/eia608_charmap.c
          caption/eia608_charmap.h
  PUBLIC caption/caption.h)

target_compile_definitions(
  caption PRIVATE __STDC_CONSTANT_MACROS
                  "$<$<BOOL:${OS_WINDOWS}>:_CRT_SECURE_NO_WARNINGS>")

target_include_directories(
  caption
  PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/caption"
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

set_target_properties(
  caption
  PROPERTIES FOLDER "deps"
             VERSION "0"
             SOVERSION "0"
             POSITION_INDEPENDENT_CODE ON)
