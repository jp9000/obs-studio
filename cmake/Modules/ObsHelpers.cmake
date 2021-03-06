set(OBS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/rundir")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_lib_suffix 64)
else()
	set(_lib_suffix 32)
endif()

if(WIN32 OR APPLE)
	set(_struct_def FALSE)
else()
	set(_struct_def TRUE)
endif()

option(INSTALLER_RUN "Build a multiarch installer, needs to run indenepdently after both archs have compiled" FALSE)
option(UNIX_STRUCTURE "Build with standard unix filesystem structure" ${_struct_def})
if(APPLE)
	option(BUILD_REDISTRIBUTABLE "Fix rpath of external libraries" FALSE)
endif()

if(INSTALLER_RUN AND NOT DEFINED ENV{obsInstallerTempDir})
	message(FATAL_ERROR "Environment variable obsInstallerTempDir is needed for multiarch installer generation")
endif()

if(DEFINED ENV{obsInstallerTempDir})
	file(TO_CMAKE_PATH "$ENV{obsInstallerTempDir}" ENV{obsInstallerTempDir})
endif()

if(DEFINED ENV{obsAdditionalInstallFiles})
	file(TO_CMAKE_PATH "$ENV{obsAdditionalInstallFiles}" ENV{obsAdditionalInstallFiles})
else()
	set(ENV{obsAdditionalInstallFiles} "${CMAKE_SOURCE_DIR}/additional_install_files")
endif()

list(APPEND CMAKE_INCLUDE_PATH
	"$ENV{obsAdditionalInstallFiles}/include${_lib_suffix}"
	"$ENV{obsAdditionalInstallFiles}/include")

list(APPEND CMAKE_LIBRARY_PATH
	"$ENV{obsAdditionalInstallFiles}/lib${_lib_suffix}"
	"$ENV{obsAdditionalInstallFiles}/lib"
	"$ENV{obsAdditionalInstallFiles}/libs${_lib_suffix}"
	"$ENV{obsAdditionalInstallFiles}/libs"
	"$ENV{obsAdditionalInstallFiles}/bin${_lib_suffix}"
	"$ENV{obsAdditionalInstallFiles}/bin")

if(NOT UNIX_STRUCTURE)
	set(OBS_DATA_DESTINATION "data")
	if(APPLE)
		set(OBS_EXECUTABLE_DESTINATION "bin")
		set(OBS_EXECUTABLE32_DESTINATION "bin")
		set(OBS_EXECUTABLE64_DESTINATION "bin")
		set(OBS_LIBRARY_DESTINATION "bin")
		set(OBS_LIBRARY32_DESTINATION "bin")
		set(OBS_LIBRARY64_DESTINATION "bin")
		set(OBS_PLUGIN_DESTINATION "obs-plugins")
		set(OBS_PLUGIN32_DESTINATION "obs-plugins")
		set(OBS_PLUGIN64_DESTINATION "obs-plugins")

		set(OBS_DATA_PATH "../${OBS_DATA_DESTINATION}")
		set(OBS_INSTALL_PREFIX "")
		set(OBS_RELATIVE_PREFIX "../")

		set(OBS_SCRIPT_PLUGIN_DESTINATION "${OBS_DATA_DESTINATION}/obs-scripting")
	else()
		set(OBS_EXECUTABLE_DESTINATION "bin/${_lib_suffix}bit")
		set(OBS_EXECUTABLE32_DESTINATION "bin/32bit")
		set(OBS_EXECUTABLE64_DESTINATION "bin/64bit")
		set(OBS_LIBRARY_DESTINATION "bin/${_lib_suffix}bit")
		set(OBS_LIBRARY32_DESTINATION "bin/32bit")
		set(OBS_LIBRARY64_DESTINATION "bin/64bit")
		set(OBS_PLUGIN_DESTINATION "obs-plugins/${_lib_suffix}bit")
		set(OBS_PLUGIN32_DESTINATION "obs-plugins/32bit")
		set(OBS_PLUGIN64_DESTINATION "obs-plugins/64bit")

		set(OBS_DATA_PATH "../../${OBS_DATA_DESTINATION}")
		set(OBS_INSTALL_PREFIX "")
		set(OBS_RELATIVE_PREFIX "../../")

		set(OBS_SCRIPT_PLUGIN_DESTINATION "${OBS_DATA_DESTINATION}/obs-scripting/${_lib_suffix}bit")
	endif()
	set(OBS_CMAKE_DESTINATION "cmake")
	set(OBS_INCLUDE_DESTINATION "include")
	set(OBS_UNIX_STRUCTURE "0")

	set(OBS_SCRIPT_PLUGIN_PATH "${OBS_RELATIVE_PREFIX}${OBS_SCRIPT_PLUGIN_DESTINATION}")
else()
	if(NOT OBS_MULTIARCH_SUFFIX AND DEFINED ENV{OBS_MULTIARCH_SUFFIX})
		set(OBS_MULTIARCH_SUFFIX "$ENV{OBS_MULTIARCH_SUFFIX}")
	endif()

	set(OBS_EXECUTABLE_DESTINATION "bin")
	set(OBS_EXECUTABLE32_DESTINATION "bin32")
	set(OBS_EXECUTABLE64_DESTINATION "bin64")
	set(OBS_LIBRARY_DESTINATION "lib${OBS_MULTIARCH_SUFFIX}")
	set(OBS_LIBRARY32_DESTINATION "lib32")
	set(OBS_LIBRARY64_DESTINATION "lib64")
	set(OBS_PLUGIN_DESTINATION "${OBS_LIBRARY_DESTINATION}/obs-plugins")
	set(OBS_PLUGIN32_DESTINATION "${OBS_LIBRARY32_DESTINATION}/obs-plugins")
	set(OBS_PLUGIN64_DESTINATION "${OBS_LIBRARY64_DESTINATION}/obs-plugins")
	set(OBS_DATA_DESTINATION "share/obs")
	set(OBS_CMAKE_DESTINATION "${OBS_LIBRARY_DESTINATION}/cmake")
	set(OBS_INCLUDE_DESTINATION "include/obs")

	set(OBS_DATA_PATH "${OBS_DATA_DESTINATION}")
	set(OBS_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/")
	set(OBS_RELATIVE_PREFIX "../")
	set(OBS_UNIX_STRUCTURE "1")

	set(OBS_SCRIPT_PLUGIN_DESTINATION "${OBS_LIBRARY_DESTINATION}/obs-scripting")
	set(OBS_SCRIPT_PLUGIN_PATH "${OBS_INSTALL_PREFIX}${OBS_SCRIPT_PLUGIN_DESTINATION}")
endif()

function(obs_finish_bundle)
	if(NOT APPLE OR UNIX_STRUCTURE)
		return()
	endif()

	install(CODE
		"if(DEFINED ENV{FIXUP_BUNDLE})
			execute_process(COMMAND \"${CMAKE_SOURCE_DIR}/cmake/osxbundle/fixup_bundle.sh\" . bin WORKING_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}\")
		endif()")
endfunction()

function(obs_generate_multiarch_installer)
	install(DIRECTORY "$ENV{obsInstallerTempDir}/"
		DESTINATION "."
		USE_SOURCE_PERMISSIONS)
endfunction()

function(obs_helper_copy_dir target target_configs source dest)
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}"
			"-DCONFIG=$<CONFIGURATION>"
			"-DTARGET_CONFIGS=${target_configs}"
			"-DINPUT=${source}"
			"-DOUTPUT=${dest}"
			-P "${CMAKE_SOURCE_DIR}/cmake/copy_helper.cmake"
		VERBATIM)
endfunction()

function(obs_install_additional maintarget)
	set(addfdir "${CMAKE_SOURCE_DIR}/additional_install_files")
	if(DEFINED ENV{obsAdditionalInstallFiles})
		set(addfdir "$ENV{obsAdditionalInstallFiles}")
	endif()

	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(_lib_suffix 64)
	else()
		set(_lib_suffix 32)
	endif()

	install(DIRECTORY "${addfdir}/misc/"
		DESTINATION "."
		USE_SOURCE_PERMISSIONS
		PATTERN ".gitignore" EXCLUDE)
	install(DIRECTORY "${addfdir}/data/"
		DESTINATION "${OBS_DATA_DESTINATION}"
		USE_SOURCE_PERMISSIONS
		PATTERN ".gitignore" EXCLUDE)

	if(INSTALLER_RUN)
		install(DIRECTORY "${addfdir}/libs32/"
			DESTINATION "${OBS_LIBRARY32_DESTINATION}"
			USE_SOURCE_PERMISSIONS
			PATTERN ".gitignore" EXCLUDE)
		install(DIRECTORY "${addfdir}/exec32/"
			DESTINATION "${OBS_EXECUTABLE32_DESTINATION}"
			USE_SOURCE_PERMISSIONS
			PATTERN ".gitignore" EXCLUDE)
		install(DIRECTORY "${addfdir}/libs64/"
			DESTINATION "${OBS_LIBRARY64_DESTINATION}"
			USE_SOURCE_PERMISSIONS
			PATTERN ".gitignore" EXCLUDE)
		install(DIRECTORY "${addfdir}/exec64/"
			DESTINATION "${OBS_EXECUTABLE64_DESTINATION}"
			USE_SOURCE_PERMISSIONS
			PATTERN ".gitignore" EXCLUDE)

		install(DIRECTORY "${addfdir}/libs32d/"
			DESTINATION "${OBS_LIBRARY32_DESTINATION}"
			USE_SOURCE_PERMISSIONS
			CONFIGURATIONS Debug
			PATTERN ".gitignore" EXCLUDE)
		install(DIRECTORY "${addfdir}/exec32d/"
			DESTINATION "${OBS_EXECUTABLE32_DESTINATION}"
			USE_SOURCE_PERMISSIONS
			CONFIGURATIONS Debug
			PATTERN ".gitignore" EXCLUDE)
		install(DIRECTORY "${addfdir}/libs64d/"
			DESTINATION "${OBS_LIBRARY64_DESTINATION}"
			USE_SOURCE_PERMISSIONS
			CONFIGURATIONS Debug
			PATTERN ".gitignore" EXCLUDE)
		install(DIRECTORY "${addfdir}/exec64d/"
			DESTINATION "${OBS_EXECUTABLE64_DESTINATION}"
			USE_SOURCE_PERMISSIONS
			CONFIGURATIONS Debug
			PATTERN ".gitignore" EXCLUDE)

		install(DIRECTORY "${addfdir}/libs32r/"
			DESTINATION "${OBS_LIBRARY32_DESTINATION}"
			USE_SOURCE_PERMISSIONS
			CONFIGURATIONS Release RelWithDebInfo MinSizeRel
			PATTERN ".gitignore" EXCLUDE)
		install(DIRECTORY "${addfdir}/exec32r/"
			DESTINATION "${OBS_EXECUTABLE32_DESTINATION}"
			USE_SOURCE_PERMISSIONS
			CONFIGURATIONS Release RelWithDebInfo MinSizeRel
			PATTERN ".gitignore" EXCLUDE)
		install(DIRECTORY "${addfdir}/libs64r/"
			DESTINATION "${OBS_LIBRARY64_DESTINATION}"
			USE_SOURCE_PERMISSIONS
			CONFIGURATIONS Release RelWithDebInfo MinSizeRel
			PATTERN ".gitignore" EXCLUDE)
		install(DIRECTORY "${addfdir}/exec64r/"
			DESTINATION "${OBS_EXECUTABLE64_DESTINATION}"
			USE_SOURCE_PERMISSIONS
			CONFIGURATIONS Release RelWithDebInfo MinSizeRel
			PATTERN ".gitignore" EXCLUDE)
	else()
		install(DIRECTORY "${addfdir}/libs${_lib_suffix}/"
			DESTINATION "${OBS_LIBRARY_DESTINATION}"
			USE_SOURCE_PERMISSIONS
			PATTERN ".gitignore" EXCLUDE)
		install(DIRECTORY "${addfdir}/exec${_lib_suffix}/"
			DESTINATION "${OBS_EXECUTABLE_DESTINATION}"
			USE_SOURCE_PERMISSIONS
			PATTERN ".gitignore" EXCLUDE)

		install(DIRECTORY "${addfdir}/libs${_lib_suffix}d/"
			DESTINATION "${OBS_LIBRARY_DESTINATION}"
			USE_SOURCE_PERMISSIONS
			CONFIGURATIONS Debug
			PATTERN ".gitignore" EXCLUDE)
		install(DIRECTORY "${addfdir}/exec${_lib_suffix}d/"
			DESTINATION "${OBS_EXECUTABLE_DESTINATION}"
			USE_SOURCE_PERMISSIONS
			CONFIGURATIONS Debug
			PATTERN ".gitignore" EXCLUDE)

		install(DIRECTORY "${addfdir}/libs${_lib_suffix}r/"
			DESTINATION "${OBS_LIBRARY_DESTINATION}"
			USE_SOURCE_PERMISSIONS
			CONFIGURATIONS Release RelWithDebInfo MinSizeRel
			PATTERN ".gitignore" EXCLUDE)
		install(DIRECTORY "${addfdir}/exec${_lib_suffix}r/"
			DESTINATION "${OBS_EXECUTABLE_DESTINATION}"
			USE_SOURCE_PERMISSIONS
			CONFIGURATIONS Release RelWithDebInfo MinSizeRel
			PATTERN ".gitignore" EXCLUDE)
	endif()

	obs_helper_copy_dir(${maintarget} ALL
		"${addfdir}/misc/"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/")
	obs_helper_copy_dir(${maintarget} ALL
		"${addfdir}/data/"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/${OBS_DATA_DESTINATION}/")
	obs_helper_copy_dir(${maintarget} ALL
		"${addfdir}/libs${_lib_suffix}/"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/${OBS_LIBRARY_DESTINATION}/")
	obs_helper_copy_dir(${maintarget} ALL
		"${addfdir}/exec${_lib_suffix}/"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/${OBS_EXECUTABLE_DESTINATION}/")

	obs_helper_copy_dir(${maintarget} "Release;MinSizeRel;RelWithDebInfo"
		"${addfdir}/exec${_lib_suffix}r/"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/${OBS_EXECUTABLE_DESTINATION}/")
	obs_helper_copy_dir(${maintarget} "Debug"
		"${addfdir}/exec${_lib_suffix}d/"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/${OBS_EXECUTABLE_DESTINATION}/")
	obs_helper_copy_dir(${maintarget} "Release;MinSizeRel;RelWithDebInfo"
		"${addfdir}/libs${_lib_suffix}r/"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/${OBS_LIBRARY_DESTINATION}/")
	obs_helper_copy_dir(${maintarget} "Debug"
		"${addfdir}/libs${_lib_suffix}d/"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/${OBS_LIBRARY_DESTINATION}/")
endfunction()

function(export_obs_core target exportname)
	install(TARGETS ${target}
		EXPORT "${exportname}Target"
		LIBRARY DESTINATION "${OBS_LIBRARY_DESTINATION}"
		ARCHIVE DESTINATION "${OBS_LIBRARY_DESTINATION}"
		RUNTIME DESTINATION "${OBS_EXECUTABLE_DESTINATION}")

	export(TARGETS ${target} FILE "${CMAKE_CURRENT_BINARY_DIR}/${exportname}Target.cmake")
	export(PACKAGE "${exportname}")

	set(CONF_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
	set(CONF_PLUGIN_DEST "${CMAKE_BINARY_DIR}/rundir/${CMAKE_BUILD_TYPE}/obs-plugins/${_lib_suffix}bit")
	set(CONF_PLUGIN_DEST32 "${CMAKE_BINARY_DIR}/rundir/${CMAKE_BUILD_TYPE}/obs-plugins/32bit")
	set(CONF_PLUGIN_DEST64 "${CMAKE_BINARY_DIR}/rundir/${CMAKE_BUILD_TYPE}/obs-plugins/64bit")
	set(CONF_PLUGIN_DATA_DEST "${CMAKE_BINARY_DIR}/rundir/${CMAKE_BUILD_TYPE}/data/obs-plugins")
	configure_file("${exportname}Config.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/${exportname}Config.cmake" @ONLY)

	file(RELATIVE_PATH _pdir "${CMAKE_INSTALL_PREFIX}/${OBS_CMAKE_DESTINATION}/${exportname}" "${CMAKE_INSTALL_PREFIX}")
	set(CONF_INCLUDE_DIRS "\${CMAKE_CURRENT_LIST_DIR}/${_pdir}${OBS_INCLUDE_DESTINATION}")
	set(CONF_PLUGIN_DEST "\${CMAKE_CURRENT_LIST_DIR}/${_pdir}${OBS_PLUGIN_DESTINATION}")
	set(CONF_PLUGIN_DEST32 "\${CMAKE_CURRENT_LIST_DIR}/${_pdir}${OBS_PLUGIN32_DESTINATION}")
	set(CONF_PLUGIN_DEST64 "\${CMAKE_CURRENT_LIST_DIR}/${_pdir}${OBS_PLUGIN64_DESTINATION}")
	set(CONF_PLUGIN_DATA_DEST "\${CMAKE_CURRENT_LIST_DIR}/${_pdir}${OBS_DATA_DESTINATION}/obs-plugins")
	configure_file("${exportname}Config.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/${exportname}Config.cmake" @ONLY)

	set(_pdir)

	configure_file("${exportname}ConfigVersion.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/${exportname}ConfigVersion.cmake" @ONLY)

	install(FILES
		"${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/${exportname}Config.cmake"
		"${CMAKE_CURRENT_BINARY_DIR}/${exportname}ConfigVersion.cmake"
		DESTINATION "${OBS_CMAKE_DESTINATION}/${exportname}")

	install(EXPORT "${exportname}Target"
		DESTINATION "${OBS_CMAKE_DESTINATION}/${exportname}")
endfunction()

function(install_obs_headers)
	foreach(hdr ${ARGN})
		if(IS_ABSOLUTE "${hdr}")
			set(subdir)
		else()
			get_filename_component(subdir "${hdr}" DIRECTORY)
			if(subdir)
				set(subdir "/${subdir}")
			endif()
		endif()
		install(FILES "${hdr}" DESTINATION "${OBS_INCLUDE_DESTINATION}${subdir}")
	endforeach()
endfunction()

function(obs_debug_copy_helper target dest)
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}"
			"-DCONFIG=$<CONFIGURATION>"
			"-DFNAME=$<TARGET_FILE_NAME:${target}>"
			"-DINPUT=$<TARGET_FILE_DIR:${target}>"
			"-DOUTPUT=${dest}"
			-P "${CMAKE_SOURCE_DIR}/cmake/copy_on_debug_helper.cmake"
		VERBATIM)
endfunction()

function(install_obs_pdb ttype target)
	if(NOT MSVC)
		return()
	endif()

	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(_bit_suffix "64bit")
	else()
		set(_bit_suffix "32bit")
	endif()

	obs_debug_copy_helper(${target} "${CMAKE_CURRENT_BINARY_DIR}/pdbs")

	if("${ttype}" STREQUAL "PLUGIN")
		obs_debug_copy_helper(${target} "${OBS_OUTPUT_DIR}/$<CONFIGURATION>/obs-plugins/${_bit_suffix}")

		if(DEFINED ENV{obsInstallerTempDir})
			obs_debug_copy_helper(${target} "$ENV{obsInstallerTempDir}/${OBS_PLUGIN_DESTINATION}")
		endif()

		install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pdbs/"
			DESTINATION "${OBS_PLUGIN_DESTINATION}"
			CONFIGURATIONS Debug RelWithDebInfo)
	else()
		obs_debug_copy_helper(${target} "${OBS_OUTPUT_DIR}/$<CONFIGURATION>/bin/${_bit_suffix}")

		if(DEFINED ENV{obsInstallerTempDir})
			obs_debug_copy_helper(${target} "$ENV{obsInstallerTempDir}/${OBS_EXECUTABLE_DESTINATION}")
		endif()

		install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pdbs/"
			DESTINATION "${OBS_EXECUTABLE_DESTINATION}"
			CONFIGURATIONS Debug RelWithDebInfo)
	endif()
endfunction()

function(install_obs_core target)
	if(APPLE)
		set(_bit_suffix "")
	elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(_bit_suffix "64bit/")
	else()
		set(_bit_suffix "32bit/")
	endif()

	if("${ARGV1}" STREQUAL "EXPORT")
		export_obs_core("${target}" "${ARGV2}")
	else()
		install(TARGETS ${target}
			LIBRARY DESTINATION "${OBS_LIBRARY_DESTINATION}"
			RUNTIME DESTINATION "${OBS_EXECUTABLE_DESTINATION}")
	endif()

	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"$<TARGET_FILE:${target}>"
			"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/bin/${_bit_suffix}$<TARGET_FILE_NAME:${target}>"
		VERBATIM)

	if(DEFINED ENV{obsInstallerTempDir})
		get_property(target_type TARGET ${target} PROPERTY TYPE)
		if("${target_type}" STREQUAL "EXECUTABLE")
			set(tmp_target_dir "${OBS_EXECUTABLE_DESTINATION}")
		else()
			set(tmp_target_dir "${OBS_LIBRARY_DESTINATION}")
		endif()

		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy
				"$<TARGET_FILE:${target}>"
				"$ENV{obsInstallerTempDir}/${tmp_target_dir}/$<TARGET_FILE_NAME:${target}>"
			VERBATIM)
	endif()

	install_obs_pdb(CORE ${target})
endfunction()

function(install_obs_bin target mode)
	foreach(bin ${ARGN})
		if(APPLE)
			set(_bit_suffix "")
		elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(_bit_suffix "64bit/")
		else()
			set(_bit_suffix "32bit/")
		endif()

		if(NOT IS_ABSOLUTE "${bin}")
			set(bin "${CMAKE_CURRENT_SOURCE_DIR}/${bin}")
		endif()

		get_filename_component(fname "${bin}" NAME)

		if(NOT "${mode}" MATCHES "INSTALL_ONLY")
			add_custom_command(TARGET ${target} POST_BUILD
				COMMAND "${CMAKE_COMMAND}" -E copy
					"${bin}"
					"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/bin/${_bit_suffix}${fname}"
				VERBATIM)
		endif()

		install(FILES "${bin}"
			DESTINATION "${OBS_EXECUTABLE_DESTINATION}")

		if(DEFINED ENV{obsInstallerTempDir})
			add_custom_command(TARGET ${target} POST_BUILD
				COMMAND "${CMAKE_COMMAND}" -E copy
					"${bin}"
					"$ENV{obsInstallerTempDir}/${OBS_EXECUTABLE_DESTINATION}/${fname}"
				VERBATIM)
		endif()
	endforeach()
endfunction()

function(install_obs_plugin target)
	if(APPLE)
		set(_bit_suffix "")
	elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(_bit_suffix "64bit/")
	else()
		set(_bit_suffix "32bit/")
	endif()

	set_target_properties(${target} PROPERTIES
		PREFIX "")

	install(TARGETS ${target}
		LIBRARY DESTINATION "${OBS_PLUGIN_DESTINATION}"
		RUNTIME DESTINATION "${OBS_PLUGIN_DESTINATION}")
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"$<TARGET_FILE:${target}>"
			"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/obs-plugins/${_bit_suffix}$<TARGET_FILE_NAME:${target}>"
		VERBATIM)

	if(DEFINED ENV{obsInstallerTempDir})
		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy
				"$<TARGET_FILE:${target}>" "$ENV{obsInstallerTempDir}/${OBS_PLUGIN_DESTINATION}/$<TARGET_FILE_NAME:${target}>"
			VERBATIM)
	endif()

	install_obs_pdb(PLUGIN ${target})
endfunction()

function(install_obs_data target datadir datadest)
	install(DIRECTORY ${datadir}/
		DESTINATION "${OBS_DATA_DESTINATION}/${datadest}"
		USE_SOURCE_PERMISSIONS)
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_directory
			"${CMAKE_CURRENT_SOURCE_DIR}/${datadir}" "${OBS_OUTPUT_DIR}/$<CONFIGURATION>/data/${datadest}"
		VERBATIM)

	if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND DEFINED ENV{obsInstallerTempDir})
		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_directory
				"${CMAKE_CURRENT_SOURCE_DIR}/${datadir}" "$ENV{obsInstallerTempDir}/${OBS_DATA_DESTINATION}/${datadest}"
			VERBATIM)
	endif()
