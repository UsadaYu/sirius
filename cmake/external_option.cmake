option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

option(SIRIUS_ASAN "Enable asan" OFF)

option(SIRIUS_GCOV "Enable gcov" OFF)

option(SIRIUS_WARNING_ALL "Enable all compile warnings" ON)

option(SIRIUS_WARNING_ERROR "Compile warnings as errors" OFF)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  option(SIRIUS_PIC_ENABLE "Position independent, enable `pic`" ON)
endif()

set(
  SIRIUS_MODULE_PRINT_NAME "libsirius"
  CACHE STRING
  "Module print name"
)

set(
  SIRIUS_WRITE_SIZE "4096"
  CACHE STRING
  "write file size in bytes at a time, generally recommended with page size"
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR
  CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  set(
    SIRIUS_INTERNAL_LOG_LEVEL "4"
    CACHE STRING
    "Internal log level: 0: disable the log; 1: error; 2: warn; 3: info; 4: debug"
  )
else()
  set(
    SIRIUS_INTERNAL_LOG_LEVEL "3"
    CACHE STRING
    "Internal log level: 0: disable the log; 1: error; 2: warn; 3: info; 4: debug"
  )
endif()
