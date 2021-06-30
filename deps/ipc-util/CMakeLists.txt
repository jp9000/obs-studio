project(ipc-util)

add_library(ipc-util STATIC)
add_library(OBS::ipc-util ALIAS ipc-util)

target_sources(ipc-util PRIVATE ipc-util/pipe.h)

set_target_properties(ipc-util PROPERTIES FOLDER "deps"
                                          POSITION_INDEPENDENT_CODE ON)

target_include_directories(
  ipc-util PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

if(OS_WINDOWS)
  if(MSVC)
    target_compile_options(ipc-util PRIVATE "$<IF:$<CONFIG:Debug>,/MTd,/MT>"
                                            "/Zl")
  endif()

  target_sources(ipc-util PRIVATE ipc-util/pipe-windows.c
                                  ipc-util/pipe-windows.h)
else()
  target_sources(ipc-util PRIVATE ipc-util/pipe-posix.c ipc-util/pipe-posic.h)
endif()
