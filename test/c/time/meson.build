groups = [
  {
    'name': 'Time1',
    'sources': ['Time1.c'],
    'stds': test_c_stds,
  },
  {
    'name': 'Time2',
    'sources': ['Time2.cpp'],
    'stds': test_cpp_stds,
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
