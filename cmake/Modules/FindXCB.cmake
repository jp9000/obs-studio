# Try to find XCB on a Unix system
#
# This will define:
#
#   XCB_FOUND        - True if xcb is available
#   XCB_LIBRARIES    - Link these to use xcb
#   XCB_INCLUDE_DIRS - Include directory for xcb
#   XCB_DEFINITIONS  - Compiler flags for using xcb
#
# In addition the following more fine grained variables will be defined:
#
#   XCB_XCB_FOUND        XCB_XCB_INCLUDE_DIR        XCB_XCB_LIBRARY
#   XCB_UTIL_FOUND       XCB_UTIL_INCLUDE_DIR       XCB_UTIL_LIBRARY
#   XCB_COMPOSITE_FOUND  XCB_COMPOSITE_INCLUDE_DIR  XCB_COMPOSITE_LIBRARY
#   XCB_DAMAGE_FOUND     XCB_DAMAGE_INCLUDE_DIR     XCB_DAMAGE_LIBRARY
#   XCB_XFIXES_FOUND     XCB_XFIXES_INCLUDE_DIR     XCB_XFIXES_LIBRARY
#   XCB_RENDER_FOUND     XCB_RENDER_INCLUDE_DIR     XCB_RENDER_LIBRARY
#   XCB_RANDR_FOUND      XCB_RANDR_INCLUDE_DIR      XCB_RANDR_LIBRARY
#   XCB_SHAPE_FOUND      XCB_SHAPE_INCLUDE_DIR      XCB_SHAPE_LIBRARY
#   XCB_DRI2_FOUND       XCB_DRI2_INCLUDE_DIR       XCB_DRI2_LIBRARY
#   XCB_GLX_FOUND        XCB_GLX_INCLUDE_DIR        XCB_GLX_LIBRARY
#   XCB_SHM_FOUND        XCB_SHM_INCLUDE_DIR        XCB_SHM_LIBRARY
#   XCB_XV_FOUND         XCB_XV_INCLUDE_DIR         XCB_XV_LIBRARY
#   XCB_XINPUT_FOUND     XCB_XINPUT_INCLUDE_DIR     XCB_XINPUT_LIBRARY
#   XCB_SYNC_FOUND       XCB_SYNC_INCLUDE_DIR       XCB_SYNC_LIBRARY
#   XCB_XTEST_FOUND      XCB_XTEST_INCLUDE_DIR      XCB_XTEST_LIBRARY
#   XCB_ICCCM_FOUND      XCB_ICCCM_INCLUDE_DIR      XCB_ICCCM_LIBRARY
#   XCB_EWMH_FOUND       XCB_EWMH_INCLUDE_DIR       XCB_EWMH_LIBRARY
#   XCB_IMAGE_FOUND      XCB_IMAGE_INCLUDE_DIR      XCB_IMAGE_LIBRARY
#   XCB_RENDERUTIL_FOUND XCB_RENDERUTIL_INCLUDE_DIR XCB_RENDERUTIL_LIBRARY
#   XCB_KEYSYMS_FOUND    XCB_KEYSYMS_INCLUDE_DIR    XCB_KEYSYMS_LIBRARY
#
# Copyright (c) 2011 Fredrik Höglund <fredrik@kde.org>
# Copyright (c) 2013 Martin Gräßlin <mgraesslin@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

set(knownComponents XCB
                    COMPOSITE
                    DAMAGE
                    DRI2
                    EWMH
                    GLX
                    ICCCM
                    IMAGE
                    KEYSYMS
                    RANDR
                    RENDER
                    RENDERUTIL
                    SHAPE
                    SHM
                    SYNC
                    UTIL
                    XFIXES
                    XTEST
                    XV
                    XINPUT
                    XINERAMA)

unset(unknownComponents)

set(pkgConfigModules)
set(requiredComponents)

if (XCB_FIND_COMPONENTS)
  set(comps ${XCB_FIND_COMPONENTS})
else()
  set(comps ${knownComponents})
endif()

