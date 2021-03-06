project(obs-cmocka)

# Fix libobs path
macro(fixLink target_arg)
	if(APPLE AND UNIX)
		add_custom_command (TARGET ${target_arg}
			POST_BUILD COMMAND "${CMAKE_INSTALL_NAME_TOOL}"
			"-change" "@rpath/libobs.0.dylib" "@executable_path/../../libobs/libobs.0.dylib"
			"$<TARGET_FILE:${target_arg}>" VERBATIM)
	endif()
endmacro()

set(CMAKE_MACOSX_RPATH TRUE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
list(APPEND CMAKE_INSTALL_RPATH "@loader_path/" "@executable_path/")

find_package(CMocka CONFIG REQUIRED)
include_directories(${CMOCKA_INCLUDE_DIR})
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/libobs")

# fix rpath on linux
if (UNIX AND NOT APPLE)
	set(CMAKE_INSTALL_RPATH "$ORIGIN";../../libobs)
endif()

# Serializer test
add_executable(test_serializer test_serializer.c)
target_link_libraries(test_serializer ${CMOCKA_LIBRARIES} libobs)

add_test(test_serializer ${CMAKE_CURRENT_BINARY_DIR}/test_serializer)
fixLink(test_serializer)


# darray test
add_executable(test_darray test_darray.c)
target_link_libraries(test_darray ${CMOCKA_LIBRARIES} libobs)

add_test(test_darray ${CMAKE_CURRENT_BINARY_DIR}/test_darray)
fixLink(test_darray)

# bitstream test
add_executable(test_bitstream test_bitstream.c)
target_link_libraries(test_bitstream ${CMOCKA_LIBRARIES} libobs)

add_test(test_bitstream ${CMAKE_CURRENT_BINARY_DIR}/test_bitstream)
fixLink(test_bitstream)
