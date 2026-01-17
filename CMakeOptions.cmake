# --- Global ---
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

option(SIRIUS_BUILD_OBJECT_ONLY "Build objects only" OFF)

# --- Sirius ---
# Sirius
set(SIRIUS_TARGET_CMAKE_NAMESPACE
    "Sirius"
    CACHE STRING "The cmake interaction file namespace of the target")

option(SIRIUS_WARNING_ALL "Enable all compile warnings" ON)

option(SIRIUS_WARNING_AS_ERROR "Regard all warnings as errors" OFF)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  option(SIRIUS_PIC_ENABLE "Position independent, enable `-fPIC/-fPIE`" ON)
endif()

set(SIRIUS_LOG_LEVEL
    "3"
    CACHE STRING
          "Log level: 0: disable the log; 1: error; 2: warn; 3: info; 4: debug")

option(SIRIUS_ASAN "Enable address sanitizer" OFF)

# For example, on Windows, msvc and clang might have used the same-named
# libraries but with different sources.
option(SIRIUS_ASAN_FALLBACK_ENABLE "Whether to fall back to custom ASAN libs"
       OFF)

set(SIRIUS_ASAN_FALLBACK_LIBS
    "clang_rt.asan_dynamic-x86_64;clang_rt.asan_dynamic_runtime_thunk-x86_64"
    CACHE STRING "Libraries")

set(SIRIUS_ASAN_FALLBACK_LIBDIRS
    "D:/070_Code/110_LLVM/lib/clang/21/lib/windows"
    CACHE STRING "The search path for `SIRIUS_ASAN_FALLBACK_LIBS`")

# Sirius::sirius
set(SIRIUS_SIRIUS_LIBRARY_NAME
    "sirius"
    CACHE STRING "The name of the target library `sirius`")

set(SIRIUS_SIRIUS_PRINT_NAME
    "${SIRIUS_TARGET_CMAKE_NAMESPACE}::${SIRIUS_SIRIUS_LIBRARY_NAME}"
    CACHE STRING "`Sirius::sirius` module print name")

set(SIRIUS_SIRIUS_LOG_BUF_SIZE
    "2048"
    CACHE
      STRING
      "The maximum number of bytes written to the file descriptor at a single time"
)

# --- Test ---
option(SIRIUS_TEST_ENABLE "Enable test" OFF)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  option(SIRIUS_TEST_PIE_ENABLE "Position independent, enable `-fPIC/-fPIE`" ON)
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
