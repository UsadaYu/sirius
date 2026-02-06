# --- Variables ---
set(_sirius_win_dll
    "_SIRIUS_WIN_DLL"
    CACHE INTERNAL "")

set(_sirius_libraries "")

foreach(t "PRIVATE" "PUBLIC")
  set(LIB_${t}_COMPILE_DEFINITIONS ${GLOB_${t}_COMPILE_DEFINITIONS})
  set(LIB_${t}_COMPILE_OPTIONS ${GLOB_${t}_COMPILE_OPTIONS})
  set(LIB_${t}_INCLUDE_DIRECTORIES ${GLOB_${t}_INCLUDE_DIRECTORIES})
  set(LIB_${t}_LINK_DIRECTORIES ${GLOB_${t}_LINK_DIRECTORIES})
  set(LIB_${t}_LINK_LIBRARIES ${GLOB_${t}_LINK_LIBRARIES})
  set(LIB_${t}_LINK_OPTIONS ${GLOB_${t}_LINK_OPTIONS})
  set(LIB_${t}_SYSTEM_INCLUDE_DIRECTORIES
      ${GLOB_${t}_SYSTEM_INCLUDE_DIRECTORIES})
endforeach()
unset(t)

set(LIB_PKGCONFIG_LIBS "")
set(LIB_PKGCONFIG_LIBS_PRIVATE "")
set(LIB_PKGCONFIG_CFLAGS "")

set(__cmake_target_export_name "${SIRIUS_NAMESPACE}Targets")

# --- Preprocessing ---
list(APPEND LIB_PKGCONFIG_LIBS ${GLOB_PKGCONFIG_LIBS_ASAN})
list(APPEND LIB_PKGCONFIG_CFLAGS ${GLOB_PKGCONFIG_CFLAGS_ASAN})

# --- Compile Definitions ---
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(GLOB_LIBRARY_TYPE STREQUAL "SHARED")
    list(APPEND LIB_PUBLIC_COMPILE_DEFINITIONS ${_sirius_win_dll})
    list(APPEND LIB_PKGCONFIG_CFLAGS "-D${_sirius_win_dll}")
  endif()
endif()

# --- Compile Options ---

# --- Include Directories ---

# --- Link Directories ---

# --- Link Libraries ---

# --- Link Options ---

# --- System Include Directories ---

# --- utils ---
set(LIB_TARGET ${SIRIUS_UTILS_LIBRARY_NAME})
list(APPEND _sirius_libraries ${LIB_TARGET})
add_subdirectory(utils)

# --- c ---
set(LIB_TARGET ${SIRIUS_C_LIBRARY_NAME})
list(APPEND _sirius_libraries ${LIB_TARGET})
add_subdirectory(c)

# --- kit ---
set(LIB_TARGET ${SIRIUS_KIT_LIBRARY_NAME})
list(APPEND _sirius_libraries ${LIB_TARGET})
add_subdirectory(kit)

# --- thread ---
set(LIB_TARGET ${SIRIUS_THREAD_LIBRARY_NAME})
list(APPEND _sirius_libraries ${LIB_TARGET})
add_subdirectory(thread)

# ---
foreach(lib IN LISTS _sirius_libraries)
  foreach(t "PRIVATE" "PUBLIC")
    target_compile_definitions(${lib} ${t} ${LIB_${t}_COMPILE_DEFINITIONS})
    target_compile_options(${lib} ${t} ${LIB_${t}_COMPILE_OPTIONS})
    target_include_directories(${lib} ${t} ${LIB_${t}_INCLUDE_DIRECTORIES})
    target_link_directories(${lib} ${t} ${LIB_${t}_LINK_DIRECTORIES})
    target_link_libraries(${lib} ${t} ${LIB_${t}_LINK_LIBRARIES})
    target_link_options(${lib} ${t} ${LIB_${t}_LINK_OPTIONS})
    target_include_directories(${lib} SYSTEM ${t}
                               ${LIB_${t}_SYSTEM_INCLUDE_DIRECTORIES})
  endforeach()
  unset(t)

  target_include_directories(
    ${lib} SYSTEM
    INTERFACE
      "$<BUILD_INTERFACE:$<JOIN:${LIB_PRIVATE_INCLUDE_DIRECTORIES},$<SEMICOLON>>>"
      "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}>")

  set_target_properties(${lib} PROPERTIES POSITION_INDEPENDENT_CODE
                                          ${SIRIUS_PIC_ENABLE})
endforeach()
unset(lib)

# --- CMake Package ---
add_subdirectory(cmake)
