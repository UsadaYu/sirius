# --- Import ---
fs = import('fs')

# --- Variables ---
bin_compile_args = sirius_compile_args
bin_compile_c_args = sirius_compile_c_args
bin_compile_cpp_args = sirius_compile_cpp_args
bin_dependencies = sirius_dependencies
bin_include_directories = sirius_include_directories
bin_link_args = sirius_link_args
bin_override_options = sirius_override_options

# [
#   'target': 'target',
#   'compile_args': 'compile_args',
#   'dependencies': 'dependencies',
#   'sources': 'sources',
#   'subdir': 'subdir',
# ]
bin_targets = []

subdir('log')

foreach bin_target : bin_targets
  sources = []
  foreach source : bin_target['sources']
    sources += join_paths(bin_target['subdir'], source)
  endforeach
  exe = executable(
    bin_target['target'],
    sources,
    c_args: bin_target['compile_args'] + bin_compile_args + bin_compile_c_args,
    cpp_args: bin_target['compile_args'] + bin_compile_args + bin_compile_cpp_args,
    dependencies: bin_target['dependencies'] + bin_dependencies,
    include_directories: bin_include_directories,
    install: true,
    link_args: bin_link_args,
    override_options: bin_override_options,
    pie: glob_sirius_pic_enable,
  )
endforeach
