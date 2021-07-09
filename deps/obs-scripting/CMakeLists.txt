option(ENABLE_SCRIPTING_LUA "Enable Lua scripting support" ON)
option(ENABLE_SCRIPTING_PYTHON "Enable Python scripting support" ON)

if(NOT ENABLE_SCRIPTING)
  message(STATUS "OBS: Scripting plugin disabled")
  return()
endif()

project(obs-scripting)

if(ENABLE_SCRIPTING_LUA)
  add_subdirectory(obslua)
  find_package(Luajit)

  if(NOT TARGET Luajit::Luajit)
    message(FATAL_ERROR "OBS: Luajit not found - support disabled")
    return()
  else()
    message(STATUS "OBS: Luajit found - support enabled")
  endif()
else()
  message(STATUS "OBS: Luajit support disabled")
endif()

if(ENABLE_SCRIPTING_PYTHON)
  add_subdirectory(obspython)
  if(OS_WINDOWS)
    find_package(PythonWindows)
  else()
    find_package(Python COMPONENTS Interpreter Development)
  endif()

  if(NOT TARGET Python::Python)
    message(FATAL_ERROR "OBS: Python not found - support disabled")
    return()
  else()
    message(STATUS "OBS: Python ${Python_VERSION} found - support enabled")
  endif()
else()
  message(STATUS "OBS: Python support disabled")
endif()

if(NOT TARGET Luajit::Luajit AND NOT TARGET Python::Python)
  message(
    WARNING "OBS: No supported scripting libraries found; plugin disabled")
  return()
endif()

if(OS_MACOS)
  find_package(SWIG 4 REQUIRED)
elseif(OS_POSIX)
  find_package(SWIG 3 REQUIRED)
elseif(OS_WINDOWS)
  find_package(SwigWindows 3 REQUIRED)
endif()

add_library(obs-scripting SHARED)
add_library(OBS::scripting ALIAS obs-scripting)

target_sources(
  obs-scripting
  PUBLIC obs-scripting.h
  PRIVATE obs-scripting.c cstrcache.cpp cstrcache.h obs-scripting-logging.c
          obs-scripting-callback.h)

target_link_libraries(obs-scripting PRIVATE OBS::libobs)

target_compile_features(obs-scripting PRIVATE cxx_auto_type)

target_include_directories(
  obs-scripting PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
                       "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/config>")

if(OS_WINDOWS)
  set(MODULE_DESCRIPTION "OBS Studio scripting module")
  configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
                 obs-scripting.rc)

  target_sources(obs-scripting PRIVATE obs-scripting.rc)

  target_link_libraries(obs-scripting PRIVATE OBS::w32-pthreads)

elseif(OS_MACOS)
  target_link_libraries(obs-scripting PRIVATE objc)
endif()

set_target_properties(
  obs-scripting
  PROPERTIES FOLDER "scripting"
             VERSION "${OBS_VERSION_MAJOR}"
             SOVERSION "1")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/swig)

if(TARGET Luajit::Luajit)
  add_custom_command(
    OUTPUT swig/swigluarun.h
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PRE_BUILD
    COMMAND ${SWIG_EXECUTABLE} -lua -external-runtime swig/swigluarun.h
    COMMENT "obs-scripting - generating Luajit SWIG interface headers")

  set_source_files_properties(swig/swigluarun.h PROPERTIES GENERATED ON)

  target_link_libraries(obs-scripting PRIVATE Luajit::Luajit)

  target_sources(
    obs-scripting
    PRIVATE obs-scripting-lua.c obs-scripting-lua.h obs-scripting-lua-source.c
            ${CMAKE_CURRENT_BINARY_DIR}/swig/swigluarun.h)

  target_include_directories(
    obs-scripting PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")

  if(ENABLE_UI)
    target_link_libraries(obs-scripting PRIVATE OBS::frontend-api)

    target_sources(obs-scripting PRIVATE obs-scripting-lua-frontend.c)

    target_compile_definitions(obs-scripting PRIVATE UI_ENABLED=ON)
  endif()

endif()

if(TARGET Python::Python)
  add_custom_command(
    OUTPUT swig/swigpyrun.h
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PRE_BUILD
    COMMAND ${SWIG_EXECUTABLE} -python -external-runtime swig/swigpyrun.h
    COMMENT "obs-scripting - generating Python 3 SWIG interface headers")

  set_source_files_properties(swig/swigpyrun.h PROPERTIES GENERATED ON)

  target_sources(
    obs-scripting
    PRIVATE obs-scripting-python.c obs-scripting-python.h
            obs-scripting-python-import.h
            ${CMAKE_CURRENT_BINARY_DIR}/swig/swigpyrun.h)

  target_include_directories(
    obs-scripting PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")

  get_filename_component(_PYTHON_PATH "${Python_LIBRARIES}" PATH)
  get_filename_component(_PYTHON_FILE "${Python_LIBRARIES}" NAME)

  string(REGEX REPLACE "\\.[^.]*$" "" _PYTHON_FILE ${_PYTHON_FILE})

  if(OS_WINDOWS)
    string(REGEX REPLACE "_d" "" _PYTHON_FILE ${_PYTHON_FILE})
  endif()
  set(OBS_SCRIPT_PYTHON_PATH "${_PYTHON_FILE}")

  unset(_PYTHON_FILE)
  unset(_PYTHON_PATH)

  if(OS_WINDOWS OR OS_MACOS)
    target_include_directories(obs-scripting PRIVATE ${Python_INCLUDE_DIRS})

    target_sources(obs-scripting PRIVATE obs-scripting-python-import.c)
  else()
    target_link_libraries(obs-scripting PRIVATE Python::Python)
  endif()

  if(ENABLE_UI)
    target_link_libraries(obs-scripting PRIVATE OBS::frontend-api)

    target_sources(obs-scripting PRIVATE obs-scripting-python-frontend.c)

    target_compile_definitions(obs-scripting PRIVATE UI_ENABLED=ON)
  endif()
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/obs-scripting-config.h.in
               ${CMAKE_BINARY_DIR}/config/obs-scripting-config.h)

target_sources(obs-scripting
               PUBLIC ${CMAKE_BINARY_DIR}/config/obs-scripting-config.h)

setup_binary_target(obs-scripting)
