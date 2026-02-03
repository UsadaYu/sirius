get_filename_component(main_name "Lock1.c" NAME_WE)

utils_query_compiler_config(ACTION "test_matrix" RESULT std_flags LANGUAGE "c")

# ---
set(_libsirius_c "${SIRIUS_NAMESPACE}::${SIRIUS_C_LIBRARY_NAME}")
set(_test_link_libraries "${TEST_LINK_LIBRARIES}")
list(APPEND TEST_LINK_LIBRARIES ${_libsirius_c})
set(target_lang "C")

# --- Lock1-Spinlock ---
foreach(std_flag IN LISTS std_flags)
  utils_check_compiler_flag(${target_lang} ${std_flag} std_ret)
  if(NOT std_ret)
    continue()
  endif()
  utils_get_safe_name(std_flag suffix)
  set(target "${main_name}-Spinlock_${suffix}")
  if(TARGET ${target})
    message(AUTHOR_WARNING "Test target `${target}` already exists, skipping")
    continue()
  endif()

  add_executable(${target} "Lock1.c")
  target_compile_options(
    ${target} PRIVATE $<$<COMPILE_LANGUAGE:${target_lang}>:${std_flag}>)
  target_compile_definitions(${target} PRIVATE "LOCK_TYPE=0")

  test_add_single_test(TARGET ${target} DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endforeach()

# --- Lock1-Mutex ---
set(target_lang "C")
foreach(std_flag IN LISTS std_flags)
  utils_check_compiler_flag(${target_lang} ${std_flag} std_ret)
  if(NOT std_ret)
    continue()
  endif()
  utils_get_safe_name(std_flag suffix)
  set(target "${main_name}-Mutex_${suffix}")
  if(TARGET ${target})
    message(AUTHOR_WARNING "Test target `${target}` already exists, skipping")
    continue()
  endif()

  add_executable(${target} "Lock1.c")
  target_compile_options(
    ${target} PRIVATE $<$<COMPILE_LANGUAGE:${target_lang}>:${std_flag}>)
  target_compile_definitions(${target} PRIVATE "LOCK_TYPE=1")

  test_add_single_test(TARGET ${target} DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endforeach()

# ---
set(TEST_LINK_LIBRARIES "${_test_link_libraries}")

# --- Lock2-Mutex ---
test_add_exes_and_tests(MAIN "Lock2-Mutex.cpp" LANGUAGE "CXX" DIRECTORY
                        ${CMAKE_CURRENT_BINARY_DIR})
