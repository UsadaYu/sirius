#pragma once

#include "sirius/attributes.h"
#include "sirius/file.h"
#include "sirius/thread/macro.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Custom log module name.
 *
 * @example
 * - (1) CFLAGS += -D_SIRIUS_LOG_PRINT_NAME='"$(_SIRIUS_LOG_PRINT_NAME)"'
 */
#ifndef _SIRIUS_LOG_PRINT_NAME
#  define _SIRIUS_LOG_PRINT_NAME "unknown"
#endif

#ifndef SIRIUS_LOG_LEVEL_NONE
#  define SIRIUS_LOG_LEVEL_NONE (0)
#endif
#ifndef SIRIUS_LOG_LEVEL_ERROR
#  define SIRIUS_LOG_LEVEL_ERROR (1)
#endif
#ifndef SIRIUS_LOG_LEVEL_WARN
#  define SIRIUS_LOG_LEVEL_WARN (2)
#endif
#ifndef SIRIUS_LOG_LEVEL_INFO
#  define SIRIUS_LOG_LEVEL_INFO (3)
#endif
#ifndef SIRIUS_LOG_LEVEL_DEBUG
#  define SIRIUS_LOG_LEVEL_DEBUG (4)
#endif

/**
 * @brief Compile macro, level: 0, 1, 2, 3, 4.
 * The default log printing level.
 *
 * @example
 * - (1) CFLAGS += -D_SIRIUS_LOG_LEVEL=2
 */
#ifndef _SIRIUS_LOG_LEVEL
#  define _SIRIUS_LOG_LEVEL SIRIUS_LOG_LEVEL_INFO
#endif

/**
 * @brief ANSI Colors.
 */
#ifndef LOG_COLOR_NONE
#  define LOG_COLOR_NONE "\033[m"
#endif
#ifndef LOG_RED
#  define LOG_RED "\033[0;32;31m"
#endif
#ifndef LOG_GREEN
#  define LOG_GREEN "\033[0;32;32m"
#endif
#ifndef LOG_BLUE
#  define LOG_BLUE "\033[0;32;34m"
#endif
#ifndef LOG_GRAY
#  define LOG_GRAY "\033[1;30m"
#endif
#ifndef LOG_CYAN
#  define LOG_CYAN "\033[0;36m"
#endif
#ifndef LOG_PURPLE
#  define LOG_PURPLE "\033[0;35m"
#endif
#ifndef LOG_BROWN
#  define LOG_BROWN "\033[0;33m"
#endif
#ifndef LOG_YELLOW
#  define LOG_YELLOW "\033[1;33m"
#endif
#ifndef LOG_WHITE
#  define LOG_WHITE "\033[1;37m"
#endif

#ifndef LOG_LEVEL_STR_ERROR
#  define LOG_LEVEL_STR_ERROR "Error"
#endif
#ifndef LOG_LEVEL_STR_WARN
#  define LOG_LEVEL_STR_WARN "Warn "
#endif
#ifndef LOG_LEVEL_STR_INFO
#  define LOG_LEVEL_STR_INFO "Info "
#endif
#ifndef LOG_LEVEL_STR_DEBUG
#  define LOG_LEVEL_STR_DEBUG "Debug"
#endif

typedef struct {
  /**
   * @note Set to default `stdout` / `stderr` when `nullptr`.
   */
  const char *log_path;

  /**
   * @note Not yet.
   */
  enum SiriusThreadProcess shared;
} sirius_log_fs_t;

typedef struct {
  /**
   * @note Not yet.
   */
  int disable_color;

  sirius_log_fs_t out;
  sirius_log_fs_t err;
} sirius_log_config_t;

/**
 * @brief Set the path of the daemon executable file.
 * Currently, it is only necessary to do it under Windows.
 */
sirius_api int sirius_log_set_daemon_path(const char *path);

sirius_api void sirius_log_configure(const sirius_log_config_t *cfg);

sirius_api void sirius_log_impl(int level, const char *level_str,
                                const char *color, const char *module,
                                const char *file, const char *func, int line,
                                const char *fmt, ...);

sirius_api void sirius_logsp_impl(int level, const char *level_str,
                                  const char *color, const char *module,
                                  const char *fmt, ...);

