# --- Generate Header Files ---
version_parts = glob_version.split('.')
conf_data = configuration_data()
conf_data.set('SIRIUS_VERSION_MAJOR', version_parts[0])
conf_data.set('SIRIUS_VERSION_MINOR', version_parts[1])
conf_data.set('SIRIUS_VERSION_PATCH', version_parts[2])
v_h = configure_file(
    input: 'sirius_common.h.in',
    output: 'sirius_common.h',
    configuration: conf_data,
    install_dir: 'include/sirius'
)

glob_include += include_directories(join_paths('..'))

# # 3. 公开头文件路径 (第二类)
# # 注意：Meson 会自动处理构建目录中的生成文件，只需包含其所在目录
# inc_public = include_directories('include')

# # 4. 构建
# lib = library('myproj', 
#     'src/core.cpp',
#     include_directories: [inc_private, inc_public],
#     install: true
# )

# # 5. 安装公开头文件
# install_headers('include/myproj/api.h', subdir: 'myproj')
