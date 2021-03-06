project(libobs)

find_package(Threads REQUIRED)

find_package(FFmpeg REQUIRED
	COMPONENTS avformat avutil swscale swresample
	OPTIONAL_COMPONENTS avcodec)
include_directories(${FFMPEG_INCLUDE_DIRS})

if (NOT "${FFMPEG_AVCODEC_LIBRARIES}" STREQUAL "")
	list(REMOVE_ITEM FFMPEG_LIBRARIES ${FFMPEG_AVCODEC_LIBRARIES})
endif()

if(DEBUG_FFMPEG_MUX)
	add_definitions(-DSHOW_SUBPROCESSES)
endif()

if(UNIX)
	if (NOT APPLE)
		find_package(X11 REQUIRED)
		find_package(X11_XCB REQUIRED)
		find_package(XCB OPTIONAL_COMPONENTS XINPUT)
		find_package(XCB)
		find_library(M_LIBRARY NAMES m)
		find_library(DL_LIBRARY NAMES dl)
		if (XCB_XINPUT_FOUND)
			set(USE_XINPUT "1")
		else()
			set(USE_XINPUT "0")
		endif()
		find_package(PulseAudio)
		if (NOT "${PULSEAUDIO_LIBRARY}" STREQUAL "")
			message(STATUS "Found PulseAudio - Audio Monitor enabled")
			set(HAVE_PULSEAUDIO "1")
		else()
			set(HAVE_PULSEAUDIO "0")
		endif()
	else()
		set(HAVE_PULSEAUDIO "0")
		set(USE_XINPUT "0")
	endif()
	find_package(Gio QUIET)
else()
	set(HAVE_DBUS "0")
	set(HAVE_PULSEAUDIO "0")
	set(USE_XINPUT "0")
endif()

find_package(ImageMagick QUIET COMPONENTS MagickCore)

if(NOT ImageMagick_MagickCore_FOUND AND NOT FFMPEG_AVCODEC_FOUND)
	message(FATAL_ERROR "Either MagickCore or Libavcodec is required, but neither were found.")
elseif(NOT ImageMagick_MagickCore_FOUND AND LIBOBS_PREFER_IMAGEMAGICK)
	message(FATAL_ERROR "ImageMagick support was requested, but was not found.")
endif()

option(LIBOBS_PREFER_IMAGEMAGICK "Prefer ImageMagick over ffmpeg for image loading" OFF)

if(NOT FFMPEG_AVCODEC_FOUND OR (ImageMagick_MagickCore_FOUND AND LIBOBS_PREFER_IMAGEMAGICK))
	message(STATUS "Using ImageMagick for image loading in libobs")

	if(${ImageMagick_VERSION_STRING} LESS 7)
		set(LIBOBS_IMAGEMAGICK_DIR_STYLE LIBOBS_IMAGEMAGICK_DIR_STYLE_6L)
	elseif(${ImageMagick_VERSION_STRING} GREATER_EQUAL 7)
		set(LIBOBS_IMAGEMAGICK_DIR_STYLE LIBOBS_IMAGEMAGICK_DIR_STYLE_7GE)
	endif()

	set(libobs_image_loading_SOURCES
		graphics/graphics-magick.c)
	set(libobs_image_loading_LIBRARIES
		${ImageMagick_LIBRARIES})

	include_directories(${ImageMagick_INCLUDE_DIRS})
else()
	message(STATUS "Using libavcodec for image loading in libobs")

	set(libobs_image_loading_SOURCES
		graphics/graphics-ffmpeg.c)
	set(libobs_image_loading_LIBRARIES
		${FFMPEG_AVCODEC_LIBRARIES})
endif()

find_package(ZLIB REQUIRED)

include_directories(SYSTEM ${ZLIB_INCLUDE_DIRS})

add_definitions(-DLIBOBS_EXPORTS)

include_directories(${OBS_JANSSON_INCLUDE_DIRS})

