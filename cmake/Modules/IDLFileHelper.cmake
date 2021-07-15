macro(add_idl_files_base generated_files with_tlb)
  foreach(filename ${ARGN})
    get_filename_component(file_we ${filename} NAME_WE)
    get_filename_component(file_path ${filename} PATH)

    set(file_c ${file_we}_i.c)
    set(file_h ${file_we}.h)
    set(bin_file_h ${CMAKE_CURRENT_BINARY_DIR}/${file_h})
    set(bin_file_c ${CMAKE_CURRENT_BINARY_DIR}/${file_c})

    if(MSVC)
      if(${with_tlb})
        set(file_tlb ${file_we}.tlb)
        set(bin_file_tlb ${CMAKE_CURRENT_BINARY_DIR}/${file_tlb})
        set(tlb_opt "")
      else()
        set(tlb_opt "/notlb")
      endif()

      add_custom_command(
        OUTPUT ${bin_file_h} ${bin_file_c}
        DEPENDS ${filename}
        COMMAND midl /h ${file_h} /iid ${file_c} ${tlb_opt}
                ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    else()
      execute_process(
        COMMAND echo
        COMMAND ${CMAKE_C_COMPILER} -v -x c++ -E -
        ERROR_VARIABLE cpp_inc_output
        OUTPUT_QUIET ERROR_STRIP_TRAILING_WHITESPACE)

      string(REPLACE ";" " " include_dirs ${cpp_inc_output})
      string(REPLACE "\n" ";" include_dirs ${cpp_inc_output})

      set(include_params)
      foreach(include_dir ${include_dirs})
        string(SUBSTRING ${include_dir} 0 1 first_char)
        if(${first_char} STREQUAL " ")
          string(LENGTH "${include_dir}" include_dir_len)
          math(EXPR include_dir_len "${include_dir_len} - 1")
          string(SUBSTRING ${include_dir} 1 ${include_dir_len} include_dir)
          set(include_params "-I\"${include_dir}\" ${include_params}")
        endif()
      endforeach()

      if(WIN32)
        separate_arguments(include_params WINDOWS_COMMAND ${include_params})
      endif()

      add_custom_command(
        OUTPUT ${file_h}
        DEPENDS ${filename}
        COMMAND ${CMAKE_WIDL} ${include_params} -h -o ${file_h}
                ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

      file(WRITE ${bin_file_c} "#include <initguid.h>\n#include <${file_h}>\n")
    endif()

    set_source_files_properties(${bin_file_h} ${bin_file_c} PROPERTIES GENERATED
                                                                       TRUE)

    set(${generated_files} ${${generated_file}} ${bin_file_h} ${bin_file_c})

    set_source_files_properties(${filename} PROPERTIES HEADER_FILE_ONLY TRUE)
  endforeach(filename ${ARGN})
endmacro(add_idl_files_base)

macro(add_idl_files generated_files)
  add_idl_files_base(${generated_files} FALSE ${ARGN})
endmacro(add_idl_files)

macro(add_idl_files_with_tlb generated_files)
  add_idl_files_base(${generated_files} TRUE ${ARGN})
endmacro(add_idl_files_with_tlb)
