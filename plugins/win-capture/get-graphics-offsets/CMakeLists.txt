project(get-graphics-offsets)

set(get-graphics-offsets_HEADERS
	../graphics-hook-info.h
	../hook-helpers.h
	get-graphics-offsets.h)

set(get-graphics-offsets_SOURCES
	get-graphics-offsets.c
	dxgi-offsets.cpp
	d3d8-offsets.cpp
	d3d9-offsets.cpp)

if(MSVC)
	add_compile_options($<IF:$<CONFIG:Debug>,/MTd,/MT>)
endif()

add_executable(get-graphics-offsets
	${get-graphics-offsets_SOURCES}
	${get-graphics-offsets_HEADERS})

target_link_libraries(get-graphics-offsets
	d3d9.lib
	dxgi.lib
	d3d11.lib)

set_target_properties(get-graphics-offsets PROPERTIES FOLDER "plugins/win-capture")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_output_suffix "64")
else()
	set(_output_suffix "32")
endif()

set_target_properties(get-graphics-offsets
	PROPERTIES
		OUTPUT_NAME "get-graphics-offsets${_output_suffix}")

install_obs_datatarget(get-graphics-offsets "obs-plugins/win-capture")
