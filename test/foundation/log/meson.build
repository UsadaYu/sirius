groups = [
  {
    'name': 'Log1',
    'sources': ['Log1.c'],
    'stds': test_c_stds,
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
            '-D_SIRIUS_LOG_PRINT_NAME="@0@"'.format(target),
            '-DGEN_FILE_NAME="@0@"'.format(gen),
        ],
        'sources': group['sources'],
        'subdir': fs.name(meson.current_source_dir()),
      }
    ]
  endforeach
endforeach
