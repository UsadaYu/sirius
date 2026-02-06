groups = [
  {
    'name': 'Sem1',
    'sources': ['Sem1.c'],
    'stds': test_c_stds,
  },
]

foreach group : groups
  foreach std : group['stds']
    target = group['name'] + '_' + std['suffix']
    test_targets += [
      {
        'target': target,
        'compile_args': [
            std['std'],
            '-D_SIRIUS_LOG_PRINT_NAME="@0@"'.format(target)
        ],
        'sources': group['sources'],
        'subdir': fs.name(meson.current_source_dir()),
      }
    ]
  endforeach
endforeach
