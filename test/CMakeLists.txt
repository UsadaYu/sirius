message(STATUS "------------")
message(STATUS "--- TEST ---")
message(STATUS "------------")

# --- Include ---
include(CheckCompilerFlag)

# --- Environment ---
set(TEST_ENV "")
set(TEST_ENV_MODIFICATION "")

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(var "")

  if(GLOB_LIBRARY_TYPE STREQUAL "SHARED")
    set(var "${SIRIUS_NAMESPACE}::${SIRIUS_C_LIBRARY_NAME}")
    list(APPEND TEST_ENV_MODIFICATION
         "PATH=path_list_prepend:$<TARGET_FILE_DIR:${var}>")
    set(var "${SIRIUS_NAMESPACE}::${SIRIUS_KIT_LIBRARY_NAME}")
    list(APPEND TEST_ENV_MODIFICATION
         "PATH=path_list_prepend:$<TARGET_FILE_DIR:${var}>")
    set(var "${SIRIUS_NAMESPACE}::${SIRIUS_THREAD_LIBRARY_NAME}")
    list(APPEND TEST_ENV_MODIFICATION
         "PATH=path_list_prepend:$<TARGET_FILE_DIR:${var}>")
    set(var "${SIRIUS_NAMESPACE}::${SIRIUS_FOUNDATION_LIBRARY_NAME}")
    list(APPEND TEST_ENV_MODIFICATION
         "PATH=path_list_prepend:$<TARGET_FILE_DIR:${var}>")
  endif()

  set(var "${SIRIUS_EXE_DAEMON_NAME}")
  list(APPEND TEST_ENV "SIRIUS_ENV_EXE_DAEMON_PATH=$<TARGET_FILE:${var}>")

  unset(var)
endif()

# --- Global Variables of Testing ---
set(TEST_COMPILE_DEFINITIONS "")
set(TEST_COMPILE_OPTIONS "")
set(TEST_INCLUDE_DIRECTORIES "")
set(TEST_LINK_DIRECTORIES "")
set(TEST_LINK_LIBRARIES "")
set(TEST_LINK_OPTIONS "")
set(TEST_SYSTEM_INCLUDE_DIRECTORIES "")

set(test_c_stds "")
set(test_cxx_stds "")

# --- Preprocessing ---
foreach(lang "c" "cxx")
  utils_query_compiler_config(ACTION "test_matrix" RESULT stds LANGUAGE ${lang})
  set(test_${lang}_stds ${stds})
  foreach(std IN LISTS stds)
    utils_check_compiler_flag(${lang} ${std} std_ret)
    if(NOT std_ret)
      list(REMOVE_ITEM test_${lang}_stds ${std})
    endif()
  endforeach()
endforeach()

# --- Compile Definitions ---
list(APPEND TEST_COMPILE_DEFINITIONS
     "_SIRIUS_LOG_LEVEL=${SIRIUS_TEST_LOG_LEVEL}")

# --- Compile Options ---
if(MSVC)
  list(APPEND TEST_COMPILE_OPTIONS "/W4")
else()
  list(APPEND TEST_COMPILE_OPTIONS "-Wall")
endif()

if(MSVC)
  # ``__cplusplus`` is not used in the code to make version judgments, here just
  # for insurance.
  list(APPEND TEST_COMPILE_OPTIONS "/Zc:__cplusplus")
endif()

# --- Include Directories ---
list(APPEND TEST_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR})

# --- Link Directories ---
list(APPEND TEST_LINK_DIRECTORIES ${SIRIUS_TEST_EXTRA_LINK_DIRECTORIES})

# --- Link Libraries ---
list(APPEND TEST_LINK_LIBRARIES ${SIRIUS_TEST_EXTRA_LINK_LIBRARIES})

list(APPEND TEST_LINK_LIBRARIES
     "${SIRIUS_NAMESPACE}::${SIRIUS_FOUNDATION_LIBRARY_NAME}")

# --- Link Options ---

# --- System Include Directories ---

# ---

#[================================[
test_add_single_test
--------------------

Create a test.

.. code-block:: cmake

  test_add_single_test([TARGET <variable>]
                       [DIRECTORY <variable>])

Options:

``TARGET``
  [in] Target name.

``DIRECTORY``
  [in] Target DIRECTORY.
