project(linux-jack)

if(DISABLE_JACK)
	message(STATUS "JACK support disabled")
	return()
endif()

find_package(Jack)
if(NOT JACK_FOUND AND ENABLE_JACK)
	message(FATAL_ERROR "JACK Audio Connection Kit not found but set as enabled")
elseif(NOT JACK_FOUND)
	message(STATUS "JACK Audio Connection Kit not found, disabling JACK plugin")
	return()
endif()

include_directories(
	SYSTEM "${CMAKE_SOURCE_DIR}/libobs"
	${JACK_INCLUDE_DIR}
)

set(linux-jack_SOURCES
	linux-jack.c
	jack-wrapper.c
	jack-input.c
)

add_library(linux-jack MODULE
	${linux-jack_SOURCES}
)
target_link_libraries(linux-jack
	libobs
	${JACK_LIBRARIES}
)
set_target_properties(linux-jack PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(linux-jack data)
