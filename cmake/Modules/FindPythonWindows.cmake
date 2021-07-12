# Once done these will be defined:
#
# PYTHON_FOUND PYTHON_INCLUDE_DIRS PYTHON_LIBRARIES
#
# For use in OBS:
#
# PYTHON_INCLUDE_DIR

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(_LIB_SUFFIX 64)
else()
  set(_LIB_SUFFIX 32)
endif()

find_path(
  PYTHON_INCLUDE_DIR
  NAMES Python.h
  HINTS ${_PYTHON_INCLUDE_DIRS}
  PATH_SUFFIXES include include/python)

find_library(
  PYTHON_LIB
  NAMES ${_PYTHON_LIBRARIES} python36
  HINTS ${_PYTHON_LIBRARY_DIRS}
  PATH_SUFFIXES
    lib${_lib_suffix}
    lib
    libs${_lib_suffix}
    libs
    bin${_lib_suffix}
    bin
    ../lib${_lib_suffix}
    ../lib
    ../libs${_lib_suffix}
    ../libs
    ../bin${_lib_suffix}
    ../bin)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PythonWindows DEFAULT_MSG PYTHON_LIB
                                  PYTHON_INCLUDE_DIR)
mark_as_advanced(PYTHON_INCLUDE_DIR PYTHON_LIB)

if(PYTHONWINDOWS_FOUND)
  set(Python_FOUND TRUE)
  set(Python_INCLUDE_DIRS ${PYTHON_INCLUDE_DIR})
  set(Python_LIBRARIES ${PYTHON_LIB})
  set(PYTHONLIBS_FOUND TRUE)

  if(NOT TARGET Python::Python)
    if(IS_ABSOLUTE "${Python_LIBRARIES}")
      add_library(Python::Python UNKNOWN IMPORTED)
      set_target_properties(Python::Python PROPERTIES IMPORTED_LOCATION
                                                      "${Python_LIBRARIES}")
    else()
      add_library(Python::Python INTERFACE IMPORTED)
      set_target_properties(Python::Python PROPERTIES IMPORTED_LIBNAME
                                                      "${Python_LIBRARIES}")
    endif()

    set_target_properties(
      Python::Python PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                "${Python_INCLUDE_DIRS}")
  endif()
endif()