#]================================]
function(test_add_single_test)
  set(options "")
  set(one_value_keywords TARGET DIRECTORY)
  set(multi_value_keywords "")
  cmake_parse_arguments(ARG "${options}" "${one_value_keywords}"
                        "${multi_value_keywords}" ${ARGN})
  utils_error_if_var_is_null(ARG_TARGET)

  utils_set_output_directory_to_mirror(TARGET ${ARG_TARGET} DIRECTORY
                                       ${ARG_DIRECTORY})

  target_compile_definitions(${ARG_TARGET}
                             PRIVATE "_SIRIUS_LOG_PRINT_NAME=\"${ARG_TARGET}\"")

  target_compile_definitions(${ARG_TARGET} PRIVATE ${TEST_COMPILE_DEFINITIONS})
  target_compile_options(${ARG_TARGET} PRIVATE ${TEST_COMPILE_OPTIONS})
  target_include_directories(${ARG_TARGET} PRIVATE ${TEST_INCLUDE_DIRECTORIES})
  target_link_directories(${ARG_TARGET} PRIVATE ${TEST_LINK_DIRECTORIES})
  target_link_libraries(${ARG_TARGET} PRIVATE ${TEST_LINK_LIBRARIES})
  target_link_options(${ARG_TARGET} PRIVATE ${TEST_LINK_OPTIONS})
  target_include_directories(${ARG_TARGET} SYSTEM
                             PRIVATE ${TEST_SYSTEM_INCLUDE_DIRECTORIES})

  set_target_properties(${ARG_TARGET} PROPERTIES POSITION_INDEPENDENT_CODE
                                                 ${SIRIUS_TEST_PIE_ENABLE})

  add_test(
    NAME ${ARG_TARGET}
    COMMAND ${ARG_TARGET}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  set_tests_properties(
    ${ARG_TARGET}
    PROPERTIES ENVIRONMENT_MODIFICATION
               "$<JOIN:${TEST_ENV_MODIFICATION},$<SEMICOLON>>" ENVIRONMENT
               "$<JOIN:${TEST_ENV},$<SEMICOLON>>")
endfunction()

#[================================[
test_add_exes_and_tests
---------------------------

Create a set of executables and their tests.

.. code-block:: cmake

  test_add_exes_and_tests([MAIN <variable>]
                          [LANGUAGE <variable>]
                          [DIRECTORY <variable>]
                          [SOURCES <variable>]
                          [TARGETS <list>)

Options:

``MAIN``
  [in] The file of the main function.

``LANGUAGE``
  [in] Language.
  Options: ``C``, ``CXX``.

``DIRECTORY``
  [in] Target directory.

``SOURCES``
  [in] Sources list.

``TARGETS``
  [out] Targets list.
#]================================]
function(test_add_exes_and_tests)
  set(options "")
  set(one_value_keywords MAIN LANGUAGE DIRECTORY)
  set(multi_value_keywords SOURCES TARGETS)
  cmake_parse_arguments(ARG "${options}" "${one_value_keywords}"
                        "${multi_value_keywords}" ${ARGN})
  utils_error_if_var_is_null(ARG_MAIN)
  utils_error_if_var_is_null(ARG_DIRECTORY)

  if(NOT ARG_LANGUAGE)
    utils_get_source_language(${ARG_MAIN} detected_lang)
    set(target_lang ${detected_lang})
  else()
    set(target_lang ${ARG_LANGUAGE})
  endif()
  string(TOUPPER "${target_lang}" target_lang)
  set(std_flags "")
  if(target_lang STREQUAL "C")
    set(std_flags ${test_c_stds})
  elseif(target_lang STREQUAL "CXX")
    set(std_flags ${test_cxx_stds})
  else()
    message(
      FATAL_ERROR "Unsupported or undetectable language for `${ARG_MAIN}`")
  endif()

  get_filename_component(main_name ${ARG_MAIN} NAME_WE)

  set(created_targets "")

  foreach(std_flag IN LISTS std_flags)
    utils_get_safe_name(std_flag suffix)
    set(target "${main_name}_${suffix}")
    if(TARGET ${target})
      message(AUTHOR_WARNING "Test target `${target}` already exists, skipping")
      continue()
    endif()

    add_executable(${target} "${ARG_MAIN}" ${ARG_SOURCES})

    target_compile_options(
      ${target} PRIVATE $<$<COMPILE_LANGUAGE:${target_lang}>:${std_flag}>)

    test_add_single_test(TARGET ${target} DIRECTORY ${ARG_DIRECTORY})

    list(APPEND created_targets ${target})
  endforeach()

  if(ARG_TARGETS)
    set(${ARG_TARGETS}
        ${created_targets}
        PARENT_SCOPE)
  endif()
endfunction()

# --- Test ---
add_subdirectory(c)
add_subdirectory(foundation)
add_subdirectory(kit)
add_subdirectory(thread)
