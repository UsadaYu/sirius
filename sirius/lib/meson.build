# --- Variables ---
_sirius_win_dll = '_SIRIUS_WIN_DLL'

lib_compile_args = sirius_compile_args
lib_compile_c_args = sirius_compile_c_args
lib_compile_cpp_args = sirius_compile_cpp_args
lib_dependencies = sirius_dependencies
lib_include_directories = sirius_include_directories
# lib_link_whole = sirius_link_whole
# lib_link_with = sirius_link_with
lib_link_args = sirius_link_args
lib_override_options = sirius_override_options

lib_pkgconfig_extra_cflags = sirius_pkgconfig_extra_cflags
lib_pkgconfig_libraries = sirius_pkgconfig_libraries
lib_pkgconfig_libraries_private = sirius_pkgconfig_libraries_private
lib_pkgconfig_requires = sirius_pkgconfig_requires
lib_pkgconfig_requires_private = sirius_pkgconfig_requires_private

# --- Compile Args ---
if host_machine.system() == 'windows' and glob_library_type == 'shared'
  lib_compile_args += ['-D' + _sirius_win_dll]
  lib_pkgconfig_extra_cflags += ['-D' + _sirius_win_dll]
endif

# --- Dependencies ---

# --- Include Directories ---

# --- Link Args ---

# --- Dependency Relationship ---
# LIBRARY         DEPENDENCY
# c               None
# foundation      None
# thread          foundation
# kit             foundation, thread

subdir('c')
subdir('foundation')
subdir('thread')
subdir('kit')

# --- * ---
