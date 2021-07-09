# Universal build for Apple Silicon
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")

project(mac-dal-plugin)

find_library(COCOA Cocoa)
find_library(COREMEDIA CoreMedia)
find_library(COREMEDIAIO CoreMediaIO)
find_library(COREVIDEO CoreVideo)

mark_as_advanced(COCOA COREMEDIA COREMEDIAIO COREVIDEO)

add_library(mac-dal-plugin MODULE)
add_library(OBS::mac-dal-plugin ALIAS mac-dal-plugin)

target_sources(
  mac-dal-plugin
  PRIVATE OBSDALPlugIn.mm
          OBSDALPlugIn.h
          OBSDALPlugInMain.mm
          OBSDALPlugInInterface.mm
          OBSDALPlugInInterface.h
          OBSDALObjectStore.mm
          OBSDALObjectStore.h
          OBSDALDevice.mm
          OBSDALDevice.h
          OBSDALMachClient.mm
          OBSDALMachClient.h
          CMSampleBufferUtils.mm
          OBSDALStream.mm
          OBSDALStream.h
          CMSampleBufferUtils.h
          Defines.h
          Logging.h
          ${CMAKE_CURRENT_SOURCE_DIR}/../common/MachProtocol.h)

target_include_directories(
  mac-dal-plugin
  PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/../common")

target_compile_options(mac-dal-plugin PRIVATE -fobjc-arc -fobjc-weak)

target_link_libraries(mac-dal-plugin PRIVATE ${COCOA} ${COREMEDIA}
                                             ${COREMEDIAIO} ${COREVIDEO})

set(MACOSX_PLUGIN_BUNDLE_TYPE "BNDL")

set_target_properties(
  mac-dal-plugin
  PROPERTIES BUNDLE ON
             BUNDLE_EXTENSION "plugin"
             OUTPUT_NAME "obs-mac-virtualcam"
             FOLDER "plugins"
             VERSION "0"
             SOVERSION "0"
             LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../../"
             MACOSX_BUNDLE_INFO_PLIST
             "${CMAKE_SOURCE_DIR}/cmake/bundle/macOS/Virtualcam-Info.plist.in")

setup_binary_target(mac-dal-plugin)
