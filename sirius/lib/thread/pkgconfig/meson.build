pkg = import('pkgconfig')
pkg.generate(
  sirius_thread_library,
  description: 'sirius::' + thread_library_name,
  extra_cflags: thread_pkgconfig_extra_cflags,
  libraries: thread_pkgconfig_libraries,
  libraries_private: thread_pkgconfig_libraries_private,
  name: thread_library_name,
  requires: thread_pkgconfig_requires,
  requires_private: thread_pkgconfig_requires_private,
  url: 'https://github.com/UsadaYu/sirius',
  version: thread_version,
)
