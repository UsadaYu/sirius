pkg = import('pkgconfig')
pkg.generate(
  sirius_c_library,
  description: 'sirius::' + c_library_name,
  extra_cflags: c_pkgconfig_extra_cflags,
  libraries: c_pkgconfig_libraries,
  libraries_private: c_pkgconfig_libraries_private,
  name: c_library_name,
  requires: c_pkgconfig_requires,
  requires_private: c_pkgconfig_requires_private,
  url: 'https://github.com/UsadaYu/sirius',
  version: c_version,
)
