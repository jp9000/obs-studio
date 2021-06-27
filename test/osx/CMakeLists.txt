project(osx-text)

find_library(COCOA Cocoa)
mark_as_advanced(COCOA)

add_executable(macOS_test)

target_sources(macOS_test PRIVATE test.mm)

target_link_libraries(macOS_test PRIVATE OBS::libobs ${COCOA})

target_compile_options(macOS_test PRIVATE -fobjc-arc)

target_compile_features(macOS_test PRIVATE cxx_std_11)

set_target_properties(macOS_test PROPERTIES FOLDER "tests and examples")

define_graphic_modules(macOS_test)
