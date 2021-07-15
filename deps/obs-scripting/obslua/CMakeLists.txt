if(POLICY CMP0086)
  cmake_policy(SET CMP0086 NEW)
endif()

if(POLICY CMP0078)
  cmake_policy(SET CMP0078 NEW)
endif()

project(obslua)

find_package(Luajit REQUIRED)

if(OS_MACOS)
  find_package(SWIG 4 REQUIRED)
elseif(OS_POSIX)
  find_package(SWIG 3 REQUIRED)
elseif(OS_WINDOWS)
  find_package(SwigWindows 3 REQUIRED)
endif()
include(UseSWIG)

set_source_files_properties(obslua.i PROPERTIES USE_TARGET_INCLUDE_DIRECTORIES
                                                TRUE)

swig_add_library(
  obslua
  LANGUAGE lua
  TYPE MODULE
  SOURCES obslua.i ../cstrcache.cpp ../cstrcache.h)

target_link_libraries(obslua PRIVATE OBS::scripting OBS::libobs Luajit::Luajit)

set_target_properties(
  obslua PROPERTIES SWIG_COMPILE_DEFINITIONS
                    "SWIG_TYPE_TABLE=obslua;SWIG_LUA_INTERPRETER_NO_DEBUG")

set_target_properties(
  obslua
  PROPERTIES FOLDER "scripting"
             VERSION "${OBS_VERSION_MAJOR}"
             SOVERSION "${OBS_VERSION_CANONICAL}")

target_compile_definitions(obslua PRIVATE SWIG_TYPE_TABLE=obslua
                                          SWIG_LUA_INTERPRETER_NO_DEBUG)

if(ENABLE_UI)
  target_link_libraries(obslua PRIVATE OBS::frontend-api)

  target_compile_definitions(obslua PRIVATE ENABLE_UI)
endif()

if(OS_WINDOWS)
  if(MSVC)
    target_compile_options(obslua PRIVATE "/wd4054;/wd4197;/wd4244;/wd4267")
  endif()
elseif(OS_MACOS)
  set_target_properties(obslua PROPERTIES MACHO_CURRENT_VERSION 0
                                          MACHO_COMPATIBILITY_VERSION 0)
endif()

setup_script_plugin_target(obslua)
