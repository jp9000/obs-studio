if(POLICY CMP0087)
  cmake_policy(SET CMP0087 NEW)
endif()

set(OBS_STANDALONE_PLUGIN_DIR "${CMAKE_SOURCE_DIR}/release")
set(INCLUDED_LIBOBS_CMAKE_MODULES ON)

include(GNUInstallDirs)
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
  set(OS_MACOS ON)
  set(OS_POSIX ON)
elseif("${CMAKE_SYSTEM_NAME}" MATCHES "Linux|FreeBSD|OpenBSD")
  set(OS_POSIX ON)
  string(TOUPPER "${CMAKE_SYSTEM_NAME}" _SYSTEM_NAME_U)
  set(OS_${_SYSTEM_NAME_U} ON)
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  set(OS_WINDOWS ON)
  set(OS_POSIX OFF)
endif()

# Old-Style plugin detected, find "modern" libobs variant instead and set global
# include directories to fix "bad" plugin behavior
if(DEFINED LIBOBS_INCLUDE_DIR AND NOT TARGET OBS::libobs)
  message(
    DEPRECATION
      "You are using an outdated method of adding 'libobs' to your project. Refer to the updated wiki on how to build and export 'libobs' and use it in your plugin projects."
  )
  find_package(libobs REQUIRED)
  if(TARGET OBS::libobs)
    set_target_properties(OBS::libobs PROPERTIES IMPORTED_GLOBAL TRUE)
    message(STATUS "OBS: Using modern libobs target")

    add_library(libobs ALIAS OBS::libobs)
    if(OS_WINDOWS)
      add_library(w32-pthreads ALIAS OBS::w32-pthreads)
    endif()
  endif()
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND OS_WINDOWS)
  set(CMAKE_INSTALL_PREFIX
      "${OBS_STANDALONE_PLUGIN_DIR}"
      CACHE STRING "Directory to install OBS plugin after building" FORCE)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      "RelWithDebInfo"
      CACHE STRING
            "OBS build type [Release, RelWithDebInfo, Debug, MinSizeRel]" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Release RelWithDebInfo
                                               Debug MinSizeRel)
endif()

file(RELATIVE_PATH RELATIVE_INSTALL_PATH "${CMAKE_SOURCE_DIR}"
     "${CMAKE_INSTALL_PREFIX}")
file(RELATIVE_PATH RELATIVE_BUILD_PATH "${CMAKE_SOURCE_DIR}"
     "${CMAKE_BINARY_DIR}")

