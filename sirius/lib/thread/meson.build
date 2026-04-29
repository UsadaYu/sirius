# --- Variables ---
thread_version = glob_version

thread_library_name = get_option('thread-library-name')

thread_compile_args = lib_compile_args
thread_compile_c_args = lib_compile_c_args
thread_compile_cpp_args = lib_compile_cpp_args
thread_dependencies = lib_dependencies
thread_include_directories = lib_include_directories
thread_link_args = lib_link_args
thread_override_options = lib_override_options

thread_pkgconfig_extra_cflags = lib_pkgconfig_extra_cflags
thread_pkgconfig_libraries = lib_pkgconfig_libraries
thread_pkgconfig_libraries_private = lib_pkgconfig_libraries_private
thread_pkgconfig_requires = lib_pkgconfig_requires
thread_pkgconfig_requires_private = lib_pkgconfig_requires_private

# --- Source Files ---
thread_sources = [
  'cond.c',
  'mutex.c',
  'sem.c',
]
if host_machine.system() == 'windows'
  thread_sources += join_paths('windows', 'thread.cpp')
else
  thread_sources += join_paths('posix', 'thread.cpp')
endif

# --- Compile Args ---
thread_compile_args += [
  '-D_SIRIUS_LOG_MODULE_NAME="@0@"'.format(thread_library_name),
]

# --- Dependencies ---
thread_dependencies += [
  dependency('threads', required: true),
  sirius_foundation_dep,
]

# --- Link Args ---

# --- Library Target ---
if glob_library_type == 'object'
  sirius_thread_library = static_library(
    thread_library_name,
    c_args: thread_compile_args + thread_compile_c_args,
    cpp_args: thread_compile_args + thread_compile_cpp_args,
    dependencies: thread_dependencies,
    gnu_symbol_visibility: 'hidden',
    include_directories: thread_include_directories,
    install: false,
    link_args: thread_link_args,
    override_options: thread_override_options,
    pic: glob_sirius_pic_enable,
    sources: thread_sources,
  )

  thread_objects = sirius_thread_library.extract_all_objects(
    recursive: true,
  )

  sirius_thread_dep = declare_dependency(
    compile_args: thread_pkgconfig_extra_cflags,
    dependencies: thread_dependencies,
    include_directories: sirius_api_directories,
    link_args: thread_pkgconfig_libraries + thread_pkgconfig_libraries_private,
    objects: thread_objects,
  )
else
  sirius_thread_library = library(
    thread_library_name,
    c_args: thread_compile_args + thread_compile_c_args,
    cpp_args: thread_compile_args + thread_compile_cpp_args,
    dependencies: thread_dependencies,
    gnu_symbol_visibility: 'hidden',
    include_directories: thread_include_directories,
    install: true,
    link_args: thread_link_args,
    override_options: thread_override_options,
    pic: glob_sirius_pic_enable,
    sources: thread_sources,
    version: thread_version,
  )

  sirius_thread_dep = declare_dependency(
    compile_args: thread_pkgconfig_extra_cflags,
    dependencies: thread_dependencies,
    include_directories: sirius_api_directories,
    link_args: thread_pkgconfig_libraries + thread_pkgconfig_libraries_private,
    link_with: sirius_thread_library,
  )
endif

# --- Pkg Config ---
subdir('pkgconfig')
