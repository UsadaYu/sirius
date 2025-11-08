#ifndef SIRIUS_TEST_H
#define SIRIUS_TEST_H

#ifdef __cplusplus
#  include <chrono>
#  include <cmath>
#  include <cstdlib>
#  include <filesystem>
#  include <format>
#  include <fstream>
#  include <iomanip>
#  include <iostream>
#  include <map>
#  include <memory>
#  include <random>
#  include <sstream>
#  include <stdexcept>
#  include <string>
#  include <thread>
#  include <vector>
#else
#  ifdef __STDC_VERSION__
#    if (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMICS__)
#      include <stdatomic.h>
#    endif

#    if (__STDC_VERSION__ >= 202311L)
#    else
#      include <stdbool.h>
#      define nullptr NULL
#    endif
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sirius/sirius_log.h"
#include "sirius/sirius_time.h"

#ifdef _WIN32
#  include <io.h>
#  include <Windows.h>
#  define t_dprintf(fd, ...) \
    do { \
      char msg[1024]; \
      snprintf(msg, sizeof(msg), ##__VA_ARGS__); \
      _write(fd, msg, (unsigned int)strlen(msg)); \
    } while (0)
#else
#  define t_dprintf dprintf
#endif

#define t_assert(expr) \
  do { \
    if (unlikely(!(expr))) { \
      sirius_error("assert: %s\n", #expr); \
      abort(); \
    } \
  } while (0)

#ifdef __cplusplus
}
#endif

#endif // SIRIUS_TEST_H
