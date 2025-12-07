cmake_minimum_required(VERSION 3.25)

set(SIRIUS_VERSION_MAJOR 1)
set(SIRIUS_VERSION_MINOR 3)
set(SIRIUS_VERSION_PATCH 3)
set(SIRIUS_VERSION
    "${SIRIUS_VERSION_MAJOR}.${SIRIUS_VERSION_MINOR}.${SIRIUS_VERSION_PATCH}")

message(STATUS "CMAKE_GENERATOR: ${CMAKE_GENERATOR}")

project(
  PROJECT-SIRIUS
  VERSION ${SIRIUS_VERSION}
  DESCRIPTION "Sirius"
  LANGUAGES "C" "CXX"
  HOMEPAGE_URL "https://github.com/UsadaYu/sirius")

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- options ---
set(SIRIUS_TARGET_CMAKE_NAMESPACE
    "Sirius"
    CACHE STRING "The cmake interaction file namespace of the target")
set(SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS
    "sirius"
    CACHE STRING "The name of the target library `sirius`")
option(SIRIUS_BUILD_OBJECT_ONLY "Build objects only" OFF)
option(SIRIUS_TEST_ENABLE "Enable test" OFF)

# --- target information ---
set(TARGET_DIR "${PROJECT_BINARY_DIR}/${SIRIUS_TARGET_CMAKE_NAMESPACE}")
set(TARGET_LIB_DIR "${TARGET_DIR}/${CMAKE_INSTALL_LIBDIR}")
set(TARGET_INCLUDE_DIR "${TARGET_DIR}/${CMAKE_INSTALL_INCLUDEDIR}")

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(SIRIUS_WIN_DLL "SIRIUS_WIN_DLL")
endif()

add_subdirectory(cmake/Sirius)

if(SIRIUS_TEST_ENABLE)
  enable_testing()
  add_subdirectory(cmake/unittests)
endif()