if(WIN32)
	set(MODULE_DESCRIPTION "OBS Library")
	file(STRINGS obs-config.h _version_parse REGEX "^.*(MAJOR|MINOR|PATCH)_VER[ \t]+[0-9]+[ \t]*$")

	string(REGEX REPLACE ".*MAJOR_VER[ \t]+([0-9]+).*" "\\1" UI_VERSION_MAJOR "${_version_parse}")
	string(REGEX REPLACE ".*MINOR_VER[ \t]+([0-9]+).*" "\\1" UI_VERSION_MINOR "${_version_parse}")
	string(REGEX REPLACE ".*PATCH_VER[ \t]+([0-9]+).*" "\\1" UI_VERSION_PATCH "${_version_parse}")
	set(UI_VERSION "${UI_VERSION_MAJOR}.${UI_VERSION_MINOR}.${UI_VERSION_PATCH}")

	configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in libobs.rc)
	set(libobs_PLATFORM_SOURCES
		obs-win-crash-handler.c
		obs-windows.c
		util/threading-windows.c
		util/pipe-windows.c
		util/platform-windows.c
		libobs.rc)
	set(libobs_PLATFORM_HEADERS
		util/threading-windows.h
		util/windows/win-registry.h
		util/windows/win-version.h
		util/windows/ComPtr.hpp
		util/windows/CoTaskMemPtr.hpp
		util/windows/HRError.hpp
		util/windows/WinHandle.hpp)
	set(libobs_audio_monitoring_SOURCES
		audio-monitoring/win32/wasapi-output.c
		audio-monitoring/win32/wasapi-enum-devices.c
		)
	set(libobs_audio_monitoring_HEADERS
		audio-monitoring/win32/wasapi-output.h
		)
	set(libobs_PLATFORM_DEPS winmm psapi)
	if(MSVC)
		set(libobs_PLATFORM_DEPS
		${libobs_PLATFORM_DEPS}
		w32-pthreads)
	endif()
elseif(APPLE)
	set(libobs_PLATFORM_SOURCES
		obs-cocoa.m
		util/threading-posix.c
		util/pipe-posix.c
		util/platform-nix.c
		util/platform-cocoa.m)
	set(libobs_PLATFORM_HEADERS
		util/threading-posix.h
		util/apple/cfstring-utils.h)
	set(libobs_audio_monitoring_SOURCES
		audio-monitoring/osx/coreaudio-enum-devices.c
		audio-monitoring/osx/coreaudio-output.c
		)
	set(libobs_audio_monitoring_HEADERS
		audio-monitoring/osx/mac-helpers.h
		)

	set_source_files_properties(${libobs_PLATFORM_SOURCES}
		PROPERTIES
			COMPILE_FLAGS "-fobjc-arc")

	find_library(COCOA Cocoa)
	mark_as_advanced(COCOA)
	include_directories(${COCOA})

	find_library(COREAUDIO CoreAudio)
	mark_as_advanced(COREAUDIO)
	include_directories(${COREAUDIO})

	find_library(AUDIOTOOLBOX AudioToolbox)
	mark_as_advanced(AUDIOTOOLBOX)
	include_directories(${AUDIOTOOLBOX})

	find_library(AUDIOUNIT AudioUnit)
	mark_as_advanced(AUDIOUNIT)
	include_directories(${AUDIOUNIT})

	find_library(APPKIT AppKit)
	mark_as_advanced(APPKIT)
	include_directories(${APPKIT})

	find_library(IOKIT IOKit)
	mark_as_advanced(IOKIT)
	include_directories(${IOKIT})

	find_library(CARBON Carbon)
	mark_as_advanced(CARBON)
	include_directories(${CARBON})

	set(libobs_PLATFORM_DEPS
		${COCOA}
		${COREAUDIO}
		${AUDIOUNIT}
		${AUDIOTOOLBOX}
		${APPKIT}
		${IOKIT}
		${CARBON})
