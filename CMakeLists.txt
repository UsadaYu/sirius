cmake_minimum_required(VERSION 3.15)

set(SIRIUS_VERSION_MAJOR 1)
set(SIRIUS_VERSION_MINOR 1)
set(SIRIUS_VERSION_PATCH 0)
set(
  SIRIUS_VERSION
  "${SIRIUS_VERSION_MAJOR}.${SIRIUS_VERSION_MINOR}.${SIRIUS_VERSION_PATCH}"
)

project(
  PROJECT-SIRIUS
  VERSION ${SIRIUS_VERSION}
  DESCRIPTION "Base library encapsulation"
  LANGUAGES "C" "CXX"
  HOMEPAGE_URL "https://github.com/UsadaYu/sirius"
)

# target information #
set(SIRIUS_TARGET_NAME "sirius" CACHE STRING "target name")
set(TARGET_DIR ${PROJECT_BINARY_DIR}/artifact/)
set(TARGET_LIB_DIR ${TARGET_DIR}/lib)

add_subdirectory(cmake)

option(SIRIUS_TEST_ENABLE "enable test" OFF)
if(SIRIUS_TEST_ENABLE)
  enable_testing()
  add_subdirectory(unittests)
endif()
