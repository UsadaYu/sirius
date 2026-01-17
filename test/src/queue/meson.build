groups = [
  {
    'name': 'Queue1',
    'sources': ['Queue1.cpp'],
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
