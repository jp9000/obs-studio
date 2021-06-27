project(win-test)

add_executable(win-test WIN32)

target_sources(win-test PRIVATE test.cpp)

target_link_libraries(win-test PRIVATE OBS::libobs)

set_target_properties(win-test PROPERTIES FOLDER "tests and examples")

define_graphic_modules(win-test)
