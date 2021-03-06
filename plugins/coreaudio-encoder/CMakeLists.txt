project(coreaudio-encoder)

set(coreaudio-encoder_SOURCES
	encoder.cpp)

if (WIN32)
	# Set compiler flag before adding resource file
	if (MINGW)
		set_source_files_properties(${coreaudio-encoder_SOURCES}
			PROPERTIES COMPILE_FLAGS "-Wno-multichar")
	endif()

	set(MODULE_DESCRIPTION "OBS Core Audio encoder")
	configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in coreaudio-encoder.rc)
	list(APPEND coreaudio-encoder_SOURCES
		coreaudio-encoder.rc)
	set(coreaudio-encoder_HEADERS windows-imports.h)
	set(coreaudio-encoder_LIBS )
else()
	find_library(COREFOUNDATION CoreFoundation)
	find_library(COREAUDIO CoreAudio)
	find_library(AUDIOTOOLBOX AudioToolbox)
	set(coreaudio-encoder_HEADERS )
	set(coreaudio-encoder_LIBS
		${COREFOUNDATION}
		${COREAUDIO}
		${AUDIOTOOLBOX})

	include_directories(${COREFOUNDATION}
		${COREAUDIO}
		${AUDIOTOOLBOX})
endif()

add_library(coreaudio-encoder MODULE
	${coreaudio-encoder_SOURCES}
	${coreaudio-encoder_HEADERS})

target_link_libraries(coreaudio-encoder
	libobs
	${coreaudio-encoder_LIBS})
set_target_properties(coreaudio-encoder PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(coreaudio-encoder data)
