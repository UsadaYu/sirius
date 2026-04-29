# --- Variables ---
foundation_version = glob_version

foundation_library_name = get_option('foundation-library-name')

foundation_compile_args = lib_compile_args
foundation_compile_c_args = lib_compile_c_args
foundation_compile_cpp_args = lib_compile_cpp_args
foundation_dependencies = lib_dependencies
foundation_include_directories = lib_include_directories
foundation_link_args = lib_link_args
foundation_override_options = lib_override_options

foundation_pkgconfig_extra_cflags = lib_pkgconfig_extra_cflags
foundation_pkgconfig_libraries = lib_pkgconfig_libraries
foundation_pkgconfig_libraries_private = lib_pkgconfig_libraries_private
foundation_pkgconfig_requires = lib_pkgconfig_requires
foundation_pkgconfig_requires_private = lib_pkgconfig_requires_private

# --- Source Files ---
foundation_sources = [
  join_paths('structor.cpp'),
]

# --- Compile Args ---
foundation_compile_args += [
  '-D_SIRIUS_LOG_MODULE_NAME="@0@"'.format(foundation_library_name),
  '-D_SIRIUS_FOUNDATION_BUILDING',
]

# --- Dependencies ---
# thread
foundation_dependencies += [dependency('threads', required: true)]

# --- Link Args ---

# --- Library Target ---
if glob_library_type == 'object'
  sirius_foundation_library = static_library(
    foundation_library_name,
    c_args: foundation_compile_args + foundation_compile_c_args,
    cpp_args: foundation_compile_args + foundation_compile_cpp_args,
    dependencies: foundation_dependencies,
    gnu_symbol_visibility: 'hidden',
    include_directories: foundation_include_directories,
    install: false,
    link_args: foundation_link_args,
    override_options: foundation_override_options,
    pic: glob_sirius_pic_enable,
    sources: foundation_sources,
  )

  foundation_objects = sirius_foundation_library.extract_all_objects(
    recursive: true,
  )

  sirius_foundation_dep = declare_dependency(
    compile_args: foundation_pkgconfig_extra_cflags,
    dependencies: foundation_dependencies,
    include_directories: sirius_api_directories,
    link_args: foundation_pkgconfig_libraries + foundation_pkgconfig_libraries_private,
    objects: foundation_objects,
  )
else
  sirius_foundation_library = library(
    foundation_library_name,
    c_args: foundation_compile_args + foundation_compile_c_args,
    cpp_args: foundation_compile_args + foundation_compile_cpp_args,
    dependencies: foundation_dependencies,
    gnu_symbol_visibility: 'hidden',
    include_directories: foundation_include_directories,
    install: true,
    link_args: foundation_link_args,
    override_options: foundation_override_options,
    pic: glob_sirius_pic_enable,
    sources: foundation_sources,
    version: foundation_version,
  )

  sirius_foundation_dep = declare_dependency(
    compile_args: foundation_pkgconfig_extra_cflags,
    dependencies: foundation_dependencies,
    include_directories: sirius_api_directories,
    link_args: foundation_pkgconfig_libraries + foundation_pkgconfig_libraries_private,
    link_with: sirius_foundation_library,
  )
endif

# --- Pkg Config ---
subdir('pkgconfig')
