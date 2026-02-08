# --- Generate Header Files ---
conf_data = configuration_data()
conf_data.set('SIRIUS_VERSION_MAJOR', glob_version_major)
conf_data.set('SIRIUS_VERSION_MINOR', glob_version_minor)
conf_data.set('SIRIUS_VERSION_PATCH', glob_version_patch)
v_h = configure_file(
  input: 'version.h.in',
  output: 'version.h',
  configuration: conf_data,
  install_dir: 'include/sirius'
)

glob_include += include_directories(join_paths('..'))

# --- Install ---
# c
subdir = join_paths('c')
api_sirius = []
api_sirius += [
  join_paths(subdir, 'fs.h'),
  join_paths(subdir, 'time.h'),
]
install_headers(api_sirius, subdir: join_paths('sirius', subdir))

# foundation
subdir = join_paths('foundation')
api_sirius = []
api_sirius += [
  join_paths(subdir, 'log.h'),
  join_paths(subdir, 'thread.h'),
]
install_headers(api_sirius, subdir: join_paths('sirius', subdir))

# internal
subdir = join_paths('internal')
api_sirius = []
api_sirius += [
  join_paths(subdir, 'attributes.h'),
  join_paths(subdir, 'common.h'),
  join_paths(subdir, 'macro.h'),
]
install_headers(api_sirius, subdir: join_paths('sirius', subdir))

# kit
subdir = join_paths('kit')
api_sirius = []
api_sirius += [
  join_paths(subdir, 'queue.h'),
]
install_headers(api_sirius, subdir: join_paths('sirius', subdir))

# thread
subdir = join_paths('thread')
api_sirius = []
api_sirius += [
  join_paths(subdir, 'cond.h'),
  join_paths(subdir, 'cpu.h'),
  join_paths(subdir, 'macro.h'),
  join_paths(subdir, 'mutex.h'),
  join_paths(subdir, 'sem.h'),
  join_paths(subdir, 'spinlock.h'),
  join_paths(subdir, 'thread.h'),
]
install_headers(api_sirius, subdir: subdir)
