if(POLICY CMP0090)
  cmake_policy(SET CMP0090 NEW)
endif()

project(libobs)

find_package(Threads REQUIRED)
find_package(
  FFmpeg REQUIRED
  COMPONENTS avformat avutil swscale swresample
  OPTIONAL_COMPONENTS avcodec)
find_package(ZLIB REQUIRED)

add_library(libobs SHARED)
add_library(OBS::libobs ALIAS libobs)

target_sources(
  libobs
  PRIVATE obs.c
          obs.h
          obs.hpp
          obs-audio.c
          obs-audio-controls.c
          obs-audio-controls.h
          obs-avc.c
          obs-avc.h
          obs-data.c
          obs-data.h
          obs-defs.h
          obs-display.c
          obs-encoder.c
          obs-encoder.h
          obs-ffmpeg-compat.h
          obs-hotkey.c
          obs-hotkey.h
          obs-hotkeys.h
          obs-missing-files.c
          obs-missing-files.h
          obs-hotkey-name-map.c
          obs-interaction.h
          obs-internal.h
          obs-module.c
          obs-module.h
          obs-output.c
          obs-output.h
          obs-output-delay.c
          obs-properties.c
          obs-properties.h
          obs-service.c
          obs-service.h
          obs-scene.c
          obs-scene.h
          obs-source.c
          obs-source.h
          obs-source-deinterlace.c
          obs-source-transition.c
          obs-ui.h
          obs-video.c
          obs-video-gpu-encode.c
          obs-view.c
          obs-config.h)

target_sources(
  libobs
  PRIVATE util/simde/check.h
          util/simde/debug-trap.h
          util/simde/hedley.h
          util/simde/simde-align.h
          util/simde/simde-arch.h
          util/simde/simde-common.h
          util/simde/simde-constify.h
          util/simde/simde-detect-clang.h
          util/simde/simde-diagnostic.h
          util/simde/simde-features.h
          util/simde/simde-math.h
          util/simde/x86/mmx.h
          util/simde/x86/sse2.h
          util/simde/x86/sse.h)

target_sources(
  libobs
  PRIVATE callback/calldata.c
          callback/calldata.h
          callback/decl.c
          callback/decl.h
          callback/signal.c
          callback/signal.h
          callback/proc.c
          callback/proc.h)

target_sources(
  libobs
  PRIVATE graphics/graphics.c
          graphics/graphics.h
          graphics/graphics-imports.c
          graphics/graphics-internal.h
          graphics/axisang.c
          graphics/axisang.h
          graphics/bounds.c
          graphics/bounds.h
          graphics/device-exports.h
          graphics/effect.c
          graphics/effect.h
          graphics/effect-parser.c
          graphics/effect-parser.h
          graphics/half.h
          graphics/image-file.c
          graphics/image-file.h
          graphics/math-extra.c
          graphics/math-extra.h
          graphics/matrix3.c
          graphics/matrix3.h
          graphics/matrix4.c
          graphics/matrix4.h
          graphics/plane.c
          graphics/plane.h
          graphics/quat.c
          graphics/quat.h
          graphics/shader-parser.c
          graphics/shader-parser.h
          graphics/srgb.h
          graphics/texture-render.c
          graphics/vec2.c
          graphics/vec2.h
          graphics/vec3.c
          graphics/vec3.h
          graphics/vec4.c
          graphics/vec4.h
          graphics/libnsgif/libnsgif.c
          graphics/libnsgif/libnsgif.h
          graphics/graphics-ffmpeg.c)

target_sources(
  libobs
  PRIVATE media-io/audio-io.c
          media-io/audio-io.h
          media-io/audio-math.h
          media-io/audio-resampler.h
          media-io/audio-resampler-ffmpeg.c
          media-io/format-conversion.c
          media-io/format-conversion.h
          media-io/frame-rate.h
          media-io/media-remux.c
          media-io/media-remux.h
          media-io/video-fourcc.c
          media-io/video-frame.c
          media-io/video-frame.h
          media-io/video-io.c
          media-io/video-io.h
          media-io/media-io-defs.h
          media-io/video-matrices.c
          media-io/video-scaler-ffmpeg.c
          media-io/video-scaler.h)

