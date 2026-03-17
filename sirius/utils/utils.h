#pragma once
/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#ifdef __cplusplus
#  include "utils/trace.hpp"

namespace sirius {
#endif

// --- max / min ---
#undef UTILS_MIN
#define UTILS_MIN(x, y) ((x) < (y) ? (x) : (y))
#undef UTILS_MAX
#define UTILS_MAX(x, y) ((x) > (y) ? (x) : (y))

// --- utils_localtime_r ---
static inline void utils_localtime_r(const time_t *__restrict timer,
                                     struct tm *__restrict result) {
#if defined(_WIN32) || defined(_WIN64)
  localtime_s(result, timer);
#else
  localtime_r(timer, result);
#endif
}

// --- utils_strnlen_s ---
static inline size_t utils_strnlen_s(const char *string, size_t max_len) {
#if defined(__STDC_LIB_EXT1__)
  return strnlen_s(string, max_len);
#elif defined(_GNU_SOURCE) || \
  (defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200809L))
  return string ? strnlen(string, max_len) : 0;
#else
  size_t len = 0;
  if (string) {
    while (len < max_len && string[len]) {
      ++len;
    }
  }
  return len;
#endif
}

#ifdef __cplusplus
namespace utils {
namespace utils {
template <typename T>
concept IntegralOrEnum = std::integral<T> || std::is_enum_v<T>;

constexpr size_t next_power_of_2(size_t n);
} // namespace utils
} // namespace utils
#endif

#ifdef __cplusplus
constexpr size_t utils::utils::next_power_of_2(size_t n)
#else
static inline size_t utils_next_power_of_2(size_t n)
#endif
{
  if (n <= 1)
    return 2;

  --n;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
#if defined(__SIZEOF_SIZE_T__) && __SIZEOF_SIZE_T__ == 8
  n |= n >> 32;
#endif
  ++n;

  return n;
}

#ifdef __cplusplus
} // namespace sirius
#endif
