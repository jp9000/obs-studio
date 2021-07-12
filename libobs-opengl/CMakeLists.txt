project(libobs-opengl)

add_library(libobs-opengl SHARED)
add_library(OBS::libobs-opengl ALIAS libobs-opengl)

target_sources(
  libobs-opengl
  PRIVATE gl-helpers.c
          gl-helpers.h
          gl-indexbuffer.c
          gl-shader.c
          gl-shaderparser.c
          gl-shaderparser.h
          gl-stagesurf.c
          gl-subsystem.c
          gl-subsystem.h
          gl-texture2d.c
          gl-texture3d.c
          gl-texturecube.c
          gl-vertexbuffer.c
          gl-zstencil.c)

target_link_libraries(libobs-opengl PRIVATE OBS::libobs OBS::obsglad)

set_target_properties(
  libobs-opengl
  PROPERTIES FOLDER "core"
             VERSION "${OBS_VERSION_MAJOR}"
             SOVERSION "1")

if(OS_WINDOWS)
  set(MODULE_DESCRIPTION "OBS Library OpenGL wrapper")
  configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
                 libobs-opengl.rc)

  target_sources(libobs-opengl PRIVATE gl-windows.c libobs-opengl.rc)

elseif(OS_MACOS)
  find_library(COCOA Cocoa)
  find_library(IOSURF IOSurface)

  target_sources(libobs-opengl PRIVATE gl-cocoa.m)

  target_link_libraries(libobs-opengl PRIVATE ${COCOA} ${IOSURF})

  set_target_properties(libobs-opengl PROPERTIES PREFIX "")

elseif(OS_POSIX)
  find_package(X11 REQUIRED)
  find_package(XCB COMPONENTS XCB)
  find_package(X11_XCB REQUIRED)

  target_sources(libobs-opengl PRIVATE gl-egl-common.c gl-nix.c gl-x11-egl.c
                                       gl-x11-glx.c)

  target_link_libraries(libobs-opengl PRIVATE XCB::XCB X11::X11_xcb)

  set_target_properties(libobs-opengl PROPERTIES PREFIX "")

  if(ENABLE_WAYLAND)
    find_package(
      OpenGL
      COMPONENTS EGL
      REQUIRED)
    find_package(Wayland REQUIRED)

    target_sources(libobs-opengl PRIVATE gl-wayland-egl.c)

    target_link_libraries(libobs-opengl PRIVATE OpenGL::EGL Wayland::EGL)
  endif()
endif()

setup_binary_target(libobs-opengl)
