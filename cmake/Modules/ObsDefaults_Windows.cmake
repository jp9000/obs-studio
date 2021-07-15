cmake_minimum_required(VERSION 3.20)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX
      "${CMAKE_BINARY_DIR}/rundir"
      CACHE STRING "Directory to install OBS after building" FORCE)
endif()

# Enable building Windows modules with file descriptors
# https://github.com/obsproject/obs-studio/commit/51be039cf82fc347587d16b48f74e65e86bee301
set(MODULE_DESCRIPTION "OBS Studio")

macro(setup_obs_project)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_ARCH_SUFFIX 64)
  else()
    set(_ARCH_SUFFIX 32)
  endif()

  set(OBS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/rundir")

  set(OBS_EXECUTABLE_DESTINATION "bin/${_ARCH_SUFFIX}bit")
  set(OBS_EXECUTABLE32_DESTINATION "bin/32bit")
  set(OBS_EXECUTABLE64_DESTINATION "bin/64bit")
  set(OBS_LIBRARY_DESTINATION "bin/${_ARCH_SUFFIX}bit")
  set(OBS_LIBRARY32_DESTINATION "bin/32bit")
  set(OBS_LIBRARY64_DESTINATION "bin/64bit")
  set(OBS_PLUGIN_DESTINATION "obs-plugins/${_ARCH_SUFFIX}bit")
  set(OBS_PLUGIN32_DESTINATION "obs-plugins/32bit")
  set(OBS_PLUGIN64_DESTINATION "obs-plugins/64bit")

  set(OBS_INCLUDE_DESTINATION "include")
  set(OBS_CMAKE_DESTINATION "cmake")
  set(OBS_DATA_DESTINATION "data")
  set(OBS_DATA_PATH "../../${OBS_DATA_DESTINATION}")
  set(OBS_INSTALL_PREFIX "")

  set(OBS_SCRIPT_PLUGIN_DESTINATION
      "${OBS_DATA_DESTINATION}/obs-scripting/${_ARCH_SUFFIX}bit")
  set(OBS_SCRIPT_PLUGIN_PATH "../../${OBS_SCRIPT_PLUGIN_DESTINATION}")

  string(REPLACE "-" ";" UI_VERSION_SPLIT ${OBS_VERSION})
  list(GET UI_VERSION_SPLIT 0 UI_VERSION)
  string(REPLACE "." ";" UI_VERSION_SEMANTIC ${UI_VERSION})
  list(GET UI_VERSION_SEMANTIC 0 UI_VERSION_MAJOR)
  list(GET UI_VERSION_SEMANTIC 1 UI_VERSION_MINOR)
  list(GET UI_VERSION_SEMANTIC 2 UI_VERSION_PATCH)

  if(INSTALLER_RUN AND NOT DEFINED ENV{obsInstallerTempDir})
    message(
      FATAL_ERROR
        "Environment variable obsInstallerTempDir is needed for multiarch installer generation"
    )
  endif()

  if(DEFINED ENV{DepsPath${_ARCH_SUFFIX}})
    set(DepsPath${_ARCH_SUFFIX} "$ENV{DepsPath${_ARCH_SUFFIX}}")
  elseif(DEFINED ENV{DepsPath})
    set(DepsPath "$ENV{DepsPath}")
  endif()

  if(DEFINED ENV{QTDIR${_ARCH_SUFFIX}})
    set(QTDIR${_ARCH_SUFFIX} "$ENV{QTDIR${_ARCH_SUFFIX}}")
  elseif(DEFINED ENV{QTDIR})
    set(QTDIR "$ENV{QTDIR}")
  endif()

  if(DEFINED DepsPath${_ARCH_SUFFIX})
    list(APPEND CMAKE_PREFIX_PATH "${DepsPath${_ARCH_SUFFIX}}"
         "${DepsPath${_ARCH_SUFFIX}}/bin")
  elseif(DEFINED DepsPath)
    list(APPEND CMAKE_PREFIX_PATH "${DepsPath}" "${DepsPath}/bin")
  endif()

  if(DEFINED QTDIR${_ARCH_SUFFIX})
    list(APPEND CMAKE_PREFIX_PATH "${QTDIR${_ARCH_SUFFIX}}")
  elseif(DEFINED QTDIR)
    list(APPEND CMAKE_PREFIX_PATH "${QTDIR}")
  endif()

  if(DEFINED ENV{VLCPath})
    set(VLCPath "$ENV{VLCPath}")
  endif()

  if(DEFINED VLCPath)
    set(VLC_PATH "${VLCPath}")
  endif()

  if(DEFINED ENV{CEF_ROOT_DIR})
    set(CEF_ROOT_DIR "$ENV{CEF_ROOT_DIR}")
  endif()

  if(DEFINED ENV{obsInstallerTempDir})
    file(TO_CMAKE_PATH "$ENV{obsInstallerTempDir}" ENV{obsInstallerTempDir})
  endif()

  if(DEFINED ENV{obsAdditionalInstallFiles})
    file(TO_CMAKE_PATH "$ENV{obsAdditionalInstallFiles}"
         ENV{obsAdditionalInstallFiles})
  else()
    set(ENV{obsAdditionalInstallFiles}
        "${CMAKE_SOURCE_DIR}/additional_install_files")
  endif()

  list(APPEND CMAKE_INCLUDE_PATH
       "$ENV{obsAdditionalInstallFiles}/include${_ARCH_SUFFIX}"
       "$ENV{obsAdditionalInstallFiles}/include")

  list(
    APPEND
    CMAKE_LIBRARY_PATH
    "$ENV{obsAdditionalInstallFiles}/lib${_ARCH_SUFFIX}"
    "$ENV{obsAdditionalInstallFiles}/lib"
    "$ENV{obsAdditionalInstallFiles}/libs${_ARCH_SUFFIX}"
    "$ENV{obsAdditionalInstallFiles}/libs"
    "$ENV{obsAdditionalInstallFiles}/bin${_ARCH_SUFFIX}"
    "$ENV{obsAdditionalInstallFiles}/bin")

  if(BUILD_FOR_DISTRIBUTION OR DEFINED ENV{CI})
    set_option(ENABLE_RTMPS ON)
    set_option(ENABLE_D3D12_HOOK ON)
  endif()
endmacro()
