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

# --- sirius ---
subdir = 'sirius'
api_sirius = []

api_sirius += [
  'attributes.h',
  'cpu.h',
  'file.h',
  'macro.h',
]
install_headers(api_sirius, subdir: subdir)

# --- sirius/c ---
subdir = join_paths('sirius', 'c')
api_sirius = []

api_sirius += [
  'c/time.h',
]
install_headers(api_sirius, subdir: subdir)

# --- sirius/internal ---
subdir = join_paths('sirius', 'internal')
api_sirius = []

api_sirius += [
  'internal/attributes.h',
  'internal/common.h',
  'internal/macro.h',
]
install_headers(api_sirius, subdir: subdir)

# --- sirius/thread ---
subdir = join_paths('sirius', 'thread')
api_sirius = []

api_sirius += [
  'thread/cond.h',
  'thread/mutex.h',
  'thread/sem.h',
  'thread/spinlock.h',
  'thread/thread.h',
]
install_headers(api_sirius, subdir: subdir)

# --- sirius/thread/internal ---
subdir = join_paths('sirius', 'thread', 'internal')
api_sirius = []

api_sirius += [
  'thread/internal/thread.h',
]
install_headers(api_sirius, subdir: subdir)

# --- sirius/utils ---
subdir = join_paths('sirius', 'utils')
api_sirius = []

api_sirius += [
  'utils/log.h',
  'utils/math.h',
  'utils/queue.h',
]
install_headers(api_sirius, subdir: subdir)
