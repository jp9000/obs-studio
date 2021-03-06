# Once done these will be defined:
#
#  LIBFDK_FOUND
#  LIBFDK_INCLUDE_DIRS
#  LIBFDK_LIBRARIES
#
# For use in OBS: 
#
#  Libfdk_INCLUDE_DIR

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(_LIBFDK QUIET fdk-aac)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_lib_suffix 64)
else()
	set(_lib_suffix 32)
endif()

find_path(Libfdk_INCLUDE_DIR
	NAMES fdk-aac/aacenc_lib.h
	HINTS
		ENV LibfdkPath${_lib_suffix}
		ENV LibfdkPath
		ENV DepsPath${_lib_suffix}
		ENV DepsPath
		${LibfdkPath${_lib_suffix}}
		${LibfdkPath}
		${DepsPath${_lib_suffix}}
		${DepsPath}
		${_LIBFDK_INCLUDE_DIRS}
	PATHS
		/usr/include /usr/local/include /opt/local/include /sw/include
	PATH_SUFFIXES
		include)

find_library(Libfdk_LIB
	NAMES ${_LIBFDK_LIBRARIES} fdk-aac libfdk-aac
	HINTS
		ENV LibfdkPath${_lib_suffix}
		ENV LibfdkPath
		ENV DepsPath${_lib_suffix}
		ENV DepsPath
		${LibfdkPath${_lib_suffix}}
		${LibfdkPath}
		${DepsPath${_lib_suffix}}
		${DepsPath}
		${_LIBFDK_LIBRARY_DIRS}
	PATHS
		/usr/lib /usr/local/lib /opt/local/lib /sw/lib
	PATH_SUFFIXES
		lib${_lib_suffix} lib
		libs${_lib_suffix} libs
		bin${_lib_suffix} bin
		../lib${_lib_suffix} ../lib
		../libs${_lib_suffix} ../libs
		../bin${_lib_suffix} ../bin)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libfdk DEFAULT_MSG Libfdk_LIB Libfdk_INCLUDE_DIR)
mark_as_advanced(Libfdk_INCLUDE_DIR Libfdk_LIB)

if(LIBFDK_FOUND)
	set(LIBFDK_INCLUDE_DIRS ${Libfdk_INCLUDE_DIR})
	set(LIBFDK_LIBRARIES ${Libfdk_LIB})
endif()
