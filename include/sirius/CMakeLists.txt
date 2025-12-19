# --- Generate Header Files ---
configure_file("sirius_common.h.in" "sirius_common.h" @ONLY)

set(GLOB_INCLUDE
    ${GLOB_INCLUDE} "${CMAKE_CURRENT_BINARY_DIR}/.."
    PARENT_SCOPE)

install(
  DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sirius"
  COMPONENT "${PROJECT_NAME}"
  FILES_MATCHING
  PATTERN "*.def"
  PATTERN "*.h"
  PATTERN "*.hpp"
  PATTERN "*.hxx"
  PATTERN "*.*.in" EXCLUDE
  PATTERN "CMakeFiles" EXCLUDE
  PATTERN "*.cmake" EXCLUDE)
