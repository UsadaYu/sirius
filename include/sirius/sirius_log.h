/**
 * @name sirius_log.h
 *
 * @author UsadaYu
 *
 * @date
 * Create: 2024-07-30
 * Update: 2025-07-10
 *
 * @brief Log print.
 */

#ifndef SIRIUS_LOG_H
#define SIRIUS_LOG_H

#include "custom/log.h"
#include "custom/macro.h"
#include "sirius_attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Custom log module name.
 *
 * @example
 * - (1) CFLAGS += -Dlog_module_name='"$(log_module_name)"'
 */
#ifndef log_module_name
#  define log_module_name "unknown"
#endif

/**
 * @brief Disable log printing.
 */
#define sirius_log_level_none (0)
/**
 * @brief Print error message.
 */
#define sirius_log_level_error (1)
/**
 * @brief Print warning message.
 */
#define sirius_log_level_warn (2)
/**
 * @brief Print generic message.
 */
#define sirius_log_level_info (3)
/**
 * @brief Print debug message.
 */
#define sirius_log_level_debg (4)

/**
 * @brief Compile macro, level: 0, 1, 2, 3, 4.
 * The default log printing level.
 *
 * @example
 * - (1) CFLAGS += -Dexternal_log_level=2
 */
#ifndef external_log_level
#  define external_log_level sirius_log_level_info
#elif (external_log_level > sirius_log_level_debg)
#  undef external_log_level
#  define external_log_level sirius_log_level_debg
#endif

/**
 * @brief The file name without a path prefix.
 *
 * @note The `basename` function provided by posix is not used here. The
 * prototype of the basename function is: `char *basename(char *path);`.
 * Therefore, implicitly converting a `const char *` type to a `char *` type may
 * result in a compilation warning.
 */
#define sirius_file (_custom_basename(__FILE__))

/**
 * @brief Log color.
 *
 * @note Only color printing under ANSI is supported.
 */
#define log_color_none "\033[m"
#define log_red "\033[0;32;31m"
#define log_green "\033[0;32;32m"
#define log_blue "\033[0;32;34m"
#define log_gray "\033[1;30m"
#define log_cyan "\033[0;36m"
#define log_purple "\033[0;35m"
#define log_brown "\033[0;33m"
#define log_yellow "\033[1;33m"
#define log_white "\033[1;37m"

typedef struct {
  /**
   * @brief File descriptor for error/warn log writing. The original
   * configuration will not change if the parameter is null.
   */
  int *fd_err;

  /**
   * @brief File descriptor for info/debug log writing. The original
   * configuration will not change if the parameter is null.
   */
  int *fd_out;
} sirius_log_config_t;

sirius_api void sirius_log_config(sirius_log_config_t cfg);

/**
 * @brief Print log.
 *
 * @param[in] log_level Log level.
 * @param[in] color Print color.
 * @param[in] module Module name.
 * @param[in] file File.
 * @param[in] func Function.
 * @param[in] line Line.
 * @param[in] fmt User's message.
 */
sirius_api void sirius_log(int log_level, const char *color, const char *module,
                           const char *file, const char *func, int line,
                           const char *fmt, ...);

/**
 * @brief Print log, but hide the file information and thread id.
 *
 * @param[in] log_level Log level.
 * @param[in] color Print color.
 * @param[in] module Module name.
 * @param[in] fmt User's message.
 */
sirius_api void sirius_logsp(int log_level, const char *color,
                             const char *module, const char *fmt, ...);

#define _LOG_WRITE(level, color, fmt, ...) \
  do { \
    sirius_log(level, color, log_module_name, sirius_file, __func__, __LINE__, \
               fmt, ##__VA_ARGS__); \
  } while (0)

#define _LOG_WRITESP(level, color, fmt, ...) \
  do { \
    sirius_logsp(level, color, log_module_name, fmt, ##__VA_ARGS__); \
  } while (0)

#if (external_log_level >= sirius_log_level_error)
#  define _ERROR(fmt, ...) \
    _LOG_WRITE(sirius_log_level_error, log_red, fmt, ##__VA_ARGS__)
#else
#  define _ERROR(fmt, ...) \
    do { \
      _custom_swallow(__VA_ARGS__); \
    } while (0)
#endif

#if (external_log_level >= sirius_log_level_warn)
#  define _WARN(fmt, ...) \
    _LOG_WRITE(sirius_log_level_warn, log_yellow, fmt, ##__VA_ARGS__)
#  define _WARNSP(fmt, ...) \
    _LOG_WRITESP(sirius_log_level_warn, log_yellow, fmt, ##__VA_ARGS__)
#else
#  define _WARN(fmt, ...) \
    do { \
      _custom_swallow(__VA_ARGS__); \
    } while (0)
#  define _WARNSP(fmt, ...) \
    do { \
      _custom_swallow(__VA_ARGS__); \
    } while (0)
#endif

#if (external_log_level >= sirius_log_level_info)
#  define _INFO(fmt, ...) \
    _LOG_WRITE(sirius_log_level_info, log_green, fmt, ##__VA_ARGS__)
#  define _INFOSP(fmt, ...) \
    _LOG_WRITESP(sirius_log_level_info, log_green, fmt, ##__VA_ARGS__)
#else
#  define _INFO(fmt, ...) \
    do { \
      _custom_swallow(__VA_ARGS__); \
    } while (0)
#  define _INFOSP(fmt, ...) \
    do { \
      _custom_swallow(__VA_ARGS__); \
    } while (0)
#endif

#if (external_log_level >= sirius_log_level_debg)
#  define _DEBG(fmt, ...) \
    _LOG_WRITE(sirius_log_level_debg, log_color_none, fmt, ##__VA_ARGS__)
#  define _DEBGSP(fmt, ...) \
    _LOG_WRITESP(sirius_log_level_debg, log_color_none, fmt, ##__VA_ARGS__)
#else
#  define _DEBG(fmt, ...) \
    do { \
      _custom_swallow(__VA_ARGS__); \
    } while (0)
#  define _DEBGSP(fmt, ...) \
    do { \
      _custom_swallow(__VA_ARGS__); \
    } while (0)
#endif

#define sirius_log_write(level, color, fmt, ...) \
  _LOG_WRITE(level, color, fmt, ##__VA_ARGS__)
#define sirius_error(fmt, ...) _ERROR(fmt, ##__VA_ARGS__)
#define sirius_warn(fmt, ...) _WARN(fmt, ##__VA_ARGS__)
#define sirius_info(fmt, ...) _INFO(fmt, ##__VA_ARGS__)
#define sirius_debg(fmt, ...) _DEBG(fmt, ##__VA_ARGS__)

#define sirius_log_writesp(level, color, fmt, ...) \
  _LOG_WRITESP(level, color, fmt, ##__VA_ARGS__)
#define sirius_warnsp(fmt, ...) _WARNSP(fmt, ##__VA_ARGS__)
#define sirius_infosp(fmt, ...) _INFOSP(fmt, ##__VA_ARGS__)
#define sirius_debgsp(fmt, ...) _DEBGSP(fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // SIRIUS_LOG_H
