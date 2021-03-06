# Once done these will be defined:
#
#  LIBV4L2_FOUND
#  LIBV4L2_INCLUDE_DIRS
#  LIBV4L2_LIBRARIES

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(_V4L2 QUIET v4l-utils)
endif()

find_path(V4L2_INCLUDE_DIR
	NAMES libv4l2.h
	HINTS
		${_V4L2_INCLUDE_DIRS}
	PATHS
		/usr/include /usr/local/include /opt/local/include)

find_library(V4L2_LIB
	NAMES v4l2
	HINTS
		${_V4L2_LIBRARY_DIRS}
	PATHS
		/usr/lib /usr/local/lib /opt/local/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libv4l2 DEFAULT_MSG V4L2_LIB V4L2_INCLUDE_DIR)
mark_as_advanced(V4L2_INCLUDE_DIR V4L2_LIB)

if(LIBV4L2_FOUND)
	set(LIBV4L2_INCLUDE_DIRS ${V4L2_INCLUDE_DIR})
	set(LIBV4L2_LIBRARIES ${V4L2_LIB})
endif()
