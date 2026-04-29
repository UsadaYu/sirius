# --- Variables ---
c_version = glob_version

c_library_name = get_option('c-library-name')

c_compile_args = lib_compile_args
c_compile_c_args = lib_compile_c_args
c_compile_cpp_args = lib_compile_cpp_args
c_dependencies = lib_dependencies
c_include_directories = lib_include_directories
c_link_args = lib_link_args
c_override_options = lib_override_options

c_pkgconfig_extra_cflags = lib_pkgconfig_extra_cflags
c_pkgconfig_libraries = lib_pkgconfig_libraries
c_pkgconfig_libraries_private = lib_pkgconfig_libraries_private
c_pkgconfig_requires = lib_pkgconfig_requires
c_pkgconfig_requires_private = lib_pkgconfig_requires_private

# --- Source Files ---
c_sources = [
  join_paths('fcntl', 'fs.cpp'),
  join_paths('time.c'),
]

# --- Compile Args ---
c_compile_args += [
  '-D_SIRIUS_LOG_MODULE_NAME="@0@"'.format(c_library_name),
]

# --- Dependencies ---
# winmm
if host_machine.system() == 'windows'
  c_dependencies += cpp.find_library('winmm', required: true)
endif

# --- Library Target ---
if glob_library_type == 'object'
  sirius_c_library = static_library(
    c_library_name,
    c_args: c_compile_args + c_compile_c_args,
    cpp_args: c_compile_args + c_compile_cpp_args,
    dependencies: c_dependencies,
    gnu_symbol_visibility: 'hidden',
    include_directories: c_include_directories,
    install: false,
    link_args: c_link_args,
    override_options: c_override_options,
    pic: glob_sirius_pic_enable,
    sources: c_sources,
  )

  c_objects = sirius_c_library.extract_all_objects(recursive: true)

  sirius_c_dep = declare_dependency(
    compile_args: c_pkgconfig_extra_cflags,
    dependencies: c_dependencies,
    include_directories: sirius_api_directories,
    link_args: c_pkgconfig_libraries + c_pkgconfig_libraries_private,
    objects: c_objects,
  )
else
  sirius_c_library = library(
    c_library_name,
    c_args: c_compile_args + c_compile_c_args,
    cpp_args: c_compile_args + c_compile_cpp_args,
    dependencies: c_dependencies,
    gnu_symbol_visibility: 'hidden',
    include_directories: c_include_directories,
    install: true,
    link_args: c_link_args,
    override_options: c_override_options,
    pic: glob_sirius_pic_enable,
    sources: c_sources,
    version: c_version,
  )

  sirius_c_dep = declare_dependency(
    compile_args: c_pkgconfig_extra_cflags,
    dependencies: c_dependencies,
    include_directories: sirius_api_directories,
    link_args: c_pkgconfig_libraries + c_pkgconfig_libraries_private,
    link_with: sirius_c_library,
  )
endif

# --- Pkg Config ---
subdir('pkgconfig')
