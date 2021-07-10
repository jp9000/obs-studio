if (NOT WIN32)
	return()
endif()

project(obs-ipc)

if(NOT DEFINED PACKAGE_SID)
	set(PACKAGE_SID "S-1-15-2-1")
endif()

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/ipc-config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/ipc-config.h")

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/deps/json11")

if(MSVC)
	set(obs-ipc_PLATFORM_DEPS
		w32-pthreads)
endif()

set(obs-ipc_SOURCES
	../../deps/json11/json11.cpp
	obs-ipc.cpp
	responder.cpp
	interface.cpp
	preview.cpp)

set(obs-ipc_HEADERS
	../../deps/json11/json11.hpp
    obs-ipc.hpp
	responder.hpp
	interface.hpp
	preview.hpp)

add_library(obs-ipc MODULE
	${obs-ipc_SOURCES}
	${obs-ipc_HEADERS})
target_link_libraries(obs-ipc
	libobs
	ipc-util
	obs-frontend-api
	rpcrt4
	${obs-ipc_PLATFORM_DEPS})
set_target_properties(obs-ipc PROPERTIES FOLDER "plugins")

install_obs_plugin(obs-ipc data)
