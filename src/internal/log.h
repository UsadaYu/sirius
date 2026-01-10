#ifndef INTERNAL_LOG_H
#define INTERNAL_LOG_H

#include "sirius/sirius_log.h"

/**
 * @brief The maximum number of bytes written to the file descriptor at a single
 * time.
 *
 * @example
 * - (1) CFLAGS += -DLOG_BUF_SIZE=$(LOG_BUF_SIZE)
 */
#ifndef LOG_BUF_SIZE
#  define LOG_BUF_SIZE 2048
#else
#  if LOG_BUF_SIZE > 32768
#    undef LOG_BUF_SIZE
#    define LOG_BUF_SIZE 4096
#  elif LOG_BUF_SIZE < 512
#    undef LOG_BUF_SIZE
#    define LOG_BUF_SIZE 2048
#  endif
#endif // LOG_BUF_SIZE

#ifndef STDIN_FILENO
/**
 * @brief Standard input file descriptor.
 */
#  define STDIN_FILENO (0)
#endif

#ifndef STDOUT_FILENO
/**
 * @brief Standard output file descriptor.
 */
#  define STDOUT_FILENO (1)
#endif

#ifndef STDERR_FILENO
/**
 * @brief Standard error output file descriptor.
 */
#  define STDERR_FILENO (2)
#endif

#if defined(_WIN32) || defined(_WIN64)
#  define _sirius_strerror_r(errnum, buf, size) strerror_s(buf, size, errnum)
#else
#  include <string.h>
#  define _sirius_strerror_r strerror_r
#endif

#define internal_str_error(err_c, fun) \
  do { \
    char e[LOG_BUF_SIZE]; \
    if (likely(0 == _sirius_strerror_r(err_c, e, sizeof(e)))) { \
      sirius_error(fun ": %d, %s\n", err_c, e); \
    } else { \
      sirius_error(fun ": %d\n", err_c); \
    } \
  } while (0)

#define internal_str_warn(err_c, fun) \
  do { \
    char e[LOG_BUF_SIZE]; \
    if (likely(0 == _sirius_strerror_r(err_c, e, sizeof(e)))) { \
      sirius_warn(fun ": %d, %s\n", err_c, e); \
    } else { \
      sirius_warn(fun ": %d\n", err_c); \
    } \
  } while (0)

#if defined(_WIN32) || defined(_WIN64)
#  define internal_win_fmt_error(err_c, fun) \
    do { \
      char e[LOG_BUF_SIZE]; \
      DWORD flags = \
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS; \
      DWORD size = FormatMessage(flags, nullptr, err_c, \
                                 MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), e, \
                                 sizeof(e) / sizeof(TCHAR), nullptr); \
      if (unlikely(size == 0)) { \
        sirius_error(fun ": %d\n", err_c); \
      } else { \
        sirius_error(fun ": %d, %s", err_c, e); \
      } \
    } while (0)

#  define internal_win_fmt_warn(err_c, fun) \
    do { \
      char e[LOG_BUF_SIZE]; \
      DWORD flags = \
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS; \
      DWORD size = FormatMessage(flags, nullptr, err_c, \
                                 MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), e, \
                                 sizeof(e) / sizeof(TCHAR), nullptr); \
      if (unlikely(size == 0)) { \
        sirius_warn(fun ": %d\n", err_c); \
      } else { \
        sirius_warn(fun ": %d, %s", err_c, e); \
      } \
    } while (0)
#endif

#endif // INTERNAL_LOG_H
