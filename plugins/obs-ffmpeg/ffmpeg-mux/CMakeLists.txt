project(obs-ffmpeg-mux)

if(DEBUG_FFMPEG_MUX)
	add_definitions(-DDEBUG_FFMPEG)
endif()

find_package(FFmpeg REQUIRED
	COMPONENTS avcodec avutil avformat)
include_directories(${FFMPEG_INCLUDE_DIRS})

set(obs-ffmpeg-mux_SOURCES
	ffmpeg-mux.c)

set(obs-ffmpeg-mux_HEADERS
	ffmpeg-mux.h)

add_executable(obs-ffmpeg-mux
	${obs-ffmpeg-mux_SOURCES}
	${obs-ffmpeg-mux_HEADERS})

target_link_libraries(obs-ffmpeg-mux
	libobs
	${FFMPEG_LIBRARIES})

set_target_properties(obs-ffmpeg-mux PROPERTIES FOLDER "plugins/obs-ffmpeg")

install_obs_core(obs-ffmpeg-mux)
