project(obs-filters)

option(ENABLE_SPEEXDSP
       "Enable building with SpeexDSP-based noise suppression filter" ON)
option(ENABLE_RNNOISE "Enable building with RNNoise noise supression filter" ON)

if(OS_WINDOWS)
  option(
    ENABLE_NVAFX
    "Enable building with NVIDIA Audio Effects SDK (requires redistributable to be installed)"
    ON)
endif()

add_library(obs-filters MODULE)
add_library(OBS::filters ALIAS obs-filters)

set(HAS_NOISEREDUCTION OFF)

if(NOT ENABLE_SPEEXDSP)
  message(STATUS "OBS: SpeexDSP noise suppression not enabled")
else()
  find_package(Libspeexdsp REQUIRED)

  target_sources(obs-filters PRIVATE noise-suppress-filter.c)

  target_link_libraries(obs-filters PRIVATE LibspeexDSP::LibspeexDSP)

  target_compile_definitions(obs-filters PRIVATE LIBSPEEXDSP_ENABLED)
endif()

if(NOT ENABLE_RNNOISE)
  message(STATUS "OBS: RNNoise noise suppression not enabled")
else()
  find_package(Librnnoise QUIET)

  if(NOT TARGET Librnnoise::Librnnoise)
    message(STATUS "OBS:   using bundled RNNoise library")
    add_library(obs-rnnoise INTERFACE)
    add_library(Librnnoise::Librnnoise ALIAS obs-rnnoise)

    set(_RNNOISE_SOURCES
        rnnoise/src/arch.h
        rnnoise/src/celt_lpc.c
        rnnoise/src/celt_lpc.h
        rnnoise/src/common.h
        rnnoise/src/denoise.c
        rnnoise/src/kiss_fft.c
        rnnoise/src/kiss_fft.h
        rnnoise/src/opus_types.h
        rnnoise/src/pitch.c
        rnnoise/src/pitch.h
        rnnoise/src/rnn_data.c
        rnnoise/src/rnn_data.h
        rnnoise/src/rnn_reader.c
        rnnoise/src/rnn.c
        rnnoise/src/rnn.h
        rnnoise/src/tansig_table.h
        rnnoise/src/_kiss_fft_guts.h
        rnnoise/include/rnnoise.h)

    target_sources(obs-rnnoise INTERFACE ${_RNNOISE_SOURCES})

    target_include_directories(
      obs-rnnoise INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/rnnoise/include")

    target_compile_definitions(obs-rnnoise INTERFACE COMPILE_OPUS)

    if(OS_LINUX)
      set_property(SOURCE ${_RNNOISE_SOURCES} PROPERTY COMPILE_FLAGS
                                                       "-fvisibility=hidden")
    endif()

    source_group("rnnoise" FILES ${_RNNOISE_SOURCES})
  endif()

  target_sources(obs-filters PRIVATE noise-suppress-filter.c)

  target_link_libraries(obs-filters PRIVATE Librnnoise::Librnnoise)

  target_compile_definitions(obs-filters PRIVATE LIBRNNOISE_ENABLED)
endif()

if(NOT ENABLE_NVAFX)
  message(STATUS "OBS: NVidia Audio FX not enabled")
  set(LIBNVAFX_FOUND OFF)
else()
  message(STATUS "OBS: NVidia Audio FX support enabled")

  target_compile_definitions(obs-filters PRIVATE LIBNVAFX_ENABLED)

  set(LIBNVAFX_FOUND ON)
endif()

if(TARGET Librnnoise::Librnnoise
   OR TARGET LibspeexDSP::LibspeexDSP
   OR LIBNVAFX_FOUND)
  target_compile_definitions(obs-filters PRIVATE HAS_NOISEREDUCTION)
endif()

target_sources(
  obs-filters
  PRIVATE obs-filters.c
          color-correction-filter.c
          async-delay-filter.c
          gpu-delay.c
          crop-filter.c
          scale-filter.c
          scroll-filter.c
          chroma-key-filter.c
          color-key-filter.c
          color-grade-filter.c
          sharpness-filter.c
          gain-filter.c
          noise-gate-filter.c
          mask-filter.c
          invert-audio-polarity.c
          compressor-filter.c
          limiter-filter.c
          expander-filter.c
          luma-key-filter.c)

target_include_directories(
  obs-filters PRIVATE "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/config>")

target_link_libraries(obs-filters PRIVATE OBS::libobs)

set_target_properties(obs-filters PROPERTIES FOLDER "plugins" PREFIX "")

if(OS_WINDOWS)
  if(MSVC)
    target_link_libraries(obs-filters PRIVATE OBS::w32-pthreads)
  endif()

  set(MODULE_DESCRIPTION "OBS A/V Filters")
  configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
                 obs-filters.rc)

  target_sources(obs-filters PRIVATE obs-filters.rc)
endif()

setup_plugin_target(obs-filters)