elseif(UNIX)
	set(libobs_PLATFORM_SOURCES
		obs-nix.c
		obs-nix-platform.c
		obs-nix-x11.c
		util/threading-posix.c
		util/pipe-posix.c
		util/platform-nix.c)

	set(libobs_PLATFORM_HEADERS
		util/threading-posix.h
		obs-nix-platform.h)

	if(ENABLE_WAYLAND)
		find_package(Wayland COMPONENTS Client REQUIRED)

		set(libobs_PLATFORM_SOURCES ${libobs_PLATFORM_SOURCES}
			obs-nix-wayland.c)

		include_directories(
			${WAYLAND_CLIENT_INCLUDE_DIR})
		add_definitions(
			${WAYLAND_DEFINITIONS})
		set(libobs_PLATFORM_DEPS
			${libobs_PLATFORM_DEPS}
			${WAYLAND_CLIENT_LIBRARIES})
	endif()

	if(HAVE_PULSEAUDIO)
		set(libobs_audio_monitoring_HEADERS
			audio-monitoring/pulse/pulseaudio-wrapper.h)

		set(libobs_audio_monitoring_SOURCES
			audio-monitoring/pulse/pulseaudio-wrapper.c
			audio-monitoring/pulse/pulseaudio-enum-devices.c
			audio-monitoring/pulse/pulseaudio-output.c)
	else()
		set(libobs_audio_monitoring_SOURCES
			audio-monitoring/null/null-audio-monitoring.c)
	endif()
	if(GIO_FOUND)
		set(libobs_PLATFORM_SOURCES ${libobs_PLATFORM_SOURCES}
			util/platform-nix-dbus.c)
		include_directories(${GIO_INCLUDE_DIRS})
		add_definitions(
			${GIO_DEFINITIONS})
		set(libobs_PLATFORM_DEPS
			${libobs_PLATFORM_DEPS}
			${GIO_LIBRARIES})
	endif()

	include_directories(
		${X11_X11_INCLUDE_PATH}
		${X11_XCB_INCLUDE_DIRS})
	add_definitions(
		${X11_XCB_DEFINITIONS})
	set(libobs_PLATFORM_DEPS
		${libobs_PLATFORM_DEPS}
		${DL_LIBRARY}
		${M_LIBRARY}
		${X11_X11_LIB}
		${XCB_LIBRARIES}
		${X11_XCB_LIBRARIES})

	if(USE_XINPUT)
		include_directories(
			${XCB_XINPUT_INCLUDE_DIR})
		add_definitions(
			${XCB_DEFINITIONS})
		set(libobs_PLATFORM_DEPS
			${XCB_XINPUT_LIBRARY}
			${libobs_PLATFORM_DEPS})
	endif()

	if(HAVE_PULSEAUDIO)
		set(libobs_PLATFORM_DEPS
			${libobs_PLATFORM_DEPS}
			${PULSEAUDIO_LIBRARY})
	endif()

	if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
		# use the sysinfo compatibility library on bsd
		find_package(Libsysinfo REQUIRED)
		include_directories(${SYSINFO_INCLUDE_DIRS})
		set(libobs_PLATFORM_DEPS
			${libobs_PLATFORM_DEPS}
			${SYSINFO_LIBRARIES})
	endif()
endif()

if(MSVC)
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /EHc-")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHc-")
endif()

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/obsconfig.h.in"
	"${CMAKE_BINARY_DIR}/config/obsconfig.h")

set(libobs_config_HEADERS
	"${CMAKE_BINARY_DIR}/config/obsconfig.h"
	obs-config.h)

set(libobs_callback_SOURCES
	callback/calldata.c
	callback/decl.c
	callback/signal.c
	callback/proc.c)
set(libobs_callback_HEADERS
	callback/calldata.h
	callback/decl.h
	callback/proc.h
	callback/signal.h)