# Set up OS-specific environment and helper functions
if(OS_MACOS)
  if(NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES
        "x86_64"
        CACHE
          STRING
          "OBS plugin build architecture for macOS - x86_64 required at least"
          FORCE)
  endif()

  if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET
        "10.13"
        CACHE STRING "OBS plugin deployment target for macOS - 10.13+ required"
              FORCE)
  endif()

  if(NOT DEFINED OBS_CODESIGN_LINKER)
    set(OBS_CODESIGN_LINKER
        ON
        CACHE BOOL "Enable linker code-signing on macOS (macOS 11+ required" FORCE)
  endif()

  if(NOT DEFINED OBS_BUNDLE_CODESIGN_IDENTITY)
    set(OBS_BUNDLE_CODESIGN_IDENTITY
        "-"
        CACHE STRING "Codesign identity for macOS" FORCE)
  endif()

  # Xcode configuration
  if(XCODE)
    # Tell Xcode to pretend the linker signed binaries so that editing with
    # install_name_tool preserves ad-hoc signatures. This option is supported
    # by `codesign` on macOS 11 or higher.
    #
    # See CMake Issue 21854:
    # (https://gitlab.kitware.com/cmake/cmake/-/issues/21854).

    set(CMAKE_XCODE_GENERATE_SCHEME ON)
    if(OBS_CODESIGN_LINKER)
      set(CMAKE_XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "-o linker-signed")
    endif()
  endif()

  # Set default options for bundling on macOS
  set(CMAKE_MACOSX_RPATH ON)
  set(CMAKE_SKIP_BUILD_RPATH OFF)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
  set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks/")
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH OFF)

  function(setup_plugin_target target)
    if(NOT DEFINED MACOSX_PLUGIN_GUI_IDENTIFIER)
      message(
        FATAL_ERROR
          "No 'MACOSX_PLUGIN_GUI_IDENTIFIER' set, but is required to build plugin bundles on macOS - example: 'com.yourname.pluginname'"
      )
    endif()

    if(NOT DEFINED MACOSX_PLUGIN_BUNDLE_VERSION)
      message(
        FATAL_ERROR
          "No 'MACOSX_PLUGIN_BUNDLE_VERSION' set, but is required to build plugin bundles on macOS - example: '25'"
      )
    endif()

    if(NOT DEFINED MACOSX_PLUGIN_SHORT_VERSION_STRING)
      message(
        FATAL_ERROR
          "No 'MACOSX_PLUGIN_SHORT_VERSION_STRING' set, but is required to build plugin bundles on macOS - example: '1.0.2'"
      )
    endif()

    set(MACOSX_PLUGIN_BUNDLE_NAME
        "${target}"
        PARENT_SCOPE)
    set(MACOSX_PLUGIN_BUNDLE_VERSION
        "${MACOSX_BUNDLE_BUNDLE_VERSION}"
        PARENT_SCOPE)
    set(MACOSX_PLUGIN_SHORT_VERSION_STRING
        "${MACOSX_BUNDLE_SHORT_VERSION_STRING}"
        PARENT_SCOPE)
    set(MACOSX_PLUGIN_EXECUTABLE_NAME
        "${target}"
        PARENT_SCOPE)
    set(MACOSX_PLUGIN_BUNDLE_TYPE
        "BNDL"
        PARENT_SCOPE)

    install(
      TARGETS ${target}
      LIBRARY DESTINATION "."
              COMPONENT obs_plugins
              NAMELINK_COMPONENT ${target}_Development)

    install(
      CODE "execute_process(COMMAND /bin/sh -c \"${CMAKE_INSTALL_NAME_TOOL} \\
			-change ${CMAKE_PREFIX_PATH}/lib/QtWidgets.framework/Versions/5/QtWidgets @rpath/QtWidgets.framework/Versions/5/QtWidgets \\
			-change ${CMAKE_PREFIX_PATH}/lib/QtCore.framework/Versions/5/QtCore @rpath/QtCore.framework/Versions/5/QtCore \\
			-change ${CMAKE_PREFIX_PATH}/lib/QtGui.framework/Versions/5/QtGui @rpath/QtGui.framework/Versions/5/QtGui \\
			${CMAKE_INSTALL_PREFIX}/${target}.plugin/Contents/MacOS/${target}\")")

    if(NOT XCODE)
      install(
        CODE "execute_process(COMMAND /bin/sh -c \"/usr/bin/codesign --force --sign \\\"${OBS_BUNDLE_CODESIGN_IDENTITY}\\\" --options runtime --entitlements ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../bundle/macOS/entitlements.plist ${CMAKE_INSTALL_PREFIX}/${target}.plugin\")"
      )
    endif()

    set_target_properties(
      ${target}
      PROPERTIES
        BUNDLE ON
        BUNDLE_EXTENSION "plugin"
        OUTPUT_NAME ${target}
        MACOSX_BUNDLE_INFO_PLIST
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../bundle/macOS/Plugin-Info.plist.in"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER
        "${MACOSX_PLUGIN_GUI_IDENTIFIER}"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${OBS_BUNDLE_CODESIGN_IDENTITY}"
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../bundle/macOS/entitlements.plist")

    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND
        /bin/sh -c
        "codesign --force --sign \"-\" $<$<BOOL:${OBS_CODESIGN_LINKER}>:--options linker-signed >\"$<TARGET_BUNDLE_DIR:${target}>\""
      COMMENT "Codesigning ${target}"
      VERBATIM)

    install_bundle_resources(${target})
  endfunction()

  function(install_bundle_resources target)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
      file(GLOB_RECURSE _DATA_FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/*")
      foreach(_DATA_FILE IN LISTS _DATA_FILES)
        file(RELATIVE_PATH _RELATIVE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/data/
             ${_DATA_FILE})
        get_filename_component(_RELATIVE_PATH "${_RELATIVE_PATH}" PATH)
        target_sources(${target} PRIVATE ${_DATA_FILE})
        set_source_files_properties(
          ${_DATA_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION
                                   "Resources/${_RELATIVE_PATH}")
        string(REPLACE "\\" "\\\\" _GROUP_NAME "${_RELATIVE_PATH}")
        source_group("Resources\\${_GROUP_NAME}" FILES ${_DATA_FILE})
      endforeach()
    endif()
  endfunction()
else()
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_ARCH_SUFFIX 64)
  else()
    set(_ARCH_SUFFIX 32)
  endif()
  set(OBS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/rundir")

  if(OS_POSIX)
    if(NOT LINUX_PORTABLE)
      set(OBS_LIBRARY_DESTINATION "${CMAKE_INSTALL_LIBDIR}")
      set(OBS_PLUGIN_DESTINATION "${OBS_LIBRARY_DESTINATION}/obs-plugins")
      set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
      set(OBS_DATA_DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/obs")
    else()
      set(OBS_LIBRARY_DESTINATION "bin/${_ARCH_SUFFIX}bit")
      set(OBS_PLUGIN_DESTINATION "obs-plugins/${_ARCH_SUFFIX}bit")
      set(CMAKE_INSTALL_RPATH
          "$ORIGIN/" "${CMAKE_INSTALL_PREFIX}/${OBS_LIBRARY_DESTINATION}")
      set(OBS_DATA_DESTINATION "data")
    endif()

    if(OS_LINUX)
      set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
      set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${LINUX_MAINTAINER_EMAIL}")
      set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")

      set(CPACK_GENERATOR "DEB")
      set(CPACK_DEBIAN_PACKAGE_DEPENDS
          "obs-studio (>= 27.0.0), libqt5core5a (>= 5.9.0~beta), \
				libqt5gui5 (>= 5.3.0), libqt5widgets5 (>= 5.7.0)")

      if(NOT LINUX_PORTABLE)
        set(CPACK_SET_DESTDIR ON)
      endif()
      include(CPack)
    endif()
  else()
    set(OBS_LIBRARY_DESTINATION "bin/${_ARCH_SUFFIX}bit")
    set(OBS_LIBRARY32_DESTINATION "bin/32bit")
    set(OBS_LIBRARY64_DESTINATION "bin/64bit")
    set(OBS_PLUGIN_DESTINATION "obs-plugins/${_ARCH_SUFFIX}bit")
    set(OBS_PLUGIN32_DESTINATION "obs-plugins/32bit")
    set(OBS_PLUGIN64_DESTINATION "obs-plugins/64bit")

    set(OBS_DATA_DESTINATION "data")
  endif()

  function(setup_plugin_target target)
    set_target_properties(${target} PROPERTIES PREFIX "")

    install(
      TARGETS ${target}
      RUNTIME DESTINATION "${OBS_PLUGIN_DESTINATION}"
              COMPONENT ${target}_Runtime
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

    if(OS_WINDOWS)
      add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND
          "${CMAKE_COMMAND}" -E
          "$<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>,copy,true>"
          "$<TARGET_PDB_FILE:${target}>"
          "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_PLUGIN_DESTINATION}/"
        VERBATIM)
    endif()

    if(MSVC)
      target_link_options(
        ${target}
        PRIVATE
        "LINKER:/OPT:REF"
        "$<$<NOT:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>>:LINKER\:/SAFESEH\:NO>"
        "$<$<CONFIG:DEBUG>:LINKER\:/INCREMENTAL:NO>"
        "$<$<CONFIG:RELWITHDEBINFO>:LINKER\:/INCREMENTAL:NO>")
    endif()

    setup_target_resources("${target}" "obs-plugins/${target}")

    if(OS_WINDOWS AND DEFINED OBS_BUILD_DIR)
      setup_target_for_testing(${target})
    endif()
  endfunction()

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
          "${CMAKE_COMMAND}" -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/data"
          "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_DATA_DESTINATION}/${destination}"
        VERBATIM)
    endif()
  endfunction()

  if(OS_WINDOWS)
    function(setup_target_for_testing target)
      add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND
          "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${target}>"
          "${OBS_BUILD_DIR}/rundir/$<CONFIG>/${OBS_PLUGIN_DESTINATION}/$<TARGET_FILE_NAME:${target}>"
        COMMAND
          "${CMAKE_COMMAND}" -E
          "$<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>,copy,true>"
          "$<TARGET_PDB_FILE:${target}>"
          "${OBS_BUILD_DIR}/rundir/$<CONFIG>/${OBS_PLUGIN_DESTINATION}/$<TARGET_FILE_NAME:${target}>"
        COMMAND
          "${CMAKE_COMMAND}" -E make_directory
          "${OBS_BUILD_DIR}/rundir/$<CONFIG>/${OBS_DATA_DESTINATION}/obs-plugins/${target}"
        COMMAND
          "${CMAKE_COMMAND}" -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/data"
          "${OBS_BUILD_DIR}/rundir/$<CONFIG>/${OBS_DATA_DESTINATION}/obs-plugins/${target}"
        VERBATIM)
    endfunction()
  endif()
endif()
