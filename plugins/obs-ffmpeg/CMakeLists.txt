project(obs-ffmpeg)

option(ENABLE_FFMPEG_LOGGING "Enables obs-ffmpeg logging" OFF)

find_package(
  FFmpeg REQUIRED
  COMPONENTS avcodec
             avfilter
             avdevice
             avutil
             swscale
             avformat
             swresample)

add_library(obs-ffmpeg MODULE)
add_library(OBS::ffmpeg ALIAS obs-ffmpeg)
add_subdirectory(ffmpeg-mux)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/obs-ffmpeg-config.h.in
               ${CMAKE_BINARY_DIR}/config/obs-ffmpeg-config.h)

target_sources(
  obs-ffmpeg
  PRIVATE obs-ffmpeg.c
          obs-ffmpeg-audio-encoders.c
          obs-ffmpeg-nvenc.c
          obs-ffmpeg-output.c
          obs-ffmpeg-mux.c
          obs-ffmpeg-mux.h
          obs-ffmpeg-hls-mux.c
          obs-ffmpeg-source.c
          obs-ffmpeg-compat.h
          obs-ffmpeg-formats.h
          ${CMAKE_BINARY_DIR}/config/obs-ffmpeg-config.h)

target_include_directories(
  obs-ffmpeg PRIVATE "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/config>")

target_link_libraries(
  obs-ffmpeg
  PRIVATE OBS::libobs
          OBS::media-playback
          FFmpeg::avcodec
          FFmpeg::avfilter
          FFmpeg::avformat
          FFmpeg::avdevice
          FFmpeg::avutil
          FFmpeg::swscale
          FFmpeg::swresample)

if(ENABLE_FFMPEG_LOGGING)
  target_sources(obs-ffmpeg PRIVATE obs-ffmpeg-logging.c)
endif()

set_target_properties(obs-ffmpeg PROPERTIES FOLDER "plugins/obs-ffmpeg" PREFIX
                                                                        "")

if(OS_WINDOWS)
  if(MSVC)
    target_link_libraries(obs-ffmpeg PRIVATE OBS::w32-pthreads)
  endif()

  set(MODULE_DESCRIPTION "OBS FFmpeg module")
  configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
                 obs-ffmpeg.rc)

  target_sources(obs-ffmpeg PRIVATE jim-nvenc.c jim-nvenc.h jim-nvenc-helpers.c
                                    obs-ffmpeg.rc)

elseif(OS_POSIX AND NOT OS_MACOS)
  target_sources(obs-ffmpeg PRIVATE obs-ffmpeg-vaapi.c)
endif()

setup_plugin_target(obs-ffmpeg)
