project(rtmp-services)

option(ENABLE_SERVICE_UPDATES "Checks for service updates" OFF)

set(RTMP_SERVICES_URL
    "https://obsproject.com/obs2_update/rtmp-services"
    CACHE STRING "Default services package URL" FORCE)

mark_as_advanced(RTMP_SERVICES_URL)

add_library(rtmp-services MODULE)
add_library(OBS::rtmp-services ALIAS rtmp-services)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/lookup-config.h.in
               ${CMAKE_BINARY_DIR}/config/lookup-config.h)

target_sources(
  rtmp-services
  PRIVATE service-specific/twitch.c
          service-specific/twitch.h
          service-specific/younow.c
          service-specific/younow.h
          service-specific/nimotv.c
          service-specific/nimotv.h
          service-specific/showroom.c
          service-specific/showroom.h
          service-specific/dacast.c
          service-specific/dacast.h
          rtmp-common.c
          rtmp-custom.c
          rtmp-services-main.c
          rtmp-format-ver.h
          ${CMAKE_BINARY_DIR}/config/lookup-config.h)

target_include_directories(
  rtmp-services PRIVATE "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/config>")

target_link_libraries(rtmp-services PRIVATE OBS::libobs OBS::file-updater
                                            Jansson::Jansson)

if(OS_WINDOWS)
  set(MODULE_DESCRIPTION "OBS RTMP Services")
  configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
                 rtmp-services.rc)

  target_sources(rtmp-services PRIVATE rtmp-services.rc)
endif()

set_target_properties(rtmp-services PROPERTIES FOLDER "plugins" PREFIX "")

setup_plugin_target(rtmp-services)
