# --- Variables ---
glob_sirius_win_dll = '_SIRIUS_WIN_DLL'

private_compile_args = []
private_compile_c_args = []
private_compile_cpp_args = []
private_dependencies = []
private_include_directories = []
# private_link_whole = []
# private_link_with = []
private_link_args = []
private_override_options = []

interface_compile_args = []
# interface_include_directories = []
# interface_link_whole = []
# interface_link_with = []
interface_link_args = []

# --- Compile Args ---
if host_machine.system() == 'windows' and glob_library_type == 'shared'
  private_compile_args += ['-D' + glob_sirius_win_dll]
  interface_compile_args += ['-D' + glob_sirius_win_dll]
endif

private_compile_args += [
  '-D_SIRIUS_BUILDING',
  '-D_SIRIUS_LOG_LEVEL=@0@'.format(get_option('log-level')),
]

langs = ['c', 'cpp']
foreach lang : langs
  std = run_command(
    python3, glob_compiler_script,
    '--json', glob_compiler_json,
    '--compiler', cpp.get_id(),
    '--action', 'project_flags',
    '--lang', lang,
    check: true
  ).stdout().strip()

  if lang == 'c'
    private_compile_c_args += std
  elif lang == 'cpp'
    private_compile_cpp_args += std
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
      private_compile_args += group.get('msvc_flags', [])
    else
      private_compile_args += group.get('gnu_flags', [])
    endif
  endif
endforeach

# --- Dependencies ---

# --- Include Directories ---
private_include_directories += glob_include

# --- Link Args ---

# --- Address Sanitizer ---
if glob_sirius_asan_enable and get_option('asan-fallback-enable')
  foreach lib : get_option('asan-fallback-libs')
    # Meson will prioritize searching for custom paths, but it does not support
    # excluding system paths when searching, so it may be necessary to check
    # whether the correct libraries have been found.
    private_dependencies += cpp.find_library(
      lib,
      dirs: get_option('asan-fallback-libdirs'),
      required : true
    )
  endforeach
elif glob_sirius_asan_enable
  if cpp.get_id() in ['msvc', 'clang-cl']
    warning('''
  The build options of `ASAN` may not be supported by `pkgconfig`
  You may need to check the generated `pkg-config` file
  If you don\'t use it, please ignore''')

    asan_compile_args = ['-fsanitize=address', '-MD', '-Zi', '-Od']
    private_compile_args += asan_compile_args
    interface_compile_args += asan_compile_args
    asan_link_args = ['-DEBUG', '-INCREMENTAL:NO']
    private_link_args += asan_link_args
    interface_link_args += asan_link_args
  else
    if cpp.get_id() == 'clang' and glob_library_type == 'shared'
      if host_machine.system() == 'windows'
        # To prevent conflicts with function symbols (such as `malloc`),
        # the `/MD` option needs to be added.
        # Meson does not support passing the `override_options` to the
        # interface parameter, so `b_vscrt=md` is not used here.
        private_link_args += '-D_DLL'
        interface_link_args += '-D_DLL'
      else
        # In order to ensure rigor and safety, in some cases, Meson will assume
        # that there should be no undefined behavior in the dynamic libraries.
        # On linux, this design is usually achieved through the
        # `-Wl,--no-undefined` flag by gcc or clang.
        # When gcc links a dynamic library, even if `-fsanitize=address` is
        # enabled, it usually allows some function symbols
        # (such as `__asan_stack_malloc_0`) to remain undefined, while this
        # situation is not permitted when clang is enabled with the option
        # `-Wl,--no-undefined`.
        private_override_options += ['b_lundef=false']
      endif
    endif

    asan_compile_args = [
      '-fsanitize=address',
      '-fno-omit-frame-pointer',
      '-fsanitize-recover=address',
    ]
    private_compile_args += asan_compile_args
    interface_compile_args += asan_compile_args
    asan_link_args = ['-g', '-fsanitize=address']
    private_link_args += asan_link_args
    interface_link_args += asan_link_args
  endif
endif

# --- Sirius::sirius ---
subdir('sirius')

# ---
