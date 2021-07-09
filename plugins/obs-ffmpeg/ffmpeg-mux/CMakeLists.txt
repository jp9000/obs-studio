project(obs-ffmpeg-mux)

option(ENABLE_FFMPEG_MUX_DEBUG "Enable FFmpeg-mux debugging" OFF)

find_package(FFmpeg REQUIRED COMPONENTS avcodec avutil avformat)

add_executable(obs-ffmpeg-mux)

target_sources(obs-ffmpeg-mux PRIVATE ffmpeg-mux.c ffmpeg-mux.h)

target_link_libraries(obs-ffmpeg-mux PRIVATE OBS::libobs FFmpeg::avcodec
                                             FFmpeg::avutil FFmpeg::avformat)

if(ENABLE_FFMPEG_MUX_DEBUG)
  target_compile_definitions(obs-ffmpeg-mux PRIVATE ENABLE_FFMPEG_MUX_DEBUG)
endif()

set_target_properties(obs-ffmpeg-mux PROPERTIES FOLDER "plugins/obs-ffmpeg")

setup_binary_target(obs-ffmpeg-mux)