set(libobs_graphics_SOURCES
	${libobs_image_loading_SOURCES}
	graphics/quat.c
	graphics/effect-parser.c
	graphics/axisang.c
	graphics/vec4.c
	graphics/vec2.c
	graphics/libnsgif/libnsgif.c
	graphics/texture-render.c
	graphics/image-file.c
	graphics/bounds.c
	graphics/matrix3.c
	graphics/matrix4.c
	graphics/vec3.c
	graphics/graphics.c
	graphics/shader-parser.c
	graphics/plane.c
	graphics/effect.c
	graphics/math-extra.c
	graphics/graphics-imports.c)
set(libobs_graphics_HEADERS
	graphics/half.h
	graphics/plane.h
	graphics/quat.h
	graphics/input.h
	graphics/axisang.h
	graphics/shader-parser.h
	graphics/effect.h
	graphics/math-defs.h
	graphics/matrix4.h
	graphics/graphics.h
	graphics/graphics-internal.h
	graphics/libnsgif/libnsgif.h
	graphics/device-exports.h
	graphics/image-file.h
	graphics/vec2.h
	graphics/vec4.h
	graphics/matrix3.h
	graphics/vec3.h
	graphics/math-extra.h
	graphics/bounds.h
	graphics/effect-parser.h)

set(libobs_mediaio_SOURCES
	media-io/video-io.c
	media-io/video-fourcc.c
	media-io/video-matrices.c
	media-io/audio-io.c
	media-io/video-frame.c
	media-io/format-conversion.c
	media-io/audio-resampler-ffmpeg.c
	media-io/video-scaler-ffmpeg.c
	media-io/media-remux.c)
set(libobs_mediaio_HEADERS
	media-io/media-io-defs.h
	media-io/video-io.h
	media-io/audio-io.h
	media-io/audio-math.h
	media-io/video-frame.h
	media-io/format-conversion.h
	media-io/audio-resampler.h
	media-io/video-scaler.h
	media-io/media-remux.h
	media-io/frame-rate.h)

set(libobs_util_SOURCES
	util/array-serializer.c
	util/file-serializer.c
	util/base.c
	util/platform.c
	util/cf-lexer.c
	util/bmem.c
	util/config-file.c
	util/lexer.c
	util/dstr.c
	util/utf8.c
	util/crc32.c
	util/text-lookup.c
	util/cf-parser.c
	util/profiler.c
	util/bitstream.c)
set(libobs_util_HEADERS
	util/curl/curl-helper.h
	util/sse-intrin.h
	util/array-serializer.h
	util/file-serializer.h
	util/utf8.h
	util/crc32.h
	util/base.h
	util/text-lookup.h
	util/bmem.h
	util/c99defs.h
	util/util_uint64.h
	util/util_uint128.h
	util/cf-parser.h
	util/threading.h
	util/pipe.h
	util/cf-lexer.h
	util/darray.h
	util/circlebuf.h
	util/dstr.h
	util/serializer.h
	util/config-file.h
	util/lexer.h
	util/platform.h
	util/profiler.h
	util/profiler.hpp
	util/bitstream.h)

set(libobs_libobs_SOURCES
	${libobs_PLATFORM_SOURCES}
	obs-audio-controls.c
	obs-avc.c
	obs-encoder.c
	obs-service.c
	obs-source.c
	obs-source-deinterlace.c
	obs-source-transition.c
	obs-output.c
	obs-output-delay.c
	obs.c
	obs-properties.c
	obs-data.c
	obs-missing-files.c
	obs-hotkey.c
	obs-hotkey-name-map.c
	obs-module.c
	obs-display.c
	obs-view.c
	obs-scene.c
	obs-audio.c
	obs-video-gpu-encode.c
	obs-video.c)
set(libobs_libobs_HEADERS
	util/simde/check.h
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
	util/simde/x86/sse.h
	${libobs_PLATFORM_HEADERS}
	obs-audio-controls.h
	obs-defs.h
	obs-avc.h
	obs-encoder.h
	obs-service.h
	obs-internal.h
	obs.h
	obs-ui.h
	obs-properties.h
	obs-data.h
	obs-missing-files.h
	obs-interaction.h
	obs-hotkey.h
	obs-hotkeys.h
	obs-module.h
	obs-scene.h
	obs-source.h
	obs-output.h
	obs-ffmpeg-compat.h
	obs.hpp)

