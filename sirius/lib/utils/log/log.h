#pragma once

#include "sirius/utils/log.h"
#include "utils/utils.h"

#define utils_string_error(error_code, funcion) \
  do { \
    char e[sirius_log_buf_size]; \
    if (likely(0 == UTILS_STRERROR_R(error_code, e, sizeof(e)))) { \
      sirius_error(funcion ": %d. %s\n", error_code, e); \
    } else { \
      sirius_error(funcion ": %d\n", error_code); \
    } \
  } while (0)

#define utils_string_warn(error_code, funcion) \
  do { \
    char e[sirius_log_buf_size]; \
    if (likely(0 == UTILS_STRERROR_R(error_code, e, sizeof(e)))) { \
      sirius_warn(funcion ": %d. %s\n", error_code, e); \
    } else { \
      sirius_warn(funcion ": %d\n", error_code); \
    } \
  } while (0)

#if defined(_WIN32) || defined(_WIN64)

#  define utils_win_format_error(error_code, funcion) \
    do { \
      char e[sirius_log_buf_size]; \
      DWORD flags = \
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS; \
      DWORD size = FormatMessage(flags, nullptr, error_code, \
                                 MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), e, \
                                 sizeof(e) / sizeof(TCHAR), nullptr); \
      if (unlikely(size == 0)) { \
        sirius_error(funcion ": %d\n", error_code); \
      } else { \
        sirius_error(funcion ": %d. %s", error_code, e); \
      } \
    } while (0)

#  define utils_win_format_warn(error_code, funcion) \
    do { \
      char e[sirius_log_buf_size]; \
      DWORD flags = \
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS; \
      DWORD size = FormatMessage(flags, nullptr, error_code, \
                                 MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), e, \
                                 sizeof(e) / sizeof(TCHAR), nullptr); \
      if (unlikely(size == 0)) { \
        sirius_warn(funcion ": %d\n", error_code); \
      } else { \
        sirius_warn(funcion ": %d. %s", error_code, e); \
      } \
    } while (0)

#endif
