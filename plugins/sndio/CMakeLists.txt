project(sndio)

if(DISABLE_SNDIO)
	message(STATUS "Sndio support disabled")
	return()
endif()

find_package(Sndio)
if(NOT Sndio_FOUND AND ENABLE_SNDIO)
	message(FATAL_ERROR "Sndio not found but set as enabled")
elseif(NOT Sndio_FOUND)
	message(STATUS "Sndio not found, disabling Sndio plugin")
	return()
endif()

include_directories(
	SYSTEM "${CMAKE_SOURCE_DIR}/libobs" SYSTEM "${CMAKE_SOURCE_DIR}/../../libobs"
	${Sndio_INCLUDE_DIRS}
)

set(sndio_SOURCES
	sndio.c
	sndio-input.c
)

add_library(sndio MODULE
	${sndio_SOURCES}
)
target_link_libraries(sndio
	libobs
	${Sndio_LIBRARIES}
)
set_target_properties(sndio PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(sndio data)
