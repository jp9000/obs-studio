if(POLICY CMP0090)
  cmake_policy(SET CMP0090 NEW)
endif()

project(w32-pthreads)

add_library(w32-pthreads SHARED)
add_library(OBS::w32-pthreads ALIAS w32-pthreads)

target_sources(w32-pthreads PRIVATE implement.h pthread.c pthread.h sched.h
                                    semaphore.h w32-pthreads.rc)

set(MODULE_DESCRIPTION "POSIX Threads for Windows")
configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
               w32-pthreads.rc)

target_compile_definitions(w32-pthreads PRIVATE __CLEANUP_C PTW32_BUILD)

target_include_directories(
  w32-pthreads PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

set_target_properties(w32-pthreads PROPERTIES FOLDER "deps")

setup_binary_target(w32-pthreads)
export_target(w32-pthreads)
