# --- Include ---
include(CheckCompilerFlag)

# --- Global Dependencies of Testing ---
set(GLOB_TEST_DEPS_SIRIUS
    "${SIRIUS_TARGET_CMAKE_NAMESPACE}::${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}")

# --- Global Variables of Testing ---
set(GLOB_TEST_COMPILE_DEFINITIONS "")
set(GLOB_TEST_COMPILE_OPTIONS "")
set(GLOB_TEST_INCLUDE_DIRECTORIES "")
set(GLOB_TEST_LINK_DIRECTORIES "")
set(GLOB_TEST_LINK_LIBRARIES "")
set(GLOB_TEST_LINK_OPTIONS "")
set(GLOB_TEST_SYSTEM_INCLUDE_DIRECTORIES "")

# --- Compile Definitions ---
list(APPEND GLOB_TEST_COMPILE_DEFINITIONS
     "sirius_log_level=${SIRIUS_TEST_LOG_LEVEL}")

# --- Compile Options ---
if(MSVC)
  list(APPEND GLOB_TEST_COMPILE_OPTIONS "/W4")
else()
  list(APPEND GLOB_TEST_COMPILE_OPTIONS "-Wall")
endif()

if(MSVC)
  # ``__cplusplus`` is not used in the code to make version judgments, here just
  # for insurance.
  list(APPEND GLOB_TEST_COMPILE_OPTIONS "/Zc:__cplusplus")
endif()

# --- Include Directories ---
list(APPEND GLOB_TEST_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR})

# --- Link Directories ---
list(APPEND GLOB_TEST_LINK_DIRECTORIES ${SIRIUS_TEST_EXTRA_LINK_DIRECTORIES})

# --- Link Libraries ---
list(APPEND GLOB_TEST_LINK_LIBRARIES ${GLOB_TEST_DEPS_SIRIUS})
list(APPEND GLOB_TEST_LINK_LIBRARIES ${SIRIUS_TEST_EXTRA_LINK_LIBRARIES})

# --- Link Options ---

# --- System Include Directories ---

# --- Pic ---
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(SIRIUS_TEST_PIE_ENABLE)
    list(APPEND GLOB_TEST_COMPILE_OPTIONS "-fPIC")
    list(APPEND GLOB_TEST_COMPILE_OPTIONS "-fPIE")

    list(APPEND GLOB_TEST_LINK_OPTIONS "-pie")
  else()
    list(APPEND GLOB_TEST_COMPILE_OPTIONS "-fno-PIC")
    list(APPEND GLOB_TEST_COMPILE_OPTIONS "-fno-PIE")

    list(APPEND GLOB_TEST_LINK_OPTIONS "-no-pie")
  endif()
endif()

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
                             PRIVATE "sirius_log_module_name=\"${ARG_TARGET}\"")

  target_compile_definitions(${ARG_TARGET}
                             PRIVATE ${GLOB_TEST_COMPILE_DEFINITIONS})
  target_compile_options(${ARG_TARGET} PRIVATE ${GLOB_TEST_COMPILE_OPTIONS})
  target_include_directories(${ARG_TARGET}
                             PRIVATE ${GLOB_TEST_INCLUDE_DIRECTORIES})
  target_link_directories(${ARG_TARGET} PRIVATE ${GLOB_TEST_LINK_DIRECTORIES})
  target_link_libraries(${ARG_TARGET} PRIVATE ${GLOB_TEST_LINK_LIBRARIES})
  target_link_options(${ARG_TARGET} PRIVATE ${GLOB_TEST_LINK_OPTIONS})
  target_include_directories(${ARG_TARGET} SYSTEM
                             PRIVATE ${GLOB_TEST_SYSTEM_INCLUDE_DIRECTORIES})

  add_test(
    NAME ${ARG_TARGET}
    COMMAND ${ARG_TARGET}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND SIRIUS_BUILD_TYPE STREQUAL
                                              "SHARED")
    set_tests_properties(
      ${ARG_TARGET}
      PROPERTIES
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:${GLOB_TEST_DEPS_SIRIUS}>")
  endif()
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
  set(valid_langs "C" "CXX")
  if(NOT ${target_lang} IN_LIST valid_langs)
    message(
      FATAL_ERROR "Unsupported or undetectable language for `${ARG_MAIN}`")
  endif()

  utils_query_compiler_config(ACTION "test_matrix" RESULT std_flags LANGUAGE
                              ${target_lang})
  get_filename_component(main_name ${ARG_MAIN} NAME_WE)

  set(created_targets "")

  foreach(std_flag IN LISTS std_flags)
    utils_check_compiler_flag(${target_lang} ${std_flag} std_ret)
    if(NOT std_ret)
      continue()
    endif()
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
add_subdirectory(cond)
add_subdirectory(lock)
add_subdirectory(log)
add_subdirectory(queue)
add_subdirectory(sem)
add_subdirectory(thread)
add_subdirectory(time)
