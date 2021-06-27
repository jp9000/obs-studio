if(BUILD_TESTS)
  add_subdirectory(test-input)

  if(OS_WINDOWS)
    add_subdirectory(win)
  endif()

  if(OS_POSIX)
    add_subdirectory(osx)
  endif()
endif()

if(ENABLE_UNIT_TESTS)
  add_subdirectory(cmocka)
endif()
