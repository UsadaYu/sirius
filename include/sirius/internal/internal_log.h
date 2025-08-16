#ifndef INTERNAL_LOG_H
#define INTERNAL_LOG_H

#include "internal_init.h"
#include "sirius_log.h"

/**
 * @brief Compile macro, level: 0, 1, 2, 3, 4
 *
 * @example
 *  CFLAGS += -DINTERNAL_LOG_LEVEL=2
 */
#ifndef INTERNAL_LOG_LEVEL
#define INTERNAL_LOG_LEVEL sirius_log_level_info
#elif (INTERNAL_LOG_LEVEL > sirius_log_level_debg)
#undef INTERNAL_LOG_LEVEL
#define INTERNAL_LOG_LEVEL sirius_log_level_debg
#endif

/**
 * @brief Custom write file size in bytes at a time,
 *  generally recommended with page size.
 *
 * @example
 *  CFLAGS += -DWRITE_SIZE=$(WRITE_SIZE)
 */
#ifndef WRITE_SIZE
#define WRITE_SIZE 4096
#else
#if WRITE_SIZE > 32768
#undef WRITE_SIZE
#if defined(__GNUC__) || defined(__clang__) || \
    defined(_MSC_VER)
#pragma message "WRITE_SIZE->32768"
#endif
#define WRITE_SIZE 32768
#elif WRITE_SIZE < 512
#undef WRITE_SIZE
#if defined(__GNUC__) || defined(__clang__) || \
    defined(_MSC_VER)
#pragma message "WRITE_SIZE->512"
#endif
#define WRITE_SIZE 512
#endif
#endif  // WRITE_SIZE

#ifndef STDIN_FILENO
/* Standard input file descriptor. */
#define STDIN_FILENO (0)
#endif

#ifndef STDOUT_FILENO
/* Standard output file descriptor. */
#define STDOUT_FILENO (1)
#endif

#ifndef STDERR_FILENO
/* Standard error output file descriptor. */
#define STDERR_FILENO (2)
#endif

void internal_log_fd_set(const int *const fd_err,
                         const int *const fd_out);

void internal_log(int level, const char *color,
                  const char *module, const char *file,
                  int line, const char *fmt, ...);

#if (INTERNAL_LOG_LEVEL >= sirius_log_level_error)
#define internal_error(fmt, ...)                         \
  do {                                                   \
    internal_log(sirius_log_level_error, log_red,        \
                 log_module_name, sirius_file, __LINE__, \
                 fmt, ##__VA_ARGS__);                    \
  } while (0)
#else
#define internal_error(fmt, ...)  \
  do {                            \
    _custom_swallow(__VA_ARGS__); \
  } while (0)
#endif

#if (INTERNAL_LOG_LEVEL >= sirius_log_level_warn)
#define internal_warn(fmt, ...)                          \
  do {                                                   \
    internal_log(sirius_log_level_warn, log_yellow,      \
                 log_module_name, sirius_file, __LINE__, \
                 fmt, ##__VA_ARGS__);                    \
  } while (0)
#else
#define internal_warn(fmt, ...)   \
  do {                            \
    _custom_swallow(__VA_ARGS__); \
  } while (0)
#endif

#if (INTERNAL_LOG_LEVEL >= sirius_log_level_info)
#define internal_info(fmt, ...)                          \
  do {                                                   \
    internal_log(sirius_log_level_info, log_green,       \
                 log_module_name, sirius_file, __LINE__, \
                 fmt, ##__VA_ARGS__);                    \
  } while (0)
#else
#define internal_info(fmt, ...)   \
  do {                            \
    _custom_swallow(__VA_ARGS__); \
  } while (0)
#endif

#if (INTERNAL_LOG_LEVEL >= sirius_log_level_debg)
#define internal_debg(fmt, ...)                          \
  do {                                                   \
    internal_log(sirius_log_level_debg, log_color_none,  \
                 log_module_name, sirius_file, __LINE__, \
                 fmt, ##__VA_ARGS__);                    \
  } while (0)
#else
#define internal_debg(fmt, ...)   \
  do {                            \
    _custom_swallow(__VA_ARGS__); \
  } while (0)
#endif

#ifdef _WIN32
#define _sirius_strerror_r(errnum, buf, size) \
  strerror_s(buf, size, errnum)
#else
#include <string.h>
#define _sirius_strerror_r strerror_r
#endif

#define internal_str_error(err_c, fun)                \
  do {                                                \
    char e[256];                                      \
    if (likely(0 == _sirius_strerror_r(err_c, e,      \
                                       sizeof(e)))) { \
      internal_error(fun ": %d, %s\n", err_c, e);     \
    } else {                                          \
      internal_error(fun ": %d\n", err_c);            \
    }                                                 \
  } while (0)

#define internal_str_warn(err_c, fun)                 \
  do {                                                \
    char e[256];                                      \
    if (likely(0 == _sirius_strerror_r(err_c, e,      \
                                       sizeof(e)))) { \
      internal_warn(fun ": %d, %s\n", err_c, e);      \
    } else {                                          \
      internal_warn(fun ": %d\n", err_c);             \
    }                                                 \
  } while (0)

#ifdef _WIN32
#define internal_win_fmt_error(err_c, fun)            \
  do {                                                \
    char e[256];                                      \
    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM |        \
                  FORMAT_MESSAGE_IGNORE_INSERTS;      \
    DWORD size = FormatMessage(                       \
        flags, nullptr, err_c,                        \
        MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), e, \
        sizeof(e) / sizeof(TCHAR), nullptr);          \
    if (unlikely(size == 0)) {                        \
      internal_error(fun ": %d\n", err_c);            \
    } else {                                          \
      internal_error(fun ": %d, %s\n", err_c, e);     \
    }                                                 \
  } while (0)

#define internal_win_fmt_warn(err_c, fun)             \
  do {                                                \
    char e[256];                                      \
    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM |        \
                  FORMAT_MESSAGE_IGNORE_INSERTS;      \
    DWORD size = FormatMessage(                       \
        flags, nullptr, err_c,                        \
        MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), e, \
        sizeof(e) / sizeof(TCHAR), nullptr);          \
    if (unlikely(size == 0)) {                        \
      internal_warn(fun ": %d\n", err_c);             \
    } else {                                          \
      internal_warn(fun ": %d, %s\n", err_c, e);      \
    }                                                 \
  } while (0)
#endif

#endif  // INTERNAL_LOG_H
