#pragma once

#include "internal/config.h"

#if defined(__cplusplus)
#  if defined(_MSVC_LANG)
#    define _internal_cxx_std _MSVC_LANG
#  else
#    define _internal_cxx_std __cplusplus
#  endif
#endif

#ifdef sirius_test_win_crtdbg
#  define _CRTDBG_MAP_ALLOC
#  include <crtdbg.h>
#  include <stdlib.h>
#  include <string.h>
#endif

#if defined(__cplusplus)
#  include <atomic>
#  include <cassert>
#  include <chrono>
#  include <cinttypes>
#  include <cstdio>
#  include <cstdlib>
#  include <cstring>
#  include <iostream>
#  include <string>
#  include <vector>

#  if _internal_cxx_std < 201703L
#    error "The test module requires a standard of `c++17` or higher"
#  else
#    include <filesystem>
#  endif

#  if _internal_cxx_std < 202002L
#    error "The test module requires a standard of `c++20` or higher"
#  else
#    include <format>
#  endif

#endif

#ifdef __cplusplus

#else

#  ifdef __STDC_VERSION__
#    if __STDC_VERSION__ < 202311L
#      include <stdbool.h>
#      ifndef nullptr
#        define nullptr NULL
#      endif
#    endif
#  endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- sirius::utils ---
#include <sirius/cpu.h>
#include <sirius/utils/log.h>

#if (defined(_WIN32) || defined(_WIN64)) && defined(_MSC_VER)
#  include <io.h>

#  define utils_dprintf(fd, ...) \
    do { \
      char msg[4096] = {0}; \
      snprintf(msg, sizeof(msg), ##__VA_ARGS__); \
      _write(fd, msg, (unsigned int)strlen(msg)); \
    } while (0)

#else

#  include <unistd.h>

#  define utils_dprintf(fd, ...) \
    do { \
      char msg[4096]; \
      snprintf(msg, sizeof(msg), ##__VA_ARGS__); \
      write(fd, msg, strlen(msg)); \
    } while (0)

#endif

#define utils_assert(expr) \
  do { \
    if (unlikely(!(expr))) { \
      sirius_error("Assert: %s\n", #expr); \
      abort(); \
    } \
  } while (0)

static inline void _utils_xinit(const char *content) {
  int len = 0;
  enum { MAX_LEN = 1024 };
  char buf[MAX_LEN], bar_buf[MAX_LEN];

  memset(buf, 0, MAX_LEN);
  memset(bar_buf, 0, MAX_LEN);

  len = snprintf(buf, sizeof(buf), "--- " sirius_log_module_name " %s ---",
                 content);
  for (int i = 0; i < len && i < MAX_LEN - 1; i++) {
    bar_buf[i] = '-';
  }

  utils_dprintf(1, "\n%s\n%s\n%s\n\n", bar_buf, buf, bar_buf);
}

static inline void utils_deinit() {
  _utils_xinit("test ended");
}

static inline void utils_init() {
#ifdef sirius_test_win_crtdbg
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
#endif

  _utils_xinit("test begins");
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

namespace Utils {
class Init {
 public:
  Init() {
    utils_init();
  }
  ~Init() {
    utils_deinit();
  }
};
} // namespace Utils

#endif
