#pragma once

#include "sirius/utils/log.h"
#include "utils/config.h"

#if defined(_WIN32) || defined(_WIN64)
#  define _utils_strerror_r(errnum, buf, size) strerror_s(buf, size, errnum)
#else
#  include <string.h>
#  define _utils_strerror_r strerror_r
#endif

#define utils_string_error(err_c, fun) \
  do { \
    char e[sirius_log_buf_size]; \
    if (likely(0 == _utils_strerror_r(err_c, e, sizeof(e)))) { \
      sirius_error(fun ": %d, %s\n", err_c, e); \
    } else { \
      sirius_error(fun ": %d\n", err_c); \
    } \
  } while (0)

#define utils_string_warn(err_c, fun) \
  do { \
    char e[sirius_log_buf_size]; \
    if (likely(0 == _utils_strerror_r(err_c, e, sizeof(e)))) { \
      sirius_warn(fun ": %d, %s\n", err_c, e); \
    } else { \
      sirius_warn(fun ": %d\n", err_c); \
    } \
  } while (0)

#if defined(_WIN32) || defined(_WIN64)

#  define utils_win_format_error(err_c, fun) \
    do { \
      char e[sirius_log_buf_size]; \
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

#  define utils_win_format_warn(err_c, fun) \
    do { \
      char e[sirius_log_buf_size]; \
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
