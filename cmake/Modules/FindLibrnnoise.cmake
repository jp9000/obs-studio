# Once done these will be defined:
#
#  LIBRNNOISE_FOUND
#  LIBRNNOISE_INCLUDE_DIRS
#  LIBRNNOISE_LIBRARIES
#
# For use in OBS:
#
#  RNNOISE_INCLUDE_DIR

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(_RNNOISE QUIET rnnoise)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_lib_suffix 64)
else()
	set(_lib_suffix 32)
endif()

find_path(RNNOISE_INCLUDE_DIR
	NAMES rnnoise.h
	HINTS
		ENV rnnoisePath${_lib_suffix}
		ENV rnnoisePath
		ENV DepsPath${_lib_suffix}
		ENV DepsPath
		${rnnoisePath${_lib_suffix}}
		${rnnoisePath}
		${DepsPath${_lib_suffix}}
		${DepsPath}
		${_RNNOISE_INCLUDE_DIRS}
	PATHS
		/usr/include /usr/local/include /opt/local/include /sw/include
	PATH_SUFFIXES
		include)

find_library(RNNOISE_LIB
	NAMES ${_RNNOISE_LIBRARIES} rnnoise
	HINTS
		ENV rnnoisePath${_lib_suffix}
		ENV rnnoisePath
		ENV DepsPath${_lib_suffix}
		ENV DepsPath
		${rnnoisePath${_lib_suffix}}
		${rnnoisePath}
		${DepsPath${_lib_suffix}}
		${DepsPath}
		${_RNNOISE_LIBRARY_DIRS}
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
find_package_handle_standard_args(Librnnoise DEFAULT_MSG RNNOISE_LIB RNNOISE_INCLUDE_DIR)
mark_as_advanced(RNNOISE_INCLUDE_DIR RNNOISE_LIB)

if(LIBRNNOISE_FOUND)
	set(LIBRNNOISE_INCLUDE_DIRS ${RNNOISE_INCLUDE_DIR})
	set(LIBRNNOISE_LIBRARIES ${RNNOISE_LIB})
endif()
