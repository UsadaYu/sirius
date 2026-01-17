# --- Variables ---
cmake_path(SET include_directories NORMALIZE "")

# --- Generate Header Files ---
cmake_path(SET template NORMALIZE "sirius_version.h.in")
cmake_path(SET gen NORMALIZE "${CMAKE_CURRENT_BINARY_DIR}/sirius_version.h")

configure_file(${template} ${gen} @ONLY)

# ---
list(APPEND include_directories "${CMAKE_CURRENT_BINARY_DIR}/..")
list(APPEND include_directories "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(GLOB_INCLUDE
    ${include_directories}
    PARENT_SCOPE)

# --- sirius ---
cmake_path(SET subdir NORMALIZE "sirius")
cmake_path(SET api_sirius NORMALIZE "")

list(APPEND api_sirius ${gen})
list(
  APPEND
  api_sirius
  "sirius_cpu.h"
  "sirius_macro.h"
  "sirius_queue.h"
  "sirius_thread.h"
  "sirius_attributes.h"
  "sirius_file.h"
  "sirius_math.h"
  "sirius_sem.h"
  "sirius_time.h"
  "sirius_cond.h"
  "sirius_log.h"
  "sirius_mutex.h"
  "sirius_spinlock.h")
install(
  FILES ${api_sirius}
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${subdir}"
  COMPONENT "${PROJECT_NAME}")

# --- sirius/internal ---
cmake_path(SET subdir NORMALIZE "sirius/internal")
cmake_path(SET api_sirius_internal NORMALIZE "")

list(APPEND api_sirius_internal "internal/attributes.h" "internal/common.h"
     "internal/macro.h" "internal/thread.h")
install(
  FILES ${api_sirius_internal}
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${subdir}"
  COMPONENT "${PROJECT_NAME}")
