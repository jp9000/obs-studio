# Notes:
#
# Author: Paul Harris, June 2012
# Additions: Joakim Soderberg, Febuary 2013
#
# Supports: building static/shared, release/debug/etc, can also build html docs
# and some of the tests.
# Note that its designed for out-of-tree builds, so it will not pollute your
# source tree.
#
# TODO 1: Finish implementing tests. api tests are working, but the valgrind
# variants are not flagging problems.
#
# TODO 2: There is a check_exports script that would try and incorporate.
#
# TODO 3: Consolidate version numbers, currently the version number is written
# into: * cmake (here) * autotools (the configure) * source code header files.
# Should not be written directly into header files, autotools/cmake can do
# that job.
#
# Brief intro on how to use cmake:
# > mkdir build (somewhere - we do out-of-tree builds)
# > use cmake, ccmake, or cmake-gui to configure the project. for linux, you
# can only choose one variant: release,debug,etc... and static or shared.
# >> example:
# >> cd build
# >> ccmake -i ../path_to_jansson_dir
# >>  inside, configure your options. press C until there are no lines
#     with * next to them.
# >>  note, I like to configure the 'install' path to ../install, so I get
#     self-contained clean installs I can point other projects to.
# >>  press G to 'generate' the project files.
# >> make (to build the project)
# >> make install
# >> make test (to run the tests, if you enabled them)
#
# Brief description on how it works:
# There is a small heirachy of CMakeLists.txt files which define how the
# project is built.
# Header file detection etc is done, and the results are written into config.h
# and jansson_config.h, which are generated from the corresponding
# config.h.cmake and jansson_config.h.cmake template files.
# The generated header files end up in the build directory - not in
# the source directory.
# The rest is down to the usual make process.



cmake_minimum_required (VERSION 2.8.12)
# required for exports? cmake_minimum_required (VERSION 2.8.6)
project (jansson C)

# Options
option(JANSSON_BUILD_SHARED_LIBS "Build shared libraries." OFF)
option(USE_URANDOM "Use /dev/urandom to seed the hash function." ON)
option(USE_WINDOWS_CRYPTOAPI "Use CryptGenRandom to seed the hash function." ON)

if (MSVC)
   # This option must match the settings used in your program, in particular if you
	# are linking statically
	option(JANSSON_STATIC_CRT "Link the static CRT libraries" ON )
endif ()

# Disabled by OBS
option(JANSSON_EXAMPLES "Compile example applications" OFF)

if (UNIX)
   option(JANSSON_COVERAGE "(GCC Only! Requires gcov/lcov to be installed). Include target for doing coverage analysis for the test suite. Note that -DCMAKE_BUILD_TYPE=Debug must be set" OFF)
   option(JANSSON_COVERALLS "Generate coverage info for Coveralls" OFF)
   option(JANSSON_COVERALLS_UPLOAD "Upload coverage info to Coveralls (Only works via Travis)" ON)
endif ()

# Set some nicer output dirs.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(JANSSON_TEMP_DIR ${PROJECT_BINARY_DIR}/tmp)

# Give the debug version a different postfix for windows,
# so both the debug and release version can be built in the
# same build-tree on Windows (MSVC).
if (WIN32)
   set(CMAKE_DEBUG_POSTFIX "_d")
endif (WIN32)

# This is how I thought it should go
# set (JANSSON_VERSION "2.3.1")
# set (JANSSON_SOVERSION 2)

set(JANSSON_DISPLAY_VERSION "2.9")

# This is what is required to match the same numbers as automake's
set(JANSSON_VERSION "4.9.0")
set(JANSSON_SOVERSION 4)

# for CheckFunctionKeywords
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include (CheckCSourceCompiles)
include (CheckFunctionExists)
include (CheckFunctionKeywords)
include (CheckIncludeFiles)
include (CheckTypeSize)

if (MSVC)
   # Turn off Microsofts "security" warnings.
   add_definitions( "/W3 /D_CRT_SECURE_NO_WARNINGS /wd4005 /wd4996 /nologo" )

   # Disabled by OBS, options already set by top level CMakeLists
   if (FALSE)
      if (JANSSON_STATIC_CRT)
         set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
         set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
      endif()
   endif()

