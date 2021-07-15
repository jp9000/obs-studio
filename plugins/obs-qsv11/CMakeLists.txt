project(obs-qsv11)

add_library(obs-qsv11 MODULE)
add_library(OBS::qsv11 ALIAS obs-qsv11)

add_library(libmfx INTERFACE)
add_library(OBS::libmfx ALIAS libmfx)

target_sources(
  libmfx
  INTERFACE libmfx/src/main.cpp
            libmfx/src/mfx_critical_section.cpp
            libmfx/src/mfx_dispatcher.cpp
            libmfx/src/mfx_dispatcher_log.cpp
            libmfx/src/mfx_driver_store_loader.cpp
            libmfx/src/mfx_dxva2_device.cpp
            libmfx/src/mfx_function_table.cpp
            libmfx/src/mfx_library_iterator.cpp
            libmfx/src/mfx_load_dll.cpp
            libmfx/src/mfx_load_plugin.cpp
            libmfx/src/mfx_plugin_hive.cpp
            libmfx/src/mfx_win_reg_key.cpp
            libmfx/include/msdk/include/mfxadapter.h
            libmfx/include/msdk/include/mfxastructures.h
            libmfx/include/msdk/include/mfxaudio.h
            libmfx/include/msdk/include/mfxaudio++.h
            libmfx/include/msdk/include/mfxcommon.h
            libmfx/include/msdk/include/mfxdefs.h
            libmfx/include/msdk/include/mfxjpeg.h
            libmfx/include/msdk/include/mfxmvc.h
            libmfx/include/msdk/include/mfxplugin.h
            libmfx/include/msdk/include/mfxplugin++.h
            libmfx/include/msdk/include/mfxsession.h
            libmfx/include/msdk/include/mfxstructures.h
            libmfx/include/msdk/include/mfxvideo.h
            libmfx/include/msdk/include/mfxvideo++.h
            libmfx/include/msdk/include/mfxvstructures.h
            libmfx/include/mfx_critical_section.h
            libmfx/include/mfx_dispatcher.h
            libmfx/include/mfx_dispatcher_defs.h
            libmfx/include/mfx_dispatcher_log.h
            libmfx/include/mfx_driver_store_loader.h
            libmfx/include/mfx_dxva2_device.h
            libmfx/include/mfx_exposed_functions_list.h
            libmfx/include/mfx_library_iterator.h
            libmfx/include/mfx_load_dll.h
            libmfx/include/mfx_load_plugin.h
            libmfx/include/mfx_plugin_hive.h
            libmfx/include/mfx_vector.h
            libmfx/include/mfx_win_reg_key.h
            libmfx/include/mfxaudio_exposed_functions_list.h)

target_include_directories(
  libmfx
  INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libmfx/include/msdk/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libmfx/include>")

target_sources(
  obs-qsv11
  PRIVATE obs-qsv11.c
          obs-qsv11-plugin-main.c
          common_directx9.cpp
          common_directx9.h
          common_directx11.cpp
          common_directx11.h
          common_utils.cpp
          common_utils.h
          common_utils_windows.cpp
          device_directx9.cpp
          device_directx9.h
          QSV_Encoder.cpp
          QSV_Encoder.h
          QSV_Encoder_Internal.cpp
          QSV_Encoder_Internal.h
          bits/linux_defs.h
          bits/windows_defs.h)

target_link_libraries(
  obs-qsv11
  PRIVATE OBS::libobs
          OBS::libmfx
          d3d9
          d3d11
          dxva2
          dxgi
          dxguid)

target_compile_definitions(obs-qsv11 PRIVATE DX11_D3D)

if(OS_WINDOWS)
  set(MODULE_DESCRIPTION "OBS QSV encoder")
  configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
                 obs-qsv11.rc)

  target_sources(obs-qsv11 PRIVATE obs-qsv11.rc)

  target_compile_definitions(
    obs-qsv11 PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS
                      _CRT_NONSTDC_NO_WARNINGS)
endif()

set_target_properties(obs-qsv11 PROPERTIES FOLDER "plugins")

file(GLOB _OBS_QSV11_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.c
     ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB _OBS_QSV11_HEADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.h
     ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

source_group("obs-qsv11\\Source Files" FILES ${_OBS_QSV11_SOURCE_FILES})
source_group("obs-qsv11\\Header Files" FILES ${_OBS_QSV11_HEADER_FILES})

get_target_property(_LIBMFX_SOURCES OBS::libmfx INTERFACE_SOURCES)

foreach(_LIBMFX_SOURCE ${_LIBMFX_SOURCES})
  get_filename_component(_EXT ${_LIBMFX_SOURCE} EXT)
  if(${_EXT} STREQUAL "hpp" OR ${_EXT} STREQUAL "h")
    source_group("libmfx\\Header Files" FILES ${_LIBMFX_SOURCE})
  elseif(${_EXT} STREQUAL "cpp" OR ${_EXT} STREQUAL "c")
    source_group("libmfx\\Source Files" FILES ${_LIBMFX_SOURCE})
  endif()
endforeach()

setup_plugin_target(obs-qsv11)
