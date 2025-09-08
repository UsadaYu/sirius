cmake_minimum_required(VERSION 3.15)

set(SIRIUS_VERSION_MAJOR 1)
set(SIRIUS_VERSION_MINOR 2)
set(SIRIUS_VERSION_PATCH 1)
set(SIRIUS_VERSION
    "${SIRIUS_VERSION_MAJOR}.${SIRIUS_VERSION_MINOR}.${SIRIUS_VERSION_PATCH}")

project(
  PROJECT-SIRIUS
  VERSION ${SIRIUS_VERSION}
  DESCRIPTION "Sirius"
  LANGUAGES "C" "CXX"
  HOMEPAGE_URL "https://github.com/UsadaYu/sirius")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED ON)

# target information
set(SIRIUS_TARGET_NAME
    "sirius"
    CACHE STRING "Target name")
set(TARGET_DIR ${PROJECT_BINARY_DIR}/artifact/)
set(TARGET_LIB_DIR ${TARGET_DIR}/lib)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(SIRIUS_WIN_DLL "SIRIUS_WIN_DLL")
endif()

add_subdirectory(cmake)

option(SIRIUS_TEST_ENABLE "Enable test" OFF)
if(SIRIUS_TEST_ENABLE)
  enable_testing()
  add_subdirectory(unittests)
endif()
