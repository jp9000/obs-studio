cmake_minimum_required(VERSION 2.8.12)
project(obslua)

if(POLICY CMP0078)
	# UseSWIG generates standard target names.
	cmake_policy(SET CMP0078 OLD)
endif()
if(POLICY CMP0086)
	# UseSWIG honors SWIG_MODULE_NAME via -module flag.
	cmake_policy(SET CMP0086 OLD)
endif()

find_package(SWIG 2 REQUIRED)
include(${SWIG_USE_FILE})

add_definitions(-DSWIG_TYPE_TABLE=obslua -DSWIG_LUA_INTERPRETER_NO_DEBUG)

if(MSVC)
	add_compile_options("/wd4054")
	add_compile_options("/wd4197")
	add_compile_options("/wd4244")
	add_compile_options("/wd4267")
endif()

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/libobs")
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if(CMAKE_VERSION VERSION_GREATER 3.7.2)
	SWIG_ADD_LIBRARY(obslua
		LANGUAGE lua
		TYPE MODULE
		SOURCES obslua.i ../cstrcache.cpp ../cstrcache.h)
else()
	SWIG_ADD_MODULE(obslua lua obslua.i ../cstrcache.cpp ../cstrcache.h)
endif()
SWIG_LINK_LIBRARIES(obslua obs-scripting libobs ${LUA_LIBRARIES} ${EXTRA_LIBS})

set_target_properties(obslua PROPERTIES FOLDER "scripting")

function(install_plugin_bin_swig target additional_target)
	if(APPLE)
		set(_bit_suffix "")
	elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(_bit_suffix "64bit/")
	else()
		set(_bit_suffix "32bit/")
	endif()

	set_target_properties(${additional_target} PROPERTIES
		PREFIX "")

	install(TARGETS "${additional_target}"
		LIBRARY DESTINATION "${OBS_SCRIPT_PLUGIN_DESTINATION}")

	add_custom_command(TARGET ${additional_target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"$<TARGET_FILE:${additional_target}>"
			"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/data/obs-scripting/${_bit_suffix}$<TARGET_FILE_NAME:${additional_target}>"
		VERBATIM)
endfunction()

install_plugin_bin_swig(obs-scripting obslua)
