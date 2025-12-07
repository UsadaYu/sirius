include(${CMAKE_CURRENT_SOURCE_DIR}/external_option.cmake)

# requirement
file(READ "${PROJECT_SOURCE_DIR}/deps.txt" _require_content)
macro(version_read VERSION_VARIABLE REQUIRE_CONTENT REGEX_PREFIX)
  string(REGEX MATCH "${REGEX_PREFIX}_VERSION=([0-9.]+)" _version_match
               "${REQUIRE_CONTENT}")
  if(_version_match)
    set(${VERSION_VARIABLE} "${CMAKE_MATCH_1}")
  endif()
endmacro()
version_read(_required_gnu_version "${_require_content}" "GNU")
version_read(_required_clang_version "${_require_content}" "CLANG")
version_read(_required_msvc_version "${_require_content}" "MSVC")
unset(_require_content)

macro(version_check REQUIRE_CONTENT)
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${REQUIRE_CONTENT})
    message(
      FATAL_ERROR
        "
  `${CMAKE_CXX_COMPILER_ID}` version is not accepted
  Current version: ${CMAKE_CXX_COMPILER_VERSION}
  Required version: ${REQUIRE_CONTENT}
  please edit cmake cache or rebuild
")
  endif()
endmacro()

if(NOT CMAKE_C_COMPILER_ID STREQUAL CMAKE_CXX_COMPILER_ID)
  message(
    FATAL_ERROR
      "
  The compilers for C and CXX need to be of the same type
  C compiler: ${CMAKE_C_COMPILER_ID}; CXX compiler: ${CMAKE_CXX_COMPILER_ID}
")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  version_check("${_required_gnu_version}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  version_check("${_required_clang_version}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  version_check("${_required_msvc_version}")
else()
  message(FATAL_ERROR "Compiler `${CMAKE_CXX_COMPILER_ID}` is not supported")
endif()
unset(_required_gnu_version)
unset(_required_clang_version)
unset(_required_msvc_version)

# preprocessing
cmake_path(SET _src_dir NORMALIZE "${PROJECT_SOURCE_DIR}/include/sirius/custom")
cmake_path(SET _dst_dir NORMALIZE "${TARGET_INCLUDE_DIR}/sirius/custom")
file(COPY "${_src_dir}/" DESTINATION "${_dst_dir}")
cmake_path(SET _src_dir NORMALIZE "${PROJECT_SOURCE_DIR}/include/sirius")
cmake_path(SET _dst_dir NORMALIZE "${TARGET_INCLUDE_DIR}/sirius")
file(GLOB _header_files "${_src_dir}/*.h")
file(COPY ${_header_files} DESTINATION "${_dst_dir}")
configure_file(
  "${PROJECT_SOURCE_DIR}/include/sirius/internal/sirius_common.h.in"
  "${_dst_dir}/sirius_common.h" @ONLY)
cmake_path(SET _src_dir NORMALIZE
           "${PROJECT_SOURCE_DIR}/include/sirius/internal")
cmake_path(SET _dst_dir NORMALIZE
           "${TARGET_DIR}/internal/include/sirius/internal")
file(COPY "${_src_dir}/" DESTINATION "${_dst_dir}")
unset(_dst_dir)
unset(_src_dir)

# install directory
cmake_path(SET _target_bin_dir NORMALIZE
           "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
cmake_path(SET _target_lib_dir NORMALIZE
           "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
cmake_path(SET _target_include_dir NORMALIZE
           "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}")
cmake_path(SET _target_pkgconfig_dir NORMALIZE "${_target_lib_dir}/pkgconfig")
cmake_path(SET _target_cmake_package_dir NORMALIZE
           "${_target_lib_dir}/cmake/${SIRIUS_TARGET_CMAKE_NAMESPACE}")

# args
set(PRIVATE_INCLUDE_DIR_LIST "")
set(PRIVATE_LINK_DIR_LIST "")
set(PRIVATE_LINK_LIBS_LIST "")
set(ITFC_INCLUDE_DIR_LIST "")
set(ITFC_LINK_DIR_LIST "")
set(ITFC_LINK_LIBS_LIST "")

# --- include directories ---
list(APPEND PRIVATE_INCLUDE_DIR_LIST ${TARGET_INCLUDE_DIR})
list(APPEND PRIVATE_INCLUDE_DIR_LIST "${TARGET_DIR}/internal/include")

list(APPEND ITFC_INCLUDE_DIR_LIST ${_target_include_dir})

# --- link directories ---
list(APPEND ITFC_LINK_DIR_LIST ${SIRIUS_EXTRA_LINK_DIRECTORIES})
if(BUILD_SHARED_LIBS)
  list(APPEND PRIVATE_LINK_DIR_LIST ${SIRIUS_EXTRA_LINK_DIRECTORIES})
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

# --- source ---
set(_src_list "")
set(_src_dir ${PROJECT_SOURCE_DIR}/src)
aux_source_directory(${_src_dir} _aux_src_list)
aux_source_directory(${_src_dir}/internal _aux_src_internal_list)
list(APPEND _src_list ${_aux_src_list})
list(APPEND _src_list ${_aux_src_internal_list})
unset(_aux_src_internal_list)
unset(_aux_src_list)

if(SIRIUS_BUILD_OBJECT_ONLY)
  add_library(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS} OBJECT ${_src_list})
else()
  if(BUILD_SHARED_LIBS)
    add_library(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS} SHARED ${_src_list})

    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
      target_compile_definitions(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
                                 PUBLIC ${SIRIUS_WIN_DLL})
    endif()
  else()
    add_library(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS} STATIC ${_src_list})
  endif()
endif()
add_library(
  ${SIRIUS_TARGET_CMAKE_NAMESPACE}::${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
  ALIAS ${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS})

foreach(_suffix "" "_DEBUG" "_RELEASE" "_RELWITHDEBINFO" "_MINSIZEREL")
  set_target_properties(
    ${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
    PROPERTIES ARCHIVE_OUTPUT_DIRECTORY${_suffix} ${TARGET_LIB_DIR}
               LIBRARY_OUTPUT_DIRECTORY${_suffix} ${TARGET_LIB_DIR}
               RUNTIME_OUTPUT_DIRECTORY${_suffix} ${TARGET_LIB_DIR})
endforeach()
unset(_suffix)

# --- version ---
set_target_properties(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
                      PROPERTIES VERSION ${SIRIUS_VERSION})

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(SIRIUS_PIC_ENABLE)
    target_compile_options(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
                           PRIVATE "-fPIC")
  else()
    target_compile_options(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
                           PRIVATE "-fno-PIC")
  endif()

  target_compile_options(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
                         PRIVATE "-fvisibility=hidden")
endif()

# --- all warnings ---
if(SIRIUS_WARNING_ALL)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
                           PRIVATE "/W4")
  else()
    target_compile_options(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
                           PRIVATE "-Wall")
  endif()
endif()

# --- regard all warnings as errors ---
if(SIRIUS_WARNING_ERROR)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
                           PRIVATE "/WX")
  else()
    target_compile_options(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
                           PRIVATE "-Werror")
  endif()
endif()

target_compile_definitions(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
                           PRIVATE "SIRIUS_BUILDING")

target_compile_definitions(
  ${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
  PRIVATE "sirius_log_module_name=\"${SIRIUS_MODULE_PRINT_NAME}\"")

target_compile_definitions(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
                           PRIVATE "WRITE_SIZE=${SIRIUS_WRITE_SIZE}")

target_compile_definitions(
  ${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
  PRIVATE "INTERNAL_LOG_LEVEL=${SIRIUS_INTERNAL_LOG_LEVEL}")

target_include_directories(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
                           PRIVATE ${PRIVATE_INCLUDE_DIR_LIST})
target_link_directories(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS} PRIVATE
                        ${PRIVATE_LINK_DIR_LIST})
target_link_libraries(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
                      PRIVATE ${PRIVATE_LINK_LIBS_LIST})

target_include_directories(
  ${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS} SYSTEM
  INTERFACE "$<BUILD_INTERFACE:${TARGET_INCLUDE_DIR}>"
            "$<INSTALL_INTERFACE:${ITFC_INCLUDE_DIR_LIST}>")
target_link_directories(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS} INTERFACE
                        ${ITFC_LINK_DIR_LIST})
target_link_libraries(${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
                      INTERFACE ${ITFC_LINK_LIBS_LIST})

# --- install ---
macro(_sirius_install)
  if(SIRIUS_INSTALL)
    install(${ARGV})
  endif()
endmacro()

include(CMakePackageConfigHelpers)

# pkg interface
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(__pkgconfig_link_dir "")
  set(__pkgconfig_link_libraries "")
  set(__pkgconfig_private_link_dir "")
  set(__pkgconfig_private_link_libraries "")
  set(__pkgconfig_cflags "")

  # link directories
  if(NOT SIRIUS_BUILD_OBJECT_ONLY)
    list(APPEND __pkgconfig_link_dir "-L${_target_lib_dir}")
  endif()
  if(BUILD_SHARED_LIBS)
    foreach(dir IN LISTS PRIVATE_LINK_DIR_LIST)
      if(NOT dir)
        continue()
      endif()
      list(APPEND __pkgconfig_private_link_dir "-L${dir}")
    endforeach()
  else()
    foreach(dir IN LISTS ITFC_LINK_DIR_LIST)
      if(NOT dir)
        continue()
      endif()
      list(APPEND __pkgconfig_link_dir "-L${dir}")
    endforeach()
  endif()
  unset(dir)

  # link libraries
  list(APPEND __pkgconfig_link_libraries
       "-l${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}")
  if(BUILD_SHARED_LIBS)
    foreach(lib IN LISTS PRIVATE_LINK_LIBS_LIST)
      if(NOT lib)
        continue()
      elseif(lib MATCHES "::")
        string(REGEX REPLACE ".*::" "" clean "${lib}")
      else()
        set(clean "${lib}")
      endif()
      list(APPEND __pkgconfig_private_link_libraries "-l${clean}")
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
      list(APPEND __pkgconfig_link_libraries "-l${clean}")
    endforeach()
  endif()
  unset(clean)
  unset(lib)

  # cflags
  if(BUILD_SHARED_LIBS AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND __pkgconfig_cflags "-D${SIRIUS_WIN_DLL} ")
  endif()

  foreach(dir IN LISTS ITFC_INCLUDE_DIR_LIST)
    if(NOT dir)
      continue()
    endif()
    list(APPEND __pkgconfig_cflags "-isystem ${dir}")
  endforeach()

  list(JOIN __pkgconfig_link_dir " " __pkgconfig_link_dir)
  list(JOIN __pkgconfig_link_libraries " " __pkgconfig_link_libraries)
  list(JOIN __pkgconfig_private_link_dir " " __pkgconfig_private_link_dir)
  list(JOIN __pkgconfig_private_link_libraries " "
       __pkgconfig_private_link_libraries)
  list(JOIN __pkgconfig_cflags " " __pkgconfig_cflags)

  set(_pkg_file
      "${TARGET_LIB_DIR}/pkgconfig/${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}.pc")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/sirius.pc.in" ${_pkg_file} @ONLY)
  _sirius_install(FILES ${_pkg_file} COMPONENT "${PROJECT_NAME}" DESTINATION
                  ${_target_pkgconfig_dir})
endif()

set(_cmake_target_export_name "${SIRIUS_TARGET_CMAKE_NAMESPACE}Targets")

# library
_sirius_install(
  TARGETS
  ${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}
  EXPORT
  ${_cmake_target_export_name}
  COMPONENT
  "${PROJECT_NAME}"
  ARCHIVE
  DESTINATION
  ${_target_lib_dir}
  LIBRARY
  DESTINATION
  ${_target_lib_dir}
  RUNTIME
  DESTINATION
  ${_target_bin_dir})

# include
_sirius_install(DIRECTORY "${TARGET_INCLUDE_DIR}/sirius" COMPONENT
                "${PROJECT_NAME}" DESTINATION ${_target_include_dir})

# cmake interface
set(_version_file
    "${TARGET_LIB_DIR}/cmake/${SIRIUS_TARGET_CMAKE_NAMESPACE}ConfigVersion.cmake"
)
set(_config_file
    "${TARGET_LIB_DIR}/cmake/${SIRIUS_TARGET_CMAKE_NAMESPACE}Config.cmake")

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in ${_config_file}
  INSTALL_DESTINATION ${_target_cmake_package_dir})
write_basic_package_version_file(
  ${_version_file}
  VERSION ${SIRIUS_VERSION}
  COMPATIBILITY AnyNewerVersion)
_sirius_install(FILES ${_version_file} ${_config_file} DESTINATION
                ${_target_cmake_package_dir})
_sirius_install(
  EXPORT
  ${_cmake_target_export_name}
  COMPONENT
  "${PROJECT_NAME}"
  FILE
  "${_cmake_target_export_name}.cmake"
  NAMESPACE
  ${SIRIUS_TARGET_CMAKE_NAMESPACE}::
  DESTINATION
  ${_target_cmake_package_dir})

unset(_config_file)
unset(_version_file)

unset(_cmake_target_export_name)
unset(_sirius_install)

# ---
