# --- Variables ---

# --- Compile Arguments ---
sirius_args = [
  '-Dsirius_log_module_name="@0@"'.format(get_option('module_print_name')),
  '-DLOG_BUF_SIZE=@0@'.format(get_option('write_size')),
  '-Dsirius_log_level=@0@'.format(get_option('internal_log_level')),
  '-DSIRIUS_BUILDING'
]

if host_machine.system() == 'windows' and get_option(
  'default_library') == 'shared'
  sirius_args += ['-D' + glob_sirius_win_dll]
endif

# --- Include Directories ---
inc_src = [
  glob_include,
  include_directories('.'),
]

# --- Dependencies ---
deps = [dependency('threads')]

if host_machine.system() == 'windows'
  winmm_dep = cpp.find_library('winmm', required: true)
  deps += [winmm_dep]
endif

# --- Source Files ---
sources = [
  'internal/init.c',
  'internal/log.c',

  'sirius_log.c',
  'sirius_mutex.c',
  'sirius_queue.c',
  'sirius_thread.c',
  'sirius_time.c'
]

# --- Library Target ---
libsirius = library(
  glob_sirius_sirius,
  sources: sources,
  c_args: sirius_args,
  cpp_args: sirius_args,
  include_directories: inc_src,
  dependencies: deps,
  gnu_symbol_visibility: 'hidden',
  pic: get_option('pic_enable'),
  install: true,
  version: glob_version,
)

# sirius_lib = target(sirius_lib_type, glob_sirius,
#   sources,
#   cpp_args: sirius_args,
#   include_directories: include_directories('.'),
#   dependencies: [deps_threads, depses],
#   gnu_symbol_visibility: 'hidden',
#   pic: get_option('pic_enable'),
#   install: true,
#   version: meson.project_version()
# )

# # --- Alias / Dependency Object ---
# sirius_dep = declare_dependency(
#   include_directories: include_directories('src'),
#   link_with: sirius_lib,
#   compile_args: sirius_args
# )

# # --- Output Summary ---
# summary({
#     'Build Type': get_option('buildtype'),
#     'Library Type': get_option('default_library'),
#     'Install Prefix': get_option('prefix'),
# }, section: 'Configuration')
