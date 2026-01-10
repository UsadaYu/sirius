# \rm -rf builddir ; meson setup builddir -Dbuildtype=release -Ddefault_library=shared
# ninja -C builddir --verbose

project(
  'sirius',
  'c', 'cpp',
  default_options: [
    'buildtype=debugoptimized',
    'b_ndebug=if-release',
    'c_std=c17',
    'cpp_std=c++20',
  ],
  meson_version: '>=1.8.3',
  version: run_command(
    'scripts/build/json_reader.py',
    'config/build/version.json',
    'version',
    check: true
  ).stdout(),
)

# --- Global Dependencies ---
# compiler
c = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')
if c.get_id() != cpp.get_id()
  error('''
  The compilers for C and CXX require the same type
  C compiler:     @0@
  CXX compiler:   @1@'''
  .format(c.get_id(), cpp.get_id()))
endif

#############################
message('-------- @0@ --------'.format(c.get_id()))
#############################

compiler_groups = [
  ['gcc', 'gnu'],
  ['clang', 'clang'],
  ['msvc', 'msvc'],
]
foreach group : compiler_groups
  compiler_id = group[0]
  deps_key = group[1]
  deps_json = 'config/build/deps.json'
  script = 'scripts/build/json_reader.py'
  res = run_command(
    'python3', script, deps_json, deps_key, 'version', check: true)
  version = res.stdout().strip()
  if c.get_id() == compiler_id
    if c.version().version_compare('<@0@'.format(version))
      error('''
  C compiler - @0@, whose version is not accepted
  Current version: @1@
  Required version: @2@
  Please edit options and rebuild'''
    .format(c.get_id(), c.version(), version))
    endif

    if cpp.version().version_compare('<' + version)
      error('''
  CXX compiler - @0@, whose version is not accepted
  Current version: @1@
  Required version: @2@
  Please edit options and rebuild'''
    .format(cpp.get_id(), cpp.version(), version))
    endif
  endif
endforeach

add_project_arguments('-D_GNU_SOURCE', language: 'c')

# --- Global Variables ---
glob_version = meson.project_version().split()[0]
glob_sirius_sirius = get_option('target-library-name-libsirius')
glob_sirius_win_dll = 'SIRIUS_WIN_DLL'
glob_include = []

glob_include += include_directories(join_paths('include'))

# --- Preprocessing ---
subdir(join_paths('include', 'sirius'))

# --- Sirius ---
subdir('src')

# --- Unit Tests ---
if get_option('test-enable')
  subdir('test')
endif
