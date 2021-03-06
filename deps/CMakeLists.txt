
if(NOT MINGW)
add_subdirectory(w32-pthreads)
endif()

add_subdirectory(glad)
add_subdirectory(ipc-util)

if(BUILD_LIBFF)
	add_subdirectory(libff)
endif()

add_subdirectory(media-playback)
add_subdirectory(file-updater)
add_subdirectory(obs-scripting)

if(WIN32)
	add_subdirectory(blake2)
	add_subdirectory(lzma)
endif()

add_subdirectory(libcaption)

find_package(Jansson 2.5 QUIET)

if(NOT JANSSON_FOUND)
	message(STATUS "Jansson >=2.5 not found, building bundled version")

	add_subdirectory(jansson)

	set(OBS_JANSSON_IMPORT "jansson" CACHE INTERNAL "Internal var")
	set(OBS_JANSSON_INCLUDE_DIRS "" CACHE INTERNAL "Internal var")
else()
	message(STATUS "Using system Jansson library")

	set(OBS_JANSSON_IMPORT "${JANSSON_LIBRARIES}" CACHE INTERNAL "Internal var")
	set(OBS_JANSSON_INCLUDE_DIRS "${JANSSON_INCLUDE_DIRS}" CACHE INTERNAL "Internal var")
endif()
