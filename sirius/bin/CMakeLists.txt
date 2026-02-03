# --- Variables ---
set(BIN_COMPILE_DEFINITIONS "")
set(BIN_COMPILE_OPTIONS "")
set(BIN_INCLUDE_DIRECTORIES "")
set(BIN_LINK_DIRECTORIES "")
set(BIN_LINK_LIBRARIES "")
set(BIN_LINK_OPTIONS "")
set(BIN_SYSTEM_INCLUDE_DIRECTORIES "")

foreach(t "PRIVATE" "PUBLIC")
  list(APPEND BIN_COMPILE_DEFINITIONS ${GLOB_${t}_COMPILE_DEFINITIONS})
  list(APPEND BIN_COMPILE_OPTIONS ${GLOB_${t}_COMPILE_OPTIONS})
  list(APPEND BIN_INCLUDE_DIRECTORIES ${GLOB_${t}_INCLUDE_DIRECTORIES})
  list(APPEND BIN_LINK_DIRECTORIES ${GLOB_${t}_LINK_DIRECTORIES})
  list(APPEND BIN_LINK_LIBRARIES ${GLOB_${t}_LINK_LIBRARIES})
  list(APPEND BIN_LINK_OPTIONS ${GLOB_${t}_LINK_OPTIONS})
  list(APPEND BIN_SYSTEM_INCLUDE_DIRECTORIES
       ${GLOB_${t}_SYSTEM_INCLUDE_DIRECTORIES})
endforeach()
unset(t)

function(bin_set_exe_args)
  set(options "")
  set(one_value_keywords TARGET DIRECTORY)
  set(multi_value_keywords "")
  cmake_parse_arguments(ARG "${options}" "${one_value_keywords}"
                        "${multi_value_keywords}" ${ARGN})
  sirius_error_if_var_is_null(ARG_TARGET)

  utils_set_output_directory_to_mirror(TARGET ${ARG_TARGET} DIRECTORY
                                       ${ARG_DIRECTORY})

  target_compile_definitions(${ARG_TARGET}
                             PRIVATE "sirius_log_module_name=\"${ARG_TARGET}\"")

  target_compile_definitions(${ARG_TARGET} PRIVATE ${BIN_COMPILE_DEFINITIONS})
  target_compile_options(${ARG_TARGET} PRIVATE ${BIN_COMPILE_OPTIONS})
  target_include_directories(${ARG_TARGET} PRIVATE ${BIN_INCLUDE_DIRECTORIES})
  target_link_directories(${ARG_TARGET} PRIVATE ${BIN_LINK_DIRECTORIES})
  target_link_libraries(${ARG_TARGET} PRIVATE ${BIN_LINK_LIBRARIES})
  target_link_options(${ARG_TARGET} PRIVATE ${BIN_LINK_OPTIONS})
  target_include_directories(${ARG_TARGET} SYSTEM
                             PRIVATE ${BIN_SYSTEM_INCLUDE_DIRECTORIES})

  set_target_properties(${ARG_TARGET} PROPERTIES POSITION_INDEPENDENT_CODE
                                                 ${SIRIUS_PIC_ENABLE})

  install(
    TARGETS ${ARG_TARGET}
    COMPONENT "${PROJECT_NAME}"
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endfunction()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  add_subdirectory(windows/daemon)
endif()
