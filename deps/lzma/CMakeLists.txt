project(lzma)

add_library(lzma STATIC)
add_library(OBS::lzma ALIAS lzma)

target_sources(lzma PRIVATE config.h)

target_sources(
  lzma
  PRIVATE liblzma/check/check.c liblzma/check/crc32_table.c
          liblzma/check/crc32_fast.c liblzma/check/crc64_table.c
          liblzma/check/crc64_fast.c liblzma/check/sha256.c)

target_sources(
  lzma
  PRIVATE liblzma/common/common.c
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

target_sources(
  lzma PRIVATE liblzma/delta/delta_common.c liblzma/delta/delta_encoder.c
               liblzma/delta/delta_decoder.c)

target_sources(
  lzma
  PRIVATE liblzma/lzma/lzma_encoder.c
          liblzma/lzma/lzma_encoder_presets.c
          liblzma/lzma/lzma_encoder_optimum_fast.c
          liblzma/lzma/lzma_encoder_optimum_normal.c
          liblzma/lzma/fastpos_table.c
          liblzma/lzma/lzma_decoder.c
          liblzma/lzma/lzma2_encoder.c
          liblzma/lzma/lzma2_decoder.c)

target_sources(lzma PRIVATE liblzma/lz/lz_encoder.c liblzma/lz/lz_encoder_mf.c
                            liblzma/lz/lz_decoder.c)

target_sources(lzma PRIVATE liblzma/rangecoder/price_table.c)

target_sources(
  lzma
  PRIVATE liblzma/simple/simple_coder.c
          liblzma/simple/simple_encoder.c
          liblzma/simple/simple_decoder.c
          liblzma/simple/arm.c
          liblzma/simple/armthumb.c
          liblzma/simple/ia64.c
          liblzma/simple/powerpc.c
          liblzma/simple/sparc.c
          liblzma/simple/x86.c)

target_compile_definitions(lzma PRIVATE HAVE_CONFIG_H
                                        TUKLIB_SYMBOL_PREFIX=lzma_)

target_include_directories(
  lzma
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
          ${CMAKE_CURRENT_SOURCE_DIR}/common
          ${CMAKE_CURRENT_SOURCE_DIR}/liblzma/check
          ${CMAKE_CURRENT_SOURCE_DIR}/liblzma/common
          ${CMAKE_CURRENT_SOURCE_DIR}/liblzma/delta
          ${CMAKE_CURRENT_SOURCE_DIR}/liblzma/lz
          ${CMAKE_CURRENT_SOURCE_DIR}/liblzma/lzma
          ${CMAKE_CURRENT_SOURCE_DIR}/liblzma/rangecoder
          ${CMAKE_CURRENT_SOURCE_DIR}/liblzma/simple
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/liblzma/api)

if(OS_WINDOWS)
  if(MSVC)
    target_compile_options(lzma PRIVATE "$<IF:$<CONFIG:Debug>,/MTd,/MT>" "/Zl"
                                        "/wd4244" "/wd4267")
  endif()

  target_compile_definitions(lzma PRIVATE inline=_inline restrict=__restrict)
endif()

set_target_properties(lzma PROPERTIES FOLDER "deps")