set(libobs_SOURCES
	${libobs_callback_SOURCES}
	${libobs_graphics_SOURCES}
	${libobs_mediaio_SOURCES}
	${libobs_util_SOURCES}
	${libobs_libobs_SOURCES}
	${libobs_audio_monitoring_SOURCES}
	)

set(libobs_HEADERS
	${libobs_config_HEADERS}
	${libobs_callback_HEADERS}
	${libobs_graphics_HEADERS}
	${libobs_mediaio_HEADERS}
	${libobs_util_HEADERS}
	${libobs_libobs_HEADERS}
	${libobs_audio_monitoring_HEADERS}
	)

source_group("callback\\Source Files" FILES ${libobs_callback_SOURCES})
source_group("callback\\Header Files" FILES ${libobs_callback_HEADERS})
source_group("graphics\\Source Files" FILES ${libobs_graphics_SOURCES})
source_group("graphics\\Header Files" FILES ${libobs_graphics_HEADERS})
source_group("libobs\\Source Files" FILES ${libobs_libobs_SOURCES})
source_group("libobs\\Header Files" FILES ${libobs_libobs_HEADERS})
source_group("media-io\\Source Files" FILES ${libobs_mediaio_SOURCES})
source_group("media-io\\Header Files" FILES ${libobs_mediaio_HEADERS})
source_group("util\\Source Files" FILES ${libobs_util_SOURCES})
source_group("util\\Header Files" FILES ${libobs_util_HEADERS})
source_group("audio-monitoring\\Source Files" FILES ${libobs_audio_monitoring_SOURCES})
source_group("audio-monitoring\\Header Files" FILES ${libobs_audio_monitoring_HEADERS})

include_directories(${CMAKE_SOURCE_DIR}/deps/libcaption)
set(libobs_PLATFORM_DEPS
	${libobs_PLATFORM_DEPS}
	caption)

add_library(libobs SHARED ${libobs_SOURCES} ${libobs_HEADERS})
if(UNIX AND NOT APPLE)
	set(DEST_DIR "${CMAKE_INSTALL_PREFIX}")
	foreach(LIB "obs" "rt")
		set(PRIVATE_LIBS "${PRIVATE_LIBS} -l${LIB}")
	endforeach()
	if(LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)64(le)?")
		set(PPC64_CFLAGS "-DNO_WARN_X86_INTRINSICS -mvsx")
	endif()
	CONFIGURE_FILE("libobs.pc.in" "libobs.pc" @ONLY)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libobs.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif()

set_target_properties(libobs PROPERTIES
	FOLDER "core"
	OUTPUT_NAME obs
	VERSION "0"
	SOVERSION "0")
target_compile_definitions(libobs
	PUBLIC
		HAVE_OBSCONFIG_H)

target_compile_definitions(libobs
	PUBLIC
	${ARCH_SIMD_DEFINES})

target_compile_options(libobs
	PUBLIC
	${ARCH_SIMD_FLAGS})

target_compile_options(libobs
	PUBLIC
		"${THREADS_DEFINITIONS}")
target_include_directories(libobs
	PUBLIC
		"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
		"$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/config>"
		"$<INSTALL_INTERFACE:${OBS_INCLUDE_DESTINATION}>")
target_link_libraries(libobs
	PRIVATE
		${libobs_PLATFORM_DEPS}
		${libobs_image_loading_LIBRARIES}
		${OBS_JANSSON_IMPORT}
		${FFMPEG_LIBRARIES}
		${ZLIB_LIBRARIES}
	PUBLIC
		${THREADS_LIBRARIES})

install_obs_core(libobs EXPORT LibObs)
install_obs_data(libobs data libobs)
install_obs_headers(${libobs_HEADERS})

obs_install_additional(libobs)
