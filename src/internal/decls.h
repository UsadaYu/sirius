#ifndef INTERNAL_DECLS_H
#define INTERNAL_DECLS_H

#if 0
#  if defined(_WIN32) || defined(_WIN64)
#    ifdef __CYGWIN__
#      ifndef _POSIX_C_SOURCE
#        define _POSIX_C_SOURCE 200809L
#      endif
#    elif defined(__MINGW32__) || defined(__MINGW64__)
#      ifndef _XOPEN_SOURCE
#        define _XOPEN_SOURCE 700
#      endif
#    endif
#  elif defined(__linux__)
#    ifndef _GNU_SOURCE
#      define _GNU_SOURCE
#    endif
#  elif defined(__APPLE__) && defined(__MACH__)
#    ifndef _DARWIN_C_SOURCE
#      define _DARWIN_C_SOURCE 1
#    endif
#  elif defined(__unix__) || defined(__unix)
#    ifndef _POSIX_C_SOURCE
#      define _POSIX_C_SOURCE 200809L
#    endif
#    ifndef _XOPEN_SOURCE
#      define _XOPEN_SOURCE 700
#    endif
#  else
#    warning "The unrecognized system will be treated as a POSIX system"
#    ifndef _POSIX_C_SOURCE
#      define _POSIX_C_SOURCE 200809L
#    endif
#  endif
#endif

#if defined(_WIN32) || defined(_WIN64)
#  include <windows.h>
#endif

#include <assert.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __STDC_VERSION__
#  if (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMICS__)
#    include <stdatomic.h>
#  endif

#  if (__STDC_VERSION__ >= 202311L)
#  else
#    include <stdbool.h>

#    ifndef nullptr
#      define nullptr NULL
#    endif
#  endif

#elif defined(__cplusplus)

#else
#  include <stdbool.h>

#  ifndef nullptr
#    define nullptr NULL
#  endif

#endif

#if (defined(_WIN32) || defined(_WIN64)) && defined(_MSC_VER)
#else
#  include <libgen.h>
#  include <pthread.h>
#  include <semaphore.h>
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
#  include <io.h>
#  include <synchapi.h>

#else
#  error "Unsupported operating system"

#endif

#endif // INTERNAL_DECLS_H
