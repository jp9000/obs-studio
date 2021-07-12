# * Try to find libX11-xcb Once done this will define
#
# X11_XCB_FOUND - system has libX11-xcb X11_XCB_LIBRARIES - Link these to use
# libX11-xcb X11_XCB_INCLUDE_DIR - the libX11-xcb include dir
# X11_XCB_DEFINITIONS - compiler switches required for using libX11-xcb

# Copyright (c) 2011 Fredrik Höglund <fredrik@kde.org> Copyright (c) 2008 Helio
# Chissini de Castro, <helio@kde.org> Copyright (c) 2007 Matthias Kretz,
# <kretz@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if(NOT WIN32)
  # use pkg-config to get the directories and then use these values in the
  # FIND_PATH() and FIND_LIBRARY() calls
  find_package(PkgConfig)
  pkg_check_modules(PKG_X11_XCB QUIET x11-xcb)

  set(X11_XCB_DEFINITIONS ${PKG_X11_XCB_CFLAGS})

  find_path(
    X11_XCB_INCLUDE_DIR
    NAMES X11/Xlib-xcb.h
    HINTS ${PKG_X11_XCB_INCLUDE_DIRS})
  find_library(
    X11_XCB_LIBRARIES
    NAMES X11-xcb
    HINTS ${PKG_X11_XCB_LIBRARY_DIRS})

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(X11_XCB DEFAULT_MSG X11_XCB_LIBRARIES
                                    X11_XCB_INCLUDE_DIR)

  mark_as_advanced(X11_XCB_INCLUDE_DIR X11_XCB_LIBRARIES)

  if(X11_XCB_FOUND AND NOT TARGET X11::X11_xcb)
    add_library(X11::X11_xcb UNKNOWN IMPORTED)
    set_target_properties(
      X11::X11_xcb
      PROPERTIES IMPORTED_LOCATION "${X11_XCB_LIBRARIES}"
                 INTERFACE_INCLUDE_DIRECTORIES "${X11_XCB_INCLUDE_DIR}"
                 INTERFACE_LINK_LIBRARIES "XCB::XCB;X11::X11")
  endif()
endif()
