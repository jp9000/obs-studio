project(linux-v4l2)

option(ENABLE_V4L2 "Build OBS with v4l2 support" ON)
if(NOT ENABLE_V4L2)
  message(STATUS "OBS: linux-v4l2 plugin not enabled")
  return()
endif()

option(ENABLE_UDEV "Build linux-v4l2 with UDEV support" ON)

find_package(Libv4l2 REQUIRED)

add_library(linux-v4l2 MODULE)
add_library(OBS::v4l2 ALIAS linux-v4l2)

target_sources(linux-v4l2 PRIVATE linux-v4l2.c v4l2-controls.c v4l2-input.c
                                  v4l2-helpers.c v4l2-output.c)

target_link_libraries(linux-v4l2 PRIVATE OBS::libobs LIB4L2::LIB4L2)

set_target_properties(linux-v4l2 PROPERTIES FOLDER "plugins")

if(NOT ENABLE_UDEV)
  target_compile_definitions(linux-v4l2 PRIVATE HAVE_UDEV)
else()
  find_package(Udev REQUIRED)
  target_sources(linux-v4l2 PRIVATE v4l2-udev.c)

  target_link_libraries(linux-v4l2 PRIVATE Udev::Udev)
endif()

setup_plugin_target(linux-v4l2)
