project(updater)

option(ENABLE_UPDATER "Build with Windows updater" ON)

if(NOT ENABLE_UPDATER)
  message(STATUS "OBS: Windows updater not enabled")
  return()
endif()

if(NOT DEFINED STATIC_ZLIB_PATH OR "${STATIC_ZLIB_PATH}" STREQUAL "")
  message(STATUS "STATIC_ZLIB_PATH not set, windows updater disabled")
  return()
endif()

add_executable(updater WIN32)

target_sources(
  updater
  PRIVATE updater.cpp
          updater.hpp
          patch.cpp
          http.cpp
          hash.cpp
          resource.h
          updater.rc
          init-hook-files.c
          updater.manifest
          ${CMAKE_SOURCE_DIR}/UI/win-update/win-update-helpers.cpp
          ${CMAKE_SOURCE_DIR}/UI/win-update/win-update-helpers.hpp
          ${CMAKE_SOURCE_DIR}/deps/json11/json11.hpp
          ${CMAKE_SOURCE_DIR}/deps/json11/json11.cpp)

target_include_directories(
  updater PRIVATE ${CMAKE_SOURCE_DIR}/libobs ${CMAKE_SOURCE_DIR}/UI/win-update
                  ${CMAKE_SOURCE_DIR}/deps/json11)

target_compile_definitions(updater PRIVATE NOMINMAX)

if(MSVC)
  target_compile_options(updater PRIVATE $<IF:$<CONFIG:DEBUG>,/MTd,/MT>)
endif()

target_link_libraries(
  updater
  PRIVATE OBS::blake2
          OBS::lzma
          ${STATIC_ZLIB_PATH}
          psapi
          comctl32
          shell32
          winhttp)

set_target_properties(updater PROPERTIES FOLDER "frontend")
