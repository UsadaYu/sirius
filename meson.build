project(
  'Sirius',
  'c', 'cpp',
  default_options: [
    'buildtype=debugoptimized',
    'b_ndebug=if-release',
  ],
  meson_version: '>=1.8.3',
  version: run_command(
    'scripts/build/version.py',
    '--json', 'config/build/version.json',
    check: true
  ).stdout(),
)

# --- Global Variables ---
glob_version = meson.project_version().split()[0]
glob_sirius_sirius = get_option('target-library-name-libsirius')
glob_sirius_win_dll = 'SIRIUS_WIN_DLL'
glob_include = []
glob_compiler_script = join_paths(
  meson.project_source_root(), 'scripts/build/compiler.py'
)
glob_compiler_json = join_paths(
  meson.project_source_root(), 'config/build/compiler.json'
)

# --- Global Dependencies ---
# python3
python3 = import('python').find_installation('python3', required: true)

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


min_version = run_command(
  python3, glob_compiler_script,
  '--json', glob_compiler_json,
  '--compiler', cpp.get_id(),
  '--action', 'min_version',
  check: true
).stdout()

if c.version().version_compare('<@0@'.format(min_version))
      error('''
  C compiler - @0@, whose version is not accepted
  Current version:  @1@
  Required version: @2@
  Please edit options and rebuild'''
    .format(c.get_id(), c.version(), min_version))
endif
if cpp.version().version_compare('<' + min_version)
      error('''
  CXX compiler - @0@, whose version is not accepted
  Current version:  @1@
  Required version: @2@
  Please edit options and rebuild'''
    .format(cpp.get_id(), cpp.version(), min_version))
endif

# --- Preprocessing ---
subdir(join_paths('include', 'sirius'))

glob_include += include_directories(join_paths('include'))

# --- Sirius ---
subdir('src')

# --- Unit Tests ---
if get_option('test-enable')
  subdir('test')
endif
