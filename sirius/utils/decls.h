#pragma once

#if defined(_WIN32) || defined(_WIN64)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <Windows.h>
#endif

#ifdef __cplusplus
#  include <cerrno>
#  include <cfloat>
#  include <cinttypes>
#  include <climits>
#  include <cmath>
#  include <cstdarg>
#  include <cstddef>
#  include <cstdint>
#  include <cstdio>
#  include <cstdlib>
#  include <cstring>
#  include <ctime>
#  include <iostream>
#else
#  include <errno.h>
#  include <float.h>
#  include <inttypes.h>
#  include <limits.h>
#  include <math.h>
#  include <stdarg.h>
#  include <stddef.h>
#  include <stdint.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#  include <time.h>
#endif

#ifdef __cplusplus
#  if __cplusplus >= 201103L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L)
#    include <atomic>
#  endif
#  if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#    include <filesystem>
#  endif
#  if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#    include <format>
#    include <source_location>
#  endif
#  if __cplusplus >= 202302L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202302L)
#    include <expected>
#  endif
#elif defined(__STDC_VERSION__)
#  if (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMICS__)
#    include <stdatomic.h>
#  endif
#  if (__STDC_VERSION__ >= 202311L)
#    include <stdckdint.h>
#  else
#    include <stdbool.h>
#    ifndef nullptr
#      define nullptr NULL
#    endif
#  endif
#endif

#if (defined(_WIN32) || defined(_WIN64)) && defined(_MSC_VER)
#else
#  include <pthread.h>
#  include <sys/time.h>
#  include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

/**
 * @ref https://sourceforge.net/p/predef/wiki/OperatingSystems/
 */
#if defined(__unix__) || defined(__linux__) || \
  (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__) || \
  defined(__OpenBSD__) || defined(__NetBSD__)
#  include <sys/ipc.h>
#  include <sys/select.h>
#  include <sys/shm.h>
#  if defined(__linux__)
#    include <sys/syscall.h>
#  endif
#  if defined(__FreeBSD__)
#    include <pthread_np.h>
#  endif
#  if defined(__NetBSD__)
#    include <lwp.h>
#  endif

#elif defined(_WIN32) || defined(_WIN64)
#  include <synchapi.h>
#else
#  error "Unsupported operating system"
#endif
