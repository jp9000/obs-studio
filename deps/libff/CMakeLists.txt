cmake_minimum_required (VERSION 2.8.12)
project (libff)

find_package(FFmpeg REQUIRED
	COMPONENTS avcodec avfilter avdevice avutil swscale avformat swresample)

include_directories(${FFMPEG_INCLUDE_DIRS})

if(WIN32 AND NOT MINGW)
	include_directories(../w32-pthreads)
	add_definitions(-Dinline=__inline)
endif(WIN32 AND NOT MINGW)

set(libff_HEADERS
	libff/ff-callbacks.h
	libff/ff-circular-queue.h
	libff/ff-clock.h
	libff/ff-frame.h
	libff/ff-packet-queue.h
	libff/ff-threading.h
	libff/ff-timer.h
	libff/ff-util.h
	#
	libff/ff-demuxer.h
	#
	libff/ff-decoder.h)

set(libff_SOURCES
	libff/ff-callbacks.c
	libff/ff-circular-queue.c
	libff/ff-clock.c
	libff/ff-packet-queue.c
	libff/ff-timer.c
	libff/ff-util.c
	#
	libff/ff-demuxer.c
	#
	libff/ff-decoder.c
	libff/ff-audio-decoder.c
	libff/ff-video-decoder.c)

if (WIN32)
	list(APPEND libff_SOURCES
		libff/ff-threading-windows.c)
else(WIN32)
	list(APPEND libff_SOURCES
		libff/ff-threading-posix.c)
endif(WIN32)

add_library (libff STATIC
	${libff_HEADERS}
	${libff_SOURCES})
set_target_properties(libff PROPERTIES
	FOLDER "deps"
	POSITION_INDEPENDENT_CODE ON)

target_include_directories(libff
	PUBLIC .)

target_link_libraries (libff
	${FFMPEG_LIBRARIES})
