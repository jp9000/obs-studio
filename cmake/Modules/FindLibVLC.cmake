# Once done these will be defined:
#
#  LIBVLC_FOUND
#  LIBVLC_INCLUDE_DIRS
#  LIBVLC_LIBRARIES
#

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(_VLC QUIET libvlc)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_lib_suffix 64)
else()
	set(_lib_suffix 32)
endif()

find_path(VLC_INCLUDE_DIR
	NAMES libvlc.h
	HINTS
		ENV VLCPath${_lib_suffix}
		ENV VLCPath
		ENV DepsPath${_lib_suffix}
		ENV DepsPath
		${VLCPath${_lib_suffix}}
		${VLCPath}
		${DepsPath${_lib_suffix}}
		${DepsPath}
		${_VLC_INCLUDE_DIRS}
	PATHS
		/usr/include /usr/local/include /opt/local/include /sw/include
	PATH_SUFFIXES
		vlc include/vlc include)

find_library(VLC_LIB
	NAMES ${_VLC_LIBRARIES} VLC libVLC
	HINTS
		ENV VLCPath${_lib_suffix}
		ENV VLCPath
		ENV DepsPath${_lib_suffix}
		ENV DepsPath
		${VLCPath${_lib_suffix}}
		${VLCPath}
		${DepsPath${_lib_suffix}}
		${DepsPath}
		${_VLC_LIBRARY_DIRS}
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
# OBS doesnt depend on linking, so we dont include VLC_LIB here as required.
find_package_handle_standard_args(LibVLC DEFAULT_MSG VLC_INCLUDE_DIR)
mark_as_advanced(VLC_INCLUDE_DIR VLC_LIB)

if(LIBVLC_FOUND)
	set(LIBVLC_LIBRARIES ${VLC_LIB})
	set(LIBVLC_INCLUDE_DIRS ${VLC_INCLUDE_DIR})
endif()