endif()

if (NOT WIN32 AND NOT APPLE)
   add_definitions("-fPIC")
endif()

message("C compiler: ${CMAKE_C_COMPILER_ID}")

# Coverage only works with GCC for a debug build.
if (JANSSON_COVERALLS)
   set(JANSSON_COVERAGE ON)
endif()

if (JANSSON_COVERAGE)
   include(CodeCoverage)
   include(Coveralls)

   # This adds coverage arguments to gcc/clang.
   coveralls_turn_on_coverage()
endif()

check_include_files (endian.h HAVE_ENDIAN_H)
check_include_files (fcntl.h HAVE_FCNTL_H)
check_include_files (sched.h HAVE_SCHED_H)
check_include_files (unistd.h HAVE_UNISTD_H)
check_include_files (sys/param.h HAVE_SYS_PARAM_H)
check_include_files (sys/stat.h HAVE_SYS_STAT_H)
check_include_files (sys/time.h HAVE_SYS_TIME_H)
check_include_files (sys/time.h HAVE_SYS_TYPES_H)

check_function_exists (close HAVE_CLOSE)
check_function_exists (getpid HAVE_GETPID)
check_function_exists (gettimeofday HAVE_GETTIMEOFDAY)
check_function_exists (open HAVE_OPEN)
check_function_exists (read HAVE_READ)
check_function_exists (sched_yield HAVE_SCHED_YIELD)

# Check for the int-type includes
check_include_files (stdint.h HAVE_STDINT_H)

# Check our 64 bit integer sizes
check_type_size (__int64 __INT64)
check_type_size (int64_t INT64_T)
check_type_size ("long long" LONG_LONG_INT)

# Check our 32 bit integer sizes
check_type_size (int32_t INT32_T)
check_type_size (__int32 __INT32)
check_type_size ("long" LONG_INT)
check_type_size ("int" INT)
if (HAVE_INT32_T)
   set (JSON_INT32 int32_t)
elseif (HAVE___INT32)
   set (JSON_INT32 __int32)
elseif (HAVE_LONG_INT AND (${LONG_INT} EQUAL 4))
   set (JSON_INT32 long)
elseif (HAVE_INT AND (${INT} EQUAL 4))
   set (JSON_INT32 int)
else ()
   message (FATAL_ERROR "Could not detect a valid 32-bit integer type")
endif ()

check_type_size ("unsigned long" UNSIGNED_LONG_INT)
check_type_size ("unsigned int" UNSIGNED_INT)
check_type_size ("unsigned short" UNSIGNED_SHORT)

check_type_size (uint32_t UINT32_T)
check_type_size (__uint32 __UINT32)
if (HAVE_UINT32_T)
   set (JSON_UINT32 uint32_t)
elseif (HAVE___UINT32)
   set (JSON_UINT32 __uint32)
elseif (HAVE_UNSIGNED_LONG_INT AND (${UNSIGNED_LONG_INT} EQUAL 4))
   set (JSON_UINT32 "unsigned long")
elseif (HAVE_UNSIGNED_INT AND (${UNSIGNED_INT} EQUAL 4))
   set (JSON_UINT32 "unsigned int")
else ()
   message (FATAL_ERROR "Could not detect a valid unsigned 32-bit integer type")
endif ()

check_type_size (uint16_t UINT16_T)
check_type_size (__uint16 __UINT16)
if (HAVE_UINT16_T)
   set (JSON_UINT16 uint16_t)
elseif (HAVE___UINT16)
   set (JSON_UINT16 __uint16)
elseif (HAVE_UNSIGNED_INT AND (${UNSIGNED_INT} EQUAL 2))
   set (JSON_UINT16 "unsigned int")
elseif (HAVE_UNSIGNED_SHORT AND (${UNSIGNED_SHORT} EQUAL 2))
   set (JSON_UINT16 "unsigned short")
