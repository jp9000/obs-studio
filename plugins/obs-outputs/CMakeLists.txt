project(obs-outputs)

set(WITH_RTMPS AUTO CACHE STRING "Enable RTMPS support with mbedTLS")
set_property(CACHE WITH_RTMPS PROPERTY STRINGS AUTO ON OFF)

find_package(PkgConfig)

option(STATIC_MBEDTLS "Statically link mbedTLS into binary" OFF)

if (WITH_RTMPS STREQUAL "AUTO")
	find_package(MbedTLS)
	find_package(ZLIB)
	if (NOT MBEDTLS_FOUND OR NOT ZLIB_FOUND)
		set(WITH_RTMPS "OFF")
		message(WARNING "mbedTLS or zlib was not found, RTMPS will be auto-disabled")
	endif()
endif()

if (WITH_RTMPS)
	find_package(MbedTLS REQUIRED)
	find_package(ZLIB REQUIRED)
	add_definitions(-DCRYPTO -DUSE_MBEDTLS)
	include_directories(${MBEDTLS_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS})
else()
	add_definitions(-DNO_CRYPTO)
endif()

set(COMPILE_FTL FALSE)

if (PKG_CONFIG_FOUND)
	pkg_check_modules(FTL libftl)
endif()

if (FTL_FOUND)
	find_package(Libcurl REQUIRED)
	message(STATUS "Found ftl-sdk (system): ftl outputs enabled")

	set(ftl_SOURCES ftl-stream.c)

	include_directories(${LIBCURL_INCLUDE_DIRS} ${FTL_INCLUDE_DIRS})
	set(COMPILE_FTL TRUE)
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ftl-sdk/CMakeLists.txt")
	find_package(Libcurl REQUIRED)
	message(STATUS "Found ftl-sdk: ftl outputs enabled")

	add_definitions(-DFTL_STATIC_COMPILE)

	include_directories(${LIBCURL_INCLUDE_DIRS})

	set(ftl_SOURCES
		ftl-stream.c
		ftl-sdk/libftl/hmac/hmac.c
		ftl-sdk/libftl/hmac/sha2.c
		ftl-sdk/libftl/ftl-sdk.c
		ftl-sdk/libftl/handshake.c
		ftl-sdk/libftl/ingest.c
		ftl-sdk/libftl/ftl_helpers.c
		ftl-sdk/libftl/media.c
		ftl-sdk/libftl/gettimeofday/gettimeofday.c
		ftl-sdk/libftl/logging.c)
	set(ftl_HEADERS
		ftl-sdk/libftl/hmac/hmac.h
		ftl-sdk/libftl/hmac/sha2.h
		ftl-sdk/libftl/ftl.h
		ftl-sdk/libftl/ftl_private.h)
	set(ftl_IMPORTS
		${OBS_JANSSON_IMPORT}
		${LIBCURL_LIBRARIES})

	if (WIN32)
		list(APPEND ftl_SOURCES
			ftl-sdk/libftl/win32/socket.c
			ftl-sdk/libftl/gettimeofday/gettimeofday.c
			ftl-sdk/libftl/win32/threads.c)
		list(APPEND ftl_HEADERS
			ftl-sdk/libftl/gettimeofday/gettimeofday.h
			ftl-sdk/libftl/win32/threads.h)

		include_directories(ftl-sdk/libftl/win32)
	else()
		list(APPEND ftl_SOURCES
			ftl-sdk/libftl/posix/socket.c
			ftl-sdk/libftl/posix/threads.c)
		list(APPEND ftl_HEADERS
			ftl-sdk/libftl/posix/threads.h)

		include_directories(ftl-sdk/libftl/posix)
	endif()

	include_directories(ftl-sdk/libftl)

	set(COMPILE_FTL TRUE)
endif()

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/obs-outputs-config.h.in"
	"${CMAKE_BINARY_DIR}/plugins/obs-outputs/config/obs-outputs-config.h")

include_directories("${CMAKE_BINARY_DIR}/plugins/obs-outputs/config")

if(WIN32)
	set(obs-outputs_PLATFORM_DEPS
		ws2_32
		winmm
		Iphlpapi)

	if (WITH_RTMPS OR (WITH_RTMPS STREQUAL "AUTO"))
		SET(obs-outputs_PLATFORM_DEPS
			${obs-outputs_PLATFORM_DEPS}
			crypt32)
	endif()
endif()

if(MSVC)
	set(obs-outputs_PLATFORM_DEPS
		${obs-outputs_PLATFORM_DEPS}
		w32-pthreads)
endif()

if(APPLE AND (WITH_RTMPS OR (WITH_RTMPS STREQUAL "AUTO")))
	find_library(FOUNDATION_FRAMEWORK Foundation)
	find_library(SECURITY_FRAMEWORK Security)

	set(obs-outputs_PLATFORM_DEPS
		${obs-outputs_PLATFORM_DEPS}
		${FOUNDATION_FRAMEWORK}
		${SECURITY_FRAMEWORK})
endif()

set(obs-outputs_librtmp_HEADERS
	librtmp/amf.h
	librtmp/bytes.h
	librtmp/cencode.h
	librtmp/dh.h
	librtmp/dhgroups.h
	librtmp/handshake.h
	librtmp/http.h
	librtmp/log.h
	librtmp/md5.h
	librtmp/rtmp.h
	librtmp/rtmp_sys.h)
set(obs-outputs_librtmp_SOURCES
	librtmp/amf.c
	librtmp/cencode.c
	librtmp/hashswf.c
	librtmp/log.c
	librtmp/md5.c
	librtmp/parseurl.c
	librtmp/rtmp.c)

if(NOT WIN32)
	set_source_files_properties(${obs-outputs_librtmp_SOURCES} PROPERTIES
		COMPILE_FLAGS "-fvisibility=hidden")
endif()

set(obs-outputs_HEADERS
	"${CMAKE_BINARY_DIR}/plugins/obs-outputs/config/obs-outputs-config.h"
	obs-output-ver.h
	rtmp-helpers.h
	rtmp-stream.h
	net-if.h
	flv-mux.h)
set(obs-outputs_SOURCES
	obs-outputs.c
	null-output.c
	rtmp-stream.c
	rtmp-windows.c
	flv-output.c
	flv-mux.c
	net-if.c)

if(WIN32)
	set(MODULE_DESCRIPTION "OBS output module")
	configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in obs-outputs.rc)
	list(APPEND obs-outputs_SOURCES
		obs-outputs.rc)
endif()

add_library(obs-outputs MODULE
	${ftl_SOURCES}
	${ftl_HEADERS}
	${obs-outputs_SOURCES}
	${obs-outputs_HEADERS}
	${obs-outputs_librtmp_SOURCES}
	${obs-outputs_librtmp_HEADERS})

if(FTL_FOUND)
	target_link_libraries(obs-outputs ${FTL_LIBRARIES})
	target_include_directories(obs-outputs PUBLIC ${FTL_INCLUDE_DIRS})
endif()

target_link_libraries(obs-outputs
	libobs
	${MBEDTLS_LIBRARIES}
	${ZLIB_LIBRARIES}
	${ftl_IMPORTS}
	${obs-outputs_PLATFORM_DEPS})
set_target_properties(obs-outputs PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(obs-outputs data)
