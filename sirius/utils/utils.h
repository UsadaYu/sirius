#pragma once

#include "utils/decls.h"

#ifdef __cplusplus
#  include <vector>

namespace sirius {
#endif

// --- max / min ---
#ifndef UTILS_MIN
#  define UTILS_MIN(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef UTILS_MAX
#  define UTILS_MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifdef __cplusplus
struct UTraceContext {
  std::string function_name;
  std::string file_name;
  uint32_t line;

  std::string to_string() const {
    return std::format("{0:<18} {1}", std::format("{0}:{1}", file_name, line),
                       function_name);
  }
};

class UTrace {
 public:
  explicit UTrace(std::string message,
                  std::source_location loc = std::source_location::current())
      : message_(std::move(message)) {
    add_context(loc);
  }

  void add_context(std::source_location loc = std::source_location::current()) {
    std::filesystem::path path(loc.file_name());
    trace_.push_back(
      {loc.function_name(), path.filename().string(), loc.line()});
  }

  const std::string &message() const { return message_; }
  const std::vector<UTraceContext> &trace() const { return trace_; }

  void msg_append(std::string_view message) {
    if (!message.empty()) {
      if (message.starts_with('\n')) {
        message_.append(message);
      } else {
        message_.append(std::format("\n{0}", message));
      }
    }
  }

  std::string
  join_self_all(std::source_location loc = std::source_location::current()) {
    add_context(loc);
    return join_all();
  }

  std::string join_all() const {
    std::string result = message_;
    if (!message_.empty() && !message_.ends_with('\n')) {
      result.push_back('\n');
    }
    return result.append(join_trace());
  }

  std::string join_trace() const {
    std::string result;
    for (auto it = trace_.begin(); it != trace_.end(); ++it) {
      if (it != trace_.begin()) {
        result.push_back('\n');
      }
      result.append(it->to_string());
    }
    return result;
  }

 private:
  std::string message_;
  std::vector<UTraceContext> trace_;
};

#  define utrace_return(ret) \
    do { \
      ret.error().add_context(); \
      return std::unexpected(std::move(ret.error())); \
    } while (0)

#  define utrace_transform_error(e) \
    do { \
      e.add_context(); \
      return std::move(e); \
    } while (0)

#  define utrace_transform_error_default() \
    transform_error([](UTrace &&e) { utrace_transform_error(e); })
#endif

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
constexpr size_t utils::utils::next_power_of_2(size_t n) {
#else
static inline size_t utils_next_power_of_2(size_t n) {
#endif
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
