pkg = import('pkgconfig')
pkg.generate(
  sirius_library,
  description: 'sirius::' + libsirius,
  extra_cflags: sirius_interface_compile_args,
  libraries: sirius_interface_link_args,
  name: libsirius,
  url: 'https://github.com/UsadaYu/sirius',
  version: glob_version,
)
