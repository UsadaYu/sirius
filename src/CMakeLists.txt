include(CMakePackageConfigHelpers)

# --- Variables ---
set(libsirius ${SIRIUS_TARGET_LIBRARY_NAME_LIBSIRIUS})

# --- Source Files ---
cmake_path(SET sources NORMALIZE "")

list(APPEND sources "internal/init.c")
list(APPEND sources "internal/log.c")

list(APPEND sources "sirius_log.c")
list(APPEND sources "sirius_mutex.c")
list(APPEND sources "sirius_queue.c")
list(APPEND sources "sirius_sem.c")
list(APPEND sources "sirius_thread.c")
list(APPEND sources "sirius_time.c")

# --- Sirius::sirius ---
add_library(${libsirius} ${SIRIUS_BUILD_TYPE} ${sources})
add_library(${SIRIUS_TARGET_CMAKE_NAMESPACE}::${libsirius} ALIAS ${libsirius})
unset(sources)

utils_set_output_directory_to_mirror(${libsirius})

# --- `pkgconfig` Variable ---
set(__pkgconfig_libs "")
set(__pkgconfig_libs_private "")
set(__pkgconfig_cflags "")

# --- Compile Features / Target Properties ---
set_target_properties(${libsirius} PROPERTIES VERSION ${PROJECT_VERSION})

utils_set_highest_c_standard(${libsirius})
utils_set_highest_cxx_standard(${libsirius})

# --- Compile Definitions ---
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(SIRIUS_BUILD_TYPE STREQUAL "SHARED")
    target_compile_definitions(${libsirius} PUBLIC ${GLOB_SIRIUS_WIN_DLL})

    list(APPEND __pkgconfig_cflags "-D${GLOB_SIRIUS_WIN_DLL}")
  endif()
endif()

target_compile_definitions(
  ${libsirius}
  PRIVATE "SIRIUS_BUILDING"
          "sirius_log_module_name=\"${SIRIUS_MODULE_PRINT_NAME}\""
          "WRITE_SIZE=${SIRIUS_WRITE_SIZE}"
          "INTERNAL_LOG_LEVEL=${SIRIUS_INTERNAL_LOG_LEVEL}")

# --- Compile Options ---
# pic
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(SIRIUS_PIC_ENABLE)
    target_compile_options(${libsirius} PRIVATE "-fPIC")
  else()
    target_compile_options(${libsirius} PRIVATE "-fno-PIC")
  endif()
endif()

# symbol
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_compile_options(${libsirius} PRIVATE "-fvisibility=hidden")
endif()

# all warnings
if(SIRIUS_WARNING_ALL)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${libsirius} PRIVATE "/W4")
  else()
    target_compile_options(${libsirius} PRIVATE "-Wall")
  endif()
endif()

# regard all warnings as errors
if(SIRIUS_WARNING_ERROR)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${libsirius} PRIVATE "/WX")
  else()
    target_compile_options(${libsirius} PRIVATE "-Werror")
  endif()
endif()

# --- Include Directories ---
target_include_directories(${libsirius} PRIVATE ${GLOB_INCLUDE}
                                                ${CMAKE_CURRENT_SOURCE_DIR})

# --- Link Directories ---
list(APPEND __pkgconfig_libs
     "-L${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

# --- Link Libraries ---
list(APPEND __pkgconfig_libs "-l${libsirius}")

# thread
find_package(Threads)
target_link_libraries(${libsirius} PUBLIC "Threads::Threads")

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  check_cxx_compiler_flag("-pthread" COMPILE_HAVE_PTHREAD)
  if(COMPILE_HAVE_PTHREAD)
    list(APPEND __pkgconfig_cflags "-pthread")
    list(APPEND __pkgconfig_libs_private "-pthread")
  else()
    check_linker_flag(CXX "-lpthread" LINK_HAVE_PTHREAD)
    list(APPEND __pkgconfig_libs_private "-lpthread")
  endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  find_library(libwinmm winmm)
  if(libwinmm)
    target_link_libraries(${libsirius} PUBLIC ${libwinmm})

    list(APPEND __pkgconfig_libs ${libwinmm})
  else()
    message(
      FATAL_ERROR
        "Could not find the library `winmm` required by `${libsirius}`")
  endif()
endif()

# --- Link Options ---

# --- System Include Directories ---
string(REPLACE ";" "$<SEMICOLON>" dir "${GLOB_INCLUDE}")
target_include_directories(
  ${libsirius} SYSTEM
  INTERFACE "$<BUILD_INTERFACE:${dir}>"
            "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}>"
)
unset(dir)

list(APPEND __pkgconfig_cflags
     "-I${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}")

# --- Pkg Config ---
add_subdirectory(pkgconfig)

# --- Cmake Package ---
add_subdirectory(cmake)

# libraries
install(
  TARGETS ${libsirius}
  EXPORT ${__cmake_target_export_name}
  COMPONENT "${PROJECT_NAME}"
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ---
