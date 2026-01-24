#pragma once

#include "sirius/attributes.h"
#include "sirius/file.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Custom log module name.
 *
 * @example
 * - (1) CFLAGS += -Dsirius_log_module_name='"$(sirius_log_module_name)"'
 */
#ifndef sirius_log_module_name
#  define sirius_log_module_name "unknown"
#endif

#define sirius_log_level_none (0)
#define sirius_log_level_error (1)
#define sirius_log_level_warn (2)
#define sirius_log_level_info (3)
#define sirius_log_level_debug (4)

/**
 * @brief Compile macro, level: 0, 1, 2, 3, 4.
 * The default log printing level.
 *
 * @example
 * - (1) CFLAGS += -Dsirius_log_level=2
 */
#ifndef sirius_log_level
#  define sirius_log_level sirius_log_level_info
#endif

/**
 * @brief ANSI Colors.
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

#define log_level_str_debug "Debug"
#define log_level_str_info "Info "
#define log_level_str_warn "Warn "
#define log_level_str_error "Error"

typedef struct {
  int *fd_err;
  int *fd_out;
} sirius_log_config_t;

sirius_api void sirius_log_configure(sirius_log_config_t cfg);

sirius_api void sirius_log_impl(int level, const char *level_str,
                                const char *color, const char *module,
                                const char *file, const char *func, int line,
                                const char *fmt, ...);

sirius_api void sirius_logsp_impl(int level, const char *level_str,
                                  const char *color, const char *module,
                                  const char *fmt, ...);

#define _SIRIUS_LOG_VOID(level, fmt, ...) \
  do { \
    if (0) { \
      sirius_log_impl(level, "", "", "", "", "", 0, fmt, ##__VA_ARGS__); \
    } \
  } while (0)

#define _SIRIUS_LOGSP_VOID(level, fmt, ...) \
  do { \
    if (0) { \
      sirius_logsp_impl(level, "", "", "", fmt, ##__VA_ARGS__); \
    } \
  } while (0)

#if (sirius_log_level >= sirius_log_level_error)
#  define _SIRIUS_ERROR(fmt, ...) \
    sirius_log_impl(sirius_log_level_error, log_level_str_error, log_red, \
                    sirius_log_module_name, sirius_file_name, __func__, \
                    __LINE__, fmt, ##__VA_ARGS__)
#else
#  define _SIRIUS_ERROR(fmt, ...) \
    _SIRIUS_LOG_VOID(sirius_log_level_error, fmt, ##__VA_ARGS__)
#endif

#if (sirius_log_level >= sirius_log_level_warn)
#  define _SIRIUS_WARN(fmt, ...) \
    sirius_log_impl(sirius_log_level_warn, log_level_str_warn, log_yellow, \
                    sirius_log_module_name, sirius_file_name, __func__, \
                    __LINE__, fmt, ##__VA_ARGS__)
#  define _SIRIUS_WARNSP(fmt, ...) \
    sirius_logsp_impl(sirius_log_level_warn, log_level_str_warn, log_yellow, \
                      sirius_log_module_name, fmt, ##__VA_ARGS__)
#else
#  define _SIRIUS_WARN(fmt, ...) \
    _SIRIUS_LOG_VOID(sirius_log_level_warn, fmt, ##__VA_ARGS__)
#  define _SIRIUS_WARNSP(fmt, ...) \
    _SIRIUS_LOGSP_VOID(sirius_log_level_warn, fmt, ##__VA_ARGS__)
#endif

#if (sirius_log_level >= sirius_log_level_info)
#  define _SIRIUS_INFO(fmt, ...) \
    sirius_log_impl(sirius_log_level_info, log_level_str_info, log_green, \
                    sirius_log_module_name, sirius_file_name, __func__, \
                    __LINE__, fmt, ##__VA_ARGS__)
#  define _SIRIUS_INFOSP(fmt, ...) \
    sirius_logsp_impl(sirius_log_level_info, log_level_str_info, log_green, \
                      sirius_log_module_name, fmt, ##__VA_ARGS__)
#else
#  define _SIRIUS_INFO(fmt, ...) \
    _SIRIUS_LOG_VOID(sirius_log_level_info, fmt, ##__VA_ARGS__)
#  define _SIRIUS_INFOSP(fmt, ...) \
    _SIRIUS_LOGSP_VOID(sirius_log_level_info, fmt, ##__VA_ARGS__)
#endif

#if (sirius_log_level >= sirius_log_level_debug)
#  define _SIRIUS_DEBUG(fmt, ...) \
    sirius_log_impl(sirius_log_level_debug, log_level_str_debug, \
                    log_color_none, sirius_log_module_name, sirius_file_name, \
                    __func__, __LINE__, fmt, ##__VA_ARGS__)
#  define _SIRIUS_DEBUGSP(fmt, ...) \
    sirius_logsp_impl(sirius_log_level_debug, log_level_str_debug, \
                      log_color_none, sirius_log_module_name, fmt, \
                      ##__VA_ARGS__)
#else
#  define _SIRIUS_DEBUG(fmt, ...) \
    _SIRIUS_LOG_VOID(sirius_log_level_debug, fmt, ##__VA_ARGS__)
#  define _SIRIUS_DEBUGSP(fmt, ...) \
    _SIRIUS_LOGSP_VOID(sirius_log_level_debug, fmt, ##__VA_ARGS__)
#endif

#define sirius_error(fmt, ...) _SIRIUS_ERROR(fmt, ##__VA_ARGS__)
#define sirius_warn(fmt, ...) _SIRIUS_WARN(fmt, ##__VA_ARGS__)
#define sirius_info(fmt, ...) _SIRIUS_INFO(fmt, ##__VA_ARGS__)
#define sirius_debug(fmt, ...) _SIRIUS_DEBUG(fmt, ##__VA_ARGS__)

#define sirius_warnsp(fmt, ...) _SIRIUS_WARNSP(fmt, ##__VA_ARGS__)
#define sirius_infosp(fmt, ...) _SIRIUS_INFOSP(fmt, ##__VA_ARGS__)
#define sirius_debugsp(fmt, ...) _SIRIUS_DEBUGSP(fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