else ()
   message (FATAL_ERROR "Could not detect a valid unsigned 16-bit integer type")
endif ()

check_type_size (uint8_t UINT8_T)
check_type_size (__uint8 __UINT8)
if (HAVE_UINT8_T)
   set (JSON_UINT8 uint8_t)
elseif (HAVE___UINT8)
   set (JSON_UINT8 __uint8)
else ()
   set (JSON_UINT8 "unsigned char")
endif ()

# Check for ssize_t and SSIZE_T existance.
check_type_size(ssize_t SSIZE_T)
check_type_size(SSIZE_T UPPERCASE_SSIZE_T)
if(NOT HAVE_SSIZE_T)
   if(HAVE_UPPERCASE_SSIZE_T)
      set(JSON_SSIZE SSIZE_T)
   else()
      set(JSON_SSIZE int)
   endif()
endif()
set(CMAKE_EXTRA_INCLUDE_FILES "")

# Check for all the variants of strtoll
check_function_exists (strtoll HAVE_STRTOLL)
check_function_exists (strtoq HAVE_STRTOQ)
check_function_exists (_strtoi64 HAVE__STRTOI64)

# Figure out what variant we should use
if (HAVE_STRTOLL)
   set (JSON_STRTOINT strtoll)
elseif (HAVE_STRTOQ)
   set (JSON_STRTOINT strtoq)
elseif (HAVE__STRTOI64)
   set (JSON_STRTOINT _strtoi64)
else ()
   # fallback to strtol (32 bit)
   # this will set all the required variables
   set (JSON_STRTOINT strtol)
   set (JSON_INT_T long)
   set (JSON_INTEGER_FORMAT "\"ld\"")
endif ()

# if we haven't defined JSON_INT_T, then we have a 64 bit conversion function.
# detect what to use for the 64 bit type.
# Note: I will prefer long long if I can get it, as that is what the automake system aimed for.
if (NOT DEFINED JSON_INT_T)
   if (HAVE_LONG_LONG_INT AND ((${LONG_LONG_INT}) EQUAL 8))
      set (JSON_INT_T "long long")
   elseif (HAVE_INT64_T)
      set (JSON_INT_T int64_t)
   elseif (HAVE___INT64)
      set (JSON_INT_T __int64)
   else ()
      message (FATAL_ERROR "Could not detect 64 bit type, although I detected the strtoll equivalent")
   endif ()

   # Apparently, Borland BCC and MSVC wants I64d,
   # Borland BCC could also accept LD
   # and gcc wants ldd,
   # I am not sure what cygwin will want, so I will assume I64d

   if (WIN32) # matches both msvc and cygwin
      set (JSON_INTEGER_FORMAT "\"I64d\"")
   else ()
      set (JSON_INTEGER_FORMAT "\"lld\"")
   endif ()
endif ()


# If locale.h and localeconv() are available, define to 1, otherwise to 0.
check_include_files (locale.h HAVE_LOCALE_H)
check_function_exists (localeconv HAVE_LOCALECONV)

if (HAVE_LOCALECONV AND HAVE_LOCALE_H)
   set (JSON_HAVE_LOCALECONV 1)
else ()
   set (JSON_HAVE_LOCALECONV 0)
endif()

# check if we have setlocale
check_function_exists(setlocale HAVE_SETLOCALE)

# Check what the inline keyword is.
# Note that the original JSON_INLINE was always set to just 'inline', so this goes further.
check_function_keywords("inline")
check_function_keywords("__inline")
check_function_keywords("__inline__")

if (HAVE_INLINE)
   set(JSON_INLINE inline)
elseif (HAVE___INLINE)
   set(JSON_INLINE __inline)
elseif (HAVE___INLINE__)
   set(JSON_INLINE __inline__)
else()
   # no inline on this platform
   set (JSON_INLINE)
endif()

