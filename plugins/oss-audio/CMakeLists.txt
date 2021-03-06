project(oss-audio)

if(DISABLE_OSS)
	message(STATUS "OSS support disabled")
	return()
endif()

find_package(OSS)
if(NOT OSS_FOUND AND ENABLE_OSS)
	message(FATAL_ERROR "OSS not found but set as enabled")
elseif(NOT OSS_FOUND)
	message(STATUS "OSS not found, disabling OSS plugin")
	return()
endif()

configure_file(oss-platform.h.in oss-platform.h)

include_directories(
	SYSTEM "${CMAKE_SOURCE_DIR}/libobs"
	"${OSS_INCLUDE_DIR}"
	"${CMAKE_CURRENT_BINARY_DIR}"
)

set(oss-audio_SOURCES
	oss-audio.c
	oss-input.c
)

add_library(oss-audio MODULE
	${oss-audio_SOURCES}
)
target_link_libraries(oss-audio
	libobs
)
set_target_properties(oss-audio PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(oss-audio data)
