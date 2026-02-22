#pragma once

#include "utils/attributes.h"
#include "utils/config.h"
#include "utils/decls.h"

// --- max / min ---
#ifndef UTILS_MIN
#  define UTILS_MIN(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef UTILS_MAX
#  define UTILS_MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

// --- UTILS_LOCALTIME_R ---
#undef UTILS_LOCALTIME_R

#if defined(_WIN32) || defined(_WIN64)
#  define UTILS_LOCALTIME_R(timer, result) localtime_s(result, timer)
#else
#  define UTILS_LOCALTIME_R(timer, result) localtime_r(timer, result)
#endif

static inline size_t utils_string_length_check(const char *string,
                                               size_t max_len) {
  if (!string)
    return 0;

  size_t len = 0;
  while (len < max_len) {
    if (string[len] == '\0')
      break;
    ++len;
  }

  return likely(len < max_len) ? len : max_len;
}

#ifdef __cplusplus
namespace Utils {
namespace Utils {
template<typename T>
concept IntegralOrEnum = std::integral<T> || std::is_enum_v<T>;

constexpr size_t next_power_of_2(size_t n) {
  if (n <= 1)
    return 2;

  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
#  if defined(__SIZEOF_SIZE_T__) && __SIZEOF_SIZE_T__ == 8
  n |= n >> 32;
#  endif
  n++;

  return n;
}
} // namespace Utils
} // namespace Utils
#else
static inline size_t utils_next_power_of_2(size_t n) {
  if (n <= 1)
    return 2;

  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
#  if defined(__SIZEOF_SIZE_T__) && __SIZEOF_SIZE_T__ == 8
  n |= n >> 32;
#  endif
  n++;

  return n;
}
#endif
