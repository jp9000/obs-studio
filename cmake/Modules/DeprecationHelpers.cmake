function(upgrade_cmake_vars)
  if(DEFINED BROWSER_LEGACY)
    set(ENABLE_BROWSER_LEGACY "${BROWSER_LEGACY}" CACHE BOOL "" FORCE)
  endif()

  if(DEFINED BROWSER_PANEL_SUPPORT_ENABLED)
    set(ENABLE_BROWSER_PANELS "${BROWSER_PANEL_SUPPORT_ENABLED}" CACHE BOOL "" FORCE)
  endif()

  if(DEFINED BUILD_BROWSER)
    set(ENABLE_BROWSER_SOURCE "${BUILD_BROWSER}" CACHE BOOL "" FORCE)
  endif()

  if(DEFINED BUILD_CA_ENCODER)
    set(ENABLE_COREAUDIO_ENCODER "${BUILD_CA_ENCODER}" CACHE BOOL "" FORCE)
  endif()

  if(DEFINED BUILD_VST)
    set(ENABLE_VST "${BUILD_VST}" CACHE BOOL "" FORCE)
  endif()

  if(DEFINED CHECK_FOR_SERVICE_UPDATES)
    set(ENABLE_SERVICE_UPDATES "${CHECK_FOR_SERVICE_UPDATES}" CACHE BOOL "" FORCE)
  endif()

  if(DEFINED DEBUG_FFMPEG_MUX)
    set(ENABLE_FFMPEG_MUX_DEBUG "${DEBUG_FFMPEG_MUX}" CACHE BOOL "" FORCE)
  endif()

  if(DEFINED DISABLE_IVCAM)
    if(DISABLE_IVCAM)
      set(ENABLE_IVCAM OFF CACHE BOOL "" FORCE)
    else()
      set(ENABLE_IVCAM ON CACHE BOOL "" FORCE)
    endif()
  endif()

  if(DEFINED DISABLE_PLUGINS)
    if(DISABLE_PLUGINS)
      set(ENABLE_PLUGINS OFF CACHE BOOL "" FORCE)
    else()
      set(ENABLE_PLUGINS ON CACHE BOOL "" FORCE)
    endif()
  endif()

  if(DEFINED DISABLE_PYTHON)
    if(DISABLE_PYTHON)
      set(ENABLE_SCRIPTING_PYTHON OFF CACHE BOOL "" FORCE)
    else()
      set(ENABLE_SCRIPTING_PYTHON ON CACHE BOOL "" FORCE)
    endif()
  endif()

  if(DEFINED DISABLE_LUA)
    if(DISABLE_LUA)
      set(ENABLE_SCRIPTING_LUA OFF CACHE BOOL "" FORCE)
    else()
      set(ENABLE_SCRIPTING_LUA ON CACHE BOOL "" FORCE)
    endif()
  endif()

  if(DEFINED DISABLE_SPEEXDSP)
    if(DISABLE_SPEEXDSP)
      set(ENABLE_SPEEXDSP OFF CACHE BOOL "" FORCE)
    else()
      set(ENABLE_SPEEXDSP ON CACHE BOOL "" FORCE)
    endif()
  endif()

  if(DEFINED DISABLE_UPDATE_MODULE)
    if(DISABLE_UPDATE_MODULE)
      set(ENABLE_UPDATER OFF CACHE BOOL "" FORCE)
    else()
      set(ENABLE_UPDATER ON CACHE BOOL "" FORCE)
    endif()
  endif()

  if(DEFINED SHARED_TEXTURE_SUPPORT_ENABLED)
    set(ENABLE_BROWSER_SHARED_TEXTURE "${SHARED_TEXTURE_SUPPORT_ENABLED}" CACHE BOOL "" FORCE)
  endif()

  if(DEFINED STATIC_MBEDTLS)
    set(ENABLE_STATIC_MBEDTLS "${STATIC_MBEDTLS}" CACHE BOOL "" FORCE)
  endif()

  if(DEFINED UNIX_STRUCTURE AND UNIX_STRUCTURE)
    set(LINUX_PORTABLE OFF CACHE BOOL "" FORCE)
  endif()

  if(DEFINED USE_QT_LOOP)
    set(ENABLE_BROWSER_QT_LOOP "${USE_QT_LOOP}" CACHE BOOL "" FORCE)
  endif()

  if(DEFINED WITH_RTMPS)
    set(ENABLE_RTMPS "${WITH_RTMPS}" CACHE STRING "" FORCE)
  endif()
endfunction()

function(install_obs_plugin_with_data)
  message(
    DEPRECATION
      "OBS: The install_obs_plugin_with_data command is deprecated and will be removed soon. Use 'setup_plugin_target' instead."
  )
  _install_obs_plugin_with_data(${ARGV})
endfunction()

function(install_obs_plugin)
  message(
    DEPRECATION
      "OBS: The install_obs_plugin command is deprecated and will be removed soon. Use 'setup_plugin_target' instead."
  )
  _install_obs_plugin(${ARGV})
endfunction()

function(install_obs_datatarget)
  message(
    DEPRECATION
      "OBS: The install_obs_datatarget function is deprecated and will be removed soon. Use 'setup_target_resources' instead."
  )
  _install_obs_datatarget(${ARGV})
endfunction()

function(__deprecated_var var access)
  if(access STREQUAL "READ_ACCESS")
    message(DEPRECATION "OBS: The variable '${var}' is deprecated!")
  endif()
endfunction()

function(__deprecated_feature var access)
  if(access STREQUAL "UNKNOWN_READ_ACCESS")
    message(
      DEPRECATION
        "OBS: The feature enabled by '${var}' is deprecated and will soon be removed from OBS."
    )
  endif()
endfunction()

set(_DEPRECATED_VARS
    zlibPath
    vulkanPath
    SwigPath
    PythonPath
    mbedtlsPath
    LuajitPath
    x264Path
    VlcPath
    VLCPath
    speexPath
    rnnoisePath
    LibfdkPath
    curlPath
    JanssonPath
    FFmpegPath
    DepsPath
    DepsPath32
    DepsPath64
    QTDIR32
    QTDIR64
    DISABLE_UI
    UI_ENABLED
    UNIX_STRUCTURE
    UPDATE_SPARKLE
    LIBOBS_PREFER_IMAGEMAGICK
    DEBUG_FFMPEG_MUX
    ENABLE_WINMF
    USE_QT_LOOP
    SHARED_TEXTURE_SUPPORT_ENABLED
    BROWSER_PANEL_SUPPORT_ENABLED
    BROWSER_LEGACY
    BUILD_BROWSER
    BUILD_CAPTIONS
    BUILD_CA_ENCODER
    BUILD_VST
    CHECK_FOR_SERVICE_UPDATES
    DISABLE_IVCAM
    DISABLE_LUA
    DISABLE_PLUGINS
    DISABLE_PYTHON
    DISABLE_SPEEXDSP
    DISABLE_UPDATE_MODULE
    SHARED_TEXTURE_SUPPORT_ENABLED
    STATIC_MBEDTLS
    UNIX_STRUCTURE
    USE_QT_LOOP
    WITH_RTMPS)

foreach(_DEPRECATED_VAR ${_DEPRECATED_VARS})
  variable_watch(_DEPRECATED_VAR __deprecated_var)
endforeach()

variable_watch(FTL_FOUND __deprecated_feature)

# Upgrade pre-existing build variables to their new variants as best as possible
upgrade_cmake_vars()
