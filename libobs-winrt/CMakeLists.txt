project(libobs-winrt)

add_library(libobs-winrt MODULE)
add_library(OBS::libobs-winrt ALIAS libobs-winrt)

target_sources(libobs-winrt PRIVATE winrt-capture.cpp winrt-capture.h
                                    winrt-dispatch.cpp winrt-dispatch.h)

target_precompile_headers(
  libobs-winrt
  PRIVATE
  [["../libobs/util/windows/ComPtr.hpp"]]
  <obs-module.h>
  <d3d11.h>
  <DispatcherQueue.h>
  <dwmapi.h>
  <Windows.Graphics.Capture.Interop.h>
  <winrt/Windows.Foundation.Metadata.h>
  <winrt/Windows.Graphics.Capture.h>
  <winrt/Windows.System.h>)

target_link_libraries(libobs-winrt PRIVATE OBS::libobs Dwmapi windowsapp)

target_compile_features(libobs-winrt PRIVATE cxx_std_17)

set_target_properties(
  libobs-winrt
  PROPERTIES OUTPUT_NAME libobs-winrt
             FOLDER "core"
             PREFIX "")

setup_binary_target(libobs-winrt)
