  _artifact_path = '${PROJECT_BINARY_DIR}/test'
  _artifact_bin_path = '${_artifact_path}/bin'
  _link_dir_list = ''
  _link_libs_list = ''
  _src_dir = '${PROJECT_SOURCE_DIR}/test/src'
  # file(['GLOB', '_src_list', '${_src_dir}/*.cpp', '${_src_dir}/*.c'])
  # list(['APPEND', '_link_dir_list', sirius_test_extra_link_directories])
  # list(['APPEND', '_link_libs_list', sirius_target_cmake_namespace, '::${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}'])
  # list(['APPEND', '_link_libs_list', sirius_test_extra_link_libraries])
  # foreach(['file', _src_list])
  # get_filename_component(['full_name', file, 'NAME'])
  # string(['REGEX', 'REPLACE', '\\.[^.]+$', '', 'file_name', full_name])
  _target_name = '${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}_${file_name}'
  _target_name_exe = executable(_target_name, file)
  # foreach(['_suffix', '', '_DEBUG', '_RELEASE', '_RELWITHDEBINFO', '_MINSIZEREL'])
  # set_target_properties([_target_name, 'PROPERTIES', 'RUNTIME_OUTPUT_DIRECTORY${_suffix}', _artifact_bin_path])
  # endforeach()
  # unset('_suffix')
  # target_compile_definitions([_target_name, 'PRIVATE', 'sirius_log_module_name=\"${SIRIUS_TEST_MODULE_PRINT_NAME}\"', 'PRIVATE', 'sirius_log_level=${SIRIUS_TEST_LOG_LEVEL}'])
  if 'full_name', 'STREQUAL', 'test7.1_log.c'
    # cmake_path(['SET', '_gen', 'NORMALIZE', '${_artifact_bin_path}/_gen_test7.1_log.txt'])
    # target_compile_definitions([_target_name, 'PRIVATE', 'GEN_FILE_NAME=\"${_gen}\"'])
    # set_property(['DIRECTORY', project_source_dir, 'APPEND', 'PROPERTY', 'ADDITIONAL_CLEAN_FILES', _gen])
  endif
  if 'SIRIUS_BUILD_OBJECT_ONLY'
    # target_compile_options([_target_name, 'PRIVATE', '$<$<CXX_COMPILER_ID:MSVC>:/U${GLOB_SIRIUS_WIN_DLL}>'])
  endif
  if 'CMAKE_CXX_COMPILER_ID', 'STREQUAL', 'MSVC'
    # target_compile_options([_target_name, 'PRIVATE', '/W4'])
  else
    # target_compile_options([_target_name, 'PRIVATE', '-Wall'])
  endif
  if 'NOT', 'CMAKE_SYSTEM_NAME', 'STREQUAL', 'Windows'
    if 'SIRIUS_TEST_PIE_ENABLE'
      # target_compile_options([_target_name, 'PRIVATE', '-fPIC', '-fPIE'])
      # target_link_options([_target_name, 'PRIVATE', '-pie'])
    else
      # target_compile_options([_target_name, 'PRIVATE', '-fno-PIC', '-fno-PIE'])
      # target_link_options([_target_name, 'PRIVATE', '-no-pie'])
    endif
  endif
  # target_link_directories([_target_name, 'PRIVATE', _link_dir_list])
  # target_link_libraries([_target_name, 'PRIVATE', _link_libs_list])
  test('NAME', _target_name, 'COMMAND', _artifact_bin_path, '/${_target_name}', 'WORKING_DIRECTORY', _artifact_bin_path)
  if 'CMAKE_SYSTEM_NAME', 'STREQUAL', 'Windows', 'AND', 'BUILD_SHARED_LIBS'
    # set_tests_properties([_target_name, 'PROPERTIES', 'ENVIRONMENT_MODIFICATION', 'PATH=path_list_prepend:${GLOB_SIRIUS_BUILD_BIN_DIR}'])
  endif
  # endforeach()
