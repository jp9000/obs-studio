project(linux-pulseaudio)

if(DISABLE_PULSEAUDIO)
	message(STATUS "PulseAudio support disabled")
	return()
endif()

find_package(PulseAudio)
if(NOT PULSEAUDIO_FOUND AND ENABLE_PULSEAUDIO)
	message(FATAL_ERROR "PulseAudio not found but set as enabled")
elseif(NOT PULSEAUDIO_FOUND)
	message(STATUS "PulseAudio not found, disabling PulseAudio plugin")
	return()
endif()

include_directories(
	SYSTEM "${CMAKE_SOURCE_DIR}/libobs"
	${PULSEAUDIO_INCLUDE_DIR}
)

set(linux-pulseaudio_SOURCES
	linux-pulseaudio.c
	pulse-wrapper.c
	pulse-input.c
)

add_library(linux-pulseaudio MODULE
	${linux-pulseaudio_SOURCES}
)
target_link_libraries(linux-pulseaudio
	libobs
	${PULSEAUDIO_LIBRARY}
)
set_target_properties(linux-pulseaudio PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(linux-pulseaudio data)
