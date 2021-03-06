if(NOT DEFINED APPDATA_RELEASE_DATE)
	if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
		execute_process(COMMAND git log --tags -1 --pretty=%cd --date=short
			OUTPUT_VARIABLE APPDATA_RELEASE_DATE
			WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
			OUTPUT_STRIP_TRAILING_WHITESPACE)
	else()
		file(TIMESTAMP "${CMAKE_SOURCE_DIR}/CMakeLists.txt" APPDATA_RELEASE_DATE "%Y-%m-%d")
	endif()
endif()

configure_file(
	com.obsproject.Studio.appdata.xml.in
	com.obsproject.Studio.appdata.xml)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/com.obsproject.Studio.appdata.xml
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo)

install(FILES com.obsproject.Studio.desktop
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

install(FILES ../forms/images/obs.png
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
	RENAME com.obsproject.Studio.png)
