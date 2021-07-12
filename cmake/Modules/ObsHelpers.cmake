# Set OS-specific constants in non-deprecated way
include(GNUInstallDirs)
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
  include(ObsDefaults_macOS)
  set(OS_MACOS ON)
  set(OS_POSIX ON)
elseif("${CMAKE_SYSTEM_NAME}" MATCHES "Linux|FreeBSD|OpenBSD")
  include(ObsDefaults_Linux)
  set(OS_POSIX ON)
  string(TOUPPER "${CMAKE_SYSTEM_NAME}" _SYSTEM_NAME_U)
  set(OS_${_SYSTEM_NAME_U} ON)
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  include(ObsDefaults_Windows)
  set(OS_WINDOWS ON)
  set(OS_POSIX OFF)
endif()

# Create global property to hold list of activated modules
set_property(GLOBAL PROPERTY OBS_MODULE_LIST "")

# ##############################################################################
# GLOBAL HELPER FUNCTIONS #
# ##############################################################################

# Helper function to set up runtime or library targets
function(setup_binary_target target)
  install(
    TARGETS ${target}
    EXPORT ${target}Targets
    RUNTIME DESTINATION "${OBS_EXECUTABLE_DESTINATION}"
            COMPONENT ${target}_Runtime
    LIBRARY DESTINATION "${OBS_LIBRARY_DESTINATION}"
            COMPONENT ${target}_Runtime
            NAMELINK_COMPONENT ${target}_Development
    ARCHIVE DESTINATION "${OBS_LIBRARY_DESTINATION}"
            COMPONENT ${target}_Development
    PUBLIC_HEADER DESTINATION ${OBS_INCLUDE_DESTINATION}
    INCLUDES
    DESTINATION ${OBS_INCLUDE_DESTINATION})

  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${target}>"
      "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_EXECUTABLE_DESTINATION}/$<TARGET_FILE_NAME:${target}>"
    VERBATIM)
endfunction()

# Helper function to set up OBS plugin targets
function(setup_plugin_target target)
  set_target_properties(${target} PROPERTIES PREFIX "")

  install(
    TARGETS ${target}
    RUNTIME DESTINATION "${OBS_PLUGIN_DESTINATION}" COMPONENT ${target}_Runtime
    LIBRARY DESTINATION "${OBS_PLUGIN_DESTINATION}"
            COMPONENT ${target}_Runtime
            NAMELINK_COMPONENT ${target}_Development)

  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${target}>"
      "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_PLUGIN_DESTINATION}/$<TARGET_FILE_NAME:${target}>"
    VERBATIM)

  setup_target_resources("${target}" "obs-plugins/${target}")
  set_property(GLOBAL APPEND PROPERTY OBS_MODULE_LIST "${target}")
  message(STATUS "OBS:   ${target} enabled")
endfunction()

# Helper function to set up OBS scripting plugin targets
function(setup_script_plugin_target target)
  set_target_properties(${target} PROPERTIES PREFIX "")

  install(
    TARGETS ${target}
    LIBRARY DESTINATION "${OBS_SCRIPT_PLUGIN_DESTINATION}"
            COMPONENT ${target}_Runtime
            NAMELINK_COMPONENT ${target}_Development)

  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${target}>"
      "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_DATA_DESTINATION}/obs-scripting/${_ARCH_SUFFIX}bit/$<TARGET_FILE_NAME:${target}>"
    VERBATIM)

  if(${target} STREQUAL "obspython")
    install(
      FILES "$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_BASE_NAME:${target}>.py"
      DESTINATION "${OBS_SCRIPT_PLUGIN_DESTINATION}")

    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -E copy
        "$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_BASE_NAME:${target}>.py"
        "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_DATA_DESTINATION}/obs-scripting/${_ARCH_SUFFIX}bit/$<TARGET_FILE_BASE_NAME:${target}>.py"
      VERBATIM)
  endif()
  set_property(GLOBAL APPEND PROPERTY OBS_SCRIPTING_MODULE_LIST "${target}")
  message(STATUS "OBS:   ${target} enabled")
endfunction()

# Helper function to set up target resources (e.g. L10N files)
function(setup_target_resources target destination)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
    install(
      DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/"
      DESTINATION "${OBS_DATA_DESTINATION}/${destination}"
      USE_SOURCE_PERMISSIONS)

    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/data"
        "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_DATA_DESTINATION}/${destination}"
      VERBATIM)
  endif()
endfunction()

# Helper function to set up specific resource files for targets
function(add_target_resource target resource destination)
  install(FILES ${resource}
          DESTINATION "${OBS_DATA_DESTINATION}/${destination}")

  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_DATA_DESTINATION}/${destination}"
    VERBATIM)

  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy "${resource}"
            "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_DATA_DESTINATION}/${destination}"
    VERBATIM)
endfunction()

# Helper function to set up OBS app target
function(setup_obs_app target)
  setup_binary_target(${target})

  # detect outdated obs-browser submodule
  if(NOT TARGET OBS::browser AND TARGET obs-browser)
    target_compile_features(obs-browser-page PRIVATE cxx_std_17)

    add_library(OBS::browser ALIAS obs-browser)
  endif()

  if(TARGET OBS::browser)
    setup_target_browser(${target})
  endif()
endfunction()

# Helper function to do additional setup for browser source plugin
function(setup_target_browser target)
  install(DIRECTORY "${CEF_ROOT_DIR}/Resources/"
          DESTINATION "${OBS_PLUGIN_DESTINATION}")

  install(DIRECTORY "${CEF_ROOT_DIR}/Release/"
          DESTINATION "${OBS_PLUGIN_DESTINATION}")

  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CEF_ROOT_DIR}/Resources/"
            "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_PLUGIN_DESTINATION}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CEF_ROOT_DIR}/Release/"
            "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_PLUGIN_DESTINATION}"
    VERBATIM)
