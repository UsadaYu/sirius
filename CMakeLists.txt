cmake_minimum_required(VERSION 3.25)

cmake_path(SET version_json NORMALIZE "config/build/version.json")
set_property(
  DIRECTORY
  APPEND
  PROPERTY CMAKE_CONFIGURE_DEPENDS ${version_json})
file(READ ${version_json} data)
string(JSON version GET "${data}" "version")
string(REPLACE "." ";" version_parts ${version})
list(GET version_parts 0 SIRIUS_VERSION_MAJOR)
list(GET version_parts 1 SIRIUS_VERSION_MINOR)
list(GET version_parts 2 SIRIUS_VERSION_PATCH)
project(
  "Sirius"
  VERSION "${version}"
  DESCRIPTION "Sirius"
  LANGUAGES "C" "CXX"
  HOMEPAGE_URL "https://github.com/UsadaYu/sirius")
unset(version_parts)
unset(version)
unset(data)
unset(version_json)

# --- Global Variables ---
set(GLOB_LIBRARY_TYPE)

# --- Include ---
include(GNUInstallDirs)

include(cmake/utils.cmake)
include(CMake_options.cmake)

# --- Preprocessing ---
if(SIRIUS_BUILD_OBJECT_ONLY)
  set(GLOB_LIBRARY_TYPE
      "OBJECT"
      CACHE INTERNAL "" FORCE)
else()
  if(BUILD_SHARED_LIBS)
    set(GLOB_LIBRARY_TYPE
        "SHARED"
        CACHE INTERNAL "" FORCE)
  else()
    set(GLOB_LIBRARY_TYPE
        "STATIC"
        CACHE INTERNAL "" FORCE)
  endif()
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(GLOB_LIBRARY_TYPE STREQUAL "SHARED")
    unset(SIRIUS_PIC_ENABLE CACHE)
    set(SIRIUS_PIC_ENABLE
        ON
        CACHE INTERNAL "" FORCE)
  endif()
  if(NOT SIRIUS_PIC_ENABLE)
    unset(SIRIUS_TEST_PIE_ENABLE CACHE)
    set(SIRIUS_TEST_PIE_ENABLE
        OFF
        CACHE INTERNAL "" FORCE)
  endif()
endif()

# --- Global Dependencies ---
# python3
find_package(Python3 REQUIRED)
#[================================[
set(python_paths "${PROJECT_SOURCE_DIR}/scripts" "$ENV{PYTHONPATH}")
cmake_path(CONVERT "${python_paths}" TO_NATIVE_PATH_LIST native_python_path)
set(ENV{PYTHONPATH} "${native_python_path}")
unset(native_python_path)
unset(python_paths)
#]================================]

# compiler
if(NOT CMAKE_C_COMPILER_ID STREQUAL CMAKE_CXX_COMPILER_ID)
  message(
    FATAL_ERROR
      "
  The compilers for C and CXX require the same type
  C compiler: ${CMAKE_C_COMPILER_ID}
  CXX compiler: ${CMAKE_CXX_COMPILER_ID}
  ")
endif()

if(CMAKE_C_SIMULATE_ID OR CMAKE_CXX_SIMULATE_ID)
  # Generally, CMake will automatically verify the mixed `SIMULATE`.
  if(NOT CMAKE_C_SIMULATE_ID STREQUAL CMAKE_CXX_SIMULATE_ID)
    message(
      FATAL_ERROR
        "
  The simulate for C and CXX require the same type
  C simulate: ${CMAKE_C_SIMULATE_ID}
  CXX simulate: ${CMAKE_CXX_SIMULATE_ID}
  ")
  endif()
endif()

utils_query_compiler_config(ACTION "min_version" RESULT min_version)
if(CMAKE_C_COMPILER_VERSION VERSION_LESS ${min_version})
  message(
    FATAL_ERROR
      "
  C compiler - ${CMAKE_C_COMPILER_ID}, whose version is not accepted
  Current version: ${CMAKE_C_COMPILER_VERSION}
  Required version: ${min_version}
  Please edit options and rebuild
  ")
endif()
if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${min_version})
  message(
    FATAL_ERROR
      "
  CXX compiler - ${CMAKE_CXX_COMPILER_ID}, whose version is not accepted
  Current version: ${CMAKE_CXX_COMPILER_VERSION}
  Required version: ${min_version}
  Please edit options and rebuild
  ")
endif()
unset(min_version)

add_subdirectory(sirius)

if(SIRIUS_TEST_ENABLE)
  enable_testing()
  add_subdirectory(test)
endif()

message(STATUS "------------------------------------")
message(STATUS "- Project Name: ${PROJECT_NAME}")
message(STATUS "- Project Version: ${PROJECT_VERSION}")
message(STATUS "- Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "- Library Type: ${GLOB_LIBRARY_TYPE}")
message(STATUS "- Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "- CMAKE_GENERATOR: ${CMAKE_GENERATOR}")
message(STATUS "------------------------------------")
