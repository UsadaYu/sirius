bin_targets += [
  {
    'target': get_option('exe-log-name'),
    'compile_args': [],
    'sources': ['main.cpp'],
    'dependencies': [
      dependency(get_option('foundation-library-name')),
    ],
    # 'dependencies': dep_sirius_foundation,
    'subdir': fs.name(meson.current_source_dir()),
  }
]
