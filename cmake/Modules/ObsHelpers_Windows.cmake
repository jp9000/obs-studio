# Helper function to set up runtime or library targets
function(setup_binary_target target)
  _setup_binary_target(${ARGV})

  if(DEFINED ENV{obsInstallerTempDir})
    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${target}>"
        "$ENV{obsInstallerTempDir}/$<IF:$<STREQUAL:$<TARGET_PROPERTY:${target},TYPE>,EXECUTABLE>,${OBS_EXECUTABLE_DESTINATION},${OBS_LIBRARY_DESTINATION}>/$<TARGET_FILE_NAME:${target}>"
      VERBATIM)
  endif()

  if(MSVC)
    setup_target_pdbs(${target} BINARY)
  endif()

  if(${target} STREQUAL "libobs")
    setup_libobs_target(${target})
  endif()
endfunction()

# Helper function to set up OBS plugin targets
function(setup_plugin_target target)
  _setup_plugin_target(${ARGV})

  if(MSVC)
    setup_target_pdbs(${target} PLUGIN)
  endif()

  if(DEFINED ENV{obsInstallerTempDir})
    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${target}>"
        "$ENV{obsInstallerTempDir}/${OBS_PLUGIN_DESTINATION}/$<TARGET_FILE_NAME:${target}>"
      VERBATIM)
  endif()
endfunction()

# Helper function to set up OBS scripting plugin targets
function(setup_script_plugin_target target)
  _setup_script_plugin_target(${ARGV})

  if(DEFINED ENV{obsInstallerTempDir})
    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${target}>"
        "$ENV{obsInstallerTempDir}/${OBS_SCRIPT_PLUGIN_DESTINATION}/$<TARGET_FILE_NAME:${target}>"
      VERBATIM)

    if(${target} STREQUAL "obspython" AND ${_ARCH_SUFFIX} EQUAL 64)
      add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND
          "${CMAKE_COMMAND}" -E copy
          "$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_BASE_NAME:${target}>.py"
          "$ENV{obsInstallerTempDir}/${OBS_SCRIPT_PLUGIN_DESTINATION}/$<TARGET_FILE_BASE_NAME:${target}>.py"
        VERBATIM)
    endif()
  endif()
endfunction()

# Helper function to set up target resources (e.g. L10N files)
function(setup_target_resources target destination)
  _setup_target_resources(${ARGV})

  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
    if(${_ARCH_SUFFIX} EQUAL 64 AND DEFINED ENV{obsInstallerTempDir})
      add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND
          "${CMAKE_COMMAND}" -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/data"
          "$ENV{obsInstallerTempDir}/${OBS_DATA_DESTINATION}/${destination}"
        VERBATIM)
    endif()
  endif()
endfunction()

# Helper function to set up specific resource files for targets
function(add_target_resource target resource destination)
  _add_target_resource(${ARGV})

  if(DEFINED ENV{obsInstallerTempDir})
    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory
              "$ENV{obsInstallerTempDir}/${OBS_DATA_DESTINATION}/${destination}"
      VERBATIM)

    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy "${resource}"
              "$ENV{obsInstallerTempDir}/${OBS_DATA_DESTINATION}/${destination}"
      VERBATIM)
  endif()
endfunction()

# Helper function to set up OBS app target
function(setup_obs_app target)
  # detect outdated obs-browser submodule
  if(NOT TARGET OBS::browser AND TARGET obs-browser)
    if(MSVC)
      target_compile_options(obs-browser PRIVATE $<IF:$<CONFIG:DEBUG>,/MTd,/MT>)

      target_compile_options(obs-browser-page
                             PRIVATE $<IF:$<CONFIG:DEBUG>,/MTd,/MT>)
    endif()

    target_link_options(obs-browser PRIVATE "LINKER:/IGNORE:4099")

    target_link_options(obs-browser-page PRIVATE "LINKER:/IGNORE:4099"
                        "LINKER:/SUBSYSTEM:WINDOWS")
  endif()

  _setup_obs_app(${ARGV})

  if(MSVC)
    include(CopyMSVCBins)
  endif()
endfunction()

# Helper function to do additional setup for browser source plugin
function(setup_target_browser target)
  install(DIRECTORY "${CEF_ROOT_DIR}/Resources/"
          DESTINATION "${OBS_PLUGIN_DESTINATION}")

  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CEF_ROOT_DIR}/Resources/"
            "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_PLUGIN_DESTINATION}"
    VERBATIM)

  if(DEFINED ENV{obsInstallerTempDir})
    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CEF_ROOT_DIR}/Resources/"
              "$ENV{obsInstallerTempDir}/${OBS_PLUGIN_DESTINATION}"
      VERBATIM)
  endif()

  set(_ADDITIONAL_BROWSER_FILES "libcef.dll" "libEGL.dll" "libGLESv2.dll" "snapshot_blob.bin" "v8_context_snapshot.bin" "natives_blob.bin")

  foreach(_ADDITIONAL_BROWSER_FILE IN LISTS _ADDITIONAL_BROWSER_FILES)
    if(EXISTS "${CEF_ROOT_DIR}/Release/${_ADDITIONAL_BROWSER_FILE}")
      install(FILES "${CEF_ROOT_DIR}/Release/${_ADDITIONAL_BROWSER_FILE}" DESTINATION "${OBS_PLUGIN_DESTINATION}/")

      add_custom_command(TARGET ${target} POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy "${CEF_ROOT_DIR}/Release/${_ADDITIONAL_BROWSER_FILE}"
        "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_PLUGIN_DESTINATION}/"
        VERBATIM)

      if(DEFINED ENV{obsInstallerTempDir})
        add_custom_command(
          TARGET ${target}
          POST_BUILD
          COMMAND "${CMAKE_COMMAND}" -E copy "${CEF_ROOT_DIR}/Release/${_ADDITIONAL_BROWSER_FILE}"
                  "$ENV{obsInstallerTempDir}/${OBS_PLUGIN_DESTINATION}/"
          VERBATIM)
      endif()
    endif()
  endforeach()
