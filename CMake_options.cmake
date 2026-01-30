# --- Global ---
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

option(SIRIUS_BUILD_OBJECT_ONLY "Build objects only" OFF)

# --- sirius ---
set(SIRIUS_TARGET_CMAKE_NAMESPACE
    "sirius"
    CACHE STRING "The cmake interaction file namespace of the target")

option(SIRIUS_WARNING_ALL "Enable all compile warnings" ON)

option(SIRIUS_WARNING_AS_ERROR "Regard all warnings as errors" OFF)

option(SIRIUS_PIC_ENABLE "Position independent, enable `-fPIC/-fPIE`" ON)

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

# sirius::c
set(SIRIUS_C_LIBRARY_NAME
    "sirius_c"
    CACHE STRING "The name of the target library `sirius_c`")

# sirius::thread
set(SIRIUS_THREAD_LIBRARY_NAME
    "sirius_thread"
    CACHE STRING "The name of the target library `sirius_thread`")

# sirius::kit
set(SIRIUS_KIT_LIBRARY_NAME
    "sirius_kit"
    CACHE STRING "The name of the target library `sirius_kit`")

# sirius::utils
set(SIRIUS_UTILS_LIBRARY_NAME
    "sirius_utils"
    CACHE STRING "The name of the target library `sirius_utils`")

set(SIRIUS_UTILS_LOG_BUF_SIZE
    "4096"
    CACHE
      STRING
      "The maximum number of bytes written to the file descriptor at a single time"
)

# --- Test ---
option(SIRIUS_TEST_ENABLE "Enable test" OFF)

option(SIRIUS_TEST_PIE_ENABLE "Position independent, enable `-fPIC/-fPIE`" ON)

option(SIRIUS_TEST_WIN_CRTDBG "On windows, enable `_CRTDBG_MAP_ALLOC`" OFF)

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
