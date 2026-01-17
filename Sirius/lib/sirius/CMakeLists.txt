message(STATUS "----------------------")
message(STATUS "--- Sirius::sirius ---")
message(STATUS "----------------------")

# --- Variables ---
set(sirius_pkgconfig_libs "${PKGCONFIG_LIBS}")
set(sirius_pkgconfig_libs_private "${PKGCONFIG_LIBS_PRIVATE}")
set(sirius_pkgconfig_cflags "${PKGCONFIG_CFLAGS}")

# --- Source Files ---
cmake_path(SET sources NORMALIZE "")

list(APPEND sources "internal/initializer.c" "internal/thread.cpp")
list(
  APPEND
  sources
  "sirius_cond.c"
  "sirius_log.c"
  "sirius_mutex.c"
  "sirius_queue.c"
  "sirius_sem.c"
  "sirius_thread.c"
  "sirius_time.c")

# --- Sirius::sirius ---
add_library(${libsirius} ${GLOB_LIBRARY_TYPE} ${sources})
add_library(${SIRIUS_TARGET_CMAKE_NAMESPACE}::${libsirius} ALIAS ${libsirius})
unset(sources)

utils_set_output_directory_to_mirror(TARGET ${libsirius})

# --- Compile Features / Target Properties ---
set_target_properties(${libsirius} PROPERTIES VERSION ${PROJECT_VERSION})

# --- Compile Definitions ---
target_compile_definitions(
  ${libsirius} PRIVATE "sirius_log_module_name=\"${SIRIUS_SIRIUS_PRINT_NAME}\""
                       "LOG_BUF_SIZE=${SIRIUS_SIRIUS_LOG_BUF_SIZE}")

# --- Compile Options ---

# --- Include Directories ---
target_include_directories(${libsirius} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# --- Link Directories ---

# --- Link Libraries ---

# thread
find_package(Threads)
target_link_libraries(${libsirius} PRIVATE "Threads::Threads")

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  check_linker_flag(CXX "-pthread" CXX_LINKER_HAVE_PTHREAD)
  if(CXX_LINKER_HAVE_PTHREAD)
    list(APPEND sirius_pkgconfig_libs_private "-pthread")
  else()
    check_linker_flag(CXX "-lpthread" CXX_LINKER_HAVE_LIB_PTHREAD)
    if(CXX_LINKER_HAVE_LIB_PTHREAD)
      list(APPEND sirius_pkgconfig_libs_private "-lpthread")
    else()
      message(
        WARNING
          "
  The thread link option cannot be found
  You may need to check the generated `pkg-config` file
  If you do not use it, please ignore
  ")
    endif()
  endif()
endif()

# winmm
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  find_library(libwinmm winmm REQUIRED)
  target_link_libraries(${libsirius} PRIVATE ${libwinmm})
  list(APPEND sirius_pkgconfig_libs_private "-lwinmm")
endif()

# --- Link Options ---

# --- System Include Directories ---

# --- Pkg Config ---
add_subdirectory(pkgconfig)

# --- CMake Package ---
install(
  TARGETS ${libsirius}
  EXPORT ${__cmake_target_export_name}
  COMPONENT "${PROJECT_NAME}"
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ---
