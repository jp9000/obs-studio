project(win-capture)

add_library(win-capture MODULE)
add_library(OBS::capture ALIAS win-capture)

target_sources(
  win-capture
  PRIVATE plugin-main.c
          app-helpers.c
          app-helpers.h
          cursor-capture.c
          cursor-capture.h
          dc-capture.c
          dc-capture.h
          duplicator-monitor-capture.c
          game-capture.c
          game-capture-file-init.c
          graphics-hook-info.h
          graphics-hook-ver.h
          hook-helpers.h
          inject-library.c
          inject-library.h
          load-graphics-offsets.c
          monitor-capture.c
          nt-stuff.h
          obfuscate.c
          obfuscate.h
          window-capture.c
          window-helpers.c
          window-helpers.h)

target_link_libraries(win-capture PRIVATE OBS::libobs OBS::ipc-util Dwmapi
                                          psapi)

set_target_properties(win-capture PROPERTIES FOLDER "plugins/win-capture")

if(MSVC)
  target_link_libraries(win-capture PRIVATE OBS::w32-pthreads)
endif()

target_compile_definitions(
  win-capture PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS
                      _CRT_NONSTDC_NO_WARNINGS)

set_property(GLOBAL APPEND PROPERTY OBS_MODULE_LIST "win-capture")
message(STATUS "OBS: win-capture plugin enabled")

setup_plugin_target(win-capture)

add_subdirectory(graphics-hook)
add_subdirectory(get-graphics-offsets)
add_subdirectory(inject-helper)