endfunction()

function(install_obs_data_from_abs_path target datadir datadest)
	install(DIRECTORY ${datadir}/
		DESTINATION "${OBS_DATA_DESTINATION}/${datadest}"
		USE_SOURCE_PERMISSIONS)
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_directory
			"${datadir}" "${OBS_OUTPUT_DIR}/$<CONFIGURATION>/data/${datadest}"
		VERBATIM)

	if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND DEFINED ENV{obsInstallerTempDir})
		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_directory
				"${datadir}" "$ENV{obsInstallerTempDir}/${OBS_DATA_DESTINATION}/${datadest}"
			VERBATIM)
	endif()
endfunction()

function(install_obs_data_file target datafile datadest)
	install(FILES ${datafile}
		DESTINATION "${OBS_DATA_DESTINATION}/${datadest}")
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E make_directory
			"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/data/${datadest}"
		VERBATIM)
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${CMAKE_CURRENT_SOURCE_DIR}/${datafile}" "${OBS_OUTPUT_DIR}/$<CONFIGURATION>/data/${datadest}"
		VERBATIM)

	if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND DEFINED ENV{obsInstallerTempDir})
		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E make_directory
				"$ENV{obsInstallerTempDir}/${OBS_DATA_DESTINATION}/${datadest}"
			VERBATIM)
		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy
				"${CMAKE_CURRENT_SOURCE_DIR}/${datafile}" "$ENV{obsInstallerTempDir}/${OBS_DATA_DESTINATION}/${datadest}"
			VERBATIM)
	endif()
