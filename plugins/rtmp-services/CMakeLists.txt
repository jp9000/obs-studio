project(rtmp-services)

find_package(Libcurl REQUIRED)

include_directories(${LIBCURL_INCLUDE_DIRS})

include_directories(${OBS_JANSSON_INCLUDE_DIRS})

option(ENABLE_PEERTUBE "Build PeerTube integration" OFF)
if(ENABLE_PEERTUBE)
	set(PEERTUBE_ENABLED ON CACHE BOOL "Internal global cmake variable" FORCE)
else()
	message(STATUS "PeerTube integration is not enabled.")
	set(PEERTUBE_ENABLED OFF CACHE BOOL "Internal global cmake variable" FORCE)
endif()

set(rtmp-services_SOURCES
	service-specific/twitch.c
	service-specific/younow.c
	service-specific/nimotv.c
	service-specific/showroom.c
	service-specific/dacast.c
	rtmp-common.c
	rtmp-custom.c
	rtmp-services-main.c)

if(PEERTUBE_ENABLED)
	add_definitions(-DPEERTUBE_ENABLED)
	list(APPEND rtmp-services_SOURCES
		peertube.c)
endif()

if(WIN32)
	set(MODULE_DESCRIPTION "OBS RTMP Services")
	configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in rtmp-services.rc)
	list(APPEND rtmp-services_SOURCES
		rtmp-services.rc)
endif()

set(rtmp-services_HEADERS
	service-specific/twitch.h
	service-specific/younow.h
	service-specific/nimotv.h
	service-specific/showroom.h
	service-specific/dacast.h
	rtmp-format-ver.h)

set(RTMP_SERVICES_URL
	"https://obsproject.com/obs2_update/rtmp-services"
	CACHE STRING "Default services package URL")
option(CHECK_FOR_SERVICE_UPDATES "Checks for service updates" OFF)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/lookup-config.h.in"
	"${CMAKE_BINARY_DIR}/plugins/rtmp-services/lookup-config.h")

set(rtmp-services_config_HEADERS
	"${CMAKE_BINARY_DIR}/plugins/rtmp-services/lookup-config.h")

add_library(rtmp-services MODULE
	${rtmp-services_SOURCES}
	${rtmp-services_HEADERS}
	${rtmp-services_config_HEADERS})

target_link_libraries(rtmp-services
	libobs
	file-updater
	${OBS_JANSSON_IMPORT}
	${LIBCURL_LIBRARIES})

set_target_properties(rtmp-services PROPERTIES FOLDER "plugins")

target_include_directories(rtmp-services
	PUBLIC
		"$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/plugins/rtmp-services>")

install_obs_plugin_with_data(rtmp-services data)
