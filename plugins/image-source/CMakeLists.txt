project(image-source)

add_library(image-source MODULE)
add_library(OBS::image-source ALIAS image-source)

target_sources(image-source PRIVATE image-source.c color-source.c
                                    obs-slideshow.c)

target_link_libraries(image-source PRIVATE OBS::libobs)

if(OS_WINDOWS)
  if(MSVC)
    target_link_libraries(image-source PRIVATE OBS::w32-pthreads)
  endif()

  set(MODULE_DESCRIPTION "OBS image module")
  configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
                 image-source.rc)

  target_sources(image-source PRIVATE image-source.rc)
endif()

set_property(GLOBAL APPEND PROPERTY OBS_MODULE_LIST "image-source")

set_target_properties(image-source PROPERTIES FOLDER "plugins" PREFIX "")

setup_plugin_target(image-source)
