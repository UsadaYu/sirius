cmake_path(SET __pkgconfig_prefix NORMALIZE ${CMAKE_INSTALL_PREFIX})
set(__pkgconfig_lib_name ${libsirius})
set(__pkgconfig_version ${PROJECT_VERSION})
list(JOIN __pkgconfig_libs " " __pkgconfig_libs)
list(JOIN __pkgconfig_libs_private " " __pkgconfig_libs_private)
list(JOIN __pkgconfig_cflags " " __pkgconfig_cflags)
cmake_path(SET pkg_file NORMALIZE "${CMAKE_CURRENT_BINARY_DIR}/${libsirius}.pc")
configure_file("sirius.pc.in" ${pkg_file} @ONLY)
install(
  FILES ${pkg_file}
  COMPONENT "${PROJECT_NAME}"
  DESTINATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig")