#define _sirius_log_void(level, fmt, ...) \
  do { \
    if (0) { \
      sirius_log_impl(level, "", "", "", "", "", 0, fmt, ##__VA_ARGS__); \
    } \
  } while (0)

#define _sirius_logsp_void(level, fmt, ...) \
  do { \
    if (0) { \
      sirius_logsp_impl(level, "", "", "", fmt, ##__VA_ARGS__); \
    } \
  } while (0)

#if (_SIRIUS_LOG_LEVEL >= SIRIUS_LOG_LEVEL_ERROR)
#  define _sirius_error(fmt, ...) \
    sirius_log_impl(SIRIUS_LOG_LEVEL_ERROR, LOG_LEVEL_STR_ERROR, LOG_RED, \
                    _SIRIUS_LOG_PRINT_NAME, SIRIUS_FILE_NAME, __func__, \
                    __LINE__, fmt, ##__VA_ARGS__)
#  define _sirius_errorsp(fmt, ...) \
    sirius_logsp_impl(SIRIUS_LOG_LEVEL_ERROR, LOG_LEVEL_STR_ERROR, LOG_RED, \
                      _SIRIUS_LOG_PRINT_NAME, fmt, ##__VA_ARGS__)
#else
#  define _sirius_error(fmt, ...) \
    _sirius_log_void(SIRIUS_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#  define _sirius_errorsp(fmt, ...) \
    _sirius_logsp_void(SIRIUS_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#endif

#if (_SIRIUS_LOG_LEVEL >= SIRIUS_LOG_LEVEL_WARN)
#  define _sirius_warn(fmt, ...) \
    sirius_log_impl(SIRIUS_LOG_LEVEL_WARN, LOG_LEVEL_STR_WARN, LOG_YELLOW, \
                    _SIRIUS_LOG_PRINT_NAME, SIRIUS_FILE_NAME, __func__, \
                    __LINE__, fmt, ##__VA_ARGS__)
#  define _sirius_warnsp(fmt, ...) \
    sirius_logsp_impl(SIRIUS_LOG_LEVEL_WARN, LOG_LEVEL_STR_WARN, LOG_YELLOW, \
                      _SIRIUS_LOG_PRINT_NAME, fmt, ##__VA_ARGS__)
#else
#  define _sirius_warn(fmt, ...) \
    _sirius_log_void(SIRIUS_LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#  define _sirius_warnsp(fmt, ...) \
    _sirius_logsp_void(SIRIUS_LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#endif

#if (_SIRIUS_LOG_LEVEL >= SIRIUS_LOG_LEVEL_INFO)
#  define _sirius_info(fmt, ...) \
    sirius_log_impl(SIRIUS_LOG_LEVEL_INFO, LOG_LEVEL_STR_INFO, LOG_GREEN, \
                    _SIRIUS_LOG_PRINT_NAME, SIRIUS_FILE_NAME, __func__, \
                    __LINE__, fmt, ##__VA_ARGS__)
#  define _sirius_infosp(fmt, ...) \
    sirius_logsp_impl(SIRIUS_LOG_LEVEL_INFO, LOG_LEVEL_STR_INFO, LOG_GREEN, \
                      _SIRIUS_LOG_PRINT_NAME, fmt, ##__VA_ARGS__)
#else
#  define _sirius_info(fmt, ...) \
    _sirius_log_void(SIRIUS_LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#  define _sirius_infosp(fmt, ...) \
    _sirius_logsp_void(SIRIUS_LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#endif

#if (_SIRIUS_LOG_LEVEL >= SIRIUS_LOG_LEVEL_DEBUG)
#  define _sirius_debug(fmt, ...) \
    sirius_log_impl(SIRIUS_LOG_LEVEL_DEBUG, LOG_LEVEL_STR_DEBUG, \
                    LOG_COLOR_NONE, _SIRIUS_LOG_PRINT_NAME, SIRIUS_FILE_NAME, \
                    __func__, __LINE__, fmt, ##__VA_ARGS__)
#  define _sirius_debugsp(fmt, ...) \
    sirius_logsp_impl(SIRIUS_LOG_LEVEL_DEBUG, LOG_LEVEL_STR_DEBUG, \
                      LOG_COLOR_NONE, _SIRIUS_LOG_PRINT_NAME, fmt, \
                      ##__VA_ARGS__)
#else
#  define _sirius_debug(fmt, ...) \
    _sirius_log_void(SIRIUS_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#  define _sirius_debugsp(fmt, ...) \
    _sirius_logsp_void(SIRIUS_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#endif

#define sirius_error(fmt, ...) _sirius_error(fmt, ##__VA_ARGS__)
#define sirius_warn(fmt, ...) _sirius_warn(fmt, ##__VA_ARGS__)
#define sirius_info(fmt, ...) _sirius_info(fmt, ##__VA_ARGS__)
#define sirius_debug(fmt, ...) _sirius_debug(fmt, ##__VA_ARGS__)

#define sirius_errorsp(fmt, ...) _sirius_errorsp(fmt, ##__VA_ARGS__)
#define sirius_warnsp(fmt, ...) _sirius_warnsp(fmt, ##__VA_ARGS__)
#define sirius_infosp(fmt, ...) _sirius_infosp(fmt, ##__VA_ARGS__)
#define sirius_debugsp(fmt, ...) _sirius_debugsp(fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
