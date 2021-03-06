cmake_minimum_required(VERSION 2.8.12)

if(NOT ENABLE_SCRIPTING)
	message(STATUS "Scripting plugin disabled")
	return()
endif()

project(obs-scripting)

if(POLICY CMP0068)
	# RPATH settings on macOS do not affect install_name.
	cmake_policy(SET CMP0068 NEW)
endif()

if(MSVC)
	set(obs-scripting_PLATFORM_DEPS
		w32-pthreads)
endif()

if(APPLE)
	set(obs-scripting_PLATFORM_DEPS
		objc)
endif()

option(DISABLE_LUA "Disable Lua scripting support" OFF)
option(DISABLE_PYTHON "Disable Python scripting support" OFF)

set(COMPILE_PYTHON FALSE CACHE BOOL "" FORCE)
set(COMPILE_LUA FALSE CACHE BOOL "" FORCE)

if(NOT DISABLE_LUA)
	find_package(Luajit QUIET)

	if(NOT DISABLE_LUA AND NOT LUAJIT_FOUND)
		message(STATUS "Luajit support not found.")
		set(LUAJIT_FOUND FALSE)
	else()
		message(STATUS "Scripting: Luajit supported")
		set(COMPILE_LUA TRUE CACHE BOOL "" FORCE)
	endif()
else()
	message(STATUS "Scripting: Luajit support disabled")
	set(LUAJIT_FOUND FALSE)
endif()

if(NOT DISABLE_PYTHON)
	find_package(PythonDeps QUIET)

	if(NOT DISABLE_PYTHON AND NOT PYTHONLIBS_FOUND)
		message(STATUS "Python support not found.")
		set(PYTHON_FOUND FALSE)
		set(PYTHONLIBS_FOUND FALSE)
	else()
		message(STATUS "Scripting: Python 3 supported")
		set(PYTHON_FOUND TRUE)
		set(COMPILE_PYTHON TRUE CACHE BOOL "" FORCE)

		get_filename_component(PYTHON_LIB "${PYTHON_LIBRARIES}" NAME)
		string(REGEX REPLACE "\\.[^.]*$" "" PYTHON_LIB ${PYTHON_LIB})

		if(WIN32)
			string(REGEX REPLACE "_d" "" PYTHON_LIB "${PYTHON_LIB}")
		endif()
	endif()
else()
	message(STATUS "Scripting: Python 3 support disabled")
	set(PYTHON_FOUND FALSE)
	set(PYTHONLIBS_FOUND FALSE)
endif()

find_package(SwigDeps QUIET 2)

if(NOT SWIG_FOUND)
	message(STATUS "Scripting: SWIG not found; scripting disabled")
	return()
endif()

if(NOT PYTHONLIBS_FOUND AND NOT LUAJIT_FOUND)
	message(STATUS "Scripting: Neither Python 3 nor Luajit was found; scripting plugin disabled")
	return()
endif()

set(SCRIPTING_ENABLED ON CACHE BOOL "Internal global cmake variable" FORCE)

if(UI_ENABLED)
	set(EXTRA_LIBS obs-frontend-api)
	include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
endif()

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/obs-scripting-config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/obs-scripting-config.h")

include(${SWIG_USE_FILE})

include_directories(${CMAKE_SOURCE_DIR}/libobs)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

if(PYTHONLIBS_FOUND)
	include_directories(${PYTHON_INCLUDE_DIR})

	set(obs-scripting-python_SOURCES
		obs-scripting-python.c
		)
	set(obs-scripting-python_HEADERS
		obs-scripting-python.h
		obs-scripting-python-import.h
		)

	if(UI_ENABLED)
		set(obs-scripting-python_SOURCES
			${obs-scripting-python_SOURCES}
			obs-scripting-python-frontend.c
			)
	endif()
	if(WIN32 OR APPLE)
		set(obs-scripting-python_SOURCES
			${obs-scripting-python_SOURCES}
			obs-scripting-python-import.c
			)
	else()
		set(EXTRA_LIBS ${EXTRA_LIBS} ${PYTHON_LIBRARIES})
	endif()
endif()

if(LUAJIT_FOUND)
	include_directories(${LUAJIT_INCLUDE_DIR})

	set(obs-scripting-lua_SOURCES
		obs-scripting-lua.c
		obs-scripting-lua-source.c
		)
	set(obs-scripting-lua_HEADERS
		obs-scripting-lua.h
		)
	if(UI_ENABLED)
		set(obs-scripting-lua_SOURCES
			${obs-scripting-lua_SOURCES}
			obs-scripting-lua-frontend.c
			)
	endif()
endif()

set(obs-scripting_SOURCES
	obs-scripting.c
	obs-scripting-logging.c
	cstrcache.cpp
	)
set(obs-scripting_HEADERS
	${CMAKE_CURRENT_BINARY_DIR}/obs-scripting-config.h
	obs-scripting.h
	obs-scripting-callback.h
	cstrcache.h
	)

if(WIN32)
	set(MODULE_DESCRIPTION "OBS Studio scripting module")
	configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in obs-scripting.rc)
	list(APPEND obs-scripting_SOURCES
		obs-scripting.rc)
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/swig)

if(PYTHONLIBS_FOUND)
	set(SWIG_PY_RUNTIME swig/swigpyrun.h)
	add_custom_command(OUTPUT ${SWIG_PY_RUNTIME}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		PRE_BUILD
		COMMAND ${SWIG_EXECUTABLE} -python -external-runtime ${SWIG_PY_RUNTIME}
		COMMENT "Scripting plugin: Building Python SWIG interface header"
		)
	set_source_files_properties(${SWIG_PY_RUNTIME} PROPERTIES GENERATED TRUE)
endif()

if(LUAJIT_FOUND)
	set(SWIG_LUA_RUNTIME swig/swigluarun.h)
	add_custom_command(OUTPUT ${SWIG_LUA_RUNTIME}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		PRE_BUILD
		COMMAND ${SWIG_EXECUTABLE} -lua -external-runtime ${SWIG_LUA_RUNTIME}
		COMMENT "Scripting: Building Lua SWIG interface header"
		)
	set_source_files_properties(${SWIG_LUA_RUNTIME} PROPERTIES GENERATED TRUE)
endif()

add_library(obs-scripting SHARED
	${obs-scripting_SOURCES}
	${obs-scripting_HEADERS}
	${obs-scripting-python_SOURCES}
	${obs-scripting-python_HEADERS}
	${obs-scripting-lua_SOURCES}
	${obs-scripting-lua_HEADERS}
	${SWIG_PY_RUNTIME}
	${SWIG_LUA_RUNTIME}
	)

target_link_libraries(obs-scripting
	libobs
	${LUAJIT_LIBRARIES}
	${EXTRA_LIBS}
	${obs-scripting_PLATFORM_DEPS}
	)

set_target_properties(obs-scripting PROPERTIES FOLDER "scripting")

if(PYTHONLIBS_FOUND)
	add_subdirectory(obspython)
endif()

if(LUAJIT_FOUND)
	add_subdirectory(obslua)
endif()

install_obs_core(obs-scripting)
