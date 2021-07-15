include(FindPackageHandleStandardArgs)

set(CEF_ROOT_DIR
    ""
    CACHE PATH "Path to a CEF distributed build")
mark_as_advanced(CEF_ROOT_DIR)

find_path(CEF_INCLUDE_DIR "include/cef_version.h" HINTS "${CEF_ROOT_DIR}")

if(OS_MACOS)
  find_library(
    CEF_LIBRARY
    NAMES cef libcef cef.lib libcef.o "Chromium Embedded Framework"
    NO_DEFAULT_PATH
    PATHS "${CEF_ROOT_DIR}" "${CEF_ROOT_DIR}/Release")

  find_library(
    CEFWRAPPER_LIBRARY
    NAMES cef_dll_wrapper libcef_dll_wrapper
    NO_DEFAULT_PATH
    PATHS "${CEF_ROOT_DIR}/build/libcef_dll/Release"
          "${CEF_ROOT_DIR}/build/libcef_dll_wrapper/Release"
          "${CEF_ROOT_DIR}/build/libcef_dll"
          "${CEF_ROOT_DIR}/build/libcef_dll_wrapper")

elseif(OS_POSIX)
  find_library(
    CEF_LIBRARY
    NAMES libcef.so "Chromium Embedded Framework"
    NO_DEFAULT_PATH
    PATHS "${CEF_ROOT_DIR}" "${CEF_ROOT_DIR}/Release")

  find_library(
    CEFWRAPPER_LIBRARY
    NAMES libcef_dll_wrapper.a
    NO_DEFAULT_PATH
    PATHS "${CEF_ROOT_DIR}/build/libcef_dll_wrapper"
          "${CEF_ROOT_DIR}/libcef_dll_wrapper")

else()
  find_library(
    CEF_LIBRARY
    NAMES cef libcef cef.lib libcef.o "Chromium Embedded Framework"
    PATHS "${CEF_ROOT_DIR}" "${CEF_ROOT_DIR}/Release")

  find_library(
    CEFWRAPPER_LIBRARY
    NAMES cef_dll_wrapper libcef_dll_wrapper
    PATHS "${CEF_ROOT_DIR}/build/libcef_dll/Release"
          "${CEF_ROOT_DIR}/build/libcef_dll_wrapper/Release"
          "${CEF_ROOT_DIR}/build/libcef_dll"
          "${CEF_ROOT_DIR}/build/libcef_dll_wrapper")

  if(OS_WINDOWS)
    find_library(
      CEFWRAPPER_LIBRARY_DEBUG
      NAMES cef_dll_wrapper libcef_dll_wrapper
      PATHS "${CEF_ROOT_DIR}/build/libcef_dll/Debug"
            "${CEF_ROOT_DIR}/build/libcef_dll_wrapper/Debug")
  endif()
endif()

mark_as_advanced(CEFWRAPPER_LIBRARY CEFWRAPPER_LIBRARY_DEBUG)

if(NOT CEF_LIBRARY)
  message(
    WARNING
      "  Could NOT find Chromium Embedded Framework library (missing: CEF_LIBRARY)"
  )
  set(CEF_FOUND FALSE)
  return()
endif()

if(NOT CEFWRAPPER_LIBRARY)
  message(
    WARNING
      "  Could NOT find Chromium Embedded Framework wrapper library (missing: CEFWRAPPER_LIBRARY)"
  )
  set(CEF_FOUND FALSE)
  return()
endif()

message(
  STATUS
    "Found Chromium Embedded Framework: ${CEF_LIBRARY};${CEF_WRAPPER_LIBRARY}")

if(OS_WINDOWS)
  set(CEF_LIBRARIES ${CEF_LIBRARY} ${CEFWRAPPER_LIBRARY})

elseif(OS_MACOS)
  if(BROWSER_LEGACY)
    set(CEF_LIBRARIES ${CEFWRAPPER_LIBRARY} ${CEF_LIBRARY})
  else()
    set(CEF_LIBRARIES ${CEFWRAPPER_LIBRARY})
  endif()
else()
  set(CEF_LIBRARIES ${CEF_LIBRARY} optimized ${CEFWRAPPER_LIBRARY})

  if(CEFWRAPPER_LIBRARY_DEBUG)
    list(APPEND CEF_LIBRARIES debug ${CEFWRAPPER_LIBRARY_DEBUG})
  endif()
endif()

find_package_handle_standard_args(CEF DEFAULT_MSG CEF_LIBRARY
                                  CEFWRAPPER_LIBRARY CEF_INCLUDE_DIR)

mark_as_advanced(CEF_LIBRARY CEF_WRAPPER_LIBRARY CEF_LIBRARIES CEF_INCLUDE_DIR)

if(NOT TARGET CEF::Wrapper)
  if(IS_ABSOLUTE "${CEF_LIBRARIES}")
    add_library(CEF::Wrapper UNKNOWN IMPORTED)
    add_library(CEF::Library UNKNOWN IMPORTED)

    set_target_properties(CEF::Wrapper PROPERTIES IMPORTED_LOCATION
                                                  ${CEFWRAPPER_LIBRARY})

    set_target_properties(CEF::Library PROPERTIES IMPORTED_LOCATION
                                                  ${CEF_LIBRARY})

    if(DEFINED CEFWRAPPER_LIBRARY_DEBUG)
      add_library(CEF::Wrapper_Debug UNKNOWN IMPORTED)
      set_target_properties(
        CEF::Wrapper_Debug PROPERTIES IMPORTED_LOCATION
                                      ${CEFWRAPPER_LIBRARY_DEBUG})
    endif()
  else()
    add_library(CEF::Wrapper INTERFACE IMPORTED)
    add_library(CEF::Library INTERFACE IMPORTED)

    set_target_properties(CEF::Wrapper PROPERTIES IMPORTED_LIBNAME
                                                  ${CEFWRAPPER_LIBRARY})

    set_target_properties(CEF::Library PROPERTIES IMPORTED_LIBNAME
                                                  ${CEF_LIBRARY})

    if(DEFINED CEFWRAPPER_LIBRARY_DEBUG)
      add_library(CEF::Wrapper_Debug INTERFACE IMPORTED)
      set_target_properties(
        CEF::Wrapper_Debug PROPERTIES IMPORTED_LIBNAME
                                      ${CEFWRAPPER_LIBRARY_DEBUG})
    endif()
  endif()

  set_target_properties(CEF::Wrapper PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                                "${CEF_INCLUDE_DIR}")

  set_target_properties(CEF::Library PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                                "${CEF_INCLUDE_DIR}")

  if(DEFINED CEFWRAPPER_LIBRARY_DEBUG)
    set_target_properties(
      CEF::Wrapper_Debug PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                    "${CEF_INCLUDE_DIR}")
  endif()
endif()