endfunction()

function(install_obs_datatarget target datadest)
	install(TARGETS ${target}
		LIBRARY DESTINATION "${OBS_DATA_DESTINATION}/${datadest}"
		RUNTIME DESTINATION "${OBS_DATA_DESTINATION}/${datadest}")
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"$<TARGET_FILE:${target}>"
			"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/data/${datadest}/$<TARGET_FILE_NAME:${target}>"
		VERBATIM)

	if(DEFINED ENV{obsInstallerTempDir})
		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy
				"$<TARGET_FILE:${target}>"
				"$ENV{obsInstallerTempDir}/${OBS_DATA_DESTINATION}/${datadest}/$<TARGET_FILE_NAME:${target}>"
			VERBATIM)
	endif()
endfunction()

function(install_obs_plugin_with_data target datadir)
	install_obs_plugin(${target})
	install_obs_data(${target} "${datadir}" "obs-plugins/${target}")
endfunction()

function(define_graphic_modules target)
	foreach(dl_lib opengl d3d9 d3d11)
		string(TOUPPER ${dl_lib} dl_lib_upper)
		if(TARGET libobs-${dl_lib})
			if(UNIX AND UNIX_STRUCTURE)
				target_compile_definitions(${target}
					PRIVATE
					DL_${dl_lib_upper}="$<TARGET_SONAME_FILE_NAME:libobs-${dl_lib}>"
					)
			else()
				target_compile_definitions(${target}
					PRIVATE
					DL_${dl_lib_upper}="$<TARGET_FILE_NAME:libobs-${dl_lib}>"
					)
			endif()
		else()
			target_compile_definitions(${target}
				PRIVATE
				DL_${dl_lib_upper}=""
				)
		endif()
	endforeach()
endfunction()
