project(libobs-winrt)

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/libobs")

add_definitions(-DLIBOBS_EXPORTS)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(libobs-winrt_SOURCES
	winrt-capture.cpp
	winrt-dispatch.cpp)

set(libobs-winrt_HEADERS
	winrt-capture.h
	winrt-dispatch.h)

add_library(libobs-winrt MODULE
	${libobs-winrt_SOURCES}
	${libobs-winrt_HEADERS})
set_target_properties(libobs-winrt
	PROPERTIES
		FOLDER "core"
		OUTPUT_NAME libobs-winrt
		PREFIX "")
target_precompile_headers(libobs-winrt
	PRIVATE
		[["../libobs/util/windows/ComPtr.hpp"]]
		<obs-module.h>
		<d3d11.h>
		<DispatcherQueue.h>
		<dwmapi.h>
		<Windows.Graphics.Capture.Interop.h>
		<winrt/Windows.Foundation.Metadata.h>
		<winrt/Windows.Graphics.Capture.h>
		<winrt/Windows.System.h>)
target_link_libraries(libobs-winrt
	libobs
	Dwmapi
	windowsapp)

install_obs_core(libobs-winrt)
