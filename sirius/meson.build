message('--------------')
message('--- SIRIUS ---')
message('--------------')

# --- Global Variables ---
sirius_compile_args = []
sirius_compile_c_args = []
sirius_compile_cpp_args = []
sirius_dependencies = []
sirius_include_directories = []
# sirius_link_whole = []
# sirius_link_with = []
sirius_link_args = []
sirius_override_options = []

sirius_api_directories = []

sirius_pkgconfig_extra_cflags = []
sirius_pkgconfig_libraries = []
sirius_pkgconfig_libraries_private = []
sirius_pkgconfig_requires = []
sirius_pkgconfig_requires_private = []

# --- Preprocessing ---
subdir(join_paths('include', 'sirius'))

# ---Dependencies ---
# address sanitizer
if glob_asan_enable
  if cpp.get_id() in ['msvc', 'clang-cl']
    warning('''
  The build options of `ASAN` may not be supported by `pkgconfig`
  You may need to check the generated `pkg-config` file
  If you don\'t use it, please ignore'''
    )
  endif

  if cpp.get_id() in ['msvc', 'clang-cl']
    asan_compile_args = []
    if cpp.get_id() != 'msvc'
      asan_compile_args += '-MD'
    endif
    asan_compile_args += ['-fsanitize=address', '-Zi', '-Od']
    sirius_compile_args += asan_compile_args
    sirius_pkgconfig_extra_cflags += asan_compile_args

    asan_link_args = ['-DEBUG', '-INCREMENTAL:NO']
    sirius_link_args += asan_link_args
    sirius_pkgconfig_libraries_private += asan_link_args
  else
    asan_compile_args = [
      '-fsanitize=address',
      '-fno-omit-frame-pointer',
      '-fsanitize-recover=address',
    ]
    sirius_compile_args += asan_compile_args
    sirius_pkgconfig_extra_cflags += asan_compile_args

    asan_link_args = ['-g']
    sirius_link_args += asan_link_args
    sirius_pkgconfig_libraries_private += asan_link_args
  endif
endif

if glob_asan_enable and get_option('asan-fallback-enable')
  foreach lib : get_option('asan-fallback-libs')
    # Meson will prioritize searching for custom paths, but it does not support
    # excluding system paths when searching, so it may be necessary to check
    # whether the correct libraries have been found.
    sirius_dependencies += cpp.find_library(
      lib,
      dirs: get_option('asan-fallback-libdirs'),
      required : true
    )
  endforeach
elif glob_asan_enable
  if cpp.get_id() in ['msvc', 'clang-cl']
  else
    asan_link_args = ['-fsanitize=address']
    if cpp.get_id() == 'clang' and glob_library_type == 'shared'
      if host_machine.system() == 'windows'
        # To prevent conflicts with function symbols (such as `malloc`), the
        # `/MD` option needs to be added.
        # Meson does not support passing the `override_options` to the interface
        # parameter, so `b_vscrt=md` is not used here.
        asan_link_args += '-D_DLL'
      else
        # In order to ensure rigor and safety, in some cases, Meson will assume
        # that there should be no undefined behavior in the dynamic libraries.
        # On linux, this design is usually achieved through the
        # `-Wl,--no-undefined` flag by gcc or clang.
        # When gcc links a dynamic library, even if `-fsanitize=address` is
        # enabled, it usually allows some function symbols (such as
        # `__asan_stack_malloc_0`) to remain undefined, while this situation is
        # not permitted when clang is enabled with the option
        # `-Wl,--no-undefined`.
        sirius_override_options += ['b_lundef=false']
      endif
    endif
    sirius_link_args += asan_link_args
    sirius_pkgconfig_libraries_private += asan_link_args
  endif
endif

# --- Compile Args ---
sirius_compile_args += [
  '-D_SIRIUS_BUILDING',
  '-D_SIRIUS_LOG_LEVEL=@0@'.format(get_option('log-level')),
]
sirius_compile_args += [
  '-D_SIRIUS_NAMESPACE="@0@"'.format(get_option('namespace')),
  '-D_SIRIUS_POSIX_FILE_MODE="@0@"'.format(get_option('posix-file-mode')),
  '-D_SIRIUS_TMP_DIR="@0@"'.format(get_option('tmp-dir')),
  '-D_SIRIUS_USER_KEY="@0@"'.format(get_option('user-key')),
  '-D_SIRIUS_LOG_SHM_CAPACITY=@0@'.format(get_option('log-shm-capacity')),
  '-D_SIRIUS_LOG_BUF_SIZE=@0@'.format(get_option('log-buf-size')),
  '-D_SIRIUS_EXE_DIR="@0@"'.format(
    join_paths(get_option('prefix'), get_option('bindir'))
  ),
  '-D_SIRIUS_EXE_LOG_NAME="@0@"'.format(get_option('exe-log-name')),
]

# standard
langs = ['c', 'cpp']
foreach lang : langs
  std = run_command(
    python3,
    glob_compiler_script,
    '--json',
    glob_compiler_json,
    '--compiler',
    cpp.get_id(),
    '--action',
    'project_flags',
    '--lang',
    lang,
    check: true,
  ).stdout().strip()

  if lang == 'c'
    sirius_compile_c_args += std
  elif lang == 'cpp'
    sirius_compile_cpp_args += std
  endif
endforeach

groups = [
  {
    'option': 'warning-all',
    'gnu_flags': ['-Wall'],
    'msvc_flags': ['/W4']
  },
  {
    'option': 'warning-as-error',
    'gnu_flags': ['-Werror'],
    'msvc_flags': ['/WX']
  },
]
foreach group : groups
  if get_option(group['option'])
    if cpp.get_id() in ['msvc', 'clang-cl']
      sirius_compile_args += group.get('msvc_flags', [])
    else
      sirius_compile_args += group.get('gnu_flags', [])
    endif
  endif
endforeach

# --- Dependencies ---

# --- Include Directories ---
sirius_include_directories += include_directories(join_paths('.'))

# --- Link Args ---

# --- * ---
# Because executables depend on libraries, `lib` is written before `bin` for the
# sake of specification.
subdir('lib')
subdir('bin')
