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

// --- utils_write ---
static force_inline
#if defined(_WIN32) || defined(_WIN64)
  signed long long
#else
  ssize_t
#endif
  utils_write(int fd, const void *buffer, size_t size) {
#if defined(_WIN32) || defined(_WIN64)
  return (signed long long)_write(fd, buffer, (unsigned int)(size));
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

  if (likely(written > 0 && (size_t)written < sizeof(msg))) {
    utils_write(fd, msg, written);
  }

  return written;
}
