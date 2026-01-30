project(
  'Sirius',
  'c', 'cpp',
  default_options: [
    'buildtype=debugoptimized',
    'b_ndebug=if-release',
  ],
  meson_version: '>=1.10.1',
  version: run_command(
    find_program('python'),
    join_paths('scripts', 'build', 'version.py'),
    '--json', join_paths('config', 'build', 'version.json'),
    check: true
  ).stdout().strip(),
)

# --- Global Variables ---
c = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

glob_version = meson.project_version().split()[0]
glob_version_major = glob_version.split('.')[0]
glob_version_minor = glob_version.split('.')[1]
glob_version_patch = glob_version.split('.')[2]

glob_compiler_script = join_paths(
  meson.project_source_root(), 'scripts', 'build', 'compiler.py'
)
glob_compiler_json = join_paths(
  meson.project_source_root(), 'config', 'build', 'compiler.json'
)

# --- Preprocessing ---
if get_option('build-object-only')
  glob_library_type = 'object'
else
  if get_option('default_library') == 'shared'
    glob_library_type = 'shared'
  else
    glob_library_type = 'static'
    if get_option('default_library') != 'static'
      warning(
        'Override the original option `@0@` of `default_library` with `static`'
        .format(get_option('default_library')))
    endif
  endif
endif

if host_machine.system() == 'windows'
  glob_sirius_pic_enable = glob_library_type == 'shared' \
    ? true \
    : get_option('pic-enable')
  glob_test_pie_enable = glob_sirius_pic_enable \
    ? get_option('test-pie-enable') \
    : false
endif

glob_sirius_asan_enable = get_option('b_sanitize') == 'address' \
  ? true \
  : get_option('asan')

# --- Global Dependencies ---
# python3
python3 = import('python').find_installation('python3', required: true)

# compiler
if c.get_id() != cpp.get_id()
  error('''
  The compilers for C and CXX require the same type
  C compiler: @0@
  CXX compiler: @1@'''
  .format(c.get_id(), cpp.get_id()))
endif

_min_version = run_command(
  python3, glob_compiler_script,
  '--json', glob_compiler_json,
  '--compiler', cpp.get_id(),
  '--action', 'min_version',
  check: true
).stdout().strip()
if c.version().version_compare('<@0@'.format(_min_version))
      error('''
  C compiler - @0@, whose version is not accepted
  Current version: @1@
  Required version: @2@
  Please edit options and rebuild'''
    .format(c.get_id(), c.version(), _min_version))
endif
if cpp.version().version_compare('<@0@'.format(_min_version))
      error('''
  CXX compiler - @0@, whose version is not accepted
  Current version: @1@
  Required version: @2@
  Please edit options and rebuild'''
    .format(cpp.get_id(), cpp.version(), _min_version))
endif
_min_version = ''

subdir('sirius')

if get_option('test-enable')
  subdir('test')
endif

summary({
    'Build Type': get_option('buildtype'),
    'Library Type': glob_library_type,
    'Install Prefix': get_option('prefix'),
}, section: 'Configuration')
