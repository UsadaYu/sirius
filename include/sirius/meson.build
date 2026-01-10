# --- Generate Header Files ---
version_parts = glob_version.split('.')
conf_data = configuration_data()
conf_data.set('SIRIUS_VERSION_MAJOR', version_parts[0])
conf_data.set('SIRIUS_VERSION_MINOR', version_parts[1])
conf_data.set('SIRIUS_VERSION_PATCH', version_parts[2])
v_h = configure_file(
    input: 'sirius_version.h.in',
    output: 'sirius_version.h',
    configuration: conf_data,
    install_dir: 'include/sirius'
)

glob_include += include_directories(join_paths('..'))

# install_headers('include/sirius/sirius_version.h', subdir: 'sirius')
