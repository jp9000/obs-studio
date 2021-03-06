#
# This module defines the following variables:
#
#  FFMPEG_FOUND - All required components and the core library were found
#  FFMPEG_INCLUDE_DIRS - Combined list of all components include dirs
#  FFMPEG_LIBRARIES - Combined list of all components libraries
#  FFMPEG_VERSION_STRING - Version of the first component requested
#
# For each requested component the following variables are defined:
#
#  FFMPEG_<component>_FOUND - The component was found
#  FFMPEG_<component>_INCLUDE_DIRS - The components include dirs
#  FFMPEG_<component>_LIBRARIES - The components libraries
#  FFMPEG_<component>_VERSION_STRING - The components version string
#  FFMPEG_<component>_VERSION_MAJOR - The components major version
#  FFMPEG_<component>_VERSION_MINOR - The components minor version
#  FFMPEG_<component>_VERSION_MICRO - The components micro version
#
# <component> is the uppercase name of the component


find_package(PkgConfig QUIET)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_lib_suffix 64)
else()
	set(_lib_suffix 32)
endif()

function(find_ffmpeg_library component header)
	string(TOUPPER "${component}" component_u)
	set(FFMPEG_${component_u}_FOUND FALSE PARENT_SCOPE)
	set(FFmpeg_${component}_FOUND FALSE PARENT_SCOPE)

	if(PKG_CONFIG_FOUND)
		pkg_check_modules(PC_FFMPEG_${component} QUIET lib${component})
	endif()

	find_path(FFMPEG_${component}_INCLUDE_DIR
		NAMES
			"lib${component}/${header}" "lib${component}/version.h"
		HINTS
			ENV FFmpegPath${_lib_suffix}
			ENV FFmpegPath
			ENV DepsPath${_lib_suffix}
			ENV DepsPath
			${FFmpegPath${_lib_suffix}}
			${FFmpegPath}
			${DepsPath${_lib_suffix}}
			${DepsPath}
			${PC_FFMPEG_${component}_INCLUDE_DIRS}
		PATHS
			/usr/include /usr/local/include /opt/local/include /sw/include
		PATH_SUFFIXES ffmpeg libav include)

	find_library(FFMPEG_${component}_LIBRARY
		NAMES
			"${component}" "lib${component}"
		HINTS
			ENV FFmpegPath${_lib_suffix}
			ENV FFmpegPath
			ENV DepsPath${_lib_suffix}
			ENV DepsPath
			${FFmpegPath${_lib_suffix}}
			${FFmpegPath}
			${DepsPath${_lib_suffix}}
			${DepsPath}
			${PC_FFMPEG_${component}_LIBRARY_DIRS}
		PATHS
			/usr/lib /usr/local/lib /opt/local/lib /sw/lib
		PATH_SUFFIXES
			lib${_lib_suffix} lib
			libs${_lib_suffix} libs
			bin${_lib_suffix} bin
			../lib${_lib_suffix} ../lib
			../libs${_lib_suffix} ../libs
			../bin${_lib_suffix} ../bin)

	set(FFMPEG_${component_u}_INCLUDE_DIRS ${FFMPEG_${component}_INCLUDE_DIR} PARENT_SCOPE)
	set(FFMPEG_${component_u}_LIBRARIES ${FFMPEG_${component}_LIBRARY} PARENT_SCOPE)

	mark_as_advanced(FFMPEG_${component}_INCLUDE_DIR FFMPEG_${component}_LIBRARY)

	if(FFMPEG_${component}_INCLUDE_DIR AND FFMPEG_${component}_LIBRARY)
		set(FFMPEG_${component_u}_FOUND TRUE PARENT_SCOPE)
		set(FFmpeg_${component}_FOUND TRUE PARENT_SCOPE)

		list(APPEND FFMPEG_INCLUDE_DIRS ${FFMPEG_${component}_INCLUDE_DIR})
		list(REMOVE_DUPLICATES FFMPEG_INCLUDE_DIRS)
		set(FFMPEG_INCLUDE_DIRS "${FFMPEG_INCLUDE_DIRS}" PARENT_SCOPE)

		list(APPEND FFMPEG_LIBRARIES ${FFMPEG_${component}_LIBRARY})
		list(REMOVE_DUPLICATES FFMPEG_LIBRARIES)
		set(FFMPEG_LIBRARIES "${FFMPEG_LIBRARIES}" PARENT_SCOPE)

		set(FFMPEG_${component_u}_VERSION_STRING "unknown" PARENT_SCOPE)
		set(_vfile "${FFMPEG_${component}_INCLUDE_DIR}/lib${component}/version.h")

		if(EXISTS "${_vfile}")
			file(STRINGS "${_vfile}" _version_parse REGEX "^.*VERSION_(MAJOR|MINOR|MICRO)[ \t]+[0-9]+[ \t]*$")
			string(REGEX REPLACE ".*VERSION_MAJOR[ \t]+([0-9]+).*" "\\1" _major "${_version_parse}")
			string(REGEX REPLACE ".*VERSION_MINOR[ \t]+([0-9]+).*" "\\1" _minor "${_version_parse}")
			string(REGEX REPLACE ".*VERSION_MICRO[ \t]+([0-9]+).*" "\\1" _micro "${_version_parse}")

			set(FFMPEG_${component_u}_VERSION_MAJOR "${_major}" PARENT_SCOPE)
			set(FFMPEG_${component_u}_VERSION_MINOR "${_minor}" PARENT_SCOPE)
			set(FFMPEG_${component_u}_VERSION_MICRO "${_micro}" PARENT_SCOPE)

			set(FFMPEG_${component_u}_VERSION_STRING "${_major}.${_minor}.${_micro}" PARENT_SCOPE)
		else()
			message(STATUS "Failed parsing FFmpeg ${component} version")
		endif()
	endif()
endfunction()

set(FFMPEG_INCLUDE_DIRS)
set(FFMPEG_LIBRARIES)

if(NOT FFmpeg_FIND_COMPONENTS)
	message(FATAL_ERROR "No FFmpeg components requested")
endif()

list(GET FFmpeg_FIND_COMPONENTS 0 _first_comp)
string(TOUPPER "${_first_comp}" _first_comp)

foreach(component ${FFmpeg_FIND_COMPONENTS})
	if(component STREQUAL "avcodec")
		find_ffmpeg_library("${component}" "avcodec.h")
	elseif(component STREQUAL "avdevice")
		find_ffmpeg_library("${component}" "avdevice.h")
	elseif(component STREQUAL "avfilter")
		find_ffmpeg_library("${component}" "avfilter.h")
	elseif(component STREQUAL "avformat")
		find_ffmpeg_library("${component}" "avformat.h")
	elseif(component STREQUAL "avresample")
		find_ffmpeg_library("${component}" "avresample.h")
	elseif(component STREQUAL "avutil")
		find_ffmpeg_library("${component}" "avutil.h")
	elseif(component STREQUAL "postproc")
		find_ffmpeg_library("${component}" "postprocess.h")
	elseif(component STREQUAL "swresample")
		find_ffmpeg_library("${component}" "swresample.h")
	elseif(component STREQUAL "swscale")
		find_ffmpeg_library("${component}" "swscale.h")
	else()
		message(FATAL_ERROR "Unknown FFmpeg component requested: ${component}")
	endif()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFmpeg
	FOUND_VAR FFMPEG_FOUND
	REQUIRED_VARS FFMPEG_${_first_comp}_LIBRARIES FFMPEG_${_first_comp}_INCLUDE_DIRS
	VERSION_VAR FFMPEG_${_first_comp}_VERSION_STRING
	HANDLE_COMPONENTS)
