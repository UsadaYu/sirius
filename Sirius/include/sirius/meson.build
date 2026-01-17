# --- Generate Header Files ---
conf_data = configuration_data()
conf_data.set('SIRIUS_VERSION_MAJOR', glob_version_major)
conf_data.set('SIRIUS_VERSION_MINOR', glob_version_minor)
conf_data.set('SIRIUS_VERSION_PATCH', glob_version_patch)
v_h = configure_file(
  input: 'sirius_version.h.in',
  output: 'sirius_version.h',
  configuration: conf_data,
  install_dir: 'include/sirius'
)

glob_include += include_directories(join_paths('..'))

# --- sirius ---
subdir = 'sirius'
api_sirius = []

api_sirius += [
  'sirius_cpu.h',
  'sirius_macro.h',
  'sirius_queue.h',
  'sirius_thread.h',
  'sirius_attributes.h',
  'sirius_file.h',
  'sirius_math.h',
  'sirius_sem.h',
  'sirius_time.h',
  'sirius_cond.h',
  'sirius_log.h',
  'sirius_mutex.h',
  'sirius_spinlock.h',
]
install_headers(api_sirius, subdir: subdir)

# --- sirius/internal ---
subdir = join_paths('sirius', 'internal')
api_sirius_internal = []

api_sirius_internal += [
  'internal/attributes.h',
  'internal/common.h',
  'internal/macro.h',
  'internal/thread.h',
]
install_headers(api_sirius_internal, subdir: subdir)
