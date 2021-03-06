cmake_minimum_required(VERSION 3.2)

project(blake2)

set(BLAKE2_INCLUDE_DIR
	"${CMAKE_CURRENT_SOURCE_DIR}/src"
		CACHE PATH "blake2 include path")

include_directories(
	${LIBblake2_INCLUDE_DIRS}
	src
)

if(WIN32)
	if(MSVC)
		add_compile_options($<IF:$<CONFIG:Debug>,/MTd,/MT> /Zl)
	endif()
	add_definitions(
		-Dinline=_inline
		-Drestrict=__restrict)
endif()

set(blake2_SOURCES
	src/blake2b-ref.c)

set(blake2_HEADERS
	src/blake2.h
	src/blake2-impl.h)

add_library(blake2 STATIC
	${blake2_SOURCES}
	${blake2_HEADERS})
set_target_properties(blake2 PROPERTIES FOLDER "deps")
