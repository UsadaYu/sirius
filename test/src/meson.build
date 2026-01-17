suffix_generator = '''
import sys
import re

def gen_safe_name(name):
    suffix = re.sub(r"\+\+", "xx", name)
    suffix = re.sub(r"[^a-zA-Z0-9_.-]", "_", suffix)
    suffix = re.sub(r"_+", "_", suffix)
    suffix = re.sub(r"^[-_.]+|[-_.]+$", "", suffix)
    return suffix

if __name__ == "__main__":
    print(gen_safe_name(sys.argv[1]), end="")
'''

# --- Global Dependencies of Testing ---
test_dep_sirius = sirius_dep

# --- Global Variables of Testing ---
test_compile_args = []
test_compile_c_args = []
test_compile_cpp_args = []
test_dependencies = []
test_include_directories = []
test_link_args = []

test_c_stds = []
test_cpp_stds = []

# [
#   'target': 'target',
#   'compile_args': 'compile_args',
#   'sources': 'sources',
#   'subdir': 'subdir'
# ]
test_targets = []

# --- Preprocessing ---
langs = ['c', 'cpp']
foreach lang : langs
  stds = run_command(
    python3, glob_compiler_script,
    '--json', glob_compiler_json,
    '--compiler', cpp.get_id(),
    '--action', 'test_matrix',
    '--lang', lang,
    check: true
  ).stdout().strip().split(';')

  if lang == 'c'
    foreach std : c.get_supported_arguments(stds)
      suffix = run_command(python3, '-c', suffix_generator, std, check: true)
      suffix = suffix.stdout().strip()
      test_c_stds += [{'std': std, 'suffix': suffix}]
    endforeach
  elif lang == 'cpp'
    foreach std : cpp.get_supported_arguments(stds)
      suffix = run_command(python3, '-c', suffix_generator, std, check: true)
      suffix = suffix.stdout().strip()
      test_cpp_stds += [{'std': std, 'suffix': suffix}]
    endforeach
  endif
endforeach

# --- Compile Args ---
test_compile_args += [
  '-Dsirius_log_level=@0@'.format(get_option('test-log-level')),
]

groups = [
  {
    'gnu_flags': [
      '-Wall',
    ]
  },
  {
    'msvc_flags': [
      '/W4',
      '/Zc:__cplusplus',
    ]
  },
]
foreach group : groups
  if cpp.get_id() in ['msvc', 'clang-cl']
    test_compile_args += group.get('msvc_flags', [])
  else
    test_compile_args += group.get('gnu_flags', [])
  endif
endforeach

# --- Dependencies ---
test_dependencies += test_dep_sirius

foreach lib : get_option('test-extra-link-libraries')
  test_dependencies += cpp.find_library(
    lib,
    dirs: get_option('test-extra-link-directories'),
    required: true,
  )
endforeach

# --- Include Directories ---
test_include_directories += include_directories(join_paths('.'))

# --- Link Args ---

# --- Test ---
subdir('cond')
subdir('lock')
subdir('log')
subdir('queue')
subdir('sem')
subdir('thread')
subdir('time')

foreach test_target : test_targets
  sources = []
  foreach source : test_target['sources']
    sources += join_paths(test_target['subdir'], source)
  endforeach
  workdir = '_' + test_target['subdir']
  exe = executable(
    test_target['target'],
    sources,
    c_args: test_target['compile_args'] + test_compile_c_args,
    cpp_args: test_target['compile_args'] + test_compile_cpp_args,
    build_subdir: workdir,
    dependencies: test_dependencies,
    include_directories: test_include_directories,
    link_args: test_link_args,
    pie: glob_test_pie_enable,
  )
  test(
    test_target['target'],
    exe,
    workdir: join_paths(meson.current_build_dir(), workdir),
  )
endforeach