check_c_source_compiles ("int main() { unsigned long val; __sync_bool_compare_and_swap(&val, 0, 1); return 0; } " HAVE_SYNC_BUILTINS)
check_c_source_compiles ("int main() { char l; unsigned long v; __atomic_test_and_set(&l, __ATOMIC_RELAXED); __atomic_store_n(&v, 1, __ATOMIC_RELEASE); __atomic_load_n(&v, __ATOMIC_ACQUIRE); return 0; }" HAVE_ATOMIC_BUILTINS)

set (JANSSON_INITIAL_HASHTABLE_ORDER 3 CACHE STRING "Number of buckets new object hashtables contain is 2 raised to this power. The default is 3, so empty hashtables contain 2^3 = 8 buckets.")

# configure the public config file
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/cmake/jansson_config.h.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/include/jansson_config.h)

# Copy the jansson.h file to the public include folder
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/jansson.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/)

add_definitions(-DJANSSON_USING_CMAKE)

# configure the private config file
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/cmake/jansson_private_config.h.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/private_include/jansson_private_config.h)

# and tell the source code to include it
add_definitions(-DHAVE_CONFIG_H)

include_directories (${CMAKE_CURRENT_BINARY_DIR}/include)
include_directories (${CMAKE_CURRENT_BINARY_DIR}/private_include)

