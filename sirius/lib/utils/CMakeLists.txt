# --- Variables ---
set(lib_version "${PROJECT_VERSION}")

set(pkgconfig_libs "${LIB_PKGCONFIG_LIBS}")
set(pkgconfig_libs_private "${LIB_PKGCONFIG_LIBS_PRIVATE}")
set(pkgconfig_cflags "${LIB_PKGCONFIG_CFLAGS}")

# --- sirius::utils ---
set(sources "")
list(APPEND sources "log/log.cpp")
list(APPEND sources "thread/thread.c")
list(APPEND sources "initializer.c")

add_library(${LIB_TARGET} ${GLOB_LIBRARY_TYPE} ${sources})
add_library(${SIRIUS_TARGET_CMAKE_NAMESPACE}::${LIB_TARGET} ALIAS ${LIB_TARGET})
unset(sources)

utils_set_output_directory_to_mirror(TARGET ${LIB_TARGET})

# thread
target_link_libraries(${LIB_TARGET} PRIVATE "Threads::Threads")
list(APPEND pkgconfig_libs_private ${LIB_PKGCONFIG_THREAD})

# --- Compile Definitions ---
target_compile_definitions(
  ${LIB_TARGET} PRIVATE "sirius_log_module_name=\"${LIB_TARGET}\""
                        "_SIRIUS_LOG_BUF_SIZE=${SIRIUS_UTILS_LOG_BUF_SIZE}")

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
