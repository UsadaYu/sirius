if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  option(
    SIRIUS_TEST_PIE_ENABLE
    "Position independent, enable `pie`" OFF
  )
endif()

set(
  SIRIUS_TEST_EXTRA_LINK_DIR
  "" CACHE STRING
  "The directory of `libs` that require additional links to the `sirius test`, e.g., `/path/lib1;/path/lib2`"
)

set(
  SIRIUS_TEST_EXTRA_LINK_LIBRARIES
  "" CACHE STRING
  "The name of `libs` that require additional links to the `sirius test`, e.g., `pthread;stdc++`"
)

set(
  SIRIUS_TEST_MODULE_PRINT_NAME "unittest"
  CACHE STRING
  "Test module print name"
)

set(
  SIRIUS_TEST_EXTERNAL_LOG_LEVEL "4"
  CACHE STRING
  "External log level: 0: disable the log; 1: error; 2: warn; 3: info; 4: debug"
)
