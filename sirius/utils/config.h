#pragma once

/**
 * @brief The namespace of `sirius`.
 *
 * @example
 * CFLAGS += -D_SIRIUS_NAMESPACE='"$(_SIRIUS_NAMESPACE)"'
 */
#ifndef _SIRIUS_NAMESPACE
#  define _SIRIUS_NAMESPACE "sirius"
#endif

/**
 * @brief On POSIX, the permissions of the created files.
 *
 * @example
 * CFLAGS += -D_SIRIUS_POSIX_FILE_MODE='"$(_SIRIUS_POSIX_FILE_MODE)"'
 */
#ifndef _SIRIUS_POSIX_FILE_MODE
#  define _SIRIUS_POSIX_FILE_MODE "0775"
#endif

/**
 * @brief On POSIX, the temporary directory for the files.
 *
 * @example
 * CFLAGS += -D_SIRIUS_POSIX_TMP_DIR='"$(_SIRIUS_POSIX_TMP_DIR)"'
 */
#ifndef _SIRIUS_POSIX_TMP_DIR
#  define _SIRIUS_POSIX_TMP_DIR "/var/run/"
#endif

/**
 * @brief User-defined key.
 *
 * @example
 * CFLAGS += -D_SIRIUS_USER_KEY='"$(_SIRIUS_USER_KEY)"'
 */
#ifndef _SIRIUS_USER_KEY
#  define _SIRIUS_USER_KEY "d32d87be6fe35062a7945ffd4f4a69d4"
#endif

/**
 * @brief The capatity of the shared memory in the log module.
 *
 * @example
 * CFLAGS += -D_SIRIUS_LOG_SHM_CAPACITY=$(_SIRIUS_LOG_SHM_CAPACITY)
 */
#ifndef _SIRIUS_LOG_SHM_CAPACITY
#  define _SIRIUS_LOG_SHM_CAPACITY 512
#endif

/**
 * @brief The maximum number of bytes written to the file descriptor at a single
 * time.
 *
 * @example
 * CFLAGS += -D_SIRIUS_LOG_BUF_SIZE=$(_SIRIUS_LOG_BUF_SIZE)
 */
#ifndef _SIRIUS_LOG_BUF_SIZE
#  define _SIRIUS_LOG_BUF_SIZE 4096
#else
#  if _SIRIUS_LOG_BUF_SIZE > 40960
#    undef _SIRIUS_LOG_BUF_SIZE
#    define _SIRIUS_LOG_BUF_SIZE 40960
#  elif _SIRIUS_LOG_BUF_SIZE < 512
#    undef _SIRIUS_LOG_BUF_SIZE
#    define _SIRIUS_LOG_BUF_SIZE 2048
#  endif
#endif

/**
 * @brief The directory of executables.
 *
 * @example
 * CFLAGS += -D_SIRIUS_EXE_DIR='"$(_SIRIUS_EXE_DIR)"'
 */
#ifndef _SIRIUS_EXE_DIR
#  define _SIRIUS_EXE_DIR "/usr/bin"
#endif

/**
 * @brief The name of the daemon executable file.
 *
 * @example
 * CFLAGS += -D_SIRIUS_EXE_DAEMON_NAME='"$(_SIRIUS_EXE_DAEMON_NAME)"'
 */
#ifndef _SIRIUS_EXE_DAEMON_NAME
#  define _SIRIUS_EXE_DAEMON_NAME _SIRIUS_NAMESPACE "_daemon"
#endif
