/**
 * @name sirius_log.h
 * 
 * @author 胡益华
 * 
 * @date 2024-07-30
 * 
 * @brief 日志信息打印
 * 
 * @details
 * (1) 初始化后创建管道文件 log_pipe
 * 
 * (2) 控制日志打印等级方式： echo loglevel [lv] > log_pipe
 *  lv 参考 sirius_log_lv_t
 */

#ifndef __SIRIUS_LOG_H__
#define __SIRIUS_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 自定义模块名称
 * CFLAGS += -DLOG_MODULE_NAME='"$(LOG_MODULE_NAME)"'
 */
#ifndef LOG_MODULE_NAME
#define LOG_MODULE_NAME "unknown"
#endif // LOG_MODULE_NAME


#ifndef LOG_PRNT_BUF_SIZE_MAX
#define LOG_PRNT_BUF_SIZE_MAX 20480
#endif
#ifndef LOG_PRNT_BUF_SIZE_MIN
#define LOG_PRNT_BUF_SIZE_MIN 128
#endif
/**
 * 自定义打印栈最大值
 * CFLAGS += -DLOG_PRNT_BUF_SIZE=$(LOG_PRNT_BUF_SIZE)
 */
#ifndef LOG_PRNT_BUF_SIZE
#define LOG_PRNT_BUF_SIZE 4096
#else
#if LOG_PRNT_BUF_SIZE > LOG_PRNT_BUF_SIZE_MAX
#undef LOG_PRNT_BUF_SIZE
#if defined (__GNUC__) || \
    defined(__clang__) || \
    defined(_MSC_VER)
#pragma message "LOG_PRNT_BUF_SIZE->LOG_PRNT_BUF_SIZE_MAX"
#endif // compiler
#define LOG_PRNT_BUF_SIZE LOG_PRNT_BUF_SIZE_MAX
#elif LOG_PRNT_BUF_SIZE < LOG_PRNT_BUF_SIZE_MIN
#undef LOG_PRNT_BUF_SIZE
#if defined (__GNUC__) || \
    defined(__clang__) || \
    defined(_MSC_VER)
#pragma message "LOG_PRNT_BUF_SIZE->LOG_PRNT_BUF_SIZE_MIN"
#endif // compiler
#define LOG_PRNT_BUF_SIZE LOG_PRNT_BUF_SIZE_MIN
#endif
#endif // LOG_PRNT_BUF_SIZE

#ifndef SIRIUS_FILE
#include <libgen.h>
/**
 * @brief the file name without a path prefix
 */
#define SIRIUS_FILE     (basename(__FILE__))
#endif // SIRIUS_FILE

/* 日志颜色 */
#define LOG_NONE        "\033[m"
#define LOG_RED         "\033[0;32;31m"
#define LOG_GREEN       "\033[0;32;32m"
#define LOG_BLUE        "\033[0;32;34m"
#define LOG_DARY_GRAY   "\033[1;30m"
#define LOG_CYAN        "\033[0;36m"
#define LOG_PURPLE      "\033[0;35m"
#define LOG_BROWN       "\033[0;33m"
#define LOG_YELLOW      "\033[1;33m"
#define LOG_WHITE       "\033[1;37m"

/* 调整日志等级管道输入命令 */
#define LOG_CMD_LV      "loglevel"

/* 日志打印等级枚举 */
typedef enum {
    SIRIUS_LOG_LV_0         = 0,    // 关闭日志打印

    SIRIUS_LOG_LV_DEFAULT   = 1,    // 默认等级

    SIRIUS_LOG_LV_ERROR     = 2,    // 错误打印
    SIRIUS_LOG_LV_WARN      = 3,    // 警告打印
    SIRIUS_LOG_LV_INFO      = 4,    // 通用信息打印

    SIRIUS_LOG_LV_DEBG      = 5,    // debug

    SIRIUS_LOG_LV_MAX,
} sirius_log_lv_t;

/**
 * @brief log print
 * 
 * @param[in] log_lv: log level
 * @param[in] p_color: print color
 * @param[in] p_mod: module name
 * @param[in] p_file: file
 * @param[in] p_func: function
 * @param[in] line: line
 * @param[in] p_fmt: user information
 * 
 * @return the length of the printed string on success,
 *  error code otherwise
 */
int
sirius_log_print(sirius_log_lv_t log_lv,
    const char *p_color,
    const char *p_mod,
    const char *p_file,
    const char *p_func,
    int line,
    const char *p_fmt, ...);

#ifndef SIRIUS_LOG_WRITE
#define SIRIUS_LOG_WRITE(lv, color, format, ...) \
    do { \
        sirius_log_print(lv, color, \
            LOG_MODULE_NAME, \
            SIRIUS_FILE, __FUNCTION__, __LINE__, \
            format, ##__VA_ARGS__); \
    } while (0)
#endif

#ifndef SIRIUS_PRNT
#define SIRIUS_PRNT(format, ...) \
    SIRIUS_LOG_WRITE(SIRIUS_LOG_LV_DEFAULT, LOG_NONE, \
        format, ##__VA_ARGS__)
#endif // SIRIUS_PRNT

#ifndef SIRIUS_INFO
#define SIRIUS_INFO(format, ...) \
    SIRIUS_LOG_WRITE(SIRIUS_LOG_LV_INFO, LOG_GREEN, \
        format, ##__VA_ARGS__)
#endif // SIRIUS_INFO

#ifndef SIRIUS_WARN
#define SIRIUS_WARN(format, ...) \
    SIRIUS_LOG_WRITE(SIRIUS_LOG_LV_WARN, LOG_YELLOW, \
        format, ##__VA_ARGS__)
#endif // SIRIUS_WARN

#ifndef SIRIUS_ERROR
#define SIRIUS_ERROR(format, ...) \
    SIRIUS_LOG_WRITE(SIRIUS_LOG_LV_ERROR, LOG_RED, \
        format, ##__VA_ARGS__)
#endif // SIRIUS_ERROR

#ifndef SIRIUS_DEBG
#define SIRIUS_DEBG(format, ...) \
    SIRIUS_LOG_WRITE(SIRIUS_LOG_LV_DEBG, LOG_NONE, \
        format, ##__VA_ARGS__)
#endif // SIRIUS_DEBG

#ifdef __cplusplus
}
#endif

#endif // __SIRIUS_LOG_H__
