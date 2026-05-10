groups = [
  {
    'name': 'Log1',
    'sources': ['Log1.cpp'],
    'stds': test_cpp_stds,
  },
]

foreach group : groups
  foreach std : group['stds']
    target = group['name'] + '_' + std['suffix']
    gen = '_gen_' + target + '.log'
    test_targets += [
      {
        'target': target,
        'compile_args': [
            std['std'],
            '-D_SIRIUS_LOG_MODULE_NAME="@0@"'.format(target),
            '-D_GEN_FILE_NAME="@0@"'.format(gen),
        ],
        'dependencies': kit_dependencies,
        'sources': group['sources'],
        'subdir': join_paths(kit_updir, fs.name(meson.current_source_dir())),
      }
    ]
  endforeach
endforeach
