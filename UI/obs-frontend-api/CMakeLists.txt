if(POLICY CMP0090)
  cmake_policy(SET CMP0090 NEW)
endif()

project(obs-frontend-api)

add_library(obs-frontend-api SHARED)
add_library(OBS::frontend-api ALIAS obs-frontend-api)

target_sources(obs-frontend-api PRIVATE obs-frontend-api.h obs-frontend-api.cpp
                                        obs-frontend-internal.hpp)

target_link_libraries(obs-frontend-api PRIVATE OBS::libobs)

target_compile_features(obs-frontend-api PUBLIC cxx_auto_type cxx_std_17
                                                c_std_11)

target_include_directories(
  obs-frontend-api PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

set_target_properties(
  obs-frontend-api
  PROPERTIES FOLDER "frontend"
             VERSION "${OBS_VERSION_MAJOR}"
             SOVERSION "0"
             PUBLIC_HEADER obs-frontend-api.h)

if(OS_WINDOWS)
  set(MODULE_DESCRIPTION "OBS Frontend API")
  configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
                 obs-frontend-api.rc)

  target_sources(obs-frontend-api PRIVATE obs-frontend-api.rc)
elseif(OS_MACOS)
  set_target_properties(obs-frontend-api PROPERTIES SOVERSION "1")
endif()

setup_binary_target(obs-frontend-api)
export_target(obs-frontend-api)
