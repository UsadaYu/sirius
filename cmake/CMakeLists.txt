# requirement
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/requirement.txt" _require_content)
macro(version_read VERSION_VARIABLE REQUIRE_CONTENT REGEX_PREFIX)
  string(REGEX MATCH "${REGEX_PREFIX}_VERSION=([0-9.]+)" _version_match
               "${REQUIRE_CONTENT}")
  if(_version_match)
    set(${VERSION_VARIABLE} "${CMAKE_MATCH_1}")
  endif()
endmacro()
version_read(_required_gnuc_version "${_require_content}" "GNUC")
version_read(_required_clang_version "${_require_content}" "CLANG")
version_read(_required_msvc_version "${_require_content}" "MSVC")
unset(_require_content)

macro(version_check REQUIRE_CONTENT)
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${REQUIRE_CONTENT})
    message(
      WARNING "${CMAKE_CXX_COMPILER_ID} version: "
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
  message(FATAL_ERROR "${CMAKE_CXX_COMPILER_ID} is not supported")
endif()
unset(_required_gnuc_version)
unset(_required_clang_version)
unset(_required_msvc_version)

configure_file(
  "${PROJECT_SOURCE_DIR}/include/sirius/internal/sirius_common.h.in"
  "${PROJECT_SOURCE_DIR}/include/sirius/sirius_common.h" @ONLY)

# install directory
cmake_path(SET _target_bin_dir NORMALIZE
           "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
cmake_path(SET _target_lib_dir NORMALIZE
           "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
cmake_path(SET _target_include_dir NORMALIZE
           "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}")
cmake_path(SET _target_pkg_dir NORMALIZE
           "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig")
cmake_path(
  SET
  _target_package_dir
  NORMALIZE
  "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/cmake/${SIRIUS_TARGET_CMAKE_NAMESPACE}"
)

# args
set(PRIVATE_LINK_DIR_LIST "")
set(PRIVATE_LINK_LIBS_LIST "")
set(ITFC_INCLUDE_DIR_LIST "")
set(ITFC_LINK_DIR_LIST "")
set(ITFC_LINK_LIBS_LIST "")

# --- include directories ---
set(ITFC_INCLUDE_DIR_LIST ${_target_include_dir})

# --- link directories ---
list(APPEND ITFC_LINK_DIR_LIST ${SIRIUS_EXTRA_LINK_DIR})
if(BUILD_SHARED_LIBS)
  list(APPEND PRIVATE_LINK_DIR_LIST ${SIRIUS_EXTRA_LINK_DIR})
endif()

# --- link libraries ---
list(APPEND ITFC_LINK_LIBS_LIST ${SIRIUS_EXTRA_LINK_LIBRARIES})
if(BUILD_SHARED_LIBS)
  list(APPEND PRIVATE_LINK_LIBS_LIST ${SIRIUS_EXTRA_LINK_LIBRARIES})
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  list(APPEND ITFC_LINK_LIBS_LIST "winmm")
  if(BUILD_SHARED_LIBS)
    list(APPEND PRIVATE_LINK_LIBS_LIST "winmm")
  endif()
else()
  list(APPEND ITFC_LINK_LIBS_LIST "pthread")
  if(BUILD_SHARED_LIBS)
    list(APPEND PRIVATE_LINK_LIBS_LIST "pthread")
  endif()
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/external_option.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/internal_option.cmake)

include(CMakePackageConfigHelpers)

# pkg interface
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

  set(_pkg_interface_link_dir "")
  set(_pkg_interface_link_libraries "")
  set(_pkg_interface_private_link_dir "")
  set(_pkg_interface_private_link_libraries "")
  set(_pkg_interface_cflags "")

  list(APPEND _pkg_interface_link_dir "-L${_target_lib_dir}")
  if(BUILD_SHARED_LIBS)
    foreach(dir IN LISTS PRIVATE_LINK_DIR_LIST)
      if(NOT dir)
        continue()
      endif()
      list(APPEND _pkg_interface_private_link_dir "-L${dir}")
    endforeach()
  else()
    foreach(dir IN LISTS ITFC_LINK_DIR_LIST)
      if(NOT dir)
        continue()
      endif()
      list(APPEND _pkg_interface_link_dir "-L${dir}")
    endforeach()
  endif()
  unset(dir)

  list(APPEND _pkg_interface_link_libraries
       "-l${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME}")
  if(BUILD_SHARED_LIBS)
    foreach(lib IN LISTS PRIVATE_LINK_LIBS_LIST)
      if(NOT lib)
        continue()
      elseif(lib MATCHES "::")
        string(REGEX REPLACE ".*::" "" clean "${lib}")
      else()
        set(clean "${lib}")
      endif()

      list(APPEND _pkg_interface_private_link_libraries "-l${clean}")
    endforeach()
  else()
    foreach(lib IN LISTS ITFC_LINK_LIBS_LIST)
      if(NOT lib)
        continue()
      elseif(lib MATCHES "::")
        string(REGEX REPLACE ".*::" "" clean "${lib}")
      else()
        set(clean "${lib}")
      endif()

      list(APPEND _pkg_interface_link_libraries "-l${clean}")
    endforeach()
  endif()
  unset(clean)
  unset(lib)

  if(SIRIUS_ASAN)
    list(APPEND _pkg_interface_link_libraries "-fsanitize=address")
    list(APPEND _pkg_interface_cflags "-fsanitize=address")
    list(APPEND _pkg_interface_cflags "-fno-omit-frame-pointer")
    list(APPEND _pkg_interface_cflags "-fsanitize-recover=address")
  endif()

  if(BUILD_SHARED_LIBS AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND _pkg_interface_cflags "-D${SIRIUS_WIN_DLL} ")
  endif()

  list(JOIN _pkg_interface_link_dir " " _pkg_interface_link_dir)
  list(JOIN _pkg_interface_link_libraries " " _pkg_interface_link_libraries)
  list(JOIN _pkg_interface_private_link_dir " " _pkg_interface_private_link_dir)
  list(JOIN _pkg_interface_private_link_libraries " "
       _pkg_interface_private_link_libraries)
  list(JOIN _pkg_interface_cflags " " _pkg_interface_cflags)

  set(_pkg_file
      "${PROJECT_BINARY_DIR}/itfc/pkgconfig/${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME}.pc"
  )
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/sirius.pc.in" ${_pkg_file} @ONLY)
  install(
    FILES ${_pkg_file}
    COMPONENT "${PROJECT_NAME}"
    DESTINATION ${_target_pkg_dir})
endif()

# --- install ---
set(_cmake_target_export_name "${SIRIUS_TARGET_CMAKE_NAMESPACE}Targets")

set_target_properties(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME}
                      PROPERTIES VERSION ${SIRIUS_VERSION})

# library
install(
  TARGETS ${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME}
  EXPORT ${_cmake_target_export_name}
  COMPONENT "${PROJECT_NAME}"
  ARCHIVE DESTINATION ${_target_lib_dir}
  LIBRARY DESTINATION ${_target_lib_dir}
  RUNTIME DESTINATION ${_target_bin_dir})

# include
install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/include/sirius
  DESTINATION ${_target_include_dir}
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "internal" EXCLUDE)

# cmake interface
set(_version_file
    "${PROJECT_BINARY_DIR}/itfc/cmake/${SIRIUS_TARGET_CMAKE_NAMESPACE}ConfigVersion.cmake"
)
set(_config_file
    "${PROJECT_BINARY_DIR}/itfc/cmake/${SIRIUS_TARGET_CMAKE_NAMESPACE}Config.cmake"
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in ${_config_file}
  INSTALL_DESTINATION ${_target_package_dir})
write_basic_package_version_file(
  ${_version_file}
  VERSION ${SIRIUS_VERSION}
  COMPATIBILITY AnyNewerVersion)
install(FILES ${_version_file} ${_config_file}
        DESTINATION ${_target_package_dir})
install(
  EXPORT ${_cmake_target_export_name}
  COMPONENT "${PROJECT_NAME}"
  FILE "${_cmake_target_export_name}.cmake"
  NAMESPACE ${SIRIUS_TARGET_CMAKE_NAMESPACE}::
  DESTINATION ${_target_package_dir})

unset(_cmake_target_export_name)
