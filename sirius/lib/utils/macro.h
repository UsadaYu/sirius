#pragma once

#include "utils/attributes.h"
#include "utils/config.h"
#include "utils/decls.h"

// --- max / min ---
#define internal_max(x, y) ((x) > (y) ? (x) : (y))
#define internal_min(x, y) ((x) < (y) ? (x) : (y))

// --- stdout / stderr ---
#ifndef STDOUT_FILENO
#  define STDOUT_FILENO (1)
#endif
#ifndef STDERR_FILENO
#  define STDERR_FILENO (2)
#endif

// --- UTILS_DPRINTF ---
#ifdef UTILS_DPRINTF
#  undef UTILS_DPRINTF
#endif

#if defined(_WIN32) || defined(_WIN64)
#  define UTILS_DPRINTF(fd, ...) \
    do { \
      char msg[sirius_log_buf_size] = {0}; \
      snprintf(msg, sizeof(msg), ##__VA_ARGS__); \
      _write(fd, msg, (unsigned int)strlen(msg)); \
    } while (0)
#else
#  define UTILS_DPRINTF dprintf
#endif

#ifdef __cplusplus

constexpr unsigned int next_power_of_2(unsigned int n) {
  if (n <= 1)
    return 2;

  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  n++;

  return n;
}

#endif
