if(DISABLE_UPDATE_MODULE)
	return()
endif()

if(NOT DEFINED STATIC_ZLIB_PATH OR "${STATIC_ZLIB_PATH}" STREQUAL "")
	message(STATUS "STATIC_ZLIB_PATH not set, windows updater disabled")
	return()
endif()

project(updater)

include_directories(${LIBLZMA_INCLUDE_DIRS})
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/deps/json11")
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/libobs")
include_directories(${BLAKE2_INCLUDE_DIR})

set(updater_HEADERS
	${CMAKE_SOURCE_DIR}/deps/json11/json11.hpp
	../win-update-helpers.hpp
	resource.h
	updater.hpp
	)
set(updater_SOURCES
	${CMAKE_SOURCE_DIR}/deps/json11/json11.cpp
	../win-update-helpers.cpp
	init-hook-files.c
	updater.cpp
	patch.cpp
	http.cpp
	hash.cpp
	updater.rc
	updater.manifest
	)

add_definitions(-DNOMINMAX -DUNICODE -D_UNICODE)
if(MSVC)
	add_compile_options($<IF:$<CONFIG:Debug>,/MTd,/MT>)
endif()

add_executable(updater WIN32
	${updater_HEADERS}
	${updater_SOURCES}
	)
target_link_libraries(updater
	${STATIC_ZLIB_PATH}
	lzma
	blake2
	psapi
	comctl32
	shell32
	winhttp
	)
set_target_properties(updater PROPERTIES FOLDER "frontend")
