# --- Global ---
set(SIRIUS_TARGET_CMAKE_NAMESPACE
    "Sirius"
    CACHE STRING "The cmake interaction file namespace of the target")

set(SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS
    "sirius"
    CACHE STRING "The name of the target library `sirius`")

option(SIRIUS_BUILD_OBJECT_ONLY "Build objects only" OFF)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

if(SIRIUS_BUILD_OBJECT_ONLY)
  set(SIRIUS_BUILD_TYPE
      "OBJECT"
      CACHE INTERNAL "" FORCE)
else()
  if(BUILD_SHARED_LIBS)
    set(SIRIUS_BUILD_TYPE
        "SHARED"
        CACHE INTERNAL "" FORCE)
  else()
    set(SIRIUS_BUILD_TYPE
        "STATIC"
        CACHE INTERNAL "" FORCE)
  endif()
endif()

# --- Sirius ---
# Sirius
option(SIRIUS_WARNING_ALL "Enable all compile warnings" ON)

option(SIRIUS_WARNING_AS_ERROR "Regard all warnings as errors" OFF)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(SIRIUS_BUILD_TYPE STREQUAL "SHARED")
    if(DEFINED SIRIUS_PIC_ENABLE)
      message(STATUS "Ignore the option: SIRIUS_PIC_ENABLE")
    endif()
    unset(SIRIUS_PIC_ENABLE CACHE)
    set(SIRIUS_PIC_ENABLE
        ON
        CACHE INTERNAL "" FORCE)
  else()
    option(SIRIUS_PIC_ENABLE "Position independent, enable `-fPIC/-fPIE`" ON)
  endif()
endif()

set(SIRIUS_LOG_LEVEL
    "3"
    CACHE STRING
          "Log level: 0: disable the log; 1: error; 2: warn; 3: info; 4: debug")

option(SIRIUS_ASAN "Enable address sanitizer" OFF)

# Sirius::sirius
set(SIRIUS_MODULE_PRINT_NAME
    "${SIRIUS_TARGET_CMAKE_NAMESPACE}::${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS}"
    CACHE STRING "Sirius module print name")

set(SIRIUS_LOG_BUF_SIZE
    "2048"
    CACHE
      STRING
      "The maximum number of bytes written to the file descriptor at a single time"
)

# --- Test ---
option(SIRIUS_TEST_ENABLE "Enable test" OFF)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(NOT SIRIUS_PIC_ENABLE)
    if(DEFINED SIRIUS_TEST_PIE_ENABLE)
      message(STATUS "Ignore the option: SIRIUS_TEST_PIE_ENABLE")
    endif()
    unset(SIRIUS_TEST_PIE_ENABLE CACHE)
    set(SIRIUS_TEST_PIE_ENABLE
        OFF
        CACHE INTERNAL "" FORCE)
  else()
    option(SIRIUS_TEST_PIE_ENABLE "Position independent, enable `-fPIC/-fPIE`"
           ON)
  endif()
endif()

set(SIRIUS_TEST_EXTRA_LINK_DIRECTORIES
    ""
    CACHE
      STRING
      "The directory of `libs` that require additional links to the `sirius test`, e.g., \"/path/lib1;/path/lib2\""
)

set(SIRIUS_TEST_EXTRA_LINK_LIBRARIES
    ""
    CACHE
      STRING
      "The name of `libs` that require additional links to the `sirius test`, e.g., \"pthread;stdc++\""
)

set(SIRIUS_TEST_LOG_LEVEL
    "3"
    CACHE
      STRING
      "Test log level: 0: disable the log; 1: error; 2: warn; 3: info; 4: debug"
)