endfunction()

# Helper function to gather external libraries depended-on by libobs
function(setup_libobs_target target)
  set(_ADDITIONAL_FILES "${CMAKE_SOURCE_DIR}/additional_install_files")

  if(DEFINED ENV{obsAdditionalInstallFiles})
    set(_ADDITIONAL_FILES "$ENV{obsAdditionalInstallFiles}")
  endif()

  if(NOT INSTALLER_RUN)
    list(APPEND _LIBOBS_FIXUPS "misc:." "data:${OBS_DATA_DESTINATION}"
         "libs${_ARCH_SUFFIX}:${OBS_LIBRARY_DESTINATION}"
         "exec${_ARCH_SUFFIX}:${OBS_EXECUTABLE_DESTINATION}")
  else()
    list(
      APPEND
      _LIBOBS_FIXUPS
      "misc:."
      "data:${OBS_DATA_DESTINATION}"
      "libs32:${OBS_LIBRARY32_DESTINATION}"
      "libs64:${OBS_LIBRARY64_DESTINATION}"
      "exec32:${OBS_EXECUTABLE32_DESTINATION}"
      "exec64:${OBS_EXECUTABLE64_DESTINATION}")
  endif()

  foreach(_FIXUP IN LISTS _LIBOBS_FIXUPS)
    string(REPLACE ":" ";" _FIXUP ${_FIXUP})
    list(GET _FIXUP 0 _SOURCE)
    list(GET _FIXUP 1 _DESTINATION)

    install(
      DIRECTORY "${_ADDITIONAL_FILES}/${_SOURCE}/"
      DESTINATION "${_DESTINATION}"
      USE_SOURCE_PERMISSIONS
      PATTERN ".gitignore" EXCLUDE)

    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -E copy_directory "${_ADDITIONAL_FILES}/${_SOURCE}/"
        "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/${_DESTINATION}"
      VERBATIM)

    if(_SOURCE MATCHES "(libs|exec)(32|64)?")
      install(
        DIRECTORY "${_ADDITIONAL_FILES}/${_SOURCE}$<IF:$<CONFIG:Debug>,d,r>/"
        DESTINATION "${_DESTINATION}"
        USE_SOURCE_PERMISSIONS
        PATTERN ".gitignore" EXCLUDE)

      add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND
          "${CMAKE_COMMAND}" -E copy_directory
          "${_ADDITIONAL_FILES}/${_SOURCE}$<IF:$<CONFIG:Debug>,d,r>/"
          "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/${_DESTINATION}"
        VERBATIM)
    endif()
  endforeach()
endfunction()

# Helper function to copy PDB files if available (binary/library targets only)
function(setup_target_pdbs target destination)
  if(destination STREQUAL "BINARY")
    set(_INSTALL_LOCATION "${OBS_EXECUTABLE_DESTINATION}")
  elseif(destination STREQUAL "PLUGIN")
    set(_INSTALL_LOCATION "${OBS_PLUGIN_DESTINATION}")
  endif()

  list(APPEND _PDB_TARGET_DIRS "${CMAKE_CURRENT_BINARY_DIR}/pdbs"
       "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/${_INSTALL_LOCATION}")
  if(DEFINED ENV{obsInstallerTempDir})
    list(APPEND _PDB_TARGET_DIRS
         "$ENV{obsInstallerTempDir}/${_INSTALL_LOCATION}")
  endif()

  foreach(_PDB_TARGET_DIR IN LISTS _PDB_TARGET_DIRS)
    copy_target_pdb(${target} "${_PDB_TARGET_DIR}")
  endforeach()

  install(
    DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pdbs/"
    DESTINATION "${_INSTALL_LOCATION}"
    CONFIGURATIONS Debug RelWithDebInfo)
endfunction()

# Helper function to check for and copy available PDB files
function(copy_target_pdb target destination)
  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -E
      "$<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>,make_directory,true>"
      "${destination}"
    VERBATIM)

  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -E
      "$<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>,copy_if_different,true>"
      "$<TARGET_PDB_FILE:${target}>" "${destination}/"
    VERBATIM)
endfunction()

# Helper function to compile artifacts for multi-architecture installer
function(generate_multiarch_installer)
  if(NOT DEFINED ENV{obsInstallerTempDir})
    message(
      FATAL_ERROR
        "Function generate_multiarch_installer requires environment variable 'obsInstallerTempDir' to be set"
    )
  endif()

  add_custom_target(installer_files ALL)

  setup_libobs_target(installer_files)

  install(
    DIRECTORY "$ENV{obsInstallerTempDir}/"
    DESTINATION "."
    USE_SOURCE_PERMISSIONS)
endfunction()
