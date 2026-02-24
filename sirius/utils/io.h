#pragma once

#include "sirius/foundation/log.h"
#include "sirius/foundation/thread.h"
#include "utils/utils.h"

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

// --- UTILS_WRITE ---
#undef UTILS_WRITE

#if defined(_WIN32) || defined(_WIN64)
#  define UTILS_WRITE(fd, buffer, size) _write(fd, buffer, (unsigned int)(size))
#else
#  define UTILS_WRITE(fd, buffer, size) write(fd, buffer, size);
#endif

// --- utils_dprintf ---
static inline int utils_dprintf(int fd, const char *__restrict format, ...) {
  char msg[_SIRIUS_LOG_BUF_SIZE] = {0};

  va_list args;
  va_start(args, format);
  int written = vsnprintf(msg, sizeof(msg), format, args);
  va_end(args);

  if (likely(written > 0 && (size_t)written < sizeof(msg))) {
    UTILS_WRITE(fd, msg, written);
  }

  return written;
}
