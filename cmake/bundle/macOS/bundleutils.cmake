if(POLICY CMP0009)
  cmake_policy(SET CMP0009 NEW)
endif()
if(POLICY CMP0011)
  cmake_policy(SET CMP0011 NEW)
endif()

# Add additional search paths for dylibbundler
list(APPEND _FIXUP_BUNDLES "-s \"${CMAKE_INSTALL_PREFIX}/lib\"")
list(APPEND _FIXUP_BUNDLES "-s \"${CMAKE_INSTALL_PREFIX}/Frameworks\"")

# Unlinked modules need to be supplied manually to dylibbundler

# Find all modules (plugin and standalone)
file(GLOB _OBS_PLUGINS
     "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/PlugIns/*.plugin")
file(GLOB _OBS_SCRIPTING_PLUGINS
     "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/PlugIns/*.so")

# Add modules to fixups
foreach(_OBS_PLUGIN IN LISTS _OBS_PLUGINS)
  get_filename_component(PLUGIN_NAME "${_OBS_PLUGIN}" NAME_WLE)
  list(APPEND _FIXUP_BUNDLES
       "-x \"${_OBS_PLUGIN}/Contents/MacOS/${PLUGIN_NAME}\"")
endforeach()

if(EXISTS
   "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/MacOS/obs-ffmpeg-mux")
  list(
    APPEND
    _FIXUP_BUNDLES
    "-x \"${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/MacOS/obs-ffmpeg-mux\""
  )
endif()

# Add scripting modules to fixups
foreach(_OBS_PLUGIN IN LISTS _OBS_SCRIPTING_PLUGINS)
  list(APPEND _FIXUP_BUNDLES "-x \"${_OBS_PLUGIN}\"")
endforeach()

# Run dylibbbundler
if(DEFINED ENV{VERBOSE})
  set(_VERBOSE_FLAG "--verbose")
endif()

if(DEFINED ENV{QUIET})
  set(_QUIET_FLAG "OUTPUT_QUIET")
endif()

list(REMOVE_DUPLICATES _FIXUP_BUNDLES)
string(REPLACE ";" " " _FIXUP_BUNDLES "${_FIXUP_BUNDLES}")
message(STATUS "OBS: Bundle linked libraries and dependencies")
execute_process(
  COMMAND
    /bin/sh -c
    "${_BUNDLER_COMMAND} -a \"${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}\" -cd -of -q -f ${_FIXUP_BUNDLES} ${_VERBOSE_FLAG}"
    ${_QUIET_FLAG})

# Find all dylibs, frameworks and other code elements inside bundle
file(GLOB _OTHER_BINARIES
     "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/Frameworks/*.app")
file(GLOB _DYLIBS
     "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/Frameworks/*.dylib")
file(GLOB _FRAMEWORKS
     "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/Frameworks/*.framework")
file(GLOB_RECURSE _QT_PLUGINS
     "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/PlugIns/*.dylib")

if(EXISTS
   "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/MacOS/obs-ffmpeg-mux")
  list(APPEND _OTHER_BINARIES
       "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/MacOS/obs-ffmpeg-mux")
endif()

if(EXISTS
   "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/PlugIns/obspython.py")
  list(APPEND _OTHER_BINARIES
       "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/PlugIns/obspython.py")
endif()

if(EXISTS
   "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/Resources/obs-mac-virtualcam.plugin"
)
  list(
    APPEND
    _OTHER_BINARIES
    "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/Resources/obs-mac-virtualcam.plugin"
  )
endif()

# Create libobs symlink for legacy plugin support
execute_process(
  COMMAND
    /bin/sh -c
    "cd \"${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/Frameworks\" && ln -fs libobs.framework/Versions/Current/libobs libobs.0.dylib && ln -fsv libobs.framework/Versions/Current/libobs libobs.dylib"
    ${_QUIET_FLAG})

# Python potentially leaves __pycache__ directories inside the bundle which will
# break codesigning
if(EXISTS "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/PlugIns/__pycache__")
  file(REMOVE_RECURSE
       "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/PlugIns/__pycache__")
endif()

# Codesign all binaries inside-out
message(STATUS "OBS: Codesign dependencies")
if(EXISTS
   "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/Frameworks/Sparkle.framework"
)
  execute_process(
    COMMAND
      /usr/bin/codesign --remove-signature
      "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/Frameworks/Sparkle.framework/Versions/A/Resources/Autoupdate.app"
      ${_VERBOSE_FLAG} ${_QUIET_FLAG})
  execute_process(
    COMMAND
      /usr/bin/codesign --force --sign "${_CODESIGN_IDENTITY}" --deep --options
      runtime
      "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/Frameworks/Sparkle.framework/Versions/A/Resources/Autoupdate.app"
      ${_VERBOSE_FLAG} ${_QUIET_FLAG})
endif()
foreach(_DEPENDENCY IN LISTS _OTHER_BINARIES _DYLIBS _FRAMEWORKS _OBS_PLUGINS
                             _OBS_SCRIPTING_PLUGINS _QT_PLUGINS)
  if(NOT IS_SYMLINK "${_DEPENDENCY}")
    execute_process(COMMAND /usr/bin/codesign --remove-signature
                            "${_DEPENDENCY}" ${_VERBOSE_FLAG} ${_QUIET_FLAG})
    execute_process(
      COMMAND
        /usr/bin/codesign --force --sign "${_CODESIGN_IDENTITY}" --options
        runtime --entitlements "${_CODESIGN_ENTITLEMENTS}" "${_DEPENDENCY}"
        ${_VERBOSE_FLAG} ${_QUIET_FLAG})
  endif()
endforeach()

# Codesign main app
message(STATUS "OBS: Codesign main app")
execute_process(
  COMMAND
    /usr/bin/codesign --remove-signature
    "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}" ${_VERBOSE_FLAG} ${_QUIET_FLAG})
execute_process(
  COMMAND
    /usr/bin/codesign --force --sign "${_CODESIGN_IDENTITY}" --options runtime
    --entitlements "${_CODESIGN_ENTITLEMENTS}"
    "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}" ${_VERBOSE_FLAG} ${_QUIET_FLAG})
