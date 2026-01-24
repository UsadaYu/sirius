# --- Variables ---
set(lib_version "${PROJECT_VERSION}")

set(pkgconfig_libs "${LIB_PKGCONFIG_LIBS}")
set(pkgconfig_libs_private "${LIB_PKGCONFIG_LIBS_PRIVATE}")
set(pkgconfig_requires_private "")
set(pkgconfig_cflags "${LIB_PKGCONFIG_CFLAGS}")

# --- sirius::kit ---
set(sources "queue.c")

add_library(${LIB_TARGET} ${GLOB_LIBRARY_TYPE} ${sources})
add_library(${SIRIUS_TARGET_CMAKE_NAMESPACE}::${LIB_TARGET} ALIAS ${LIB_TARGET})
unset(sources)

utils_set_output_directory_to_mirror(TARGET ${LIB_TARGET})

set_target_properties(${LIB_TARGET} PROPERTIES VERSION ${PROJECT_VERSION})

target_compile_definitions(${LIB_TARGET}
                           PRIVATE "sirius_log_module_name=\"${LIB_TARGET}\"")

# sirius_thread
target_link_libraries(
  ${LIB_TARGET}
  PRIVATE "${SIRIUS_TARGET_CMAKE_NAMESPACE}::${SIRIUS_THREAD_LIBRARY_NAME}")
list(APPEND pkgconfig_requires_private ${SIRIUS_THREAD_LIBRARY_NAME})

# sirius_utils
target_link_libraries(
  ${LIB_TARGET}
  PRIVATE "${SIRIUS_TARGET_CMAKE_NAMESPACE}::${SIRIUS_UTILS_LIBRARY_NAME}")
list(APPEND pkgconfig_requires_private ${SIRIUS_UTILS_LIBRARY_NAME})

# --- Pkg Config ---
add_subdirectory(pkgconfig)

# --- CMake Package ---
install(
  TARGETS ${LIB_TARGET}
  EXPORT ${__cmake_target_export_name}
  COMPONENT "${PROJECT_NAME}"
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ---
