# --- Include ---
include(CheckLinkerFlag)

# ---Dependencies ---
find_package(Threads)

set(LIB_PKGCONFIG_THREAD "")
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  check_linker_flag(CXX "-pthread" CXX_LINKER_HAVE_PTHREAD)
  if(CXX_LINKER_HAVE_PTHREAD)
    set(LIB_PKGCONFIG_THREAD "-pthread")
  else()
    check_linker_flag(CXX "-lpthread" CXX_LINKER_HAVE_LIB_PTHREAD)
    if(CXX_LINKER_HAVE_LIB_PTHREAD)
      set(LIB_PKGCONFIG_THREAD "-lpthread")
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

# --- Variables ---
set(_sirius_win_dll
    "_SIRIUS_WIN_DLL"
    CACHE INTERNAL "")

set(_sirius_libraries "")

foreach(t "PRIVATE" "PUBLIC")
  set(LIB_${t}_COMPILE_DEFINITIONS "")
  set(LIB_${t}_COMPILE_OPTIONS "")
  set(LIB_${t}_INCLUDE_DIRECTORIES "")
  set(LIB_${t}_LINK_DIRECTORIES "")
  set(LIB_${t}_LINK_LIBRARIES "")
  set(LIB_${t}_LINK_OPTIONS "")
  set(LIB_${t}_SYSTEM_INCLUDE_DIRECTORIES "")
endforeach()
unset(t)

set(LIB_PKGCONFIG_LIBS "")
set(LIB_PKGCONFIG_LIBS_PRIVATE "")
set(LIB_PKGCONFIG_CFLAGS "")

set(__cmake_target_export_name "${SIRIUS_TARGET_CMAKE_NAMESPACE}Targets")

# --- Compile Definitions ---
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(GLOB_LIBRARY_TYPE STREQUAL "SHARED")
    list(APPEND LIB_PUBLIC_COMPILE_DEFINITIONS ${_sirius_win_dll})
    list(APPEND LIB_PKGCONFIG_CFLAGS "-D${_sirius_win_dll}")
  endif()
endif()

list(APPEND LIB_PRIVATE_COMPILE_DEFINITIONS
     "_SIRIUS_NAMESPACE=\"${SIRIUS_TARGET_CMAKE_NAMESPACE}\""
     "_SIRIUS_BUILDING" "sirius_log_level=${SIRIUS_LOG_LEVEL}")

# --- Compile Options ---
foreach(lang "C" "CXX")
  utils_query_compiler_config(ACTION "project_flags" RESULT flag LANGUAGE
                              ${lang})
  list(APPEND LIB_PRIVATE_COMPILE_OPTIONS
       "$<$<COMPILE_LANGUAGE:${lang}>:${flag}>")
endforeach()
unset(flag)
unset(lang)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(SIRIUS_PIC_ENABLE)
    list(APPEND LIB_PRIVATE_COMPILE_OPTIONS "-fPIC")
  else()
    list(APPEND LIB_PRIVATE_COMPILE_OPTIONS "-fno-PIC")
  endif()
endif()

if(SIRIUS_WARNING_ALL)
  if(MSVC)
    list(APPEND LIB_PRIVATE_COMPILE_OPTIONS "/W4")
  else()
    list(APPEND LIB_PRIVATE_COMPILE_OPTIONS "-Wall")
  endif()
endif()

if(SIRIUS_WARNING_AS_ERROR)
  if(MSVC)
    list(APPEND LIB_PRIVATE_COMPILE_OPTIONS "/WX")
  else()
    list(APPEND LIB_PRIVATE_COMPILE_OPTIONS "-Werror")
  endif()
endif()

# --- Include Directories ---
list(APPEND LIB_PRIVATE_INCLUDE_DIRECTORIES ${GLOB_INCLUDE}
     ${CMAKE_CURRENT_SOURCE_DIR})

# --- Link Directories ---

# --- Link Libraries ---

# --- Link Options ---

# --- System Include Directories ---

# --- Address Sanitizer ---
if(SIRIUS_ASAN AND SIRIUS_ASAN_FALLBACK_ENABLE)
  foreach(lib IN LISTS SIRIUS_ASAN_FALLBACK_LIBS)
    find_library(
      libasan ${lib}
      PATHS ${SIRIUS_ASAN_FALLBACK_LIBDIRS}
      NO_DEFAULT_PATH REQUIRED)
    list(APPEND LIB_PUBLIC_LINK_LIBRARIES ${libasan})
    list(APPEND LIB_PKGCONFIG_LIBS ${libasan})
    unset(libasan CACHE) # Necessary
  endforeach()
  unset(lib)
elseif(SIRIUS_ASAN)
  if(MSVC)
    message(
      WARNING
        "
  The build options of `ASAN` may not be supported by `pkgconfig`
  You may need to check the generated `pkg-config` file
  If you don't use it, please ignore
  ")

    set(asan_compile_options "-fsanitize=address" "-MD" "-Zi" "-Od")
    list(APPEND LIB_PUBLIC_COMPILE_OPTIONS ${asan_compile_options})
    list(APPEND LIB_PKGCONFIG_CFLAGS ${asan_compile_options})
    set(asan_link_options "-DEBUG" "-INCREMENTAL:NO")
    list(APPEND LIB_PUBLIC_LINK_OPTIONS ${asan_link_options})
    list(APPEND LIB_PKGCONFIG_LIBS ${asan_link_options})
  else()
    set(asan_compile_options "-fsanitize=address" "-fno-omit-frame-pointer"
                             "-fsanitize-recover=address")
    list(APPEND LIB_PUBLIC_COMPILE_OPTIONS ${asan_compile_options})
    list(APPEND LIB_PKGCONFIG_CFLAGS ${asan_compile_options})
    set(asan_link_options "-g" "-fsanitize=address")
    list(APPEND LIB_PUBLIC_LINK_OPTIONS ${asan_link_options})
    list(APPEND LIB_PKGCONFIG_LIBS ${asan_link_options})
  endif()
endif()

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
