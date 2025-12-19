cmake_minimum_required(VERSION 3.25)

file(READ "config/build/version.json" data)
string(JSON version GET "${data}" "version")
string(REPLACE "." ";" version_parts ${version})
list(GET version_parts 0 SIRIUS_VERSION_MAJOR)
list(GET version_parts 1 SIRIUS_VERSION_MINOR)
list(GET version_parts 2 SIRIUS_VERSION_PATCH)
project(
  PROJECT-SIRIUS
  VERSION "${version}"
  DESCRIPTION "Sirius"
  LANGUAGES "C" "CXX"
  HOMEPAGE_URL "https://github.com/UsadaYu/sirius")
unset(version_parts)
unset(version)
unset(data)

message(STATUS "CMAKE_GENERATOR: ${CMAKE_GENERATOR}")

# --- Include ---
include(GNUInstallDirs)
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckLinkerFlag)

include(cmake/utils.cmake)
include(CMakeOptions.cmake)

# --- Global Dependencies ---
# compiler
if(NOT CMAKE_C_COMPILER_ID STREQUAL CMAKE_CXX_COMPILER_ID)
  message(
    FATAL_ERROR
      "
  The compilers for C and CXX require the same type
  C compiler:     ${CMAKE_C_COMPILER_ID}
  CXX compiler:   ${CMAKE_CXX_COMPILER_ID}
")
endif()

file(READ "config/build/deps.json" deps_data)

set(compiler_ids "GNU" "Clang" "MSVC")
set(deps_keys "gnu" "clang" "msvc")
foreach(compiler_id deps_key IN ZIP_LISTS compiler_ids deps_keys)
  string(JSON var GET "${deps_data}" "${deps_key}" "version")
  if(CMAKE_C_COMPILER_ID STREQUAL compiler_id)
    if(CMAKE_C_COMPILER_VERSION VERSION_LESS ${var})
      message(
        FATAL_ERROR
          "
  C compiler - ${CMAKE_C_COMPILER_ID}, whose version is not accepted
  Current version: ${CMAKE_C_COMPILER_VERSION}
  Required version: ${var}
  please edit cmake cache and rebuild
  ")
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${var})
      message(
        FATAL_ERROR
          "
  CXX compiler - ${CMAKE_CXX_COMPILER_ID}, whose version is not accepted
  Current version: ${CMAKE_CXX_COMPILER_VERSION}
  Required version: ${var}
  please edit cmake cache and rebuild
  ")
    endif()
  endif()
  unset(var)
endforeach()
unset(deps_key)
unset(compiler_id)
unset(deps_keys)
unset(compiler_ids)

unset(deps_data)

# --- Global Variables ---
set(GLOB_SIRIUS_WIN_DLL
    "SIRIUS_WIN_DLL"
    CACHE INTERNAL "" FORCE)
cmake_path(SET GLOB_INCLUDE NORMALIZE "")
set(__cmake_target_export_name "${SIRIUS_TARGET_CMAKE_NAMESPACE}Targets")

# --- Preprocessing ---
add_subdirectory(include/sirius)

list(APPEND GLOB_INCLUDE "${PROJECT_SOURCE_DIR}/include")
install(
  DIRECTORY "include/sirius"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT "${PROJECT_NAME}"
  FILES_MATCHING
  PATTERN "*.def"
  PATTERN "*.h"
  PATTERN "*.hpp"
  PATTERN "*.hxx"
  PATTERN "*.*.in" EXCLUDE
  PATTERN "CMakeFiles" EXCLUDE
  PATTERN "*.cmake" EXCLUDE)

# --- Sirius ---
add_subdirectory(src)

if(SIRIUS_TEST_ENABLE)
  enable_testing()
  add_subdirectory(test)
endif()
