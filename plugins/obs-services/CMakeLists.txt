project(obs-services)

include_directories(${OBS_JANSSON_INCLUDE_DIRS})

set(RTMPS_DISABLED FALSE)
set(FTL_DISABLED FALSE)

if(RTMPS_NOT_AVAILABLE)
	set(RTMPS_DISABLED TRUE)
endif()

if(FTL_NOT_AVAILABLE)
	set(FTL_DISABLED TRUE)
endif()

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/protocols.hpp.in"
	"${CMAKE_BINARY_DIR}/plugins/obs-services/config/protocols.hpp")

include_directories("${CMAKE_BINARY_DIR}/plugins/obs-services/config")

set(obs-services_SOURCES
	service-instance.cpp
	service-factory.cpp
	service-manager.cpp
	plugin-main.cpp)
set(obs-services_HEADERS
	service-instance.cpp
	service-factory.cpp
	service-manager.cpp
	json-format-ver.hpp
	"${CMAKE_BINARY_DIR}/plugins/obs-services/config/protocols.hpp"
	plugin.hpp)

if(WIN32)
	set(MODULE_DESCRIPTION "OBS Core Services")
	configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in obs-services.rc)
	list(APPEND obs-services_SOURCES
		obs-services.rc)
endif()

add_library(obs-services MODULE
	${obs-services_SOURCES}
	${obs-services_HEADERS})

target_link_libraries(obs-services
	libobs
	${OBS_JANSSON_IMPORT})

set_target_properties(obs-services PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(obs-services data)
