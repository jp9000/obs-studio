project(blake2)

add_library(blake2 STATIC)
add_library(OBS::blake2 ALIAS blake2)

target_sources(blake2 PRIVATE src/blake2b-ref.c src/blake2.h src/blake2-impl.h)

target_include_directories(
  blake2 PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>")

if(OS_WINDOWS)
  if(MSVC)
    target_compile_options(blake2 PRIVATE "$<IF:$<CONFIG:Debug>,/MTd,/MT>"
                                          "/Zl")
  endif()

  target_compile_definitions(blake2 PRIVATE inline=_inline restrict=__restrict)
endif()

set_target_properties(blake2 PROPERTIES FOLDER "deps")
