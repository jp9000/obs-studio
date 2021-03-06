project(obs-transitions)

set(obs-transitions_SOURCES
	obs-transitions.c
	transition-slide.c
	transition-swipe.c
	transition-fade.c
	transition-cut.c
	transition-fade-to-color.c
	transition-luma-wipe.c
	transition-stinger.c
	)

if(WIN32)
	set(MODULE_DESCRIPTION "OBS Transitions module")
	configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in obs-transitions.rc)
	list(APPEND obs-transitions_SOURCES
		obs-transitions.rc)
endif()

add_library(obs-transitions MODULE
	${obs-transitions_SOURCES})
target_link_libraries(obs-transitions
	libobs)
set_target_properties(obs-transitions PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(obs-transitions data)
