project(media-playback)

option(ENABLE_SIMDE "Enable non-native SSE2 SIMD support" ON)

find_package(FFmpeg REQUIRED COMPONENTS avcodec avdevice avutil avformat)

add_library(media-playback INTERFACE)
add_library(OBS::media-playback ALIAS media-playback)

target_sources(
  media-playback
  INTERFACE media-playback/media.c media-playback/media.h
            media-playback/decode.c media-playback/decode.h
            media-playback/closest-format.h)

target_link_libraries(media-playback INTERFACE FFmpeg::avcodec FFmpeg::avdevice
                                               FFmpeg::avutil FFmpeg::avformat)

target_include_directories(
  media-playback INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")

target_compile_options(media-playback INTERFACE ${ARCH_SIMD_FLAGS})

target_compile_definitions(media-playback INTERFACE ${ARCH_SIMD_DEFINES})
