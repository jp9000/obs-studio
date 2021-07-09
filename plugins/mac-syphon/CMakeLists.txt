project(mac-syphon)

find_package(OpenGL REQUIRED)

find_library(COCOA Cocoa)
find_library(IOSURF IOSurface)
find_library(SCRIPTINGBRIDGE ScriptingBridge)

mark_as_advanced(COCOA IOSURF SCRIPTINGBRIDGE)

add_library(mac-syphon MODULE)
add_library(OBS::syphon ALIAS mac-syphon)

add_library(syphon-framework STATIC)
add_library(Syphon::Framework ALIAS syphon-framework)

target_sources(mac-syphon PRIVATE syphon.m plugin-main.c)

target_sources(
  syphon-framework
  PRIVATE syphon-framework/Syphon_Prefix.pch
          syphon-framework/Syphon.h
          syphon-framework/SyphonBuildMacros.h
          syphon-framework/SyphonCFMessageReceiver.m
          syphon-framework/SyphonCFMessageReceiver.h
          syphon-framework/SyphonCFMessageSender.h
          syphon-framework/SyphonCFMessageSender.m
          syphon-framework/SyphonClient.m
          syphon-framework/SyphonClient.h
          syphon-framework/SyphonClientConnectionManager.m
          syphon-framework/SyphonClientConnectionManager.h
          syphon-framework/SyphonDispatch.c
          syphon-framework/SyphonDispatch.h
          syphon-framework/SyphonIOSurfaceImage.m
          syphon-framework/SyphonIOSurfaceImage.h
          syphon-framework/SyphonImage.m
          syphon-framework/SyphonImage.h
          syphon-framework/SyphonMachMessageReceiver.m
          syphon-framework/SyphonMachMessageReceiver.h
          syphon-framework/SyphonMachMessageSender.m
          syphon-framework/SyphonMachMessageSender.h
          syphon-framework/SyphonMessageQueue.m
          syphon-framework/SyphonMessageQueue.h
          syphon-framework/SyphonMessageReceiver.m
          syphon-framework/SyphonMessageReceiver.h
          syphon-framework/SyphonMessageSender.m
          syphon-framework/SyphonMessageSender.h
          syphon-framework/SyphonMessaging.m
          syphon-framework/SyphonMessaging.h
          syphon-framework/SyphonOpenGLFunctions.c
          syphon-framework/SyphonOpenGLFunctions.h
          syphon-framework/SyphonPrivate.m
          syphon-framework/SyphonPrivate.h
          syphon-framework/SyphonServer.m
          syphon-framework/SyphonServer.h
          syphon-framework/SyphonServerConnectionManager.m
          syphon-framework/SyphonServerConnectionManager.h
          syphon-framework/SyphonServerDirectory.m
          syphon-framework/SyphonServerDirectory.h)

target_link_libraries(mac-syphon PRIVATE OBS::libobs Syphon::Framework
                                         ${SCRIPTINGBRIDGE})

target_link_libraries(syphon-framework PUBLIC OpenGL::GL ${COCOA} ${IOSURF})

target_compile_options(
  mac-syphon
  PRIVATE -include
          ${CMAKE_CURRENT_SOURCE_DIR}/syphon-framework/Syphon_Prefix.pch
          -fobjc-arc)

target_compile_options(
  syphon-framework
  PRIVATE -include
          ${CMAKE_CURRENT_SOURCE_DIR}/syphon-framework/Syphon_Prefix.pch
          -Wno-deprecated-declarations)

target_compile_definitions(syphon-framework
                           PUBLIC "SYPHON_UNIQUE_CLASS_NAME_PREFIX=OBS_")

set_target_properties(mac-syphon PROPERTIES FOLDER "plugins" PREFIX "")

set_target_properties(syphon-framework PROPERTIES FOLDER "plugins/mac-syphon"
                                                  PREFIX "")

setup_plugin_target(mac-syphon)
