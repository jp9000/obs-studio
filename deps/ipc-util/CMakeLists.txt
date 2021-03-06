# TODO: Add posix support
if(NOT WIN32)
	return()
endif()

project(ipc-util)

set(ipc-util_HEADERS
	ipc-util/pipe.h)

if(WIN32)
	set(ipc-util_HEADERS
		${ipc-util_HEADERS}
		ipc-util/pipe-windows.h)
	set(ipc-util_SOURCES
		ipc-util/pipe-windows.c)
else()
	set(ipc-util_HEADERS
		${ipc-util_HEADERS}
		ipc-util/pipe-posix.h)
	set(ipc-util_SOURCES
		ipc-util/pipe-posix.c)
endif()

if(MSVC)
	add_compile_options($<IF:$<CONFIG:Debug>,/MTd,/MT> /Zl)
endif()

add_library(ipc-util STATIC
	${ipc-util_SOURCES}
	${ipc-util_HEADERS})
set_target_properties(ipc-util PROPERTIES
	FOLDER "deps"
	POSITION_INDEPENDENT_CODE ON)
target_include_directories(ipc-util
	PUBLIC .)
target_link_libraries(ipc-util)
