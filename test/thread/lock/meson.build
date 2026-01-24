groups = [
  {
    'compile_args': ['-DLOCK_TYPE=0'],
    'name': 'Lock1-Spinlock',
    'sources': ['Lock1.c'],
    'stds': test_c_stds,
  },
  {
    'compile_args': ['-DLOCK_TYPE=1'],
    'name': 'Lock1-Mutex',
    'sources': ['Lock1.c'],
    'stds': test_c_stds,
  },
  {
    'compile_args': [],
    'name': 'Lock2',
    'sources': ['Lock2-Mutex.cpp'],
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
            '-Dsirius_log_module_name="@0@"'.format(target),
            group['compile_args'],
        ],
        'sources': group['sources'],
        'subdir': fs.name(meson.current_source_dir()),
      }
    ]
  endforeach
endforeach
