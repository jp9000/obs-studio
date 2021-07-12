# Helper function to set up runtime or library targets
function(setup_binary_target target)
  set_target_properties(
    ${target}
    PROPERTIES
      BUILD_RPATH
      "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_EXECUTABLE_DESTINATION}$<$<BOOL:LINUX_PORTABLE>:/${_ARCH_SUFFIX}bit>"
  )

  _setup_binary_target(${target})
endfunction()

# Helper function to export target to build and install tree Allows usage of
# `find_package(libobs)` by other build trees
function(export_target target)
  _export_target(${ARGV})

  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pkgconfig")
    export_target_pkgconf(${target})
  endif()
endfunction()

# Helper function to build pkgconfig file for target
function(export_target_pkgconf target)
  get_target_property(_TARGET_DEPENDENCIES ${target} INTERFACE_LINK_LIBRARIES)
  get_target_property(_TARGET_DEFINITIONS ${target}
                      INTERFACE_COMPILE_DEFINITIONS)
  get_target_property(_TARGET_OPTIONS ${target} INTERFACE_LINK_OPTIONS)

  foreach(_LIBRARY IN LISTS _TARGET_DEPENDENCIES)
    get_target_property(_LINK_LIBRARY ${_LIBRARY} INTERFACE_LINK_LIBRARIES)
    get_target_property(_LINK_DEFINITIONS ${_LIBRARY}
                        INTERFACE_COMPILE_DEFINITIONS)
    list(APPEND _LINKED_LIBRARIES "${_LINK_LIBRARY}")

    if(NOT "${_LINK_DEFINITIONS}" STREQUAL "_LINK_DEFINITIONS-NOTFOUND")
      list(APPEND _LINKED_DEFINITIONS "${_LINK_DEFINITIONS}")
    endif()
  endforeach()

  string(REPLACE ";" " " _LINKED_LIBRARIES "${_LINKED_LIBRARIES}")
  string(REPLACE ";" " " _LINKED_DEFINITIONS "${_LINKED_DEFINITIONS}")

  if(NOT "${_TARGET_DEFINITIONS}" STREQUAL "_TARGET_DEFINITIONS-NOTFOUND")
    list(JOIN _TARGET_DEFINITIONS "-D" _TARGET_DEFINITIONS)
    set(_TARGET_DEFINITIONS "-D${_TARGET_DEFINITIONS}")
  else()
    set(_TARGET_DEFINITIONS "")
  endif()

  if(NOT "${_TARGET_OPTIONS}" STREQUAL "_TARGET_OPTIONS-NOTFOUND")
    list(JOIN _TARGET_OPTIONS "-" _TARGET_OPTIONS)
    set(_TARGET_OPTIONS "-${_TARGET_OPTIONS}")
  else()
    set(_TARGET_OPTIONS "")
  endif()

  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/pkgconfig/${target}.pc.in"
                 "${target}.pc" @ONLY)

  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${target}.pc"
          DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endfunction()
