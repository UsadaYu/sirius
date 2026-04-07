#pragma once

// --- _SIRIUS_ENV_LOG_EXE_PATH ---
#undef _SIRIUS_ENV_LOG_EXE_PATH

/**
 * @brief The path of the log executables.
 */
#define _SIRIUS_ENV_LOG_EXE_PATH "SIRIUS_ENV_LOG_EXE_PATH"

/**
 * @brief Custom log module name.
 *
 * @example
 * - (1) CFLAGS += -D_SIRIUS_LOG_MODULE_NAME='"$(_SIRIUS_LOG_MODULE_NAME)"'
 */
#ifndef _SIRIUS_LOG_MODULE_NAME
#  define _SIRIUS_LOG_MODULE_NAME "unknown"
#endif

/**
 * @brief Compile macro, level: 0, 1, 2, 3, 4.
 * The default log printing level.
 *
 * @example
 * - (1) CFLAGS += -D_SIRIUS_LOG_LEVEL=2
 */
#ifndef _SIRIUS_LOG_LEVEL
#  define _SIRIUS_LOG_LEVEL 3
#endif
