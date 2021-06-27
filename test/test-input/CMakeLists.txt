project(test-input)

add_library(test-input MODULE)

target_sources(
  test-input
  PRIVATE test-filter.c
          test-input.c
          test-sinewave.c
          sync-async-source.c
          sync-audio-buffering.c
          sync-pair-vid.c
          sync-pair-aud.c
          test-random.c)

target_link_libraries(test-input PRIVATE OBS::libobs)

if(MSVC)
  target_link_libraries(test-input PRIVATE OBS::w32-pthreads)
endif()

set_target_properties(test-input PROPERTIES FOLDER "tests and examples")

setup_plugin_target(test-input)
