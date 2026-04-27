# --- Variables ---
glob_sirius_win_dll = '_SIRIUS_WIN_DLL'

private_compile_args = []
private_compile_c_args = []
private_compile_cpp_args = []
private_dependencies = []
private_include_directories = []
# private_link_whole = []
# private_link_with = []
private_link_args = []
private_override_options = []

interface_compile_args = []
# interface_include_directories = []
# interface_link_whole = []
# interface_link_with = []
interface_link_args = []

# --- Compile Args ---
if host_machine.system() == 'windows' and glob_library_type == 'shared'
  private_compile_args += ['-D' + glob_sirius_win_dll]
  interface_compile_args += ['-D' + glob_sirius_win_dll]
endif

# --- Dependencies ---

# --- Include Directories ---

# --- Link Args ---

# --- Sirius::sirius ---
subdir('sirius')

# ---
