project(linux-capture)

find_package(X11 REQUIRED)
if(NOT X11_Xcomposite_FOUND)
	message(STATUS "Xcomposite library not found, linux-capture plugin disabled")
	return()
endif()

find_package(XCB COMPONENTS XCB RANDR SHM XFIXES XINERAMA REQUIRED)
find_package(X11_XCB REQUIRED)

include_directories(SYSTEM
	"${CMAKE_SOURCE_DIR}/libobs"
	${X11_Xcomposite_INCLUDE_PATH}
	${X11_X11_INCLUDE_PATH}
	${XCB_INCLUDE_DIRS}
)

set(linux-capture_SOURCES
	linux-capture.c
	xcursor.c
	xcursor-xcb.c
	xhelpers.c
	xshm-input.c
	xcomposite-main.cpp
	xcompcap-main.cpp
	xcompcap-helper.cpp
)
set(linux-capture_HEADERS
	xcursor.h
	xcursor-xcb.h
	xhelpers.h
	xcompcap-main.hpp
	xcompcap-helper.hpp
)

add_library(linux-capture MODULE
	${linux-capture_SOURCES}
	${linux-capture_HEADERS}
)
target_link_libraries(linux-capture
	libobs
	glad
	${X11_LIBRARIES}
	${X11_Xfixes_LIB}
	${X11_X11_LIB}
	${X11_Xcomposite_LIB}
	${XCB_LIBRARIES}
)
set_target_properties(linux-capture PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(linux-capture data)
