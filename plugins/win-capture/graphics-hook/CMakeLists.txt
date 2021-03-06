project(graphics-hook)

set(COMPILE_D3D12_HOOK FALSE CACHE BOOL "Compile D3D12 hook support (required windows 10 SDK)")

find_package(Vulkan REQUIRED)
include_directories(${VULKAN_INCLUDE_DIR})

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/graphics-hook-config.h.in"
	"${CMAKE_BINARY_DIR}/plugins/win-capture/graphics-hook/config/graphics-hook-config.h")


set(graphics-hook_HEADERS
	"${CMAKE_BINARY_DIR}/plugins/win-capture/graphics-hook/config/graphics-hook-config.h"
	graphics-hook.h
	../graphics-hook-ver.h
	../graphics-hook-info.h
	../hook-helpers.h
	../funchook.h
	../obfuscate.h
	gl-decs.h
	d3d9-patches.hpp)

set(graphics-hook_SOURCES
	graphics-hook.c
	../funchook.c
	../obfuscate.c
	gl-capture.c
	d3d8-capture.cpp
	d3d9-capture.cpp
	dxgi-capture.cpp
	d3d10-capture.cpp
	d3d11-capture.cpp
	d3d12-capture.cpp)

if(MSVC)
	add_compile_options("$<IF:$<CONFIG:Debug>,/MTd,/MT>")
endif()

if (VULKAN_FOUND)
	list(APPEND graphics-hook_SOURCES
		vulkan-capture.c)
	list(APPEND graphics-hook_HEADERS
		vulkan-capture.h)
endif()

add_library(graphics-hook MODULE
	graphics-hook.rc
	${graphics-hook_SOURCES}
	${graphics-hook_HEADERS})

target_include_directories(graphics-hook PUBLIC
	"${CMAKE_BINARY_DIR}/plugins/win-capture/graphics-hook/config")

target_link_libraries(graphics-hook
	dxguid
	ipc-util
	psapi)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_output_suffix "64")
else()
	set(_output_suffix "32")
endif()

set_target_properties(graphics-hook
	PROPERTIES
		FOLDER "plugins/win-capture"
		OUTPUT_NAME "graphics-hook${_output_suffix}")

if (VULKAN_FOUND)
	add_custom_command(TARGET graphics-hook POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${CMAKE_CURRENT_SOURCE_DIR}/obs-vulkan64.json" "${OBS_OUTPUT_DIR}/$<CONFIGURATION>/data/obs-plugins/win-capture/obs-vulkan64.json"
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${CMAKE_CURRENT_SOURCE_DIR}/obs-vulkan32.json" "${OBS_OUTPUT_DIR}/$<CONFIGURATION>/data/obs-plugins/win-capture/obs-vulkan32.json"
		VERBATIM)
endif()
install_obs_datatarget(graphics-hook "obs-plugins/win-capture")
