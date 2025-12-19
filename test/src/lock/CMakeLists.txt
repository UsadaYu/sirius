get_filename_component(file_name "Lock1.c" NAME_WE)

set(standards 17 23)

# --- Lock1-Spinlock ---
foreach(std ${standards})
  utils_check_std_c(STANDARD ${std} RESULT ret)
  if(ret)
    set(target_name "${file_name}-Spinlock_std_c${std}")
    add_executable(${target_name} "Lock1.c")

    target_compile_definitions(${target_name} PRIVATE "LOCK_TYPE=0")

    set_target_properties(
      ${target_name}
      PROPERTIES C_STANDARD ${std}
                 C_STANDARD_REQUIRED ON
                 C_EXTENSIONS OFF)

    test_add_single_test(${target_name})
  endif()
endforeach()

# --- Lock1-Mutex ---
foreach(std ${standards})
  utils_check_std_c(STANDARD ${std} RESULT ret)
  if(ret)
    set(target_name "${file_name}-Mutex_std_c${std}")
    add_executable(${target_name} "Lock1.c")

    target_compile_definitions(${target_name} PRIVATE "LOCK_TYPE=1")

    set_target_properties(
      ${target_name}
      PROPERTIES C_STANDARD ${std}
                 C_STANDARD_REQUIRED ON
                 C_EXTENSIONS OFF)

    test_add_single_test(${target_name})
  endif()
endforeach()
