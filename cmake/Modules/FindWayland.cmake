# Try to find Wayland on a Unix system
#
# This will define:
#
#   WAYLAND_FOUND        - True if Wayland is found
#   WAYLAND_LIBRARIES    - Link these to use Wayland
#   WAYLAND_INCLUDE_DIRS - Include directory for Wayland
#   WAYLAND_DEFINITIONS  - Compiler flags for using Wayland
#
# In addition the following more fine grained variables will be defined:
#
#   Wayland_Client_FOUND  WAYLAND_CLIENT_INCLUDE_DIRS  WAYLAND_CLIENT_LIBRARIES
#   Wayland_Server_FOUND  WAYLAND_SERVER_INCLUDE_DIRS  WAYLAND_SERVER_LIBRARIES
#   Wayland_EGL_FOUND     WAYLAND_EGL_INCLUDE_DIRS     WAYLAND_EGL_LIBRARIES
#   Wayland_Cursor_FOUND  WAYLAND_CURSOR_INCLUDE_DIRS  WAYLAND_CURSOR_LIBRARIES
#
# Copyright (c) 2013 Martin Gräßlin <mgraesslin@kde.org>
#               2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

IF (NOT WIN32)

  # Use pkg-config to get the directories and then use these values
  # in the find_path() and find_library() calls
  find_package(PkgConfig)
  PKG_CHECK_MODULES(PKG_WAYLAND QUIET wayland-client wayland-server wayland-egl wayland-cursor)

  set(WAYLAND_DEFINITIONS ${PKG_WAYLAND_CFLAGS})

  find_path(WAYLAND_CLIENT_INCLUDE_DIRS NAMES wayland-client.h HINTS ${PKG_WAYLAND_INCLUDE_DIRS})
  find_library(WAYLAND_CLIENT_LIBRARIES NAMES wayland-client   HINTS ${PKG_WAYLAND_LIBRARY_DIRS})
  if(WAYLAND_CLIENT_INCLUDE_DIRS AND WAYLAND_CLIENT_LIBRARIES)
    set(Wayland_Client_FOUND TRUE)
  else()
    set(Wayland_Client_FOUND FALSE)
  endif()
  mark_as_advanced(WAYLAND_CLIENT_INCLUDE_DIRS WAYLAND_CLIENT_LIBRARIES)

  find_path(WAYLAND_CURSOR_INCLUDE_DIRS NAMES wayland-cursor.h HINTS ${PKG_WAYLAND_INCLUDE_DIRS})
  find_library(WAYLAND_CURSOR_LIBRARIES NAMES wayland-cursor   HINTS ${PKG_WAYLAND_LIBRARY_DIRS})
  if(WAYLAND_CURSOR_INCLUDE_DIRS AND WAYLAND_CURSOR_LIBRARIES)
    set(Wayland_Cursor_FOUND TRUE)
  else()
    set(Wayland_Cursor_FOUND FALSE)
  endif()
  mark_as_advanced(WAYLAND_CURSOR_INCLUDE_DIRS WAYLAND_CURSOR_LIBRARIES)

  find_path(WAYLAND_EGL_INCLUDE_DIRS    NAMES wayland-egl.h    HINTS ${PKG_WAYLAND_INCLUDE_DIRS})
  find_library(WAYLAND_EGL_LIBRARIES    NAMES wayland-egl      HINTS ${PKG_WAYLAND_LIBRARY_DIRS})
  if(WAYLAND_EGL_INCLUDE_DIRS AND WAYLAND_EGL_LIBRARIES)
    set(Wayland_EGL_FOUND TRUE)
  else()
    set(Wayland_EGL_FOUND FALSE)
  endif()
  mark_as_advanced(WAYLAND_EGL_INCLUDE_DIRS WAYLAND_EGL_LIBRARIES)

  find_path(WAYLAND_SERVER_INCLUDE_DIRS NAMES wayland-server.h HINTS ${PKG_WAYLAND_INCLUDE_DIRS})
  find_library(WAYLAND_SERVER_LIBRARIES NAMES wayland-server   HINTS ${PKG_WAYLAND_LIBRARY_DIRS})
  if(WAYLAND_SERVER_INCLUDE_DIRS AND WAYLAND_SERVER_LIBRARIES)
    set(Wayland_Server_FOUND TRUE)
  else()
    set(Wayland_Server_FOUND FALSE)
  endif()
  mark_as_advanced(WAYLAND_SERVER_INCLUDE_DIRS WAYLAND_SERVER_LIBRARIES)

  set(WAYLAND_INCLUDE_DIRS ${WAYLAND_CLIENT_INCLUDE_DIRS} ${WAYLAND_SERVER_INCLUDE_DIRS} ${WAYLAND_EGL_INCLUDE_DIRS} ${WAYLAND_CURSOR_INCLUDE_DIRS})
  set(WAYLAND_LIBRARIES ${WAYLAND_CLIENT_LIBRARIES} ${WAYLAND_SERVER_LIBRARIES} ${WAYLAND_EGL_LIBRARIES} ${WAYLAND_CURSOR_LIBRARIES})
  mark_as_advanced(WAYLAND_INCLUDE_DIRS WAYLAND_LIBRARIES)

  list(REMOVE_DUPLICATES WAYLAND_INCLUDE_DIRS)

  include(FindPackageHandleStandardArgs)

  find_package_handle_standard_args(Wayland REQUIRED_VARS WAYLAND_LIBRARIES WAYLAND_INCLUDE_DIRS HANDLE_COMPONENTS)

ENDIF ()
