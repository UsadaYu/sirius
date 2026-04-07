#pragma once
/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include "sirius/foundation/log.h"
#include "utils/utils.h"

#if defined(_WIN32) || defined(_WIN64)
#  include <io.h>
#else
#  include <sys/uio.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#  ifndef ssize_t
#    define ssize_t signed long long
#  endif
#endif

/**
 * @brief ANSI Colors.
 */
#undef ANSI_NONE
#define ANSI_NONE "\033[m"
#undef ANSI_RED
#define ANSI_RED "\033[0;32;31m"
#undef ANSI_GREEN
#define ANSI_GREEN "\033[0;32;32m"
#undef ANSI_BLUE
#define ANSI_BLUE "\033[0;32;34m"
#undef ANSI_GREY
#define ANSI_GREY "\033[1;30m"
#undef ANSI_CYAN
#define ANSI_CYAN "\033[0;36m"
#undef ANSI_PURPLE
#define ANSI_PURPLE "\033[0;35m"
#undef ANSI_BROWN
#define ANSI_BROWN "\033[0;33m"
#undef ANSI_YELLOW
#define ANSI_YELLOW "\033[1;33m"
#undef ANSI_WHITE
#define ANSI_WHITE "\033[1;37m"

// --- stdin / stdout / stderr ---
#ifndef STDIN_FILENO
#  define STDIN_FILENO (0)
#endif
#ifndef STDOUT_FILENO
#  define STDOUT_FILENO (1)
#endif
#ifndef STDERR_FILENO
#  define STDERR_FILENO (2)
#endif

// --- utils_write ---
static ss_force_inline ssize_t utils_write(int fd, const void *buffer,
                                           size_t size) {
#if defined(_WIN32) || defined(_WIN64)
  return (ssize_t)_write(fd, buffer, (unsigned int)(size));
#else
  return write(fd, buffer, size);
#endif
}

// --- utils_dprintf ---
static inline int utils_dprintf(int fd, const char *__restrict format, ...) {
  char msg[_SIRIUS_LOG_BUF_SIZE] = {0};

  va_list args;
  va_start(args, format);
  int written = vsnprintf(msg, sizeof(msg), format, args);
  va_end(args);

  if (ss_likely(written > 0 && (size_t)written < sizeof(msg))) {
    utils_write(fd, msg, written);
  }

  return written;
}
