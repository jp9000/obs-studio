# Once done these will be defined:
#
#  VULKAN_FOUND
#  VULKAN_INCLUDE_DIRS
#  VULKAN_LIBRARIES
#
# For use in OBS: 
#
#  VULKAN_INCLUDE_DIR

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(_VULKAN QUIET vulkan)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_lib_suffix 64)
else()
	set(_lib_suffix 32)
endif()

find_path(VULKAN_INCLUDE_DIR
	NAMES vulkan/vulkan.h
	HINTS
		ENV vulkanPath${_lib_suffix}
		ENV vulkanPath
		ENV DepsPath${_lib_suffix}
		ENV DepsPath
		ENV VULKAN_SDK
		${vulkanPath${_lib_suffix}}
		${vulkanPath}
		${DepsPath${_lib_suffix}}
		${DepsPath}
		${_VULKAN_INCLUDE_DIRS}
	PATHS
		/usr/include /usr/local/include /opt/local/include /sw/include
	PATH_SUFFIXES
		include)

find_library(VULKAN_LIB
	NAMES ${_VULKAN_LIBRARIES} vulkan-1 vulkan libvulkan
	HINTS
		ENV vulkanPath${_lib_suffix}
		ENV vulkanPath
		ENV DepsPath${_lib_suffix}
		ENV DepsPath
		ENV VULKAN_SDK
		${vulkanPath${_lib_suffix}}
		${vulkanPath}
		${DepsPath${_lib_suffix}}
		${DepsPath}
		${_VULKAN_LIBRARY_DIRS}
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
find_package_handle_standard_args(Vulkan DEFAULT_MSG VULKAN_LIB VULKAN_INCLUDE_DIR)
mark_as_advanced(VULKAN_INCLUDE_DIR VULKAN_LIB)

if(VULKAN_FOUND)
	set(VULKAN_INCLUDE_DIRS ${VULKAN_INCLUDE_DIR})
	set(VULKAN_LIBRARIES ${VULKAN_LIB})
endif()
