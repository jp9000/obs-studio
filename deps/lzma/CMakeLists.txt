cmake_minimum_required(VERSION 3.2)

project(lzma)

set(LIBLZMA_INCLUDE_DIRS
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}/liblzma/api"
		CACHE PATH "lzma include path")

set(LIBLZMA_CONFIG
	"${CMAKE_CURRENT_SOURCE_DIR}/config.h"
		CACHE PATH "lzma config.h path")

include_directories(
	${LIBLZMA_INCLUDE_DIRS}
	common
	liblzma/api
	liblzma/check
	liblzma/common
	liblzma/delta
	liblzma/lz
	liblzma/lzma
	liblzma/rangecoder
	liblzma/simple
)

add_definitions(
	-DHAVE_CONFIG_H
	-DTUKLIB_SYMBOL_PREFIX=lzma_)

if(WIN32)
	if(MSVC)
		add_compile_options($<IF:$<CONFIG:Debug>,/MTd,/MT> /Zl)
		add_compile_options("/wd4244")
		add_compile_options("/wd4267")
	endif()
	add_definitions(
		-Dinline=_inline
		-Drestrict=__restrict)
endif()

set(liblzma_check_SOURCES
	liblzma/check/check.c)

list(APPEND liblzma_check_SOURCES
	liblzma/check/crc32_table.c
	liblzma/check/crc32_fast.c
	liblzma/check/crc64_table.c
	liblzma/check/crc64_fast.c
	liblzma/check/sha256.c)

set(liblzma_common_SOURCES
	liblzma/common/common.c
	liblzma/common/block_util.c
	liblzma/common/easy_preset.c
	liblzma/common/filter_common.c
	liblzma/common/hardware_physmem.c
	liblzma/common/index.c
	liblzma/common/stream_flags_common.c
	liblzma/common/vli_size.c
	liblzma/common/alone_encoder.c
	liblzma/common/block_buffer_encoder.c
	liblzma/common/block_encoder.c
	liblzma/common/block_header_encoder.c
	liblzma/common/easy_buffer_encoder.c
	liblzma/common/easy_encoder.c
	liblzma/common/easy_encoder_memusage.c
	liblzma/common/filter_buffer_encoder.c
	liblzma/common/filter_encoder.c
	liblzma/common/filter_flags_encoder.c
	liblzma/common/index_encoder.c
	liblzma/common/stream_buffer_encoder.c
	liblzma/common/stream_encoder.c
	liblzma/common/stream_flags_encoder.c
	liblzma/common/vli_encoder.c
	liblzma/common/alone_decoder.c
	liblzma/common/auto_decoder.c
	liblzma/common/block_buffer_decoder.c
	liblzma/common/block_decoder.c
	liblzma/common/block_header_decoder.c
	liblzma/common/easy_decoder_memusage.c
	liblzma/common/filter_buffer_decoder.c
	liblzma/common/filter_decoder.c
	liblzma/common/filter_flags_decoder.c
	liblzma/common/index_decoder.c
	liblzma/common/index_hash.c
	liblzma/common/stream_buffer_decoder.c
	liblzma/common/stream_decoder.c
	liblzma/common/stream_flags_decoder.c
	liblzma/common/vli_decoder.c)

set(liblzma_delta_SOURCES
	liblzma/delta/delta_common.c
	liblzma/delta/delta_encoder.c
	liblzma/delta/delta_decoder.c)

set(liblzma_lzma_SOURCES
	liblzma/lzma/lzma_encoder.c
	liblzma/lzma/lzma_encoder_presets.c
	liblzma/lzma/lzma_encoder_optimum_fast.c
	liblzma/lzma/lzma_encoder_optimum_normal.c
	liblzma/lzma/fastpos_table.c
	liblzma/lzma/lzma_decoder.c

	liblzma/lzma/lzma2_encoder.c
	liblzma/lzma/lzma2_decoder.c)

set(liblzma_lz_SOURCES
	liblzma/lz/lz_encoder.c
	liblzma/lz/lz_encoder_mf.c
	liblzma/lz/lz_decoder.c)

set(liblzma_rangecoder_SOURCES
	liblzma/rangecoder/price_table.c)

set(liblzma_simple_SOURCES
	liblzma/simple/simple_coder.c
	liblzma/simple/simple_encoder.c
	liblzma/simple/simple_decoder.c

	liblzma/simple/arm.c
	liblzma/simple/armthumb.c
	liblzma/simple/ia64.c
	liblzma/simple/powerpc.c
	liblzma/simple/sparc.c
	liblzma/simple/x86.c)

if (WIN32)
	SET_SOURCE_FILES_PROPERTIES(
		${liblzma_check_SOURCES}
		${liblzma_common_SOURCES}
		${liblzma_delta_SOURCES}
		${liblzma_lz_SOURCES}
		${liblzma_lzma_SOURCES}
		${liblzma_rangecoder_SOURCES}
		${liblzma_simple_SOURCES}
			PROPERTIES LANGUAGE C)
endif()

add_library(lzma STATIC
	${liblzma_check_SOURCES}
	${liblzma_common_SOURCES}
	${liblzma_delta_SOURCES}
	${liblzma_lz_SOURCES}
	${liblzma_lzma_SOURCES}
	${liblzma_rangecoder_SOURCES}
	${liblzma_simple_SOURCES})
set_target_properties(lzma PROPERTIES FOLDER "deps")
