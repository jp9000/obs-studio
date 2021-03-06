project(media-playback)

find_package(FFmpeg REQUIRED
	COMPONENTS avcodec avdevice avutil avformat)

include_directories(
	${CMAKE_SOURCE_DIR}/libobs
	${FFMPEG_INCLUDE_DIRS}
	)

set(media-playback_HEADERS
	media-playback/closest-format.h
	media-playback/decode.h
	media-playback/media.h
	)
set(media-playback_SOURCES
	media-playback/decode.c
	media-playback/media.c
	)

add_library(media-playback STATIC
	${media-playback_HEADERS}
	${media-playback_SOURCES}
	)

target_compile_options(media-playback
	PUBLIC
	${ARCH_SIMD_FLAGS})

target_include_directories(media-playback
	PUBLIC .
	)

target_link_libraries(media-playback
	${FFMPEG_LIBRARIES}
	)
set_target_properties(media-playback PROPERTIES
	FOLDER "deps"
	POSITION_INDEPENDENT_CODE ON)