target_sources(
  libobs
  PRIVATE util/array-serializer.c
          util/array-serializer.h
          util/base.c
          util/base.h
          util/bitstream.c
          util/bitstream.h
          util/bmem.c
          util/bmem.h
          util/c99defs.h
          util/cf-lexer.c
          util/cf-lexer.h
          util/cf-parser.c
          util/cf-parser.h
          util/circlebuf.h
          util/config-file.c
          util/config-file.h
          util/crc32.c
          util/crc32.h
          util/dstr.c
          util/dstr.h
          util/file-serializer.c
          util/file-serializer.h
          util/lexer.c
          util/lexer.h
          util/platform.c
          util/platform.h
          util/profiler.c
          util/profiler.h
          util/profiler.hpp
          util/pipe.h
          util/serializer.h
          util/sse-intrin.h
          util/text-lookup.c
          util/text-lookup.h
          util/threading.h
          util/utf8.c
          util/utf8.h
          util/util_uint64.h
          util/util_uint128.h
          util/curl/curl-helper.h
          util/darray.h)

target_link_libraries(
  libobs
  PRIVATE FFmpeg::avcodec
          FFmpeg::avformat
          FFmpeg::avutil
          FFmpeg::swscale
          FFmpeg::swresample
          Jansson::Jansson
          OBS::caption
          ZLIB::ZLIB
  PUBLIC Threads::Threads)

set_target_properties(
  libobs
  PROPERTIES OUTPUT_NAME obs
             FOLDER "core"
             VERSION "${OBS_VERSION_MAJOR}"
             SOVERSION "0")

target_compile_definitions(libobs PUBLIC ${ARCH_SIMD_DEFINES})

target_compile_features(libobs PRIVATE cxx_alias_templates)

target_compile_options(libobs PUBLIC ${ARCH_SIMD_FLAGS})

target_include_directories(
  libobs PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
                "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/config>")

if(OS_WINDOWS)
  set(MODULE_DESCRIPTION "OBS Library")
  set(UI_VERSION "${OBS_VERSION_CANONICAL}")

  configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
                 libobs.rc)

  target_sources(
    libobs
    PRIVATE obs-win-crash-handler.c
            obs-windows.c
            util/threading-windows.c
            util/threading-windows.h
            util/pipe-windows.c
            util/platform-windows.c
            util/windows/win-registry.h
            util/windows/win-version.h
            util/windows/ComPtr.hpp
            util/windows/CoTaskMemPtr.hpp
            util/windows/HRError.hpp
            util/windows/WinHandle.hpp
            libobs.rc
            audio-monitoring/win32/wasapi-output.c
            audio-monitoring/win32/wasapi-enum-devices.c
            audio-monitoring/win32/wasapi-output.h)

  target_compile_definitions(
    libobs PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS
                   _CRT_NONSTDC_NO_WARNINGS)

  target_link_libraries(libobs PRIVATE winmm psapi)

  if(MSVC)
    target_link_libraries(libobs PRIVATE OBS::w32-pthreads)

    target_compile_options(libobs PRIVATE "$<$<COMPILE_LANGUAGE:C>:/EHc->"
                                          "$<$<COMPILE_LANGUAGE:CXX>:/EHc->")

    target_link_options(libobs PRIVATE "LINKER:/SAFESEH:NO")
  endif()

elseif(OS_MACOS)

  find_library(COCOA Cocoa)
  find_library(COREAUDIO CoreAudio)
  find_library(AUDIOTOOLBOX AudioToolbox)
  find_library(AUDIOUNIT AudioUnit)
  find_library(APPKIT AppKit)
  find_library(IOKIT IOKit)
  find_library(CARBON Carbon)

  mark_as_advanced(
    COCOA
    COREAUDIO
    AUDIOTOOLBOX
    AUDIOUNIT
    APPKIT
    IOKIT
    CARBON)

  target_link_libraries(
    libobs
    PRIVATE ${COCOA}
            ${COREAUDIO}
            ${AUDIOTOOLBOX}
            ${AUDIOUNIT}
            ${APPKIT}
            ${IOKIT}
            ${CARBON})

  target_sources(
    libobs
    PRIVATE obs-cocoa.m
            util/pipe-posix.c
            util/platform-cocoa.m
            util/platform-nix.c
            util/threading-posix.c
            util/threading-posix.h
            util/apple/cfstring-utils.h
            audio-monitoring/osx/coreaudio-enum-devices.c
            audio-monitoring/osx/coreaudio-output.c
            audio-monitoring/osx/mac-helpers.h)

  set_source_files_properties(util/platform-cocoa.m obs-cocoa.m
                              PROPERTIES COMPILE_FLAGS "-fobjc-arc")

  set_target_properties(
    libobs PROPERTIES SOVERSION "1" BUILD_RPATH
                                    "$<TARGET_FILE_DIR:OBS::libobs-opengl>")

