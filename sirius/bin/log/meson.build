bin_targets += [
  {
    'target': get_option('exe-log-name'),
    'compile_args': [
      '-D_SIRIUS_LOG_MODULE_NAME="@0@"'.format(get_option('exe-log-name')),
    ],
    'sources': [
      join_paths('main.cpp'),
    ],
    'dependencies': [
      dependency('threads', required: true),
      sirius_foundation_dep,
    ],
    'subdir': fs.name(meson.current_source_dir()),
  }
]
