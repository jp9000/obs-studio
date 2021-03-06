project(rtmp-services)

find_package(Libcurl REQUIRED)

include_directories(${LIBCURL_INCLUDE_DIRS})

include_directories(${OBS_JANSSON_INCLUDE_DIRS})

set(rtmp-services_SOURCES
	twitch.c
	younow.c
	nimotv.c
	showroom.c
	rtmp-common.c
	rtmp-custom.c
	rtmp-services-main.c)

if(WIN32)
	set(MODULE_DESCRIPTION "OBS RTMP Services")
	configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in rtmp-services.rc)
	list(APPEND rtmp-services_SOURCES
		rtmp-services.rc)
endif()

set(rtmp-services_HEADERS
	twitch.h
	younow.h
	nimotv.h
	showroom.h
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
