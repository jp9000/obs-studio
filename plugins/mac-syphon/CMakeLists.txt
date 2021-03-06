project(mac-syphon)

find_library(COCOA Cocoa)
find_library(IOSURF IOSurface)
find_library(SCRIPTINGBRIDGE ScriptingBridge)
find_package(OpenGL REQUIRED)

include_directories(${COCOA}
		    ${IOSURF}
		    ${SCRIPTINGBIRDGE}
		    ${OPENGL_INCLUDE_DIR})

set(syphon_HEADERS
	syphon-framework/Syphon_Prefix.pch
	syphon-framework/Syphon.h
	syphon-framework/SyphonBuildMacros.h
	syphon-framework/SyphonCFMessageReceiver.h
	syphon-framework/SyphonCFMessageSender.h
	syphon-framework/SyphonClient.h
	syphon-framework/SyphonClientConnectionManager.h
	syphon-framework/SyphonDispatch.h
	syphon-framework/SyphonIOSurfaceImage.h
	syphon-framework/SyphonImage.h
	syphon-framework/SyphonMachMessageReceiver.h
	syphon-framework/SyphonMachMessageSender.h
	syphon-framework/SyphonMessageQueue.h
	syphon-framework/SyphonMessageReceiver.h
	syphon-framework/SyphonMessageSender.h
	syphon-framework/SyphonMessaging.h
	syphon-framework/SyphonOpenGLFunctions.h
	syphon-framework/SyphonPrivate.h
	syphon-framework/SyphonServer.h
	syphon-framework/SyphonServerConnectionManager.h
	syphon-framework/SyphonServerDirectory.h
	)

set(syphon_SOURCES
	syphon-framework/SyphonCFMessageReceiver.m
	syphon-framework/SyphonCFMessageSender.m
	syphon-framework/SyphonClient.m
	syphon-framework/SyphonClientConnectionManager.m
	syphon-framework/SyphonDispatch.c
	syphon-framework/SyphonImage.m
	syphon-framework/SyphonIOSurfaceImage.m
	syphon-framework/SyphonMachMessageReceiver.m
	syphon-framework/SyphonMachMessageSender.m
	syphon-framework/SyphonMessageQueue.m
	syphon-framework/SyphonMessageReceiver.m
	syphon-framework/SyphonMessageSender.m
	syphon-framework/SyphonMessaging.m
	syphon-framework/SyphonOpenGLFunctions.c
	syphon-framework/SyphonPrivate.m
	syphon-framework/SyphonServer.m
	syphon-framework/SyphonServerConnectionManager.m
	syphon-framework/SyphonServerDirectory.m
	)

add_library(syphon-framework ${syphon_HEADERS}
	${syphon_SOURCES})

set(mac-syphon_HEADERS
	)

set(mac-syphon_SOURCES
	syphon.m
	plugin-main.c)

add_definitions(-DSYPHON_UNIQUE_CLASS_NAME_PREFIX=OBS_ -include
	${PROJECT_SOURCE_DIR}/syphon-framework/Syphon_Prefix.pch)

add_library(mac-syphon MODULE
	${mac-syphon_SOURCES}
	${mac-syphon_HEADERS})

target_link_libraries(mac-syphon
	libobs
	syphon-framework
	${COCOA}
	${IOSURF}
	${SCRIPTINGBRIDGE}
	${OPENGL_gl_LIBRARY})

set_property (TARGET mac-syphon APPEND_STRING PROPERTY
	COMPILE_FLAGS "-fobjc-arc")
set_target_properties(mac-syphon PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(mac-syphon data)