endfunction()

# Helper function to export target to build and install tree. Allows usage of
# `find_package(libobs)` by other build trees
function(export_target target)
  set(CMAKE_EXPORT_PACKAGE_REGISTRY ON)

  include(GenerateExportHeader)
  generate_export_header(${target} EXPORT_FILE_NAME
                         "${CMAKE_CURRENT_BINARY_DIR}/${target}_EXPORT.h")

  target_sources(${target}
                 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${target}_EXPORT.h")

  set(TARGETS_EXPORT_NAME "${target}Targets")
  include(CMakePackageConfigHelpers)
  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${target}Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${target}Config.cmake"
    INSTALL_DESTINATION ${OBS_CMAKE_DESTINATION}/${target}
    PATH_VARS OBS_PLUGIN_DESTINATION OBS_DATA_DESTINATION)

  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${target}ConfigVersion.cmake
    VERSION ${OBS_VERSION_CANONICAL}
    COMPATIBILITY SameMajorVersion)

  export(
    EXPORT ${target}Targets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGETS_EXPORT_NAME}.cmake"
    NAMESPACE OBS::)

  export(PACKAGE "${target}")

  install(
    EXPORT ${TARGETS_EXPORT_NAME}
    FILE ${TARGETS_EXPORT_NAME}.cmake
    NAMESPACE OBS::
    DESTINATION ${OBS_CMAKE_DESTINATION}/${target})

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${target}Config.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/${target}ConfigVersion.cmake
          DESTINATION ${OBS_CMAKE_DESTINATION}/${target})
endfunction()

# Helper function to install header files
function(install_headers target)
  install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/"
    DESTINATION "${OBS_INCLUDE_DESTINATION}"
    COMPONENT ${target}_Headers
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
    PATTERN "cmake" EXCLUDE
    PATTERN "pkgconfig" EXCLUDE
    PATTERN "data" EXCLUDE)

  if(NOT EXISTS "${OBS_INCLUDE_DESTINATION}/obsconfig.h")
    install(
      FILES "${CMAKE_BINARY_DIR}/config/obsconfig.h"
      DESTINATION "${OBS_INCLUDE_DESTINATION}"
      COMPONENT ${target}_Headers)
  endif()
endfunction()

# Helper function to define available graphics modules for targets
function(define_graphic_modules target)
  foreach(_GRAPHICS_API opengl d3d9 d3d11)
    string(TOUPPER ${_GRAPHICS_API} _GRAPHICS_API_u)
    if(TARGET OBS::libobs-${_GRAPHICS_API})
      if(OS_POSIX AND NOT LINUX_PORTABLE)
        target_compile_definitions(
          ${target}
          PRIVATE
            DL_${_GRAPHICS_API_u}="$<TARGET_SONAME_FILE_NAME:libobs-${_GRAPHICS_API}>"
        )
      else()
        target_compile_definitions(
          ${target}
          PRIVATE
            DL_${_GRAPHICS_API_u}="$<TARGET_FILE_NAME:libobs-${_GRAPHICS_API}>")
      endif()
    else()
      target_compile_definitions(${target} PRIVATE DL_${_GRAPHICS_API_u}="")
    endif()
  endforeach()
endfunction()

# Idea adapted from: https://github.com/edsiper/cmake-options
macro(set_option option value)
  set(${option}
      ${value}
      CACHE INTERNAL "")
endmacro()

if(OS_WINDOWS)
  include(ObsHelpers_Windows)
elseif(OS_MACOS)
  include(ObsHelpers_macOS)
elseif(OS_POSIX)
  include(ObsHelpers_Linux)
endif()

# ##############################################################################
# LEGACY FALLBACKS     #
# ##############################################################################

# Helper function to install OBS plugin with associated resource directory
function(_install_obs_plugin_with_data target source)
  setup_plugin_target(${target})

  if(NOT ${source} STREQUAL "data"
     AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
     AND NOT OS_MACOS)
    install(
      DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${source}/"
      DESTINATION "${OBS_DATA_DESTINATION}/obs-plugins/${target}"
      USE_SOURCE_PERMISSIONS)

    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${source}/"
        "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_DATA_DESTINATION}/obs-plugins/${target}"
      VERBATIM)

    if(OS_WINDOWS AND DEFINED ENV{obsInstallerTempDir})
      add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND
          "${CMAKE_COMMAND}" -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/${source}/"
          "$ENV{obsInstallerTempDir}/${OBS_DATA_DESTINATION}/obs-plugins/${target}"
        VERBATIM)
    endif()
  endif()
endfunction()

# Helper function to install OBS plugin
function(_install_obs_plugin target)
  setup_plugin_target(${target})
endfunction()

# Helper function to install data for a target only
function(_install_obs_datatarget target destination)
  install(
    TARGETS ${target}
    LIBRARY DESTINATION "${OBS_DATA_DESTINATION}/${destination}"
    RUNTIME DESTINATION "${OBS_DATA_DESTINATION}/${destination}")

  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${target}>"
      "${OBS_OUTPUT_DIR}/$<CONFIG>/data/${destination}/$<TARGET_FILE_NAME:${target}>"
    VERBATIM)

  if(OS_WINDOWS)
    if(MSVC)
      add_target_resource(${target} "$<TARGET_PDB_FILE:${target}>"
                          "${destination}")
    endif()

    if(DEFINED ENV{obsInstallerTempDir})
      add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND
          "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${target}>"
          "$ENV{obsInstallerTempDir}/${OBS_DATA_DESTINATION}/${destination}/$<TARGET_FILE_NAME:${target}>"
        VERBATIM)
    endif()
  endif()
endfunction()
