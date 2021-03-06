project(win-ivcam)

if(DISABLE_IVCAM)
	message(STATUS "Realsense camera plugin disabled")
	return()
endif()

find_package(RSSDK QUIET)
if(NOT RSSDK_FOUND AND ENABLE_IVCAM)
	message(FATAL_ERROR "RSSDK not found, but the realsense camera plugin is set as enabled")
elseif(NOT RSSDK_FOUND)
	message(STATUS "RSSDK not found, Realsense camera plugin disabled")
	return()
endif()

include(IDLFileHelper)

set(win-ivcam_seg_library_IDLS
	seg_service/seg_service.idl
	)

add_idl_files(win-ivcam_seg_library_GENERATED_FILES
	${win-ivcam_seg_library_IDLS})

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	)

set(win-ivcam_seg_library_HEADERS
	seg_library/Dependencies.h
	seg_library/SegImage.h
	seg_library/SegServer.h
	seg_library/SegServerImpl.h
	seg_library/SegService.h
	)
set(win-ivcam_seg_library_SOURCES
	seg_library/SerServer.cpp
	seg_library/SegServerImpl.cpp
	seg_library/SegImage.cpp
	${win-ivcam_seg_library_GENERATED_FILES}
	)

set(MODULE_DESCRIPTION "OBS Intel(R) RealSense(TM) module")
configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in win-ivcam.rc)
set(win-ivcam_SOURCES
	realsense.cpp
	win-ivcam.rc)

source_group("seg_library\\Source Files" FILES ${win-ivcam_seg_library_SOURCES})
source_group("seg_library\\Header Files" FILES ${win-ivcam_seg_library_HEADERS})

add_library(win-ivcam MODULE
	${win-ivcam_seg_library_HEADERS}
	${win-ivcam_seg_library_SOURCES}
	${win-ivcam_SOURCES}
	)
target_link_libraries(win-ivcam
	libobs)

install_obs_plugin(win-ivcam)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	add_subdirectory(seg_service)
endif()
