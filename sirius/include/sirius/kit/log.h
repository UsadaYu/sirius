#pragma once

#include "sirius/attributes.h"
#include "sirius/c/fs.h"
#include "sirius/config.h"
#include "sirius/file.h"
#include "sirius/thread/macro.h"

#undef SS_LOG_LEVEL_NONE
#define SS_LOG_LEVEL_NONE (0)
#undef SS_LOG_LEVEL_ERROR
#define SS_LOG_LEVEL_ERROR (1)
#undef SS_LOG_LEVEL_WARN
#define SS_LOG_LEVEL_WARN (2)
#undef SS_LOG_LEVEL_INFO
#define SS_LOG_LEVEL_INFO (3)
#undef SS_LOG_LEVEL_DEBUG
#define SS_LOG_LEVEL_DEBUG (4)

typedef struct {
  /**
   * @note Set to default `stdout` / `stderr` when `nullptr`.
   */
  const char *log_path;

  /**
   * @ref `enum SsOFlags`.
   */
  int flags;
  int mode;

  int ansi_disable;

  /**
   * @note Default: `SsThreadProcess::kSsThreadProcessShared`.
   */
  enum SsThreadProcess shared;
} ss_log_fs_t;

typedef struct {
  ss_log_fs_t out;
  ss_log_fs_t err;
} ss_log_config_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set the path of the log executable file.
 */
SIRIUS_API int ss_log_set_exe_path(const char *path);
SIRIUS_API void ss_log_configure(const ss_log_config_t *cfg);
SIRIUS_API void ss_log_impl(int level, const char *module, const char *file,
                            int line, const char *fmt, ...);
SIRIUS_API void ss_logsp_impl(int level, const char *module, const char *fmt,
                              ...);

#ifdef __cplusplus
}
#endif

#define _ss_inner_log_void(level, fmt, ...) \
  do { \
    if (0) { \
      ss_log_impl(level, "", "", 0, fmt, ##__VA_ARGS__); \
    } \
  } while (0)

#define _ss_inner_logsp_void(level, fmt, ...) \
  do { \
    if (0) { \
      ss_logsp_impl(level, "", fmt, ##__VA_ARGS__); \
    } \
  } while (0)

#if (_SIRIUS_LOG_LEVEL >= SS_LOG_LEVEL_ERROR)
#  define _ss_inner_log_error(fmt, ...) \
    ss_log_impl(SS_LOG_LEVEL_ERROR, _SIRIUS_LOG_MODULE_NAME, SS_FILE_NAME, \
                __LINE__, fmt, ##__VA_ARGS__)
#  define _ss_inner_log_errorsp(fmt, ...) \
    ss_logsp_impl(SS_LOG_LEVEL_ERROR, _SIRIUS_LOG_MODULE_NAME, fmt, \
                  ##__VA_ARGS__)
#else
#  define _ss_inner_log_error(fmt, ...) \
    _ss_inner_log_void(SS_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#  define _ss_inner_log_errorsp(fmt, ...) \
    _ss_inner_logsp_void(SS_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#endif

#if (_SIRIUS_LOG_LEVEL >= SS_LOG_LEVEL_WARN)
#  define _ss_inner_log_warn(fmt, ...) \
    ss_log_impl(SS_LOG_LEVEL_WARN, _SIRIUS_LOG_MODULE_NAME, SS_FILE_NAME, \
                __LINE__, fmt, ##__VA_ARGS__)
#  define _ss_inner_log_warnsp(fmt, ...) \
    ss_logsp_impl(SS_LOG_LEVEL_WARN, _SIRIUS_LOG_MODULE_NAME, fmt, \
                  ##__VA_ARGS__)
#else
#  define _ss_inner_log_warn(fmt, ...) \
    _ss_inner_log_void(SS_LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#  define _ss_inner_log_warnsp(fmt, ...) \
    _ss_inner_logsp_void(SS_LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#endif

#if (_SIRIUS_LOG_LEVEL >= SS_LOG_LEVEL_INFO)
#  define _ss_inner_log_info(fmt, ...) \
    ss_log_impl(SS_LOG_LEVEL_INFO, _SIRIUS_LOG_MODULE_NAME, SS_FILE_NAME, \
                __LINE__, fmt, ##__VA_ARGS__)
#  define _ss_inner_log_infosp(fmt, ...) \
    ss_logsp_impl(SS_LOG_LEVEL_INFO, _SIRIUS_LOG_MODULE_NAME, fmt, \
                  ##__VA_ARGS__)
#else
#  define _ss_inner_log_info(fmt, ...) \
    _ss_inner_log_void(SS_LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#  define _ss_inner_log_infosp(fmt, ...) \
    _ss_inner_logsp_void(SS_LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#endif

#if (_SIRIUS_LOG_LEVEL >= SS_LOG_LEVEL_DEBUG)
#  define _ss_inner_log_debug(fmt, ...) \
    ss_log_impl(SS_LOG_LEVEL_DEBUG, _SIRIUS_LOG_MODULE_NAME, SS_FILE_NAME, \
                __LINE__, fmt, ##__VA_ARGS__)
#  define _ss_inner_log_debugsp(fmt, ...) \
    ss_logsp_impl(SS_LOG_LEVEL_DEBUG, _SIRIUS_LOG_MODULE_NAME, fmt, \
                  ##__VA_ARGS__)
#else
#  define _ss_inner_log_debug(fmt, ...) \
    _ss_inner_log_void(SS_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#  define _ss_inner_log_debugsp(fmt, ...) \
    _ss_inner_logsp_void(SS_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#endif

// clang-format off
#define ss_log_error(fmt, ...) _ss_inner_log_error(fmt, ##__VA_ARGS__)
#define ss_log_warn(fmt, ...) _ss_inner_log_warn(fmt, ##__VA_ARGS__)
#define ss_log_info(fmt, ...) _ss_inner_log_info(fmt, ##__VA_ARGS__)
#define ss_log_debug(fmt, ...)    _ss_inner_log_debug(fmt, ##__VA_ARGS__)

#define ss_log_errorsp(fmt, ...) _ss_inner_log_errorsp(fmt, ##__VA_ARGS__)
#define ss_log_warnsp(fmt, ...) _ss_inner_log_warnsp(fmt, ##__VA_ARGS__)
#define ss_log_infosp(fmt, ...) _ss_inner_log_infosp(fmt, ##__VA_ARGS__)
#define ss_log_debugsp(fmt, ...) _ss_inner_log_debugsp(fmt, ##__VA_ARGS__)
// clang-format on
