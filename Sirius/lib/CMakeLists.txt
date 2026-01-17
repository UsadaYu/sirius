# --- Include ---
include(CheckLinkerFlag)

# --- Variables ---
set(GLOB_SIRIUS_WIN_DLL
    "SIRIUS_WIN_DLL"
    CACHE INTERNAL "" FORCE)

set(sirius_libraries "")

# The `INTERFACE` parameter is not required for now.
foreach(t "PRIVATE" "PUBLIC")
  set(${t}_COMPILE_DEFINITIONS "")
  set(${t}_COMPILE_OPTIONS "")
  set(${t}_INCLUDE_DIRECTORIES "")
  set(${t}_LINK_DIRECTORIES "")
  set(${t}_LINK_LIBRARIES "")
  set(${t}_LINK_OPTIONS "")
  set(${t}_SYSTEM_INCLUDE_DIRECTORIES "")
endforeach()
unset(t)

set(PKGCONFIG_LIBS "")
set(PKGCONFIG_LIBS_PRIVATE "")
set(PKGCONFIG_CFLAGS "")

set(__cmake_target_export_name "${SIRIUS_TARGET_CMAKE_NAMESPACE}Targets")

# --- Compile Definitions ---
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(GLOB_LIBRARY_TYPE STREQUAL "SHARED")
    list(APPEND PUBLIC_COMPILE_DEFINITIONS ${GLOB_SIRIUS_WIN_DLL})
    list(APPEND PKGCONFIG_CFLAGS "-D${GLOB_SIRIUS_WIN_DLL}")
  endif()
endif()

list(APPEND PRIVATE_COMPILE_DEFINITIONS "SIRIUS_BUILDING"
     "sirius_log_level=${SIRIUS_LOG_LEVEL}")

# --- Compile Options ---
foreach(lang "C" "CXX")
  utils_query_compiler_config(ACTION "project_flags" RESULT flag LANGUAGE
                              ${lang})
  list(APPEND PRIVATE_COMPILE_OPTIONS "$<$<COMPILE_LANGUAGE:${lang}>:${flag}>")
endforeach()
unset(flag)
unset(lang)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(SIRIUS_PIC_ENABLE)
    list(APPEND PRIVATE_COMPILE_OPTIONS "-fPIC")
  else()
    list(APPEND PRIVATE_COMPILE_OPTIONS "-fno-PIC")
  endif()
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  list(APPEND PRIVATE_COMPILE_OPTIONS "-fvisibility=hidden")
endif()

if(SIRIUS_WARNING_ALL)
  if(MSVC)
    list(APPEND PRIVATE_COMPILE_OPTIONS "/W4")
  else()
    list(APPEND PRIVATE_COMPILE_OPTIONS "-Wall")
  endif()
endif()

if(SIRIUS_WARNING_AS_ERROR)
  if(MSVC)
    list(APPEND PRIVATE_COMPILE_OPTIONS "/WX")
  else()
    list(APPEND PRIVATE_COMPILE_OPTIONS "-Werror")
  endif()
endif()

# --- Include Directories ---
list(APPEND PRIVATE_INCLUDE_DIRECTORIES ${GLOB_INCLUDE})

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
    list(APPEND PUBLIC_LINK_LIBRARIES ${libasan})
    list(APPEND PKGCONFIG_LIBS ${libasan})
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
    list(APPEND PUBLIC_COMPILE_OPTIONS ${asan_compile_options})
    list(APPEND PKGCONFIG_CFLAGS ${asan_compile_options})
    set(asan_link_options "-DEBUG" "-INCREMENTAL:NO")
    list(APPEND PUBLIC_LINK_OPTIONS ${asan_link_options})
    list(APPEND PKGCONFIG_LIBS ${asan_link_options})
  else()
    set(asan_compile_options "-fsanitize=address" "-fno-omit-frame-pointer"
                             "-fsanitize-recover=address")
    list(APPEND PUBLIC_COMPILE_OPTIONS ${asan_compile_options})
    list(APPEND PKGCONFIG_CFLAGS ${asan_compile_options})
    set(asan_link_options "-g" "-fsanitize=address")
    list(APPEND PUBLIC_LINK_OPTIONS ${asan_link_options})
    list(APPEND PKGCONFIG_LIBS ${asan_link_options})
  endif()
endif()

# --- Sirius::sirius ---
set(libsirius ${SIRIUS_SIRIUS_LIBRARY_NAME})
list(APPEND sirius_libraries ${libsirius})

add_subdirectory(sirius)

# ---
foreach(lib IN LISTS sirius_libraries)
  foreach(t "PRIVATE" "PUBLIC")
    target_compile_definitions(${lib} ${t} ${${t}_COMPILE_DEFINITIONS})
    target_compile_options(${lib} ${t} ${${t}_COMPILE_OPTIONS})
    target_include_directories(${lib} ${t} ${${t}_INCLUDE_DIRECTORIES})
    target_link_directories(${lib} ${t} ${${t}_LINK_DIRECTORIES})
    target_link_libraries(${lib} ${t} ${${t}_LINK_LIBRARIES})
    target_link_options(${lib} ${t} ${${t}_LINK_OPTIONS})
    target_include_directories(${lib} SYSTEM ${t}
                               ${${t}_SYSTEM_INCLUDE_DIRECTORIES})
  endforeach()
  unset(t)

  target_include_directories(
    ${lib} SYSTEM
    INTERFACE
      "$<BUILD_INTERFACE:$<JOIN:${PRIVATE_INCLUDE_DIRECTORIES},$<SEMICOLON>>>"
      "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}>")
endforeach()
unset(lib)

# --- CMake Package ---
add_subdirectory(cmake)
