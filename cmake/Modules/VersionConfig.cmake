set(OBS_COMPANY_NAME "OBS Project")
set(OBS_PRODUCT_NAME "OBS Studio")
set(OBS_WEBSITE "https://www.obsproject.com")
set(OBS_COMMENTS
    "Free and open source software for video recording and live streaming")
set(OBS_LEGAL_COPYRIGHT "(C) Hugh Bailey")

# Configure default version strings
set(_OBS_DEFAULT_VERSION "0" "0" "1")

# Set release candidate version information Must be a string in the format of
# "x.x.x-rcx"
if(DEFINED RELEASE_CANDIDATE)
  string(REPLACE "-rc" "." _OBS_RELEASE_CANDIDATE ${RELEASE_CANDIDATE})
  string(REPLACE "." ";" _OBS_VERSION ${RELEASE_CANDIDATE})
  string(REPLACE "." ";" _OBS_RELEASE_CANDIDATE ${_OBS_RELEASE_CANDIDATE})
  list(GET _OBS_RELEASE_CANDIDATE 0 1 2 _OBS_VERSION_CANONICAL)
else()
  set(_OBS_RELEASE_CANDIDATE "0" "0" "0" "0")
endif()

# Set full and canonical OBS version from current git tag or manual override
if(NOT DEFINED OBS_VERSION_OVERRIDE)
  if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
      COMMAND git describe --always --tags --dirty=-modified
      OUTPUT_VARIABLE _OBS_VERSION
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      RESULT_VARIABLE _OBS_VERSION_RESULT
      OUTPUT_STRIP_TRAILING_WHITESPACE)

    if(NOT ${_OBS_VERSION_RESULT} EQUAL 0)
      set(_OBS_VERSION "${_OBS_DEFAULT_VERSION}")
    endif()
    string(REPLACE "-" "." _CANONICAL_SPLIT ${_OBS_VERSION})
    string(REPLACE "." ";" _CANONICAL_SPLIT ${_CANONICAL_SPLIT})
    list(GET _CANONICAL_SPLIT 0 1 2 _OBS_VERSION_CANONICAL)
    string(REPLACE "." ";" _OBS_VERSION ${_OBS_VERSION})
  else()
    set(_OBS_VERSION ${_OBS_DEFAULT_VERSION})
    set(_OBS_VERSION_CANONICAL ${_OBS_DEFAULT_VERSION})
  endif()
else()
  string(REPLACE "." ";" _OBS_VERSION "${OBS_VERSION_OVERRIDE}")
  set(_OBS_VERSION_CANONICAL ${_OBS_VERSION})
endif()

list(GET _OBS_VERSION_CANONICAL 0 OBS_VERSION_MAJOR)
list(GET _OBS_VERSION_CANONICAL 1 OBS_VERSION_MINOR)
list(GET _OBS_VERSION_CANONICAL 2 OBS_VERSION_PATCH)
list(GET _OBS_RELEASE_CANDIDATE 0 OBS_RELEASE_CANDIDATE_MAJOR)
list(GET _OBS_RELEASE_CANDIDATE 1 OBS_RELEASE_CANDIDATE_MINOR)
list(GET _OBS_RELEASE_CANDIDATE 2 OBS_RELEASE_CANDIDATE_PATCH)
list(GET _OBS_RELEASE_CANDIDATE 3 OBS_RELEASE_CANDIDATE)
string(REPLACE ";" "." OBS_VERSION_CANONICAL "${_OBS_VERSION_CANONICAL}")
string(REPLACE ";" "." OBS_VERSION "${_OBS_VERSION}")

if(${OBS_RELEASE_CANDIDATE} GREATER 0)
  message(
    AUTHOR_WARNING
      "******************************************************************************\n"
      "  + OBS-Studio - Release candidate detected, OBS_VERSION is now: ${OBS_VERSION}\n"
      "******************************************************************************"
  )
endif()

# Define build number cache file
set(BUILD_NUMBER_CACHE
    ${CMAKE_SOURCE_DIR}/cmake/CMakeBuildNumber.cmake
    CACHE INTERNAL "OBS build number cache file")

# Read build number from cache file or manual override
if(NOT DEFINED OBS_BUILD_NUMBER AND EXISTS ${BUILD_NUMBER_CACHE})
  file(READ ${BUILD_NUMBER_CACHE} OBS_BUILD_NUMBER)
  math(EXPR OBS_BUILD_NUMBER "${OBS_BUILD_NUMBER}+1")
elseif(NOT DEFINED OBS_BUILD_NUMBER)
  set(OBS_BUILD_NUMBER "1")
endif()
file(WRITE ${BUILD_NUMBER_CACHE} "${OBS_BUILD_NUMBER}")

message(
  STATUS
    "OBS: Application Version: ${OBS_VERSION} - Build Number: ${OBS_BUILD_NUMBER}"
)
