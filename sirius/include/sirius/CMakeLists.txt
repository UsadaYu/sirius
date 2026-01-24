# --- Variables ---
cmake_path(SET include_directories NORMALIZE "")

# --- Generate Header Files ---
cmake_path(SET template NORMALIZE "version.h.in")
cmake_path(SET gen NORMALIZE "${CMAKE_CURRENT_BINARY_DIR}/version.h")

configure_file(${template} ${gen} @ONLY)

# ---
list(APPEND include_directories "${CMAKE_CURRENT_BINARY_DIR}/..")
list(APPEND include_directories "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(GLOB_INCLUDE
    ${include_directories}
    PARENT_SCOPE)

# ---
macro(api_install SUBDIR FILES)
  cmake_path(SET _install_dir NORMALIZE "sirius/${${SUBDIR}}")

  install(
    FILES ${${FILES}}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${_install_dir}"
    COMPONENT "${PROJECT_NAME}")

  unset(_install_dir)
endmacro()

set(subdir "c")
set(api_files "")
list(APPEND api_files "${subdir}/time.h")
api_install(subdir api_files)

set(subdir "internal")
set(api_files "${subdir}/attributes.h" "${subdir}/common.h" "${subdir}/macro.h")
api_install(subdir api_files)

set(subdir "kit")
set(api_files "${subdir}/queue.h")
api_install(subdir api_files)

set(subdir "thread")
set(api_files "${subdir}/cond.h" "${subdir}/mutex.h" "${subdir}/sem.h"
              "${subdir}/spinlock.h" "${subdir}/thread.h")
api_install(subdir api_files)

set(subdir "utils")
set(api_files "${subdir}/log.h" "${subdir}/thread.h")
api_install(subdir api_files)

set(subdir ".")
set(api_files "${subdir}/attributes.h" "${subdir}/cpu.h" "${subdir}/file.h"
              "${subdir}/macro.h" ${gen})
api_install(subdir api_files)
