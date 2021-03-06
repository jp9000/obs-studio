cmake_minimum_required(VERSION 3.5)
project(obs-virtualcam-module)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_output_suffix "64")
else()
	set(_output_suffix "32")
endif()

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/virtualcam-module.def.in"
	"${CMAKE_CURRENT_BINARY_DIR}/virtualcam-module.def")

set(MODULE_DESCRIPTION "OBS Virtual Camera output module")
configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in virtualcam-module.rc)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/..)

set(libdshowcapture_SOURCES
	../libdshowcapture/source/log.cpp
	../libdshowcapture/source/dshow-base.cpp
	../libdshowcapture/source/dshow-enum.cpp
	../libdshowcapture/source/dshow-formats.cpp
	../libdshowcapture/source/dshow-media-type.cpp
	../libdshowcapture/source/output-filter.cpp
	)

set(libdshowcapture_HEADERS
	../libdshowcapture/source/ComPtr.hpp
	../libdshowcapture/source/CoTaskMemPtr.hpp
	../libdshowcapture/source/log.hpp
	../libdshowcapture/source/dshow-base.hpp
	../libdshowcapture/source/dshow-enum.hpp
	../libdshowcapture/source/dshow-formats.hpp
	../libdshowcapture/source/dshow-media-type.hpp
	../libdshowcapture/source/output-filter.hpp
	../libdshowcapture/dshowcapture.hpp
	)

set(obs-virtualcam-module_SOURCES
	"${CMAKE_CURRENT_BINARY_DIR}/virtualcam-module.def"
	sleepto.c
	placeholder.cpp
	virtualcam-module.cpp
	virtualcam-filter.cpp
	virtualcam-module.rc
	../shared-memory-queue.c
	../tiny-nv12-scale.c
	)

set(obs-virtualcam-module_HEADERS
	sleepto.h
	virtualcam-filter.hpp
	../shared-memory-queue.h
	../tiny-nv12-scale.h
	)

if(MSVC)
	add_compile_options("$<IF:$<CONFIG:Debug>,/MTd,/MT>")
endif()

include_directories(${CMAKE_SOURCE_DIR}/libobs/util)

source_group("libdshowcapture\\Source Files" FILES ${libdshowcapture_SOURCES})
source_group("libdshowcapture\\Header Files" FILES ${libdshowcapture_HEADERS})

set(CMAKE_MODULE_LINKER_FLAGS "${MAKE_MODULE_LINKER_FLAGS} /ignore:4104")

add_library(obs-virtualcam-module MODULE
	${libdshowcapture_SOURCES}
	${libdshowcapture_HEADERS}
	${obs-virtualcam-module_SOURCES}
	${obs-virtualcam-module_HEADERS})
target_link_libraries(obs-virtualcam-module
	winmm
	strmiids
	gdiplus
	)
set_target_properties(obs-virtualcam-module PROPERTIES FOLDER "plugins/win-dshow")

set_target_properties(obs-virtualcam-module
	PROPERTIES
		OUTPUT_NAME "obs-virtualcam-module${_output_suffix}")
install_obs_datatarget(obs-virtualcam-module "obs-plugins/win-dshow")
