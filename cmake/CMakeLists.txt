file(
  READ "${CMAKE_CURRENT_SOURCE_DIR}/requirement.txt"
  _require_content
)
macro(
  version_read
  VERSION_VARIABLE REQUIRE_CONTENT REGEX_PREFIX
)
  string(
    REGEX MATCH "${REGEX_PREFIX}_VERSION=([0-9.]+)"
    _version_match "${REQUIRE_CONTENT}"
  )
  if(_version_match)
    set(${VERSION_VARIABLE} "${CMAKE_MATCH_1}")
  endif()
endmacro()
version_read(_required_gnuc_version "${_require_content}" "GNUC")
version_read(_required_clang_version "${_require_content}" "CLANG")
version_read(_required_msvc_version "${_require_content}" "MSVC")

macro(
  version_check
  REQUIRE_CONTENT
)
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${REQUIRE_CONTENT})
    message(WARNING
      "${CMAKE_CXX_COMPILER_ID} version: "
      "[current: ${CMAKE_CXX_COMPILER_VERSION}] "
      "[required: ${REQUIRE_CONTENT}]\n"
      "please edit cmake cache or rebuild")
  endif()
endmacro()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  version_check("${_required_gnuc_version}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  version_check("${_required_clang_version}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  version_check("${_required_msvc_version}")
else()
  message(FATAL_ERROR
    "${CMAKE_CXX_COMPILER_ID} is not supported")
endif()

configure_file(
  "${PROJECT_SOURCE_DIR}/include/internal/sirius_common.h.in"
  "${PROJECT_SOURCE_DIR}/include/sirius_common.h"
  @ONLY
)

include(${CMAKE_CURRENT_SOURCE_DIR}/external_option.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/internal_option.cmake)

# install directory
set(_target_lib_dirr "lib")
set(_target_pkg_dirr "lib/pkgconfig")
set(_target_package_dirr "lib/cmake/${SIRIUS_TARGET_NAME}")
set(_target_lib_dir "${CMAKE_INSTALL_PREFIX}/${_target_lib_dirr}")
set(_target_pkg_dir "${CMAKE_INSTALL_PREFIX}/${_target_pkg_dirr}")
set(_target_package_dir "${CMAKE_INSTALL_PREFIX}/${_target_package_dirr}")

# libraries install
install(
  TARGETS ${SIRIUS_TARGET_NAME}
  EXPORT "${SIRIUS_TARGET_NAME}Targets"
  DESTINATION ${_target_lib_dir}
)
set_target_properties(
  ${SIRIUS_TARGET_NAME} PROPERTIES
  VERSION ${SIRIUS_VERSION}
)
set_target_properties(
  ${SIRIUS_TARGET_NAME} PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/include"
)
set_target_properties(
  ${SIRIUS_TARGET_NAME} PROPERTIES
  INTERFACE_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/${_target_lib_dirr}"
)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set_target_properties(
    ${SIRIUS_TARGET_NAME} PROPERTIES
    INTERFACE_LINK_LIBRARIES "${SIRIUS_TARGET_NAME}"
  )

  set(_pkg_interface_link_libraries "")
else()
  set_target_properties(
    ${SIRIUS_TARGET_NAME} PROPERTIES
    INTERFACE_LINK_LIBRARIES "${SIRIUS_TARGET_NAME};pthread"
  )

  set(_pkg_interface_link_libraries "-lpthread")
endif()

# pkgconfig
set(
  _pkg_file
  "${PROJECT_BINARY_DIR}/${_target_pkg_dirr}/${SIRIUS_TARGET_NAME}.pc"
)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/sirius.pc.in"
  ${_pkg_file}
  @ONLY
)
install(
  FILES ${_pkg_file}
  DESTINATION ${_target_pkg_dir}
)

# `sirius` cmake package
set(
  _version_file
  "${PROJECT_BINARY_DIR}/${_target_package_dirr}/${SIRIUS_TARGET_NAME}ConfigVersion.cmake"
)
set(
  _config_file
  "${PROJECT_BINARY_DIR}/${_target_package_dirr}/${SIRIUS_TARGET_NAME}Config.cmake"
)
include(CMakePackageConfigHelpers)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
  ${_config_file}
  INSTALL_DESTINATION ${_target_package_dir}
)
# generate siriusConfigVersion.cmake
write_basic_package_version_file(
  ${_version_file}
  VERSION ${SIRIUS_VERSION}
  COMPATIBILITY AnyNewerVersion
)
# `sirius` cmake package install
install(
  FILES ${_version_file} ${_config_file}
  DESTINATION ${_target_package_dir}
)
install(
  EXPORT "${SIRIUS_TARGET_NAME}Targets"
  FILE "${SIRIUS_TARGET_NAME}Targets.cmake"
  NAMESPACE ${SIRIUS_TARGET_NAME}::
  DESTINATION ${_target_package_dir}
)

# header file install
install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_PREFIX}/include
  FILES_MATCHING PATTERN "*.h"
  PATTERN "internal" EXCLUDE
)
