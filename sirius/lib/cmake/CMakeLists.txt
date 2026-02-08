# --- Include ---
include(CMakePackageConfigHelpers)

cmake_path(SET cmake_package_install_dir NORMALIZE
           "${CMAKE_INSTALL_LIBDIR}/cmake/${SIRIUS_NAMESPACE}")

cmake_path(SET version_file NORMALIZE
           "${CMAKE_CURRENT_BINARY_DIR}/${SIRIUS_NAMESPACE}ConfigVersion.cmake")
cmake_path(SET config_file NORMALIZE
           "${CMAKE_CURRENT_BINARY_DIR}/${SIRIUS_NAMESPACE}Config.cmake")

configure_package_config_file("Config.cmake.in" ${config_file}
                              INSTALL_DESTINATION ${cmake_package_install_dir})
write_basic_package_version_file(
  ${version_file}
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion)
install(FILES ${version_file} ${config_file}
        DESTINATION ${cmake_package_install_dir})
install(
  EXPORT ${__cmake_target_export_name}
  COMPONENT "${PROJECT_NAME}"
  FILE "${__cmake_target_export_name}.cmake"
  NAMESPACE ${SIRIUS_NAMESPACE}::
  DESTINATION ${cmake_package_install_dir})
