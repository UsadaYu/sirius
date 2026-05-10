sirius_exe_log = executable(
  get_option('exe-log-name'),
  [
    join_paths('main.cpp'),
  ],
  c_args: [
    '-D_SIRIUS_LOG_MODULE_NAME="@0@"'.format(get_option('exe-log-name')),
    bin_compile_args,
    bin_compile_c_args,
  ],
  cpp_args: [
    '-D_SIRIUS_LOG_MODULE_NAME="@0@"'.format(get_option('exe-log-name')),
    bin_compile_args,
    bin_compile_cpp_args,
  ],
  dependencies: [
    dependency('threads', required: true),
    sirius_foundation_dep,
    bin_dependencies,
  ],
  include_directories: bin_include_directories,
  install: true,
  link_args: bin_link_args,
  override_options: bin_override_options,
  pie: glob_sirius_pic_enable,
)
