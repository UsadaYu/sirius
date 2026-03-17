#pragma once

#include "sirius/attributes.h"
#include "sirius/c/fs.h"
#include "sirius/config.h"
#include "sirius/file.h"
#include "sirius/thread/macro.h"

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

typedef struct {
  /**
   * @note Set to default `stdout` / `stderr` when `nullptr`.
   */
  const char *log_path;

  /**
   * @ref `SiriusOFlags`;
   */
  int flags;
  int mode;

  int ansi_disable;

  /**
   * @note Default: `SiriusThreadProcess::kSiriusThreadProcessShared`.
   */
  enum SiriusThreadProcess shared;
} sirius_log_fs_t;

typedef struct {
  sirius_log_fs_t out;
  sirius_log_fs_t err;
} sirius_log_config_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set the path of the log executable file.
 */
sirius_api int sirius_log_set_exe_path(const char *path);
sirius_api void sirius_log_configure(const sirius_log_config_t *cfg);
sirius_api void sirius_log_impl(int level, const char *module, const char *file,
                                int line, const char *fmt, ...);
sirius_api void sirius_logsp_impl(int level, const char *module,
                                  const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#define _sirius_log_void(level, fmt, ...) \
  do { \
    if (0) { \
      sirius_log_impl(level, "", "", 0, fmt, ##__VA_ARGS__); \
    } \
  } while (0)

#define _sirius_logsp_void(level, fmt, ...) \
  do { \
    if (0) { \
      sirius_logsp_impl(level, "", fmt, ##__VA_ARGS__); \
    } \
  } while (0)

#if (_SIRIUS_LOG_LEVEL >= SIRIUS_LOG_LEVEL_ERROR)
#  define _sirius_error(fmt, ...) \
    sirius_log_impl(SIRIUS_LOG_LEVEL_ERROR, _SIRIUS_LOG_MODULE_NAME, \
                    SIRIUS_FILE_NAME, __LINE__, fmt, ##__VA_ARGS__)
#  define _sirius_errorsp(fmt, ...) \
    sirius_logsp_impl(SIRIUS_LOG_LEVEL_ERROR, _SIRIUS_LOG_MODULE_NAME, fmt, \
                      ##__VA_ARGS__)
#else
#  define _sirius_error(fmt, ...) \
    _sirius_log_void(SIRIUS_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#  define _sirius_errorsp(fmt, ...) \
    _sirius_logsp_void(SIRIUS_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#endif

#if (_SIRIUS_LOG_LEVEL >= SIRIUS_LOG_LEVEL_WARN)
#  define _sirius_warn(fmt, ...) \
    sirius_log_impl(SIRIUS_LOG_LEVEL_WARN, _SIRIUS_LOG_MODULE_NAME, \
                    SIRIUS_FILE_NAME, __LINE__, fmt, ##__VA_ARGS__)
#  define _sirius_warnsp(fmt, ...) \
    sirius_logsp_impl(SIRIUS_LOG_LEVEL_WARN, _SIRIUS_LOG_MODULE_NAME, fmt, \
                      ##__VA_ARGS__)
#else
#  define _sirius_warn(fmt, ...) \
    _sirius_log_void(SIRIUS_LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#  define _sirius_warnsp(fmt, ...) \
    _sirius_logsp_void(SIRIUS_LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#endif

#if (_SIRIUS_LOG_LEVEL >= SIRIUS_LOG_LEVEL_INFO)
#  define _sirius_info(fmt, ...) \
    sirius_log_impl(SIRIUS_LOG_LEVEL_INFO, _SIRIUS_LOG_MODULE_NAME, \
                    SIRIUS_FILE_NAME, __LINE__, fmt, ##__VA_ARGS__)
#  define _sirius_infosp(fmt, ...) \
    sirius_logsp_impl(SIRIUS_LOG_LEVEL_INFO, _SIRIUS_LOG_MODULE_NAME, fmt, \
                      ##__VA_ARGS__)
#else
#  define _sirius_info(fmt, ...) \
    _sirius_log_void(SIRIUS_LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#  define _sirius_infosp(fmt, ...) \
    _sirius_logsp_void(SIRIUS_LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#endif

#if (_SIRIUS_LOG_LEVEL >= SIRIUS_LOG_LEVEL_DEBUG)
#  define _sirius_debug(fmt, ...) \
    sirius_log_impl(SIRIUS_LOG_LEVEL_DEBUG, _SIRIUS_LOG_MODULE_NAME, \
                    SIRIUS_FILE_NAME, __LINE__, fmt, ##__VA_ARGS__)
#  define _sirius_debugsp(fmt, ...) \
    sirius_logsp_impl(SIRIUS_LOG_LEVEL_DEBUG, _SIRIUS_LOG_MODULE_NAME, fmt, \
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
