project(glad)

find_package(OpenGL)

if(NOT WIN32 AND NOT APPLE)
	find_package(X11 REQUIRED)
	find_package(EGL REQUIRED)
endif()

set(glad_SOURCES
	src/glad.c
	include/glad/glad.h)

if(WIN32)
	set(MODULE_DESCRIPTION "Glad OpenGL Loading Library")
	configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in obsglad.rc)
	set(glad_PLATFORM_SOURCES
		src/glad_wgl.c
		include/glad/glad_wgl.h
		obsglad.rc)
elseif(NOT APPLE)
	set(glad_PLATFORM_SOURCES
		src/glad_egl.c
		src/glad_glx.c
		include/glad/glad_egl.h
		include/glad/glad_glx.h)
endif()

set(glad_include_dirs
	PRIVATE ${OPENGL_INCLUDE_DIR})

if (UNIX AND NOT APPLE)
list (APPEND glad_include_dirs
	PRIVATE
		${X11_X11_INCLUDE_PATH}
		${EGL_INCLUDE_DIRS})
endif()

add_library(glad SHARED
	${glad_SOURCES}
	${glad_PLATFORM_SOURCES})
set_target_properties(glad PROPERTIES
	FOLDER "deps"
	OUTPUT_NAME obsglad
	VERSION "0"
	SOVERSION "0"
	POSITION_INDEPENDENT_CODE ON)
target_include_directories(glad
	PUBLIC include
	${glad_include_dirs})
target_compile_definitions(glad
	PRIVATE GLAD_GLAPI_EXPORT_BUILD)

if(NOT MSVC)
	target_compile_options(glad
		PRIVATE -DPIC -fvisibility=hidden)
endif()

if(NOT WIN32 AND NOT APPLE)
	set(glad_PLATFORM_DEPS
		${X11_X11_LIB}
		${EGL_LIBRARIES})

	# only link to libdl on linux
	if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
		set(glad_PLATFORM_DEPS
			${glad_PLATFORM_DEPS}
			-ldl)
	endif()
endif()

target_link_libraries(glad
	${glad_PLATFORM_DEPS}
	${OPENGL_gl_LIBRARY})

install_obs_core(glad)
