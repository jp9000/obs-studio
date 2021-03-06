project(image-source)

if(MSVC)
	set(image-source_PLATFORM_DEPS
		w32-pthreads)
endif()

set(image-source_SOURCES
	image-source.c
	color-source.c
	obs-slideshow.c)

if(WIN32)
	set(MODULE_DESCRIPTION "OBS image module")
	configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in image-source.rc)
	list(APPEND image-source_SOURCES
		image-source.rc)
endif()

add_library(image-source MODULE
	${image-source_SOURCES})
target_link_libraries(image-source
	libobs
	${image-source_PLATFORM_DEPS})
set_target_properties(image-source PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(image-source data)
