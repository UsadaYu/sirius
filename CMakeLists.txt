cmake_minimum_required(VERSION 3.13)

project(
    PROJECT-SIRIUS
    VERSION "1.0.1"
    DESCRIPTION "Base library encapsulation"
    LANGUAGES "C" "CXX"
    HOMEPAGE_URL "https://github.com/Usada-Yu/sirius"
)

set(SIRIUS_VERSION_MAJOR 1)
set(SIRIUS_VERSION_MINOR 0)
set(SIRIUS_VERSION_PATCH 2)
set(SIRIUS_VERSION
    "${SIRIUS_VERSION_MAJOR}.${SIRIUS_VERSION_MINOR}.${SIRIUS_VERSION_PATCH}")
configure_file(
    "${PROJECT_SOURCE_DIR}/include/internal/sirius_internal_common.h.in"
    "${PROJECT_SOURCE_DIR}/include/sirius_common.h"
    @ONLY
)

option(USER_TEST_ENABLE "test enable" OFF)

add_subdirectory(cmake)

if (USER_TEST_ENABLE)
    enable_testing()
    add_subdirectory(unittests)
endif()
