# --- Generate Header Files ---
cmake_path(SET template NORMALIZE "sirius_version.h.in")
cmake_path(SET gen NORMALIZE "${CMAKE_CURRENT_BINARY_DIR}/sirius_version.h")

configure_file(${template} ${gen} @ONLY)

set(GLOB_INCLUDE
    ${GLOB_INCLUDE} "${CMAKE_CURRENT_BINARY_DIR}/.."
    PARENT_SCOPE)

install(
  FILES ${gen}
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sirius"
  COMPONENT "${PROJECT_NAME}")
