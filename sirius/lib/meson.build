# --- Variables ---
_sirius_win_dll = '_SIRIUS_WIN_DLL'

lib_compile_args = ss_compile_args
lib_compile_c_args = ss_compile_c_args
lib_compile_cpp_args = ss_compile_cpp_args
lib_dependencies = ss_dependencies
lib_include_directories = ss_include_directories
# lib_link_whole = ss_link_whole
# lib_link_with = ss_link_with
lib_link_args = ss_link_args
lib_override_options = ss_override_options

lib_pkgconfig_extra_cflags = ss_pkgconfig_extra_cflags
lib_pkgconfig_libraries = ss_pkgconfig_libraries
lib_pkgconfig_libraries_private = ss_pkgconfig_libraries_private
lib_pkgconfig_requires = ss_pkgconfig_requires
lib_pkgconfig_requires_private = ss_pkgconfig_requires_private

# --- Compile Args ---
if host_machine.system() == 'windows' and glob_library_type == 'shared'
  lib_compile_args += ['-D@0@'.format(_sirius_win_dll)]
  lib_pkgconfig_extra_cflags += ['-D@0@'.format(_sirius_win_dll)]
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
