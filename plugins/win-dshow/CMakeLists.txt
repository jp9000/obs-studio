if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libdshowcapture/dshowcapture.hpp")
	message(STATUS "libdshowcapture submodule not found!  Please fetch submodules.  win-dshow plugin disabled.")
	return()
endif()

project(win-dshow)

find_package(FFmpeg REQUIRED COMPONENTS avcodec avutil)
include_directories(${FFMPEG_INCLUDE_DIRS})

set(win-dshow_HEADERS
	encode-dstr.hpp
	ffmpeg-decode.h)

set(MODULE_DESCRIPTION "OBS DirectShow module")

set(VIRTUALCAM_GUID "" CACHE STRING "VirtualCam GUID")

set(VIRTUALCAM_ENABLED FALSE)
if(VIRTUALCAM_GUID STREQUAL "")
	message(STATUS "VirtualCam GUID not set! VirtualCam disabled.")
else()
	set(INVALID_GUID TRUE)

	string(REPLACE "-" ";" GUID_VALS ${VIRTUALCAM_GUID})

	list(LENGTH GUID_VALS GUID_VAL_COUNT)
	if(GUID_VAL_COUNT EQUAL 5)
		string(REPLACE ";" "0" GUID_HEX ${GUID_VALS})
		string(REGEX MATCH "[0-9a-fA-F]+" GUID_ACTUAL_HEX ${GUID_HEX})
		if(GUID_ACTUAL_HEX STREQUAL GUID_HEX)
			list(GET GUID_VALS 0 GUID_VALS_DATA1)
			list(GET GUID_VALS 1 GUID_VALS_DATA2)
			list(GET GUID_VALS 2 GUID_VALS_DATA3)
			list(GET GUID_VALS 3 GUID_VALS_DATA4)
			list(GET GUID_VALS 4 GUID_VALS_DATA5)
			string(LENGTH ${GUID_VALS_DATA1} GUID_VALS_DATA1_LENGTH)
			string(LENGTH ${GUID_VALS_DATA2} GUID_VALS_DATA2_LENGTH)
			string(LENGTH ${GUID_VALS_DATA3} GUID_VALS_DATA3_LENGTH)
			string(LENGTH ${GUID_VALS_DATA4} GUID_VALS_DATA4_LENGTH)
			string(LENGTH ${GUID_VALS_DATA5} GUID_VALS_DATA5_LENGTH)
			if(GUID_VALS_DATA1_LENGTH EQUAL 8 AND
			   GUID_VALS_DATA2_LENGTH EQUAL 4 AND
			   GUID_VALS_DATA3_LENGTH EQUAL 4 AND
			   GUID_VALS_DATA4_LENGTH EQUAL 4 AND
			   GUID_VALS_DATA5_LENGTH EQUAL 12)
				set(GUID_VAL01 ${GUID_VALS_DATA1})
				set(GUID_VAL02 ${GUID_VALS_DATA2})
				set(GUID_VAL03 ${GUID_VALS_DATA3})
				string(SUBSTRING ${GUID_VALS_DATA4} 0 2 GUID_VAL04)
				string(SUBSTRING ${GUID_VALS_DATA4} 2 2 GUID_VAL05)
				string(SUBSTRING ${GUID_VALS_DATA5} 0 2 GUID_VAL06)
				string(SUBSTRING ${GUID_VALS_DATA5} 2 2 GUID_VAL07)
				string(SUBSTRING ${GUID_VALS_DATA5} 4 2 GUID_VAL08)
				string(SUBSTRING ${GUID_VALS_DATA5} 6 2 GUID_VAL09)
				string(SUBSTRING ${GUID_VALS_DATA5} 8 2 GUID_VAL10)
				string(SUBSTRING ${GUID_VALS_DATA5} 10 2 GUID_VAL11)
				set(VIRTUALCAM_ENABLED TRUE)
				set(INVALID_GUID FALSE)
			endif()
		endif()
	endif()
endif()

if(INVALID_GUID)
	message(WARNING "Invalid VirtualCam GUID! VirtualCam disabled.")
endif()

if(VIRTUALCAM_ENABLED)
	add_definitions(-DVIRTUALCAM_ENABLED)
endif()

configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in win-dshow.rc)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/virtualcam-guid.h.in ${CMAKE_CURRENT_BINARY_DIR}/virtualcam-guid.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(win-dshow_SOURCES
	win-dshow.cpp
	win-dshow-encoder.cpp
	dshow-plugin.cpp
	ffmpeg-decode.c
	win-dshow.rc)

if(VIRTUALCAM_ENABLED)
	set(virtualcam-output_SOURCES
		tiny-nv12-scale.c
		shared-memory-queue.c
		virtualcam.c)
	set(virtualcam-output_HEADERS
		tiny-nv12-scale.h
		shared-memory-queue.h)
endif()

set(libdshowcapture_SOURCES
	libdshowcapture/source/capture-filter.cpp
	libdshowcapture/source/output-filter.cpp
	libdshowcapture/source/dshowcapture.cpp
	libdshowcapture/source/dshowencode.cpp
	libdshowcapture/source/device.cpp
	libdshowcapture/source/encoder.cpp
	libdshowcapture/source/dshow-base.cpp
	libdshowcapture/source/dshow-demux.cpp
	libdshowcapture/source/dshow-enum.cpp
	libdshowcapture/source/dshow-formats.cpp
	libdshowcapture/source/dshow-media-type.cpp
	libdshowcapture/source/dshow-encoded-device.cpp
	libdshowcapture/source/log.cpp)

set(libdshowcapture_HEADERS
	libdshowcapture/dshowcapture.hpp
	libdshowcapture/source/external/IVideoCaptureFilter.h
	libdshowcapture/source/capture-filter.hpp
	libdshowcapture/source/output-filter.hpp
	libdshowcapture/source/device.hpp
	libdshowcapture/source/encoder.hpp
	libdshowcapture/source/dshow-base.hpp
	libdshowcapture/source/dshow-demux.hpp
	libdshowcapture/source/dshow-device-defs.hpp
	libdshowcapture/source/dshow-enum.hpp
	libdshowcapture/source/dshow-formats.hpp
	libdshowcapture/source/dshow-media-type.hpp
	libdshowcapture/source/log.hpp)

add_library(win-dshow MODULE
	${win-dshow_SOURCES}
	${win-dshow_HEADERS}
	${virtualcam-output_SOURCES}
	${virtualcam-output_HEADERS}
	${libdshowcapture_SOURCES}
	${libdshowcapture_HEADERS})
target_link_libraries(win-dshow
	libobs
	strmiids
	ksuser
	wmcodecdspuuid
	w32-pthreads
	${FFMPEG_LIBRARIES})
set_target_properties(win-dshow PROPERTIES FOLDER "plugins/win-dshow")

source_group("libdshowcapture\\Source Files" FILES ${libdshowcapture_SOURCES})
source_group("libdshowcapture\\Header Files" FILES ${libdshowcapture_HEADERS})

install_obs_plugin_with_data(win-dshow data)

if(VIRTUALCAM_ENABLED)
	configure_file(virtualcam-install.bat.in "${CMAKE_CURRENT_BINARY_DIR}/data/virtualcam-install.bat")
	configure_file(virtualcam-uninstall.bat.in "${CMAKE_CURRENT_BINARY_DIR}/data/virtualcam-uninstall.bat")
	install_obs_data_from_abs_path(win-dshow "${CMAKE_CURRENT_BINARY_DIR}/data" "obs-plugins/win-dshow")
	add_subdirectory(virtualcam-module)
endif()
