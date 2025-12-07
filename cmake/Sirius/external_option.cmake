option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

option(SIRIUS_INSTALL "Generate the install target" ON)

option(SIRIUS_WARNING_ALL "Enable all compile warnings" ON)

option(SIRIUS_WARNING_ERROR "Compile warnings as errors" OFF)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(NOT SIRIUS_BUILD_OBJECT_ONLY AND BUILD_SHARED_LIBS)
    if(DEFINED SIRIUS_PIC_ENABLE)
      message(WARNING "Ignore the option: SIRIUS_PIC_ENABLE")
    endif()
    unset(SIRIUS_PIC_ENABLE CACHE)
    set(SIRIUS_PIC_ENABLE
        ON
        CACHE INTERNAL "" FORCE)
  else()
    option(SIRIUS_PIC_ENABLE "Position independent, enable `-fPIC/-fPIE`" ON)
  endif()
endif()

set(SIRIUS_MODULE_PRINT_NAME
    "libsirius"
    CACHE STRING "Module print name")

set(SIRIUS_WRITE_SIZE
    "1024"
    CACHE STRING "Write file size in bytes at a time")

set(SIRIUS_INTERNAL_LOG_LEVEL
    "3"
    CACHE
      STRING
      "Internal log level: 0: disable the log; 1: error; 2: warn; 3: info; 4: debug"
)

set(SIRIUS_EXTRA_LINK_DIRECTORIES
    ""
    CACHE
      STRING
      "The directory of `libs` that require additional links to the `sirius`, e.g., \"/path/lib1;/path/lib2\""
)

set(SIRIUS_EXTRA_LINK_LIBRARIES
    ""
    CACHE
      STRING
      "The name of `libs` that require additional links to the `sirius`, e.g., \"pthread;stdc++\""
)
