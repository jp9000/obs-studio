# Try to find OSS on a *nix system
#
#   OSS_FOUND        - True if OSS is available
#   OSS_INCLUDE_DIR  - Include directory of OSS header
#   OSS_HEADER_NAME  - OSS header file name
#

IF (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
        set(OSS_HEADER_NAME "sys/soundcard.h")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "DragonFly")
        set(OSS_HEADER_NAME "sys/soundcard.h")
ENDIF()

find_path(OSS_INCLUDE_DIR "${OSS_HEADER_NAME}"
          "/usr/include"
          "/usr/local/include"
)

if (OSS_INCLUDE_DIR)
        set(OSS_FOUND True)
else (OSS_INCLUDE_DIR)
        set(OSS_FOUND)
endif (OSS_INCLUDE_DIR)

if (OSS_FOUND)
        message(STATUS "Found OSS header: ${OSS_INCLUDE_DIR}/${OSS_HEADER_NAME}")
else (OSS_FOUND)
        if (OSS_FIND_REQUIRED)
                message(FATAL_ERROR "Could not find OSS header file")
        endif (OSS_FIND_REQUIRED)
endif (OSS_FOUND)

mark_as_advanced(OSS_FOUND OSS_INCLUDE_DIR OSS_HEADER_NAME)
