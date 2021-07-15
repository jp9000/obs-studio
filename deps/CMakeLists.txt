if(OS_WINDOWS)
  if(NOT MINGW)
    add_subdirectory(w32-pthreads)
  endif()
  add_subdirectory(ipc-util)

  add_subdirectory(blake2)
  add_subdirectory(lzma)
endif()

add_subdirectory(glad)
add_subdirectory(media-playback)
add_subdirectory(file-updater)
add_subdirectory(obs-scripting)
add_subdirectory(libcaption)

# Use bundled jansson version as fallback
find_package(Jansson 2.5 QUIET)
if(NOT TARGET Jansson::Jansson)
  message(STATUS "OBS: Jansson >=2.5 not found, building bundled version")

  add_subdirectory(jansson)
  add_library(Jansson::Jansson ALIAS jansson)
else()
  message(STATUS "OBS: OBS-Studio - Using system Jansson library")
endif()
