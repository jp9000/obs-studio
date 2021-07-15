project(seg_service)

find_package(RSSDK REQUIRED)

if(NOT TARGET RSS::SDK)
  message(FATAL_ERROR "OBS:   RSSDK not found")
endif()

include(IDLFileHelper)

add_executable(seg_service WIN32)

target_sources(
  seg_service
  PRIVATE SegProc.rgs
          seg_service.rgs
          seg_service.cpp
          SegProc.cpp
          stdafx.cpp
          seg_service.rc
          resource.h
          SegProc.h
          stdafx.h
          targetver.h
          xdlldata.h)

add_idl_files_with_tlb(seg_service_GENERATED_FILES seg_service.idl)

get_target_property(_RSSSDK_INCLUDE_DIRECTORY RSS::SDK
                    INTERFACE_INCLUDE_DIRECTORIES)

target_include_directories(
  seg_service PRIVATE "$<BUILD_INTERFACE:${_RSSSDK_INCLUDE_DIRECTORY}>")

if(MSVC)
  target_compile_definitions(seg_service
                             PRIVATE "$<IF:$<CONFIG:Debug>,/MTd,/MT>")
endif()
