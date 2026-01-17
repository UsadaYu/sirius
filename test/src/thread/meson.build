groups = [
  {
    'name': 'Thread1',
    'sources': ['Thread1.cpp'],
    'stds': test_cpp_stds,
  },
  {
    'name': 'Thread2',
    'sources': ['Thread2.cpp'],
    'stds': test_cpp_stds,
  },
  {
    'name': 'Thread3',
    'sources': ['Thread3.cpp'],
    'stds': test_cpp_stds,
  },
  {
    'name': 'Thread4',
    'sources': ['Thread4.c'],
    'stds': test_c_stds,
  },
  {
    'name': 'Thread5',
    'sources': ['Thread5.cpp'],
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
            '-Dsirius_log_module_name="@0@"'.format(target)
        ],
        'sources': group['sources'],
        'subdir': fs.name(meson.current_source_dir()),
      }
    ]
  endforeach
endforeach
