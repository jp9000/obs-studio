project(file-updater)

find_package(CURL REQUIRED)

add_library(file-updater INTERFACE)
add_library(OBS::file-updater ALIAS file-updater)

target_sources(file-updater INTERFACE file-updater/file-updater.c
                                      file-updater/file-updater.h)

target_link_libraries(file-updater INTERFACE CURL::libcurl)

target_include_directories(
  file-updater INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

if(OS_WINDOWS AND NOT MINGW)
  target_link_libraries(file-updater INTERFACE OBS::w32-pthreads)
endif()
