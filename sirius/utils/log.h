#pragma once

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

// --- UTILS_STRERROR_R ---
#undef UTILS_STRERROR_R

#if defined(_WIN32) || defined(_WIN64)
#  define UTILS_STRERROR_R(error_code, buf, size) \
    strerror_s(buf, size, error_code)
#else
#  define UTILS_STRERROR_R strerror_r
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

#if defined(_WIN32) || defined(_WIN64)
static inline void utils_win_last_error(DWORD error_code, const char *msg) {
  char es[512] = {0};
  char e[_SIRIUS_LOG_BUF_SIZE] = {0};
  char schrodinger_space = (likely(msg) && msg[0] == ' ') ? '\0' : ' ';

  snprintf(es, sizeof(es), "%c%s", schrodinger_space, msg);

  DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD size = FormatMessage(flags, nullptr, error_code,
                             MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), e,
                             sizeof(e) / sizeof(TCHAR), nullptr);
  if (unlikely(size == 0)) {
    utils_dprintf(STDERR_FILENO, "Error%s: %lu\n", es, error_code);
  } else {
    utils_dprintf(STDERR_FILENO, "Error%s: %lu. %s", es, error_code, e);
  }
}
#endif

static inline void utils_errno_error(int error_code, const char *msg) {
  char es[512] = {0};
  char e[_SIRIUS_LOG_BUF_SIZE];
  char schrodinger_space = (likely(msg) && msg[0] == ' ') ? '\0' : ' ';

  snprintf(es, sizeof(es), "%c%s", schrodinger_space, msg);

  if (likely(0 == UTILS_STRERROR_R(error_code, e, sizeof(e)))) {
    utils_dprintf(STDERR_FILENO, "Error%s: %d. %s", es, error_code, e);
  } else {
    utils_dprintf(STDERR_FILENO, "Error%s: %d\n", es, error_code);
  }
}
