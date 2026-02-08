message(STATUS "--------------")
message(STATUS "--- SIRIUS ---")
message(STATUS "--------------")

# --- Include ---
include(CheckLinkerFlag)

# --- Global Variables ---
set(GLOB_PKGCONFIG_LIBS_PRIVATE_THREAD "")
set(GLOB_PKGCONFIG_LIBS_ASAN "")
set(GLOB_PKGCONFIG_CFLAGS_ASAN "")

foreach(t "PRIVATE" "PUBLIC")
  set(GLOB_${t}_COMPILE_DEFINITIONS "")
  set(GLOB_${t}_COMPILE_OPTIONS "")
  set(GLOB_${t}_INCLUDE_DIRECTORIES "")
  set(GLOB_${t}_LINK_DIRECTORIES "")
  set(GLOB_${t}_LINK_LIBRARIES "")
  set(GLOB_${t}_LINK_OPTIONS "")
  set(GLOB_${t}_SYSTEM_INCLUDE_DIRECTORIES "")
endforeach()
unset(t)

# --- Preprocessing ---
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

add_subdirectory(include/sirius)

# ---Dependencies ---
# thread
find_package(Threads)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  check_linker_flag(CXX "-pthread" CXX_LINKER_HAVE_PTHREAD)
  if(CXX_LINKER_HAVE_PTHREAD)
    set(GLOB_PKGCONFIG_LIBS_PRIVATE_THREAD "-pthread")
  else()
    check_linker_flag(CXX "-lpthread" CXX_LINKER_HAVE_LIB_PTHREAD)
    if(CXX_LINKER_HAVE_LIB_PTHREAD)
      set(GLOB_PKGCONFIG_LIBS_PRIVATE_THREAD "-lpthread")
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

# address sanitizer
if(SIRIUS_ASAN)
  if(MSVC)
    message(
      WARNING
        "
  The build options of `ASAN` may not be supported by `pkgconfig`
  You may need to check the generated `pkg-config` file
  If you don't use it, please ignore
  ")
  endif()

  if(MSVC)
    set(asan_compile_options "-fsanitize=address" "-MD" "-Zi" "-Od")
    list(APPEND GLOB_PUBLIC_COMPILE_OPTIONS ${asan_compile_options})
    list(APPEND GLOB_PKGCONFIG_CFLAGS_ASAN ${asan_compile_options})
  else()
    set(asan_compile_options "-fsanitize=address" "-fno-omit-frame-pointer"
                             "-fsanitize-recover=address")
    list(APPEND GLOB_PUBLIC_COMPILE_OPTIONS ${asan_compile_options})
    list(APPEND GLOB_PKGCONFIG_CFLAGS_ASAN ${asan_compile_options})
  endif()
endif()

if(SIRIUS_ASAN AND SIRIUS_ASAN_FALLBACK_ENABLE)
  foreach(lib IN LISTS SIRIUS_ASAN_FALLBACK_LIBS)
    find_library(
      libasan ${lib}
      PATHS ${SIRIUS_ASAN_FALLBACK_LIBDIRS}
      NO_DEFAULT_PATH REQUIRED)
    list(APPEND GLOB_PUBLIC_LINK_LIBRARIES ${libasan})
    list(APPEND GLOB_PKGCONFIG_LIBS_ASAN ${libasan})
    unset(libasan CACHE) # Necessary
  endforeach()
  unset(lib)
elseif(SIRIUS_ASAN)
  if(MSVC)
    set(asan_link_options "-DEBUG" "-INCREMENTAL:NO")
    list(APPEND GLOB_PUBLIC_LINK_OPTIONS ${asan_link_options})
    list(APPEND GLOB_PKGCONFIG_LIBS_ASAN ${asan_link_options})
  else()
    set(asan_link_options "-g" "-fsanitize=address")
    list(APPEND GLOB_PUBLIC_LINK_OPTIONS ${asan_link_options})
    list(APPEND GLOB_PKGCONFIG_LIBS_ASAN ${asan_link_options})
  endif()
endif()

# --- Compile Definitions ---
list(APPEND GLOB_PRIVATE_COMPILE_DEFINITIONS "_SIRIUS_BUILDING"
     "_SIRIUS_LOG_LEVEL=${SIRIUS_LOG_LEVEL}")
list(
  APPEND
  GLOB_PRIVATE_COMPILE_DEFINITIONS
  "_SIRIUS_NAMESPACE=\"${SIRIUS_NAMESPACE}\""
  "_SIRIUS_POSIX_FILE_MODE=\"${SIRIUS_POSIX_FILE_MODE}\""
  "_SIRIUS_POSIX_TMP_DIR=\"${SIRIUS_POSIX_TMP_DIR}\""
  "_SIRIUS_USER_KEY=\"${SIRIUS_USER_KEY}\""
  "_SIRIUS_LOG_SHM_CAPACITY=${SIRIUS_LOG_SHM_CAPACITY}"
  "_SIRIUS_LOG_BUF_SIZE=${SIRIUS_LOG_BUF_SIZE}"
  "_SIRIUS_EXE_DIR=\"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\""
  "_SIRIUS_EXE_DAEMON_NAME=\"${SIRIUS_EXE_DAEMON_NAME}\"")

# --- Compile Options ---
foreach(lang "C" "CXX")
  utils_query_compiler_config(ACTION "project_flags" RESULT flag LANGUAGE
                              ${lang})
  list(APPEND GLOB_PRIVATE_COMPILE_OPTIONS
       "$<$<COMPILE_LANGUAGE:${lang}>:${flag}>")
endforeach()
unset(flag)
unset(lang)

if(SIRIUS_WARNING_ALL)
  if(MSVC)
    list(APPEND GLOB_PRIVATE_COMPILE_OPTIONS "/W4")
  else()
    list(APPEND GLOB_PRIVATE_COMPILE_OPTIONS "-Wall")
  endif()
endif()

if(SIRIUS_WARNING_AS_ERROR)
  if(MSVC)
    list(APPEND GLOB_PRIVATE_COMPILE_OPTIONS "/WX")
  else()
    list(APPEND GLOB_PRIVATE_COMPILE_OPTIONS "-Werror")
  endif()
endif()

# --- Include Directories ---
list(APPEND GLOB_PRIVATE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR})

# --- Link Directories ---

# --- Link Libraries ---

# --- Link Options ---

# --- System Include Directories ---

# ---
add_subdirectory(bin)
add_subdirectory(lib)
