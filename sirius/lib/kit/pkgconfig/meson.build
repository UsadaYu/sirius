pkg = import('pkgconfig')
pkg.generate(
  sirius_kit_library,
  description: 'sirius::' + kit_library_name,
  extra_cflags: kit_pkgconfig_extra_cflags,
  libraries: kit_pkgconfig_libraries,
  libraries_private: kit_pkgconfig_libraries_private,
  name: kit_library_name,
  requires: kit_pkgconfig_requires,
  requires_private: kit_pkgconfig_requires_private,
  url: 'https://github.com/UsadaYu/sirius',
  version: kit_version,
)
