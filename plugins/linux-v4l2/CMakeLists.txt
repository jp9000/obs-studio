project(linux-v4l2)

if(DISABLE_V4L2)
	message(STATUS "v4l2 plugin disabled")
	return()
endif()


if(ENABLE_V4L2)
	find_package(Libv4l2 REQUIRED)
else()
	find_package(Libv4l2)
	if(NOT LIBV4L2_FOUND)
		message(STATUS "libv4l2 not found, disabling v4l2 plugin")
		return()
	endif()
endif()

if(DISABLE_UDEV)
	add_definitions(-DHAVE_UDEV)
else()
	find_package(UDev)
	if(NOT UDEV_FOUND)
		message(STATUS "udev disabled for v4l2 plugin")
	else()
		set(linux-v4l2-udev_SOURCES
			v4l2-udev.c
		)
	endif()
endif()

include_directories(
	SYSTEM "${CMAKE_SOURCE_DIR}/libobs"
	${LIBV4L2_INCLUDE_DIRS}
)

set(linux-v4l2_SOURCES
	linux-v4l2.c
	v4l2-controls.c
	v4l2-input.c
	v4l2-helpers.c
	v4l2-output.c
	${linux-v4l2-udev_SOURCES}
)

add_library(linux-v4l2 MODULE
	${linux-v4l2_SOURCES}
)
target_link_libraries(linux-v4l2
	libobs
	${LIBV4L2_LIBRARIES}
	${UDEV_LIBRARIES}
)
set_target_properties(linux-v4l2 PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(linux-v4l2 data)
