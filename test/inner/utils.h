#pragma once

#ifdef _SIRIUS_WIN_CRTDBG
#  define _CRTDBG_MAP_ALLOC
#  include <crtdbg.h>
#  include <stdlib.h>
#  include <string.h>
#endif

#ifdef __cplusplus
#  include <cerrno>
#  include <cfloat>
#  include <cinttypes>
#  include <climits>
#  include <cmath>
#  include <cstdarg>
#  include <cstddef>
#  include <cstdint>
#  include <cstdio>
#  include <cstdlib>
#  include <cstring>
#  include <ctime>
#  include <iostream>
#else
#  include <errno.h>
#  include <float.h>
#  include <inttypes.h>
#  include <limits.h>
#  include <math.h>
#  include <stdarg.h>
#  include <stddef.h>
#  include <stdint.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#  include <time.h>
#endif

#if defined(__cplusplus)
#  if __cplusplus >= 201103L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L)
#    include <atomic>
#  endif
#  if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#    include <filesystem>
#  endif
#  if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#    include <format>
#  endif
#elif defined(__STDC_VERSION__)
#  if (__STDC_VERSION__ >= 202311L)
#  else
#    include <stdbool.h>
#    ifndef nullptr
#      define nullptr NULL
#    endif
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

// --- utils_dprintf ---
#if defined(_WIN32) || defined(_WIN64)
#  include <io.h>
#else
#  include <unistd.h>
#endif

static inline int utils_dprintf(int fd, const char *__restrict format, ...) {
  char msg[4096] = {0};

  va_list args;
  va_start(args, format);
  int written = vsnprintf(msg, sizeof(msg), format, args);
  va_end(args);

  if (written > 0 && (size_t)written < sizeof(msg)) {
#if defined(_WIN32) || defined(_WIN64)
    _write(fd, msg, (unsigned int)(written));
#else
    write(fd, msg, (size_t)written);
#endif
  }

  return written;
}

// --- sirius ---
#include <sirius/foundation/log.h>
#include <sirius/foundation/structor.h>
#include <sirius/foundation/sync.h>

#define UTILS_ASSERT(expr) \
  do { \
    if (ss_unlikely(!(expr))) { \
      ss_log_error("Assert: %s\n", #expr); \
      abort(); \
    } \
  } while (0)

static inline void _utils_xinit(const char *content) {
  int len = 0;
  enum { kMaxLength = 1024 };
  char buf[kMaxLength], bar_buf[kMaxLength];

  memset(buf, 0, kMaxLength);
  memset(bar_buf, 0, kMaxLength);

  len = snprintf(buf, sizeof(buf), "--- " _SIRIUS_LOG_MODULE_NAME " %s ---",
                 content);
  for (int i = 0; i < len && i < kMaxLength - 1; ++i) {
    bar_buf[i] = '-';
  }

  ss_logsp_impl(0, _SIRIUS_LOG_MODULE_NAME, "\n%s\n%s\n%s\n\n", bar_buf, buf,
                bar_buf);
}

static inline void utils_deinit() {
  _utils_xinit("Test Ended");

  ss_global_destruct();
}

static inline void utils_init() {
#ifdef _SIRIUS_WIN_CRTDBG
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
#endif

#ifdef _TEST_LOG_EXE_PATH
#  if defined(_MSC_VER)
  char *env_buf = nullptr;
  size_t env_buf_size = 0;
  if (_dupenv_s(&env_buf, &env_buf_size, _TEST_LOG_EXE_PATH) == 0 &&
      env_buf != nullptr) {
    free(env_buf);
  } else {
    _putenv_s(_SIRIUS_ENV_LOG_EXE_PATH, _TEST_LOG_EXE_PATH);
  }
#  else
  if (getenv(_SIRIUS_ENV_LOG_EXE_PATH) == nullptr) {
    ss_log_set_exe_path(_TEST_LOG_EXE_PATH);
  }
#  endif
#endif

  _utils_xinit("Test Begins");
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace utils {
class Init {
 public:
  Init() { utils_init(); }

  ~Init() { utils_deinit(); }
};
} // namespace utils
#endif
