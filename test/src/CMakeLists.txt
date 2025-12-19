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
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  list(APPEND GLOB_TEST_COMPILE_OPTIONS "/W4")
else()
  list(APPEND GLOB_TEST_COMPILE_OPTIONS "-Wall")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
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

  test_add_single_test(<target>)

Options:

``TARGET``
  [in] Target name.
#]================================]
macro(test_add_single_test TARGET)
  utils_set_output_directory_to_mirror(${TARGET})

  target_compile_definitions(${TARGET}
                             PRIVATE "sirius_log_module_name=\"${TARGET}\"")

  target_compile_definitions(${TARGET} PRIVATE ${GLOB_TEST_COMPILE_DEFINITIONS})
  target_compile_options(${TARGET} PRIVATE ${GLOB_TEST_COMPILE_OPTIONS})
  target_include_directories(${TARGET} PRIVATE ${GLOB_TEST_INCLUDE_DIRECTORIES})
  target_link_directories(${TARGET} PRIVATE ${GLOB_TEST_LINK_DIRECTORIES})
  target_link_libraries(${TARGET} PRIVATE ${GLOB_TEST_LINK_LIBRARIES})
  target_link_options(${TARGET} PRIVATE ${GLOB_TEST_LINK_OPTIONS})
  target_include_directories(${TARGET} SYSTEM
                             PRIVATE ${GLOB_TEST_SYSTEM_INCLUDE_DIRECTORIES})

  add_test(
    NAME ${TARGET}
    COMMAND ${TARGET}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND SIRIUS_BUILD_TYPE STREQUAL
                                              "SHARED")
    set_tests_properties(
      ${TARGET}
      PROPERTIES
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:${GLOB_TEST_DEPS_SIRIUS}>")
  endif()
endmacro()

#[================================[
test_add_exes_and_tests
---------------------------

Create a set of executables and their tests.

.. code-block:: cmake

  test_add_exes_and_tests([MAIN <variable>]
                          [SOURCES <variable>]
                          [TARGETS <list>)

Options:

``MAIN``
  [in] The file of the main function.

``SOURCES``
  [in] Sources list.

``TARGETS``
  [out] Targets list.
#]================================]
macro(test_add_exes_and_tests)
  set(_test_options "")
  set(_test_one_value_keywords MAIN TARGETS)
  set(_test_multi_value_keywords SOURCES)
  cmake_parse_arguments(ARG "${_test_options}" "${_test_one_value_keywords}"
                        "${_test_multi_value_keywords}" ${ARGN})

  utils_error_if_var_is_null(ARG_MAIN)

  get_filename_component(_test_main ${ARG_MAIN} NAME_WE)
  utils_get_source_language(${ARG_MAIN} _test_language)

  if(_test_language STREQUAL "C")
    set(_test_standards 17 23)
    foreach(_test_std ${_test_standards})
      utils_check_std_c(STANDARD ${_test_std} RESULT _test_ret)
      if(_test_ret)
        set(_test_target "${_test_main}_std_c${_test_std}")
        add_executable(${_test_target} "${ARG_MAIN}" "${ARG_SOURCES}")
        set_target_properties(
          ${_test_target}
          PROPERTIES C_STANDARD ${_test_std}
                     C_STANDARD_REQUIRED ON
                     C_EXTENSIONS OFF)
        test_add_single_test(${_test_target})
        list(APPEND ${ARG_TARGETS} ${_test_target})
      endif()
    endforeach()
  elseif(_test_language STREQUAL "CXX")
    set(_test_standards 20 23 26)
    foreach(_test_std ${_test_standards})
      utils_check_std_cxx(STANDARD ${_test_std} RESULT _test_ret)
      if(_test_ret)
        set(_test_target "${_test_main}_std_cxx${_test_std}")
        add_executable(${_test_target} "${ARG_MAIN}" "${ARG_SOURCES}")
        set_target_properties(
          ${_test_target}
          PROPERTIES CXX_STANDARD ${_test_std}
                     CXX_STANDARD_REQUIRED ON
                     CXX_EXTENSIONS OFF)
        test_add_single_test(${_test_target})
        list(APPEND ${ARG_TARGETS} ${_test_target})
      endif()
    endforeach()
  else()
    message(FATAL_ERROR "An invalid file: ${ARG_MAIN}")
  endif()
  unset(_test_target)
  unset(_test_ret)
  unset(_test_std)
  unset(_test_standards)

  unset(_test_language)
  unset(_test_main)

  unset(_test_multi_value_keywords)
  unset(_test_one_value_keywords)
  unset(_test_options)
endmacro()

# --- Test ---
add_subdirectory(cond)
add_subdirectory(lock)
add_subdirectory(log)
add_subdirectory(queue)
add_subdirectory(sem)
add_subdirectory(thread)
add_subdirectory(time)
