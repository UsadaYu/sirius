#ifndef SIRIUS_INTERNAL_DECLS_H
#define SIRIUS_INTERNAL_DECLS_H

#ifdef _WIN32
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
#    define nullptr NULL
#  endif
#endif

#if defined(_WIN32) && defined(_MSC_VER)
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

#  if defined(__APPLE__) && defined(__MACH__)
#    error "Unsupported operating system"
#  endif

#  if defined(__linux__)
#    include <sys/syscall.h>
#  endif

#elif defined(_WIN32)
#  include <io.h>
#  include <synchapi.h>

#else
#  error "Unsupported operating system"

#endif

#endif // SIRIUS_INTERNAL_DECLS_H
