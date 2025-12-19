macro(utils_error_if_var_is_null VAR)
  if(NOT ${VAR})
    message(FATAL_ERROR "Invalid argument. Null argument: ${VAR}")
  endif()
endmacro()

macro(utils_set_output_directory_to_mirror TARGET)
  foreach(utils_suffix "" "_DEBUG" "_RELEASE" "_RELWITHDEBINFO" "_MINSIZEREL")
    set_target_properties(
      ${TARGET}
      PROPERTIES ARCHIVE_OUTPUT_DIRECTORY${utils_suffix}
                 ${CMAKE_CURRENT_BINARY_DIR}
                 LIBRARY_OUTPUT_DIRECTORY${utils_suffix}
                 ${CMAKE_CURRENT_BINARY_DIR}
                 RUNTIME_OUTPUT_DIRECTORY${utils_suffix}
                 ${CMAKE_CURRENT_BINARY_DIR})
  endforeach()
  unset(utils_suffix)
endmacro()

macro(utils_check_std_c)
  set(_utils_options "")
  set(_utils_one_value_keywords STANDARD RESULT)
  set(_utils_multi_value_keywords "")
  cmake_parse_arguments(ARG "${_utils_options}" "${_utils_one_value_keywords}"
                        "${_utils_multi_value_keywords}" ${ARGN})

  utils_error_if_var_is_null(ARG_STANDARD)
  utils_error_if_var_is_null(ARG_RESULT)

  if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    check_c_compiler_flag("/std:c${ARG_STANDARD}"
                          C_COMPILER_HAVE_STD_C${ARG_STANDARD})
  else()
    check_c_compiler_flag("-std=c${ARG_STANDARD}"
                          C_COMPILER_HAVE_STD_C${ARG_STANDARD})
  endif()

  set(${ARG_RESULT} ${C_COMPILER_HAVE_STD_C${ARG_STANDARD}})

  unset(_utils_multi_value_keywords)
  unset(_utils_one_value_keywords)
  unset(_utils_options)
endmacro()

macro(utils_check_std_cxx)
  set(_utils_options "")
  set(_utils_one_value_keywords STANDARD RESULT)
  set(_utils_multi_value_keywords "")
  cmake_parse_arguments(ARG "${_utils_options}" "${_utils_one_value_keywords}"
                        "${_utils_multi_value_keywords}" ${ARGN})

  utils_error_if_var_is_null(ARG_STANDARD)
  utils_error_if_var_is_null(ARG_RESULT)

  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    check_cxx_compiler_flag("/std:c++${ARG_STANDARD}"
                            CXX_COMPILER_HAVE_STD_CXX${ARG_STANDARD})
  else()
    check_cxx_compiler_flag("-std=c++${ARG_STANDARD}"
                            CXX_COMPILER_HAVE_STD_CXX${ARG_STANDARD})
  endif()

  set(${ARG_RESULT} ${CXX_COMPILER_HAVE_STD_CXX${ARG_STANDARD}})

  unset(_utils_multi_value_keywords)
  unset(_utils_one_value_keywords)
  unset(_utils_options)
endmacro()

function(utils_set_highest_c_standard TARGET)
  set(standards 23 17)

  foreach(std ${standards})
    set(find_flag OFF)
    set(find_gnu_c_flag OFF)

    if(NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC")
      check_c_compiler_flag("-std=gnu${std}" C_COMPILER_HAVE_STD_GNU${std})
      if(C_COMPILER_HAVE_STD_GNU${std})
        set(find_flag ON)
        set(find_gnu_c_flag ON)
      endif()
    endif()

    if(NOT find_flag)
      utils_check_std_c(STANDARD ${std} RESULT find_flag)
    endif()

    if(find_flag)
      set_target_properties(
        ${TARGET}
        PROPERTIES C_STANDARD ${std}
                   C_STANDARD_REQUIRED ON
                   C_EXTENSIONS ${find_gnu_c_flag})
      return()
    endif()
  endforeach()

  message(FATAL_ERROR "Fail to find the required c standard")
endfunction()

function(utils_set_highest_cxx_standard TARGET)
  set(standards 26 23 20)

  foreach(std ${standards})
    set(find_flag OFF)
    set(find_gnu_cxx_flag OFF)

    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
      check_cxx_compiler_flag("-std=gnu++${std}"
                              CXX_COMPILER_HAVE_STD_GNUXX${std})
      if(CXX_COMPILER_HAVE_STD_GNUXX${std})
        set(find_flag ON)
        set(find_gnu_cxx_flag ON)
      endif()
    endif()

    if(NOT find_flag)
      utils_check_std_cxx(STANDARD ${std} RESULT find_flag)
    endif()

    if(find_flag)
      set_target_properties(
        ${TARGET}
        PROPERTIES CXX_STANDARD ${std}
                   CXX_STANDARD_REQUIRED ON
                   CXX_EXTENSIONS ${find_gnu_cxx_flag})
      return()
    endif()
  endforeach()

  message(FATAL_ERROR "Fail to find the required cxx standard")
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
