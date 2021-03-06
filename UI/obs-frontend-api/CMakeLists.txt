project(obs-frontend-api)

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/libobs")

add_definitions(-DLIBOBS_EXPORTS)

set(obs-frontend-api_SOURCES
	obs-frontend-api.cpp)

if(WIN32)
	set(MODULE_DESCRIPTION "OBS Frontend API")
	configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in ./obs-frontend-api.rc)
	list(APPEND obs-frontend-api_SOURCES
		obs-frontend-api.rc)
endif()

set(obs-frontend-api_PUBLIC_HEADERS
	obs-frontend-api.h)

set(obs-frontend-api_HEADERS
	obs-frontend-internal.hpp
	${obs-frontend-api_PUBLIC_HEADERS})

add_library(obs-frontend-api SHARED
	${obs-frontend-api_SOURCES}
	${obs-frontend-api_HEADERS})
target_link_libraries(obs-frontend-api
	libobs)
set_target_properties(obs-frontend-api PROPERTIES FOLDER "frontend")

if(UNIX AND NOT APPLE)
	set_target_properties(obs-frontend-api
		PROPERTIES
			OUTPUT_NAME obs-frontend-api
			VERSION 0.0
			SOVERSION 0
			)
endif()

install_obs_core(obs-frontend-api)
install_obs_headers(${obs-frontend-api_PUBLIC_HEADERS})