elseif(OS_POSIX)
  find_package(X11 REQUIRED)
  find_package(
    XCB
    COMPONENTS XCB
    OPTIONAL_COMPONENTS XINPUT
    QUIET)
  find_package(X11_XCB REQUIRED)

  target_sources(
    libobs
    PRIVATE obs-nix.c
            obs-nix-platform.c
            obs-nix-platform.h
            obs-nix-x11.c
            util/threading-posix.c
            util/threading-posix.h
            util/pipe-posix.c
            util/platform-nix.c)

  target_link_libraries(libobs PRIVATE X11::X11_xcb XCB::XCB)

  if(USE_XDG)
    target_compile_definitions(libobs PRIVATE USE_XDG)
  endif()

  if(ENABLE_PULSEAUDIO)
    find_package(PulseAudio REQUIRED)
    message(STATUS "OBS: PulseAudio found - audio monitoring enabled")
    target_sources(
      libobs
      PRIVATE audio-monitoring/pulse/pulseaudio-output.c
              audio-monitoring/pulse/pulseaudio-enum-devices.c
              audio-monitoring/pulse/pulseaudio-wrapper.c
              audio-monitoring/pulse/pulseaudio-wrapper.h)

    target_link_libraries(libobs PRIVATE ${PULSEAUDIO_LIBRARY})
  else()
    target_sources(libobs PRIVATE audio-monitoring/null/null-audio-monitoring.c)
  endif()

  find_package(Gio)
  if(TARGET GIO::GIO)
    target_link_libraries(libobs PRIVATE GIO::GIO)

    target_sources(libobs PRIVATE util/platform-nix-dbus.c)
  endif()

  if(TARGET XCB::XINPUT)
    target_link_libraries(libobs PRIVATE XCB::XINPUT)
  endif()

  if(ENABLE_WAYLAND)
    find_package(
      Wayland
      COMPONENTS Client
      REQUIRED)
    target_link_libraries(libobs PRIVATE Wayland::Client)

    target_sources(libobs PRIVATE obs-nix-wayland.c)
  endif()

  if(OS_LINUX)
    target_link_libraries(glad PRIVATE ${CMAKE_DL_LIBS})
  endif()

  if(OS_FREEBSD)
    find_package(Sysinfo REQUIRED)
    target_link_libraries(libobs PRIVATE Sysinfo::Sysinfo)
  endif()

  set_target_properties(
    libobs PROPERTIES BUILD_RPATH "$<TARGET_FILE_DIR:OBS::libobs-opengl>")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/obsconfig.h.in
               ${CMAKE_BINARY_DIR}/config/obsconfig.h)

target_compile_definitions(libobs PUBLIC HAVE_OBSCONFIG_H)

if(ENABLE_FFMPEG_MUX_DEBUG)
  target_compile_definitions(libobs PRIVATE SHOW_SUBPROCESSES)
endif()

file(GLOB _LIBOBS_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.c
     ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.m)
file(GLOB _LIBOBS_HEADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.h
     ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

source_group("libobs\\Source Files" FILES ${_LIBOBS_SOURCE_FILES})
source_group("libobs\\Header Files" FILES ${_LIBOBS_HEADER_FILES})

set(_GROUPS "audio-monitoring;callback;graphics;media-io;util")
foreach(_GROUP IN ITEMS ${_GROUPS})
  file(
    GLOB_RECURSE _LIBOBS_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${_GROUP}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/${_GROUP}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/${_GROUP}/*.m)
  file(GLOB_RECURSE _LIBOBS_HEADER_FILES
       ${CMAKE_CURRENT_SOURCE_DIR}/${_GROUP}/*.h
       ${CMAKE_CURRENT_SOURCE_DIR}/${_GROUP}/*.hpp)

  source_group("${_GROUP}\\Source Files" FILES ${_LIBOBS_SOURCE_FILES})
  source_group("${_GROUP}\\Header Files" FILES ${_LIBOBS_HEADER_FILES})
endforeach()

setup_binary_target(libobs)
setup_target_resources(libobs libobs)
export_target(libobs)
install_headers(libobs)
