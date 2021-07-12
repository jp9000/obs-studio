include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

# Set compile options for MSVC
if(OS_WINDOWS AND MSVC)
  if(NOT EXISTS "${CMAKE_BINARY_DIR}/ALL_BUILD.vcxproj.user")
    file(
      GENERATE
      OUTPUT "${CMAKE_BINARY_DIR}/ALL_BUILD.vcxproj.user"
      INPUT "${CMAKE_SOURCE_DIR}/cmake/bundle/windows/ALL_BUILD.vcxproj.user.in"
    )
  endif()

  # Check for Win SDK version 10.0.19041 or above
  if(MSVC_VERSION LESS 1920)
    message(
      INFO
      "  + OBS-Studio - Windows API version is ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}"
    )
    string(REPLACE "." ";" WINAPI_VER
                   "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")

    list(GET WINAPI_VER 0 WINAPI_VER_MAJOR)
    list(GET WINAPI_VER 1 WINAPI_VER_MINOR)
    list(GET WINAPI_VER 2 WINAPI_VER_BUILD)

    set(WINAPI_COMPATIBLE FALSE)
    if(WINAPI_VER_MAJOR EQUAL 10)
      if(WINAPI_VER_MINOR EQUAL 0)
        if(WINAPI_VER_BUILD GREATER_EQUAL 19041)
          set(WINAPI_COMPATIBLE TRUE)
        endif()
      else()
        set(WINAPI_COMPATIBLE TRUE)
      endif()
    elseif(WINAPI_VER_MAJOR GREATER 10)
      set(WINAPI_COMPATIBLE TRUE)
    endif()

    if(NOT WINAPI_COMPATIBLE)
      message(
        FATAL_ERROR
          "OBS: OBS requires Windows 10 SDK version 10.0.19041.0 and above to compile.\n"
          "     Please download the most recent Windows 10 SDK in order to compile (or update to Visual Studio 2019)."
      )
    endif()
  endif()

  add_compile_options(
    "/MP;/W4;/wd4127;/wd4201;/wd4456;/wd4457;/wd4458;/wd4459;/wd4595"
    "$<$<CONFIG:DEBUG>:/DDEBUG=1;/D_DEBUG=1>"
    "$<$<CONFIG:RELWITHDEBINFO>:/Ob2>"
    "/DUNICODE;/D_UNICODE;/D_CRT_SECURE_NO_WARNINGS;/D_CRT_NONSTDC_NO_WARNINGS")

  add_link_options(
    "LINKER:/OPT:REF"
    "$<$<NOT:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>>:LINKER\:/SAFESEH\:NO>"
    "$<$<CONFIG:DEBUG>:LINKER\:/INCREMENTAL:NO>"
    "$<$<CONFIG:RELWITHDEBINFO>:LINKER\:/INCREMENTAL:NO;/OPT:ICF>")
else()
  add_compile_options(
    "-Wextra;-Wvla;-Wno-unused-function;-Wno-missing-field-initializers;-fno-strict-aliasing"
    "$<$<COMPILE_LANGUAGE:C>:-Werror-implicit-function-declaration;-Wno-missing-braces>"
    "$<$<BOOL:${USE_LIBCXX}>:-stdlib=libc++>"
    "$<$<CONFIG:DEBUG>:-DDEBUG=1;-D_DEBUG=1>"
    "$<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang>:-fcolor-diagnostics>"
    "$<$<COMPILE_LANG_AND_ID:C,AppleClang,Clang>:-fcolor-diagnostics>")

  if(OBS_CODESIGN_LINKER)
    add_link_options("LINKER:$<$<PLATFORM_ID:Darwin>:-adhoc_codesign>")
  endif()

  if(MINGW)
    set(CMAKE_WIDL
        "widl"
        CACHE INTERNAL "wine IDL header file generation program")
    add_compile_definitions("_WIN32_WINNT=0x0600;WINVER=0x0600")
  endif()
endif()

if(MSVC_CXX_ARCHITECTURE_ID)
  string(TOLOWER ${MSVC_CXX_ARCHITECTURE_ID} LOWERCASE_CMAKE_SYSTEM_PROCESSOR)
else()
  string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} LOWERCASE_CMAKE_SYSTEM_PROCESSOR)
endif()

if(LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES
   "(i[3-6]86|x86|x64|x86_64|amd64|e2k)")
  if(NOT MSVC)
    set(ARCH_SIMD_FLAGS "-mmmx;-msse;-msse2")
  endif()
elseif(LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)64(le)?")
  set(ARCH_SIMD_DEFINES "-DNO_WARN_X86_INTRINSICS")
  set(ARCH_SIMD_FLAGS "-mvsx")
else()
  if(CMAKE_C_COMPILER_ID MATCHES "^(Apple)?Clang|GNU"
     OR CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang|GNU")
    check_c_compiler_flag("-fopenmp-simd" C_COMPILER_SUPPORTS_OPENMP_SIMD)
    check_cxx_compiler_flag("-fopenmp-simd" CXX_COMPILER_SUPPORTS_OPENMP_SIMD)
    set(ARCH_SIMD_FLAGS
        "-DSIMDE_ENABLE_OPENMP;"
        "$<$<AND:$<COMPILE_LANGUAGE:C>,$<BOOL:C_COMPILER_SUPPORTS_OPENMP_SIMD>>:-fopenmp-simd>;"
        "$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<BOOL:CXX_COMPILER_SUPPORTS_OPENMP_SIMD>>:-fopenmp-simd>"
    )
  endif()
endif()

if(LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "e2k")
  foreach(
    TEST_C_FLAG
    "-Wno-unused-parameter"
    "-Wno-ignored-qualifiers"
    "-Wno-pointer-sign"
    "-Wno-unused-variable"
    "-Wno-sign-compare"
    "-Wno-bad-return-value-type"
    "-Wno-maybe-uninitialized")
    check_c_compiler_flag(${TEST_C_FLAG}
                          C_COMPILER_SUPPORTS_FLAG_${TEST_C_FLAG})
    if(C_COMPILER_SUPPORTS_FLAG_${TEST_C_FLAG})
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TEST_C_FLAG}")
    endif()
  endforeach()
  foreach(TEST_CXX_FLAG "-Wno-invalid-offsetof" "-Wno-maybe-uninitialized")
    check_cxx_compiler_flag(${TEST_CXX_FLAG}
                            CXX_COMPILER_SUPPORTS_FLAG_${TEST_CXX_FLAG})
    if(CXX_COMPILER_SUPPORTS_FLAG_${TEST_CXX_FLAG})
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TEST_CXX_FLAG}")
    endif()
  endforeach()
endif()