# Add the lib sources.
file(GLOB JANSSON_SRC src/*.c)

set(JANSSON_HDR_PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/src/hashtable.h
   ${CMAKE_CURRENT_SOURCE_DIR}/src/jansson_private.h
   ${CMAKE_CURRENT_SOURCE_DIR}/src/strbuffer.h
   ${CMAKE_CURRENT_SOURCE_DIR}/src/utf.h
   ${CMAKE_CURRENT_BINARY_DIR}/private_include/jansson_private_config.h)

set(JANSSON_HDR_PUBLIC
   ${CMAKE_CURRENT_BINARY_DIR}/include/jansson_config.h
   ${CMAKE_CURRENT_SOURCE_DIR}/src/jansson.h)

source_group("Library Sources" FILES ${JANSSON_SRC})
source_group("Library Private Headers" FILES ${JANSSON_HDR_PRIVATE})
source_group("Library Public Headers" FILES ${JANSSON_HDR_PUBLIC})

if(JANSSON_BUILD_SHARED_LIBS)
   add_library(jansson SHARED
      ${JANSSON_SRC}
      ${JANSSON_HDR_PRIVATE}
      ${JANSSON_HDR_PUBLIC}
      src/jansson.def)

   set_target_properties(jansson PROPERTIES
      VERSION ${JANSSON_VERSION}
      SOVERSION ${JANSSON_SOVERSION})
else()
   add_library(jansson
      ${JANSSON_SRC}
      ${JANSSON_HDR_PRIVATE}
      ${JANSSON_HDR_PUBLIC})
endif()
set_target_properties(jansson PROPERTIES FOLDER "deps")

if (JANSSON_EXAMPLES)
	add_executable(simple_parse "${PROJECT_SOURCE_DIR}/examples/simple_parse.c")
	target_link_libraries(simple_parse jansson)
endif()

# For building Documentation (uses Sphinx)
# Disabled by OBS
option(JANSSON_BUILD_DOCS "Build documentation (uses python-sphinx)." OFF)
if (JANSSON_BUILD_DOCS)
   find_package(Sphinx)

   if (NOT SPHINX_FOUND)
      message(WARNING "Sphinx not found. Cannot generate documentation!
      Set -DJANSSON_BUILD_DOCS=OFF to get rid of this message.")
   else()
      if (Sphinx_VERSION_STRING VERSION_LESS 1.0)
         message(WARNING "Your Sphinx version is too old!
               This project requires Sphinx v1.0 or above to produce
               proper documentation (you have v${Sphinx_VERSION_STRING}).
               You will get output but it will have errors.")
      endif()

      # configured documentation tools and intermediate build results
      set(BINARY_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/_build")

      # Sphinx cache with pickled ReST documents
      set(SPHINX_CACHE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_doctrees")

      # CMake could be used to build the conf.py file too,
      # eg it could automatically write the version of the program or change the theme.
      # if(NOT DEFINED SPHINX_THEME)
      #    set(SPHINX_THEME default)
      # endif()
      #
      # if(NOT DEFINED SPHINX_THEME_DIR)
      #    set(SPHINX_THEME_DIR)
      # endif()
      #
      # configure_file(
      #    "${CMAKE_CURRENT_SOURCE_DIR}/conf.py.in"
      #    "${BINARY_BUILD_DIR}/conf.py"
      #    @ONLY)

      # TODO: Add support for all sphinx builders: http://sphinx-doc.org/builders.html

      # Add documentation targets.
      set(DOC_TARGETS html)

      option(JANSSON_BUILD_MAN "Create a target for building man pages." ON)

      if (JANSSON_BUILD_MAN)
         if (Sphinx_VERSION_STRING VERSION_LESS 1.0)
            message(WARNING "Sphinx version 1.0 > is required to build man pages. You have v${Sphinx_VERSION_STRING}.")
         else()
            list(APPEND DOC_TARGETS man)
         endif()
      endif()

      option(JANSSON_BUILD_LATEX "Create a target for building latex docs (to create PDF)." OFF)

      if (JANSSON_BUILD_LATEX)
         find_package(LATEX)

         if (NOT LATEX_COMPILER)
            message("Couldn't find Latex, can't build latex docs using Sphinx")
         else()
            message("Latex found! If you have problems building, see Sphinx documentation for required Latex packages.")
            list(APPEND DOC_TARGETS latex)
         endif()
      endif()

      # The doc target will build all documentation targets.
      add_custom_target(doc)

      foreach (DOC_TARGET ${DOC_TARGETS})
         add_custom_target(${DOC_TARGET}
            ${SPHINX_EXECUTABLE}
            # -q   # Enable for quiet mode
            -b ${DOC_TARGET}
            -d "${SPHINX_CACHE_DIR}"
            # -c "${BINARY_BUILD_DIR}" # enable if using cmake-generated conf.py
            "${CMAKE_CURRENT_SOURCE_DIR}/doc"
            "${CMAKE_CURRENT_BINARY_DIR}/doc/${DOC_TARGET}"
            COMMENT "Building ${DOC_TARGET} documentation with Sphinx")

         add_dependencies(doc ${DOC_TARGET})
      endforeach()

      message("Building documentation enabled for: ${DOC_TARGETS}")
   endif()
endif ()

# Disabled by OBS
option(JANSSON_WITHOUT_TESTS "Don't build tests ('make test' to execute tests)" ON)

if (NOT JANSSON_WITHOUT_TESTS)
   option(JANSSON_TEST_WITH_VALGRIND "Enable valgrind tests." OFF)

   ENABLE_TESTING()

   if (JANSSON_TEST_WITH_VALGRIND)
      # TODO: Add FindValgrind.cmake instead of having a hardcoded path.

      add_definitions(-DVALGRIND)

      # enable valgrind
      set(CMAKE_MEMORYCHECK_COMMAND valgrind)
      set(CMAKE_MEMORYCHECK_COMMAND_OPTIONS
         "--error-exitcode=1 --leak-check=full --show-reachable=yes --track-origins=yes -q")

      set(MEMCHECK_COMMAND
         "${CMAKE_MEMORYCHECK_COMMAND} ${CMAKE_MEMORYCHECK_COMMAND_OPTIONS}")
      separate_arguments(MEMCHECK_COMMAND)
   endif ()

   #
   # Test suites.
   #
   if (CMAKE_COMPILER_IS_GNUCC)
      add_definitions(-Wall -Wextra -Wdeclaration-after-statement)
   endif ()

   set(api_tests
         test_array
         test_copy
         test_dump
         test_dump_callback
         test_equal
         test_load
         test_loadb
         test_number
         test_object
         test_pack
         test_simple
         test_unpack)

   # Doing arithmetic on void pointers is not allowed by Microsofts compiler
   # such as secure_malloc and secure_free is doing, so exclude it for now.
   if (NOT MSVC)
      list(APPEND api_tests test_memory_funcs)
   endif()

   # Helper macro for building and linking a test program.
   macro(build_testprog name dir)
       add_executable(${name} ${dir}/${name}.c)
       add_dependencies(${name} jansson)
       target_link_libraries(${name} jansson)
   endmacro(build_testprog)

   # Create executables and tests/valgrind tests for API tests.
   foreach (test ${api_tests})
      build_testprog(${test} ${PROJECT_SOURCE_DIR}/test/suites/api)

      if (JANSSON_TEST_WITH_VALGRIND)
         add_test(memcheck__${test}
             ${MEMCHECK_COMMAND} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${test}
             WORKING_DIRECTORY ${JANSSON_TEMP_DIR})
      else()
         add_test(${test}
             ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${test}
             WORKING_DIRECTORY ${JANSSON_TEMP_DIR})
      endif ()
   endforeach ()

   # Test harness for the suites tests.
   build_testprog(json_process ${PROJECT_SOURCE_DIR}/test/bin)

   set(SUITE_TEST_CMD ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/json_process)
   set(SUITES encoding-flags valid invalid invalid-unicode)
   foreach (SUITE ${SUITES})
       file(GLOB TESTDIRS ${jansson_SOURCE_DIR}/test/suites/${SUITE}/*)

       foreach (TESTDIR ${TESTDIRS})
         if (IS_DIRECTORY ${TESTDIR})
            get_filename_component(TNAME ${TESTDIR} NAME)

            if (JANSSON_TEST_WITH_VALGRIND)
               add_test(memcheck__${SUITE}__${TNAME}
                        ${MEMCHECK_COMMAND} ${SUITE_TEST_CMD} ${TESTDIR})
            else()
               add_test(${SUITE}__${TNAME}
                        ${SUITE_TEST_CMD} ${TESTDIR})
            endif()

            if ((${SUITE} STREQUAL "valid" OR ${SUITE} STREQUAL "invalid") AND NOT EXISTS ${TESTDIR}/nostrip)
               if (JANSSON_TEST_WITH_VALGRIND)
                  add_test(memcheck__${SUITE}__${TNAME}__strip
                           ${MEMCHECK_COMMAND} ${SUITE_TEST_CMD} --strip ${TESTDIR})
               else()
                  add_test(${SUITE}__${TNAME}__strip
                           ${SUITE_TEST_CMD} --strip ${TESTDIR})
               endif()
            endif ()
         endif ()
       endforeach ()
   endforeach ()

   if (JANSSON_COVERAGE)
      setup_target_for_coverage(
            coverage             # Coverage make target "make coverage".
            coverage             # Name of output directory.
            make                 # Name of test runner executable.
            test)                # Arguments to the test runner above (make test).

      if (JANSSON_COVERALLS)
         set(COVERAGE_SRCS ${JANSSON_SRC})
         coveralls_setup("${COVERAGE_SRCS}" ${JANSSON_COVERALLS_UPLOAD})
      endif ()
   endif ()

   # Enable using "make check" just like the autotools project.
   # By default cmake creates a target "make test"
   add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND}
                     DEPENDS json_process ${api_tests})
endif ()

#
# Installation preparation.
#

# Allow the user to override installation directories.
set(JANSSON_INSTALL_LIB_DIR       lib CACHE PATH "Installation directory for libraries")
set(JANSSON_INSTALL_BIN_DIR       bin CACHE PATH "Installation directory for executables")
set(JANSSON_INSTALL_INCLUDE_DIR   include CACHE PATH "Installation directory for header files")

if(WIN32 AND NOT CYGWIN)
  set(DEF_INSTALL_CMAKE_DIR cmake)
else()
  set(DEF_INSTALL_CMAKE_DIR lib/cmake/jansson)
endif()

set(JANSSON_INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH "Installation directory for CMake files")

# Create pkg-conf file.
# (We use the same files as ./configure does, so we
#  have to defined the same variables used there).
set(prefix      ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${CMAKE_INSTALL_PREFIX})
set(libdir      ${CMAKE_INSTALL_PREFIX}/${JANSSON_INSTALL_LIB_DIR})
set(VERSION     ${JANSSON_DISPLAY_VERSION})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/jansson.pc.in
               ${CMAKE_CURRENT_BINARY_DIR}/jansson.pc @ONLY)

# Make sure the paths are absolute.
foreach(p LIB BIN INCLUDE CMAKE)
    set(var JANSSON_INSTALL_${p}_DIR)
    if(NOT IS_ABSOLUTE "${${var}}")
        set(${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
    endif()
endforeach()

# Export targets (This is used for other CMake projects to easily find the libraries and include files).
export(TARGETS jansson
        FILE "${PROJECT_BINARY_DIR}/JanssonTargets.cmake")
export(PACKAGE jansson)

# Generate the config file for the build-tree.
set(JANSSON__INCLUDE_DIRS
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_BINARY_DIR}/include")
set(JANSSON_INCLUDE_DIRS ${JANSSON__INCLUDE_DIRS} CACHE PATH "Jansson include directories")
configure_file(${PROJECT_SOURCE_DIR}/cmake/JanssonConfig.cmake.in
                ${PROJECT_BINARY_DIR}/JanssonConfig.cmake
                @ONLY)

# Generate the config file for the installation tree.
file(RELATIVE_PATH
    REL_INCLUDE_DIR
    "${JANSSON_INSTALL_CMAKE_DIR}"
    "${JANSSON_INSTALL_INCLUDE_DIR}") # Calculate the relative directory from the Cmake dir.

# Note the EVENT_CMAKE_DIR is defined in JanssonConfig.cmake.in,
# we escape it here so it's evaluated when it is included instead
# so that the include dirs are given relative to where the
# config file is located.
set(JANSSON__INCLUDE_DIRS
    "\${JANSSON_CMAKE_DIR}/${REL_INCLUDE_DIR}")
configure_file(${PROJECT_SOURCE_DIR}/cmake/JanssonConfig.cmake.in
                ${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/JanssonConfig.cmake
                @ONLY)

# Generate version info for both build-tree and install-tree.
configure_file(${PROJECT_SOURCE_DIR}/cmake/JanssonConfigVersion.cmake.in
                ${PROJECT_BINARY_DIR}/JanssonConfigVersion.cmake
                @ONLY)

# Define the public headers.
set_target_properties(jansson PROPERTIES PUBLIC_HEADER "${JANSSON_HDR_PUBLIC}")
#TODO: fix this.

#
# Install targets.
#
# Disabled by OBS
option(JANSSON_INSTALL "Generate installation target" OFF)
if (JANSSON_INSTALL)
install(TARGETS jansson
        EXPORT JanssonTargets
        LIBRARY DESTINATION "${JANSSON_INSTALL_LIB_DIR}" COMPONENT lib
        ARCHIVE DESTINATION "${JANSSON_INSTALL_LIB_DIR}" COMPONENT lib
        RUNTIME DESTINATION "${JANSSON_INSTALL_BIN_DIR}" COMPONENT lib # Windows DLLs
        PUBLIC_HEADER DESTINATION "${JANSSON_INSTALL_INCLUDE_DIR}" COMPONENT dev)

# Install the pkg-config.
install (FILES
         ${CMAKE_CURRENT_BINARY_DIR}/jansson.pc
         DESTINATION ${JANSSON_INSTALL_LIB_DIR}/pkgconfig COMPONENT dev)

# Install the configs.
install(FILES
    ${PROJECT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/JanssonConfig.cmake
    ${PROJECT_BINARY_DIR}/JanssonConfigVersion.cmake
    DESTINATION "${JANSSON_INSTALL_CMAKE_DIR}" COMPONENT dev)

# Install exports for the install-tree.
install(EXPORT JanssonTargets
        DESTINATION "${JANSSON_INSTALL_CMAKE_DIR}" COMPONENT dev)
endif()

# For use when simply using add_library from a parent project to build jansson.
set(JANSSON_LIBRARIES jansson CACHE STRING "Jansson libraries")

target_include_directories(jansson
        PUBLIC src "${CMAKE_CURRENT_BINARY_DIR}/include")
