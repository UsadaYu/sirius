#ifndef __SIRIUS_INTERNAL_LOG_H__
#define __SIRIUS_INTERNAL_LOG_H__

#include "sirius_internal_sys.h"

#include "sirius_log.h"

#ifndef STDIN_FILENO
/* 标准输入文件描述符 */
#define STDIN_FILENO    (0)
#endif

#ifndef STDOUT_FILENO
/* 标准输出文件描述符 */
#define STDOUT_FILENO   (1)
#endif

#ifndef STDERR_FILENO
/* 标准错误输出文件描述符 */
#define STDERR_FILENO   (2)
#endif

typedef struct {
    /* default log level */
    sirius_log_lv_t log_lv;

    /* pipe path */
    char *p_pipe;
} sirius_log_cr_t;

void
sirius_log_deinit();

int
sirius_log_init(const sirius_log_cr_t *p_cr);

#endif // __SIRIUS_INTERNAL_LOG_H__
