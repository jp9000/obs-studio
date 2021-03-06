project(w32-pthreads)

if(NOT WIN32)
	return()
endif()

set(MODULE_DESCRIPTION "POSIX Threads for Windows")
configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in w32-pthreads.rc)
set(w32-pthreads_SOURCES
	pthread.c
	w32-pthreads.rc)

set(w32-pthreads_HEADERS
	implement.h
	pthread.h
	sched.h
	semaphore.h)

add_library(w32-pthreads SHARED
	${w32-pthreads_SOURCES}
	${w32-pthreads_HEADERS})
target_compile_definitions(w32-pthreads
	PRIVATE __CLEANUP_C PTW32_BUILD)
target_include_directories(w32-pthreads
	PUBLIC
		"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
target_link_libraries(w32-pthreads)
set_target_properties(w32-pthreads PROPERTIES FOLDER "deps")

install_obs_core(w32-pthreads EXPORT w32-pthreads)
