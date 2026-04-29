# --- Variables ---
kit_version = glob_version

kit_library_name = get_option('kit-library-name')

kit_compile_args = lib_compile_args
kit_compile_c_args = lib_compile_c_args
kit_compile_cpp_args = lib_compile_cpp_args
kit_dependencies = lib_dependencies
kit_include_directories = lib_include_directories
kit_link_args = lib_link_args
kit_override_options = lib_override_options

kit_pkgconfig_extra_cflags = lib_pkgconfig_extra_cflags
kit_pkgconfig_libraries = lib_pkgconfig_libraries
kit_pkgconfig_libraries_private = lib_pkgconfig_libraries_private
kit_pkgconfig_requires = lib_pkgconfig_requires
kit_pkgconfig_requires_private = lib_pkgconfig_requires_private

# --- Source Files ---
kit_sources = [
  'log.cpp',
  'queue.c',
]

# --- Compile Args ---
kit_compile_args += [
  '-D_SIRIUS_LOG_MODULE_NAME="@0@"'.format(kit_library_name),
]

# --- Dependencies ---
kit_dependencies += [
  sirius_foundation_dep,
  sirius_thread_dep,
]

# --- Link Args ---

# --- Library Target ---
if glob_library_type == 'object'
  sirius_kit_library = static_library(
    kit_library_name,
    c_args: kit_compile_args + kit_compile_c_args,
    cpp_args: kit_compile_args + kit_compile_cpp_args,
    dependencies: kit_dependencies,
    gnu_symbol_visibility: 'hidden',
    include_directories: kit_include_directories,
    install: false,
    link_args: kit_link_args,
    override_options: kit_override_options,
    pic: glob_sirius_pic_enable,
    sources: kit_sources,
  )

  kit_objects = sirius_kit_library.extract_all_objects(
    recursive: true,
  )

  sirius_kit_dep = declare_dependency(
    compile_args: kit_pkgconfig_extra_cflags,
    dependencies: kit_dependencies,
    include_directories: sirius_api_directories,
    link_args: kit_pkgconfig_libraries + kit_pkgconfig_libraries_private,
    objects: kit_objects,
  )
else
  sirius_kit_library = library(
    kit_library_name,
    c_args: kit_compile_args + kit_compile_c_args,
    cpp_args: kit_compile_args + kit_compile_cpp_args,
    dependencies: kit_dependencies,
    gnu_symbol_visibility: 'hidden',
    include_directories: kit_include_directories,
    install: true,
    link_args: kit_link_args,
    override_options: kit_override_options,
    pic: glob_sirius_pic_enable,
    sources: kit_sources,
    version: kit_version,
  )

  sirius_kit_dep = declare_dependency(
    compile_args: kit_pkgconfig_extra_cflags,
    dependencies: kit_dependencies,
    include_directories: sirius_api_directories,
    link_args: kit_pkgconfig_libraries + kit_pkgconfig_libraries_private,
    link_with: sirius_kit_library,
  )
endif

# --- Pkg Config ---
subdir('pkgconfig')
