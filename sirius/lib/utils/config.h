#pragma once

/**
 * @brief The prefix of shared memory name.
 *
 * @example
 * CFLAGS += -D_SIRIUS_SHARED_NAME_PREFIX='"$(_SIRIUS_SHARED_NAME_PREFIX)"'
 */
#ifndef _SIRIUS_SHARED_NAME_PREFIX
#  define _SIRIUS_SHARED_NAME_PREFIX "/sirius_shared_memory"
#endif

/**
 * @brief The capatity of the shared memory in the log module.
 *
 * @example
 * CFLAGS += -D_SIRIUS_LOG_SHARED_CAPACITY=$(_SIRIUS_LOG_SHARED_CAPACITY)
 */
#ifndef _SIRIUS_LOG_SHARED_CAPACITY
#  define _SIRIUS_LOG_SHARED_CAPACITY 128
#endif

/**
 * @brief The maximum number of bytes written to the file descriptor at a single
 * time.
 *
 * @example
 * CFLAGS += -D_SIRIUS_LOG_BUF_SIZE=$(_SIRIUS_LOG_BUF_SIZE)
 */
#ifndef _SIRIUS_LOG_BUF_SIZE
#  define _SIRIUS_LOG_BUF_SIZE 2048
#else
#  if _SIRIUS_LOG_BUF_SIZE > 4096
#    undef _SIRIUS_LOG_BUF_SIZE
#    define _SIRIUS_LOG_BUF_SIZE 4096
#  elif _SIRIUS_LOG_BUF_SIZE < 512
#    undef _SIRIUS_LOG_BUF_SIZE
#    define _SIRIUS_LOG_BUF_SIZE 2048
#  endif
#endif

// ---

#ifdef sirius_shared_name_prefix
#  undef sirius_shared_name_prefix
#endif
#define sirius_shared_name_prefix _SIRIUS_SHARED_NAME_PREFIX

#ifndef sirius_log_shared_capacity
#  undef sirius_log_shared_capacity
#endif
#define sirius_log_shared_capacity _SIRIUS_LOG_SHARED_CAPACITY

#ifdef sirius_log_buf_size
#  undef sirius_log_buf_size
#endif
#define sirius_log_buf_size _SIRIUS_LOG_BUF_SIZE
