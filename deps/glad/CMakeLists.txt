project(glad)

find_package(OpenGL REQUIRED)

add_library(glad SHARED)
add_library(OBS::obsglad ALIAS glad)

target_sources(glad PRIVATE src/glad.c include/glad/glad.h)

target_include_directories(
  glad PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/include")

target_compile_definitions(glad PRIVATE GLAD_GLAPI_EXPORT_BUILD)

target_link_libraries(glad PUBLIC OpenGL::GL)

set_target_properties(
  glad
  PROPERTIES OUTPUT_NAME obsglad
             FOLDER "deps"
             VERSION "${OBS_VERSION_MAJOR}"
             SOVERSION "1")

if(OS_WINDOWS)
  set(MODULE_DESCRIPTION "Glad OpenGL Loading Library")

  configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
                 obsglad.rc)

  target_sources(glad PRIVATE src/glad_wgl.c include/glad/glad_wgl.h obsglad.rc)

elseif(OS_POSIX AND NOT OS_MACOS)
  find_package(OpenGL REQUIRED)
  find_package(X11 REQUIRED)

  target_link_libraries(glad PRIVATE X11::X11)

  target_sources(glad PRIVATE src/glad_glx.c include/glad/glad_glx.h)

  if(TARGET OpenGL::EGL)
    target_sources(glad PRIVATE src/glad_egl.c include/glad/glad_egl.h)

    target_link_libraries(glad PRIVATE OpenGL::EGL)
  endif()

endif()

if(NOT MSVC)
  target_compile_options(glad PRIVATE -fvisibility=hidden)
endif()

setup_binary_target(glad)
