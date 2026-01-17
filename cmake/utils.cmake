macro(utils_error_if_var_is_null VAR)
  if(NOT ${VAR})
    message(FATAL_ERROR "Invalid argument. Null argument: ${VAR}")
  endif()
endmacro()

macro(utils_set_output_directory_to_mirror)
  set(_utils_options "")
  set(_utils_one_value_keywords TARGET DIRECTORY)
  set(_utils_multi_value_keywords "")
  cmake_parse_arguments(ARG "${_utils_options}" "${_utils_one_value_keywords}"
                        "${_utils_multi_value_keywords}" ${ARGN})
  utils_error_if_var_is_null(ARG_TARGET)

  cmake_path(SET _utils_output_directory NORMALIZE "")
  if(ARG_DIRECTORY)
    set(_utils_output_directory ${ARG_DIRECTORY})
  else()
    set(_utils_output_directory ${CMAKE_CURRENT_BINARY_DIR})
  endif()

  foreach(_utils_suffix "" "_DEBUG" "_RELEASE" "_RELWITHDEBINFO" "_MINSIZEREL")
    set_target_properties(
      ${ARG_TARGET}
      PROPERTIES ARCHIVE_OUTPUT_DIRECTORY${_utils_suffix}
                 ${_utils_output_directory}
                 LIBRARY_OUTPUT_DIRECTORY${_utils_suffix}
                 ${_utils_output_directory}
                 RUNTIME_OUTPUT_DIRECTORY${_utils_suffix}
                 ${_utils_output_directory})
  endforeach()
  unset(_utils_suffix)

  unset(_utils_output_directory)

  unset(_utils_multi_value_keywords)
  unset(_utils_one_value_keywords)
  unset(_utils_options)
endmacro()

function(utils_get_safe_name NAME RESULT)
  string(REGEX REPLACE "\\+\\+" "xx" suffix "${${NAME}}")
  string(REGEX REPLACE "[^a-zA-Z0-9_.-]" "_" suffix "${suffix}")
  string(REGEX REPLACE "_+" "_" suffix "${suffix}")
  string(REGEX REPLACE "^[-_.]+|[-_.]+$" "" suffix "${suffix}")

  set(${RESULT}
      "${suffix}"
      PARENT_SCOPE)
endfunction()

#[================================[
utils_query_compiler_config
---------------------------

Query the configuration of the compilers.

.. code-block:: cmake

  utils_query_compiler_config([ACTION <variable>]
                              [RESULT <variable>]
                              [LANGUAGE <variable>])

The function checks the cxx compiler.

Options:

``ACTION``
  [in] Action.
  Options: ``min_version``, ``project_flags``, ``test_matrix``.

``RESULT``
  [out] Results.

``LANGUAGE``
  [in] Language (for ``project_flags`` / ``test_matrix``).
  Options: ``c``, ``cxx``.
#]================================]
set(UTILS_COMPILER_SCRIPT
    "${PROJECT_SOURCE_DIR}/scripts/build/compiler.py"
    CACHE INTERNAL "")
set(UTILS_COMPILER_JSON
    "${PROJECT_SOURCE_DIR}/config/build/compiler.json"
    CACHE INTERNAL "")
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(UTILS_BUILD_COMPILER_ID
        "clang-cl"
        CACHE INTERNAL "")
  else()
    set(UTILS_BUILD_COMPILER_ID
        "clang"
        CACHE INTERNAL "")
  endif()
else()
  set(UTILS_BUILD_COMPILER_ID
      ${CMAKE_CXX_COMPILER_ID}
      CACHE INTERNAL "")
endif()

set_property(
  DIRECTORY
  APPEND
  PROPERTY CMAKE_CONFIGURE_DEPENDS ${UTILS_COMPILER_JSON})

function(utils_query_compiler_config)
  set(options "")
  set(one_value_keywords ACTION RESULT LANGUAGE)
  set(multi_value_keywords "")
  cmake_parse_arguments(ARG "${options}" "${one_value_keywords}"
                        "${multi_value_keywords}" ${ARGN})
  utils_error_if_var_is_null(ARG_ACTION)
  utils_error_if_var_is_null(ARG_RESULT)

  set(language "")
  if(ARG_LANGUAGE)
    list(APPEND language "--lang" "${ARG_LANGUAGE}")
  endif()

  execute_process(
    COMMAND
      ${Python3_EXECUTABLE} ${UTILS_COMPILER_SCRIPT} "--json"
      ${UTILS_COMPILER_JSON} "--compiler" ${UTILS_BUILD_COMPILER_ID} "--action"
      ${ARG_ACTION} ${language}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  if(NOT result EQUAL 0)
    message(
      FATAL_ERROR
        "
  Fail to executable `${sirius_build_compiler_script}`:
  ${error}
  ")
  endif()

  set(${ARG_RESULT}
      "${output}"
      PARENT_SCOPE)
endfunction()

function(utils_get_source_language FILE RESULT)
  get_filename_component(file_ext "${FILE}" LAST_EXT)

  string(TOLOWER "${file_ext}" file_ext_lower)

  foreach(extension IN LISTS CMAKE_C_SOURCE_FILE_EXTENSIONS)
    if(${file_ext_lower} STREQUAL ".${extension}")
      set(${RESULT}
          "C"
          PARENT_SCOPE)
      return()
    endif()
  endforeach()

  foreach(extension IN LISTS CMAKE_CXX_SOURCE_FILE_EXTENSIONS)
    if(${file_ext_lower} STREQUAL ".${extension}")
      set(${RESULT}
          "CXX"
          PARENT_SCOPE)
      return()
    endif()
  endforeach()

  # other languages...

  set(${RESULT}
      "UNKNOWN"
      PARENT_SCOPE)
endfunction()

function(utils_check_compiler_flag LANG FLAG RESULT)
  string(TOLOWER "${LANG}" lang_lower)
  if(${lang_lower} STREQUAL "c")
    set(lang "C")
  elseif(${lang_lower} MATCHES "^(cxx|cpp|c\\+\\+)$")
    set(lang "CXX")
  elseif(${lang_lower} STREQUAL "fortran")
    set(lang "Fortran")
  else()
    set(lang ${LANG})
  endif()

  set(flag "${lang}_COMPILER_HAVE_${FLAG}")
  string(REGEX REPLACE "[/:=]" "-" flag "${flag}")
  string(REGEX REPLACE "\\+\\+" "xx" flag "${flag}")

  check_compiler_flag(${lang} ${FLAG} ${flag})
  set(${RESULT}
      "${${flag}}"
      PARENT_SCOPE)
endfunction()