# iterate through the list of requested components, and check that we know them all.
# If not, fail.
foreach(comp ${comps})
    list(FIND knownComponents ${comp} index )
    if("${index}" STREQUAL "-1")
        list(APPEND unknownComponents "${comp}")
    else()
        if("${comp}" STREQUAL "XCB")
            list(APPEND pkgConfigModules "xcb")
        elseif("${comp}" STREQUAL "COMPOSITE")
            list(APPEND pkgConfigModules "xcb-composite")
        elseif("${comp}" STREQUAL "DAMAGE")
            list(APPEND pkgConfigModules "xcb-damage")
        elseif("${comp}" STREQUAL "DRI2")
            list(APPEND pkgConfigModules "xcb-dri2")
        elseif("${comp}" STREQUAL "EWMH")
            list(APPEND pkgConfigModules "xcb-ewmh")
        elseif("${comp}" STREQUAL "GLX")
            list(APPEND pkgConfigModules "xcb-glx")
        elseif("${comp}" STREQUAL "ICCCM")
            list(APPEND pkgConfigModules "xcb-icccm")
        elseif("${comp}" STREQUAL "IMAGE")
            list(APPEND pkgConfigModules "xcb-image")
        elseif("${comp}" STREQUAL "KEYSYMS")
            list(APPEND pkgConfigModules "xcb-keysyms")
        elseif("${comp}" STREQUAL "RANDR")
            list(APPEND pkgConfigModules "xcb-randr")
        elseif("${comp}" STREQUAL "RENDER")
            list(APPEND pkgConfigModules "xcb-render")
        elseif("${comp}" STREQUAL "RENDERUTIL")
            list(APPEND pkgConfigModules "xcb-renderutil")
        elseif("${comp}" STREQUAL "SHAPE")
            list(APPEND pkgConfigModules "xcb-shape")
        elseif("${comp}" STREQUAL "SHM")
            list(APPEND pkgConfigModules "xcb-shm")
        elseif("${comp}" STREQUAL "SYNC")
            list(APPEND pkgConfigModules "xcb-sync")
        elseif("${comp}" STREQUAL "UTIL")
            list(APPEND pkgConfigModules "xcb-util")
        elseif("${comp}" STREQUAL "XFIXES")
            list(APPEND pkgConfigModules "xcb-xfixes")
        elseif("${comp}" STREQUAL "XTEST")
            list(APPEND pkgConfigModules "xcb-xtest")
        elseif("${comp}" STREQUAL "XV")
            list(APPEND pkgConfigModules "xcb-xv")
        elseif("${comp}" STREQUAL "XINPUT")
            list(APPEND pkgConfigModules "xcb-xinput")
        elseif("${comp}" STREQUAL "XINERAMA")
            list(APPEND pkgConfigModules "xcb-xinerama")
        endif()
    endif()
endforeach()


if(DEFINED unknownComponents)
   set(msgType STATUS)
   if(XCB_FIND_REQUIRED)
      set(msgType FATAL_ERROR)
   endif()
   if(NOT XCB_FIND_QUIETLY)
      message(${msgType} "XCB: requested unknown components ${unknownComponents}")
   endif()
   return()
endif()

