cmake_path(SET __pkgconfig_prefix NORMALIZE ${CMAKE_INSTALL_PREFIX})
string(REPLACE " " "\\ " __pkgconfig_prefix "${__pkgconfig_prefix}")

set(__pkgconfig_lib_name ${libsirius})
set(__pkgconfig_version ${PROJECT_VERSION})

set(__pkgconfig_libs "")
set(__pkgconfig_libs_private "")
set(__pkgconfig_cflags "")

foreach(var IN LISTS sirius_pkgconfig_libs)
  string(REPLACE " " "\\ " var "${var}")
  set(__pkgconfig_libs "${__pkgconfig_libs}${var} ")
endforeach()

foreach(var IN LISTS sirius_pkgconfig_libs_private)
  string(REPLACE " " "\\ " var "${var}")
  set(__pkgconfig_libs_private "${__pkgconfig_libs_private}${var} ")
endforeach()

foreach(var IN LISTS sirius_pkgconfig_cflags)
  string(REPLACE " " "\\ " var "${var}")
  set(__pkgconfig_cflags "${__pkgconfig_cflags}${var} ")
endforeach()

cmake_path(SET pkg_file NORMALIZE "${CMAKE_CURRENT_BINARY_DIR}/${libsirius}.pc")
configure_file("sirius.pc.in" ${pkg_file} @ONLY)
install(
  FILES ${pkg_file}
  COMPONENT "${PROJECT_NAME}"
  DESTINATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig")
