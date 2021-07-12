# Once done these will be defined:
#
# SYSINFO_FOUND SYSINFO_INCLUDE_DIRS SYSINFO_LIBRARIES

find_path(
  SYSINFO_INCLUDE_DIR
  NAMES sys/sysinfo.h
  PATHS /usr/include /usr/local/include /opt/local/include)

find_library(
  SYSINFO_LIB
  NAMES sysinfo libsysinfo
  PATHS /usr/lib /usr/local/lib /opt/local/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sysinfo DEFAULT_MSG SYSINFO_LIB
                                  SYSINFO_INCLUDE_DIR)
mark_as_advanced(SYSINFO_INCLUDE_DIR SYSINFO_LIB)

if(SYSINFO_FOUND)
  set(SYSINFO_INCLUDE_DIRS ${SYSINFO_INCLUDE_DIR})
  set(SYSINFO_LIBRARIES ${SYSINFO_LIB})

  if(NOT TARGET Sysinfo::Sysinfo)
    if(IS_ABSOLUTE "${SYSINFO_LIBRARIES}")
      add_library(Sysinfo::Sysinfo UNKNOWN IMPORTED)
      set_target_properties(Sysinfo::Sysinfo PROPERTIES IMPORTED_LOCATION
                                                        "${SYSINFO_LIBRARIES}")
    else()
      add_library(Sysinfo::Sysinfo INTERFACE IMPORTED)
      set_target_properties(Sysinfo::Sysinfo PROPERTIES IMPORTED_LIBNAME
                                                        "${SYSINFO_LIBRARIES}")
    endif()

    set_target_properties(
      Sysinfo::Sysinfo PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                  "${SYSINFO_INCLUDE_DIRS}")
  endif()
endif()