macro(_XCB_HANDLE_COMPONENT _comp)
    set(_header )
    set(_lib )
    if("${_comp}" STREQUAL "XCB")
        set(_header "xcb/xcb.h")
        set(_lib "xcb")
    elseif("${_comp}" STREQUAL "COMPOSITE")
        set(_header "xcb/composite.h")
        set(_lib "xcb-composite")
    elseif("${_comp}" STREQUAL "DAMAGE")
        set(_header "xcb/damage.h")
        set(_lib "xcb-damage")
    elseif("${_comp}" STREQUAL "DRI2")
        set(_header "xcb/dri2.h")
        set(_lib "xcb-dri2")
    elseif("${_comp}" STREQUAL "EWMH")
        set(_header "xcb/xcb_ewmh.h")
        set(_lib "xcb-ewmh")
    elseif("${_comp}" STREQUAL "GLX")
        set(_header "xcb/glx.h")
        set(_lib "xcb-glx")
    elseif("${_comp}" STREQUAL "ICCCM")
        set(_header "xcb/xcb_icccm.h")
        set(_lib "xcb-icccm")
    elseif("${_comp}" STREQUAL "IMAGE")
        set(_header "xcb/xcb_image.h")
        set(_lib "xcb-image")
    elseif("${_comp}" STREQUAL "KEYSYMS")
        set(_header "xcb/xcb_keysyms.h")
        set(_lib "xcb-keysyms")
    elseif("${_comp}" STREQUAL "RANDR")
        set(_header "xcb/randr.h")
        set(_lib "xcb-randr")
    elseif("${_comp}" STREQUAL "RENDER")
        set(_header "xcb/render.h")
        set(_lib "xcb-render")
    elseif("${_comp}" STREQUAL "RENDERUTIL")
        set(_header "xcb/xcb_renderutil.h")
        set(_lib "xcb-render-util")
    elseif("${_comp}" STREQUAL "SHAPE")
        set(_header "xcb/shape.h")
        set(_lib "xcb-shape")
    elseif("${_comp}" STREQUAL "SHM")
        set(_header "xcb/shm.h")
        set(_lib "xcb-shm")
    elseif("${_comp}" STREQUAL "SYNC")
        set(_header "xcb/sync.h")
        set(_lib "xcb-sync")
    elseif("${_comp}" STREQUAL "UTIL")
        set(_header "xcb/xcb_util.h")
        set(_lib "xcb-util")
    elseif("${_comp}" STREQUAL "XFIXES")
        set(_header "xcb/xfixes.h")
        set(_lib "xcb-xfixes")
    elseif("${_comp}" STREQUAL "XTEST")
        set(_header "xcb/xtest.h")
        set(_lib "xcb-xtest")
    elseif("${_comp}" STREQUAL "XV")
        set(_header "xcb/xv.h")
        set(_lib "xcb-xv")
    elseif("${_comp}" STREQUAL "XINPUT")
        set(_header "xcb/xinput.h")
        set(_lib "xcb-xinput")
    elseif("${_comp}" STREQUAL "XINERAMA")
        set(_header "xcb/xinerama.h")
        set(_lib "xcb-xinerama")
    endif()

    find_path(XCB_${_comp}_INCLUDE_DIR NAMES ${_header} HINTS ${PKG_XCB_INCLUDE_DIRS})
    find_library(XCB_${_comp}_LIBRARY NAMES ${_lib} HINTS ${PKG_XCB_LIBRARY_DIRS})
    mark_as_advanced(XCB_${_comp}_LIBRARY XCB_${_comp}_INCLUDE_DIR)

    if(XCB_${_comp}_INCLUDE_DIR AND XCB_${_comp}_LIBRARY)
        set(XCB_${_comp}_FOUND TRUE)
        list(APPEND XCB_INCLUDE_DIRS ${XCB_${_comp}_INCLUDE_DIR})
        list(APPEND XCB_LIBRARIES ${XCB_${_comp}_LIBRARY})
        if (NOT XCB_FIND_QUIETLY)
            message(STATUS "XCB[${_comp}]: Found component ${_comp}")
        endif()
    endif()
endmacro()

IF (NOT WIN32)
    # Use pkg-config to get the directories and then use these values
    # in the FIND_PATH() and FIND_LIBRARY() calls
    find_package(PkgConfig)
    pkg_check_modules(PKG_XCB QUIET ${pkgConfigModules})

    set(XCB_DEFINITIONS ${PKG_XCB_CFLAGS})

    foreach(comp ${comps})
        _xcb_handle_component(${comp})
    endforeach()

    if(XCB_INCLUDE_DIRS)
        list(REMOVE_DUPLICATES XCB_INCLUDE_DIRS)
    endif()

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(XCB
        REQUIRED_VARS XCB_LIBRARIES XCB_INCLUDE_DIRS
        HANDLE_COMPONENTS)

    # compatibility for old variable naming
    set(XCB_INCLUDE_DIR ${XCB_INCLUDE_DIRS})
ENDIF (NOT WIN32)
