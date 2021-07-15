if(POLICY CMP0078)
  cmake_policy(SET CMP0078 NEW)
endif()

if(POLICY CMP0086)
  cmake_policy(SET CMP0086 NEW)
endif()

project(obspython)

if(OS_MACOS)
  find_package(Python REQUIRED COMPONENTS Interpreter Development)
  find_package(SWIG 4 REQUIRED)
elseif(OS_POSIX)
  find_package(Python REQUIRED COMPONENTS Interpreter Development)
  find_package(SWIG 3 REQUIRED)
elseif(OS_WINDOWS)
  find_package(PythonWindows REQUIRED)
  find_package(SwigWindows 3 REQUIRED)
endif()
include(UseSWIG)

set_source_files_properties(
  obspython.i PROPERTIES USE_TARGET_INCLUDE_DIRECTORIES TRUE SWIG_FLAGS
                                                             "-builtin;-py3")

swig_add_library(
  obspython
  LANGUAGE python
  TYPE MODULE
  SOURCES obspython.i ../cstrcache.cpp ../cstrcache.h)

target_link_libraries(obspython PRIVATE OBS::scripting OBS::libobs)

set_target_properties(
  obspython
  PROPERTIES
    SWIG_COMPILE_DEFINITIONS
    "SWIG_TYPE_TABLE=obspython;Py_ENABLE_SHARED=1;SWIG_PYTHON_INTERPRETER_NO_DEBUG"
)

target_compile_features(obspython PRIVATE cxx_auto_type c_std_11)

target_compile_definitions(
  obspython PRIVATE SWIG_TYPE_TABLE=obspython Py_ENABLE_SHARED=1
                    SWIG_PYTHON_INTERPRETER_NO_DEBUG)

if(ENABLE_UI)
  target_link_libraries(obspython PRIVATE OBS::frontend-api)

  target_compile_definitions(obspython PRIVATE ENABLE_UI)
endif()

if(OS_WINDOWS)
  set_target_properties(
    obspython
    PROPERTIES
      SWIG_COMPILE_DEFINITIONS
      "SWIG_TYPE_TABLE=obspython;Py_ENABLE_SHARED=1;SWIG_PYTHON_INTERPRETER_NO_DEBUG;MS_NO_COREDLL"
  )

  target_link_libraries(obspython PRIVATE Python::Python)

  target_compile_options(obspython PRIVATE /wd4100)

  if(MSVC)
    add_custom_command(
      TARGET obspython
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_BINARY_DIR}/obspython.py"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/obspython.py"
      VERBATIM)
  endif()

elseif(OS_MACOS)
  get_target_property(_PYTHON_INCLUDE_DIRECTORY Python::Python
                      INTERFACE_INCLUDE_DIRECTORIES)

  target_include_directories(obspython PRIVATE ${_PYTHON_INCLUDE_DIRECTORY})

  target_link_options(obspython PRIVATE -undefined dynamic_lookup)

  target_compile_options(obspython PRIVATE -Wno-unused-parameter)

  if(XCODE)
    add_custom_command(
      TARGET obspython
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_BINARY_DIR}/obspython.py"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/obspython.py"
      VERBATIM)
  endif()

  set_target_properties(obspython PROPERTIES MACHO_CURRENT_VERSION 0
                                             MACHO_COMPATIBILITY_VERSION 0)
elseif(OS_POSIX)
  target_link_libraries(obspython PRIVATE Python::Python)

  target_compile_options(obspython PRIVATE -Wno-unused-parameter)

  set_target_properties(obspython PROPERTIES PREFIX "")

endif()

set_target_properties(
  obspython
  PROPERTIES FOLDER "scripting"
             VERSION "${OBS_VERSION_MAJOR}"
             SOVERSION "${OBS_VERSION_CANONICAL}")

setup_script_plugin_target(obspython)
