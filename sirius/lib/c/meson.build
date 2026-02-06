# --- Variables ---
lib_version = glob_version

libsirius = get_option('sirius-library-name')

sirius_private_compile_args = private_compile_args
sirius_private_compile_c_args = private_compile_c_args
sirius_private_compile_cpp_args = private_compile_cpp_args
sirius_private_dependencies = private_dependencies
sirius_private_include_directories = private_include_directories
sirius_private_link_args = private_link_args
sirius_private_override_options = private_override_options

sirius_interface_compile_args = interface_compile_args
sirius_interface_link_args = interface_link_args

# --- Source Files ---
sirius_sources = []

sirius_sources += [
  'internal/initializer.c',
  'internal/thread.cpp',
]
sirius_sources += [
  'sirius_cond.c',
  'sirius_log.c',
  'sirius_mutex.c',
  'sirius_queue.c',
  'sirius_sem.c',
  'sirius_thread.c',
  'sirius_time.c',
]

# --- Compile Args ---
sirius_private_compile_args += [
  '-D_SIRIUS_LOG_PRINT_NAME="@0@"'.format(get_option('sirius-print-name')),
  '-D_SIRIUS_LOG_BUF_SIZE=@0@'.format(get_option('sirius-log-buf-size')),
]

# --- Include Directories ---
sirius_private_include_directories += include_directories('.')

# --- Dependencies ---
# thread
sirius_private_dependencies += [dependency('threads')]

# winmm
if host_machine.system() == 'windows'
  sirius_private_dependencies += cpp.find_library('winmm', required: true)
endif

# --- Link Args ---

# --- Library Target ---
if glob_library_type == 'object'
  sirius_library = static_library(
    libsirius,
    c_args: sirius_private_compile_args + sirius_private_compile_c_args,
    cpp_args: sirius_private_compile_args + sirius_private_compile_cpp_args,
    dependencies: sirius_private_dependencies,
    gnu_symbol_visibility: 'hidden',
    include_directories: sirius_private_include_directories,
    install: false,
    link_args: sirius_private_link_args,
    override_options: sirius_private_override_options,
    pic: glob_sirius_pic_enable,
    sources: sirius_sources,
  )

  sirius_objects = sirius_library.extract_all_objects(recursive: true)

  sirius_dep = declare_dependency(
    compile_args: sirius_interface_compile_args,
    dependencies: sirius_private_dependencies,
    include_directories: sirius_private_include_directories,
    link_args: sirius_interface_link_args,
    objects: sirius_objects,
  )
else
  sirius_library = library(
    libsirius,
    c_args: sirius_private_compile_args + sirius_private_compile_c_args,
    cpp_args: sirius_private_compile_args + sirius_private_compile_cpp_args,
    dependencies: sirius_private_dependencies,
    gnu_symbol_visibility: 'hidden',
    include_directories: sirius_private_include_directories,
    install: true,
    link_args: sirius_private_link_args,
    override_options: sirius_private_override_options,
    pic: glob_sirius_pic_enable,
    sources: sirius_sources,
    version: lib_version,
  )

  sirius_dep = declare_dependency(
    compile_args: sirius_interface_compile_args,
    dependencies: sirius_private_dependencies,
    include_directories: sirius_private_include_directories,
    link_args: sirius_interface_link_args,
    link_with: sirius_library,
  )
endif

# --- Pkg Config ---
subdir('pkgconfig')
