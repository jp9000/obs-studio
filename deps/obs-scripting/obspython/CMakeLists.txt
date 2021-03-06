cmake_minimum_required(VERSION 2.8.12)
project(obspython)

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

add_definitions(-DSWIG_TYPE_TABLE=obspython -DMS_NO_COREDLL -DPy_ENABLE_SHARED=1 -DSWIG_PYTHON_INTERPRETER_NO_DEBUG)

if(MSVC)
	add_compile_options("/wd4054")
	add_compile_options("/wd4100")
	add_compile_options("/wd4115")
	add_compile_options("/wd4197")
	add_compile_options("/wd4701")
endif()

include_directories(${PYTHON_INCLUDE_DIR})
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/libobs")
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

#add_definitions( -DSWIG_TYPE_TABLE=libobs )
SET_SOURCE_FILES_PROPERTIES(obspython.i PROPERTIES SWIG_FLAGS "-modern")
SET_SOURCE_FILES_PROPERTIES(obspython.i PROPERTIES SWIG_FLAGS "-builtin")
SET_SOURCE_FILES_PROPERTIES(obspython.i PROPERTIES SWIG_FLAGS "-modernargs")
SET_SOURCE_FILES_PROPERTIES(obspython.i PROPERTIES SWIG_FLAGS "-includeall")
SET_SOURCE_FILES_PROPERTIES(obspython.i PROPERTIES SWIG_FLAGS "-importall")
SET_SOURCE_FILES_PROPERTIES(obspython.i PROPERTIES SWIG_FLAGS "-py3")

if(WIN32)
	string(REGEX REPLACE "_d" "" PYTHON_LIBRARIES "${PYTHON_LIBRARIES}")
endif()

if(CMAKE_VERSION VERSION_GREATER 3.7.2)
	SWIG_ADD_LIBRARY(obspython
		LANGUAGE python
		TYPE MODULE
		SOURCES  obspython.i ../cstrcache.cpp ../cstrcache.h)
else()
	SWIG_ADD_MODULE(obspython python obspython.i ../cstrcache.cpp ../cstrcache.h)
endif()

IF(APPLE)
	SWIG_LINK_LIBRARIES(obspython obs-scripting libobs)
ELSE()
	SWIG_LINK_LIBRARIES(obspython obs-scripting libobs ${PYTHON_LIBRARIES})
ENDIF()

set_target_properties(_obspython PROPERTIES FOLDER "scripting")

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

	if (APPLE)
		SET_TARGET_PROPERTIES(${additional_target} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
	endif()

	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/obspython.py"
		DESTINATION "${OBS_SCRIPT_PLUGIN_DESTINATION}")
	install(TARGETS "${additional_target}"
		LIBRARY DESTINATION "${OBS_SCRIPT_PLUGIN_DESTINATION}")

	add_custom_command(TARGET ${additional_target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${CMAKE_CURRENT_BINARY_DIR}/obspython.py"
			"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/data/obs-scripting/${_bit_suffix}/obspython.py"
		VERBATIM)
	add_custom_command(TARGET ${additional_target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"$<TARGET_FILE:${additional_target}>"
			"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/data/obs-scripting/${_bit_suffix}$<TARGET_FILE_NAME:${additional_target}>"
		VERBATIM)
endfunction()

install_plugin_bin_swig(obs-scripting _obspython)
