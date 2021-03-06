project(win-capture)

if(MSVC)
	set(win-capture_PLATFORM_DEPS
		w32-pthreads)
endif()

set(win-capture_HEADERS
	nt-stuff.h
	obfuscate.h
	app-helpers.h
	hook-helpers.h
	inject-library.h
	cursor-capture.h
	graphics-hook-info.h
	graphics-hook-ver.h
	window-helpers.h
	dc-capture.h)

set(win-capture_SOURCES
	dc-capture.c
	obfuscate.c
	app-helpers.c
	inject-library.c
	cursor-capture.c
	game-capture.c
	window-helpers.c
	monitor-capture.c
	window-capture.c
	load-graphics-offsets.c
	game-capture-file-init.c
	duplicator-monitor-capture.c
	plugin-main.c)

add_library(win-capture MODULE
	${win-capture_SOURCES}
	${win-capture_HEADERS})
target_link_libraries(win-capture
	${win-capture_PLATFORM_DEPS}
	libobs
	Dwmapi
	ipc-util
	psapi)
set_target_properties(win-capture PROPERTIES FOLDER "plugins/win-capture")

install_obs_plugin_with_data(win-capture data)

add_subdirectory(graphics-hook)
add_subdirectory(get-graphics-offsets)
add_subdirectory(inject-helper)
