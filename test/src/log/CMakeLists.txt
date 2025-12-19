# --- log::Log1 ---
get_filename_component(file_name "Log1.c" NAME_WE)

set(standards 17 23)
foreach(std ${standards})
  utils_check_std_c(STANDARD ${std} RESULT ret)
  if(ret)
    set(target_name "${file_name}_std_c${std}")
    add_executable(${target_name} "Log1.c")

    cmake_path(SET _gen NORMALIZE "_gen_log_std_c${std}.txt")
    target_compile_definitions(${target_name}
                               PRIVATE "GEN_FILE_NAME=\"${_gen}\"")
    set_property(
      DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      APPEND
      PROPERTY ADDITIONAL_CLEAN_FILES ${_gen})

    set_target_properties(
      ${target_name}
      PROPERTIES C_STANDARD ${std}
                 C_STANDARD_REQUIRED ON
                 C_EXTENSIONS OFF)

    test_add_single_test(${target_name})
  endif()
endforeach()
