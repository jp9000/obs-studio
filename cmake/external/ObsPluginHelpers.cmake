# Functions for generating external plugins

set(EXTERNAL_PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/rundir")

# Fix XCode includes to ignore warnings on system includes
function(target_include_directories_system _target)
  if(XCODE)
    foreach(_arg ${ARGN})
      if("${_arg}" STREQUAL "PRIVATE" OR "${_arg}" STREQUAL "PUBLIC" OR "${_arg}" STREQUAL "INTERFACE")
        set(_scope ${_arg})
      else()
        target_compile_options(${_target} ${_scope} -isystem${_arg})
      endif()
    endforeach()
  else()
    target_include_directories(${_target} SYSTEM ${_scope} ${ARGN})
  endif()
endfunction()

function(install_external_plugin_data_internal target source_dir target_dir)
	install(DIRECTORY ${source_dir}/
		DESTINATION "${target}/${target_dir}"
		USE_SOURCE_PERMISSIONS)
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_directory
			"${CMAKE_CURRENT_SOURCE_DIR}/${source_dir}" "${EXTERNAL_PLUGIN_OUTPUT_DIR}/$<CONFIGURATION>/${target}/${target_dir}"
		VERBATIM)
endfunction()

# Installs data
# 'target' is the destination target project being installed to
# 'data_loc' specifies the directory of the data
function(install_external_plugin_data target data_loc)
	install_external_plugin_data_internal(${target} ${data_loc} "data")
endfunction()

# Installs data in an architecture-specific data directory on windows/linux (data/32bit or data/64bit).  Does not apply for mac.
# 'target' is the destination target project being installed to
# 'data_loc' specifies the directory of the data being installed
function(install_external_plugin_arch_data target data_loc)
	install_external_plugin_data_internal(${target} ${data_loc} "data${OBS_OUTPUT_DIR_BIT_SUFFIX}")
endfunction()

# Installs data in the target's bin directory
# 'target' is the destination target project being installed to
# 'data_loc' specifies the directory of the data being installed
function(install_external_plugin_data_to_bin target data_loc)
	install_external_plugin_data_internal(${target} ${data_loc} "bin${OBS_OUTPUT_DIR_BIT_SUFFIX}")
endfunction()

# Installs an additional binary to a target
# 'target' is the destination target project being installed to
# 'additional_target' specifies the additional binary
function(install_external_plugin_additional target additional_target)
	set_target_properties(${additional_target} PROPERTIES
		PREFIX "")

	install(TARGETS ${additional_target}
		LIBRARY DESTINATION "bin"
		RUNTIME DESTINATION "bin")
	add_custom_command(TARGET ${additional_target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"$<TARGET_FILE:${additional_target}>"
			"${EXTERNAL_PLUGIN_OUTPUT_DIR}/$<CONFIGURATION>/${target}/bin${OBS_OUTPUT_DIR_BIT_SUFFIX}/$<TARGET_FILE_NAME:${additional_target}>"
		VERBATIM)
endfunction()

# Installs the binary of the target
# 'target' is the target project being installed
function(install_external_plugin target)
	install_external_plugin_additional(${target} ${target})
endfunction()

# Installs the binary and data of the target
# 'target' is the destination target project being installed to
function(install_external_plugin_with_data target data_loc)
	install_external_plugin(${target})
	install_external_plugin_data(${target} ${data_loc})
endfunction()

# Installs an additional binary to the data of a target
# 'target' is the destination target project being installed to
# 'additional_target' specifies the additional binary
function(install_external_plugin_bin_to_data target additional_target)
	install(TARGETS ${additional_target}
		LIBRARY DESTINATION "data"
		RUNTIME DESTINATION "data")
	add_custom_command(TARGET ${additional_target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"$<TARGET_FILE:${additional_target}>"
			"${EXTERNAL_PLUGIN_OUTPUT_DIR}/$<CONFIGURATION>/${target}/data/$<TARGET_FILE_NAME:${additional_target}>"
		VERBATIM)
endfunction()

# Installs an additional binary in an architecture-specific data directory on windows/linux (data/32bit or data/64bit).  Does not apply for mac.
# 'target' is the destination target project being installed to
# 'additional_target' specifies the additional binary
function(install_external_plugin_bin_to_arch_data target additional_target)
	install(TARGETS ${additional_target}
		LIBRARY DESTINATION "data${OBS_OUTPUT_DIR_BIT_SUFFIX}"
		RUNTIME DESTINATION "data${OBS_OUTPUT_DIR_BIT_SUFFIX}")
	add_custom_command(TARGET ${additional_target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"$<TARGET_FILE:${additional_target}>"
			"${EXTERNAL_PLUGIN_OUTPUT_DIR}/$<CONFIGURATION>/${target}/data${OBS_OUTPUT_DIR_BIT_SUFFIX}/$<TARGET_FILE_NAME:${additional_target}>"
		VERBATIM)
endfunction()

# Installs an additional file in an architecture-specific data directory on windows/linux (data/32bit or data/64bit).  Does not apply for mac.
# 'target' is the destination target project being installed to
# 'additional_target' specifies the additional binary
function(install_external_plugin_data_file_to_arch_data target additional_target file_target)
	get_filename_component(file_target_name ${file_target} NAME)

	install(TARGETS ${additional_target}
		LIBRARY DESTINATION "data${OBS_OUTPUT_DIR_BIT_SUFFIX}"
		RUNTIME DESTINATION "data${OBS_OUTPUT_DIR_BIT_SUFFIX}")
	add_custom_command(TARGET ${additional_target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${file_target}"
			"${EXTERNAL_PLUGIN_OUTPUT_DIR}/$<CONFIGURATION>/${target}/data${OBS_OUTPUT_DIR_BIT_SUFFIX}/${file_target_name}"
		VERBATIM)
endfunction()
