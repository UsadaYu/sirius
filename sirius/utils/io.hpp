#pragma once

#include "sirius/foundation/log.h"
#include "sirius/foundation/thread.h"
#include "utils/io.h"

namespace Utils {
// --- utils_strerror ---
#if defined(_WIN32) || defined(_WIN64)
inline int utils_strerror(int error_code, char *buffer, size_t buffer_size) {
  return strerror_s(buffer, buffer_size, error_code);
}
#else
inline int T_utils_strerror(int ret, [[maybe_unused]] char *buffer) {
  return ret;
}

inline int T_utils_strerror(char *ret, char *buffer) {
  if (ret != buffer) {
    size_t buffer_size = utils_strnlen_s(ret, _SIRIUS_LOG_BUF_SIZE);

    if (buffer_size <= 0 || buffer_size >= _SIRIUS_LOG_BUF_SIZE) [[unlikely]] {
      return EINVAL;
    }

    std::memcpy(buffer, ret, buffer_size);
    buffer[buffer_size] = '\0';
  }

  return 0;
}

inline int utils_strerror(int error_code, char *buffer, size_t buffer_size) {
  return T_utils_strerror(strerror_r(error_code, buffer, buffer_size), buffer);
}
#endif

class Io {
 public:
  static inline constexpr int64_t kPrefixLength = 55;

 private:
  Io() : ansi_enable_out_(true), ansi_enable_err_(true) {}

  ~Io() = default;

 public:
  Io(const Io &) = delete;
  Io &operator=(const Io &) = delete;
  Io(Io &&) = delete;
  Io &operator=(Io &&) = delete;

  static Io &io() {
    static Io instance;

    return instance;
  }

  template<typename... Args>
  static std::string row(std::string_view prefix,
                         std::format_string<Args...> fmt, Args &&...args) {
    std::string input =
      back_strip(std::format(fmt, std::forward<Args>(args)...));

    if (input.empty())
      return std::string {};

#if 1
    constexpr size_t lines = 4;
#else
    size_t lines = 1;
    for (char c : input) {
      if (c == '\n') {
        ++lines;
      }
    }
#endif

    std::string result;
    result.reserve(input.size() + lines * prefix.size());

    const char *begin = input.c_str();
    const char *ptr = begin;
    const char *end = begin + input.size();

    while (ptr < end) {
      const char *line_start = ptr;
      while (ptr < end && *ptr != '\n') {
        ++ptr;
      }

      if (ptr != line_start) {
        result.append(prefix);
      }

      result.append(line_start, ptr - line_start);

      if (ptr < end) {
        result.push_back('\n');
        ++ptr;
      }
    }

    return result;
  }

  template<typename... Args>
  static std::string row_gs(std::format_string<Args...> fmt, Args &&...args) {
    return row(" > ", fmt, std::forward<Args>(args)...);
  }

  static std::string s_pre(std::string_view prefix, std::string_view module,
                           std::string_view file, int line = 0) {
    time_t raw_time;
    struct tm tm_info;
    time(&raw_time);
    utils_localtime_r(&raw_time, &tm_info);

    std::string f_str = back_strip(file);
    std::string f_pos =
      f_str.empty() ? "" : std::format("{0}:{1} ", f_str, line);

    std::string result;
    result.reserve(kPrefixLength + 16);

    result = std::format("{0:5} [{1:02d}:{2:02d}:{3:02d} {4} {5}] {6}", prefix,
                         tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec,
                         module, sirius_thread_id(), f_pos);

    int64_t result_size = static_cast<int64_t>(result.size());
    if (result_size >= 0 && result_size < kPrefixLength) {
      result.append(kPrefixLength - result_size, '-');
    }

    return result;
  }

  std::string s_error(std::string_view f, int l = 0,
                      std::string_view m = _SIRIUS_LOG_MODULE_NAME) {
    return s_gen("Error", f, l, LOG_RED, ansi_enable_err_, m);
  }

  std::string s_warn(std::string_view f, int l = 0,
                     std::string_view m = _SIRIUS_LOG_MODULE_NAME) {
    return s_gen("Warn", f, l, LOG_YELLOW, ansi_enable_err_, m);
  }

  std::string s_info(std::string_view f, int l = 0,
                     std::string_view m = _SIRIUS_LOG_MODULE_NAME) {
    return s_gen("Info", f, l, LOG_GREEN, ansi_enable_out_, m);
  }

  std::string s_debug(std::string_view f, int l = 0,
                      std::string_view m = _SIRIUS_LOG_MODULE_NAME) {
    return s_gen("Debug", f, l, LOG_COLOR_NONE, ansi_enable_out_, m);
  }

#if defined(_WIN32) || defined(_WIN64)
  static std::string win_last_error(const DWORD error_code,
                                    std::string_view func) {
    std::string line1 = io().s_error("").append(
      row_gs("`{0}` (error code: {1})", func, error_code));

    wchar_t *wbuffer = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD ret = FormatMessageW(
      flags, nullptr, error_code, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
      reinterpret_cast<wchar_t *>(&wbuffer), 0, nullptr);
    if (ret == 0 || !wbuffer) [[unlikely]] {
      return line1;
    } else {
      std::string line2;
      int size_needed = WideCharToMultiByte(CP_UTF8, 0, wbuffer, (int)ret,
                                            nullptr, 0, nullptr, nullptr);
      if (size_needed > 0) {
        line2.reserve(size_needed + 16);
        WideCharToMultiByte(CP_UTF8, 0, wbuffer, (int)ret, &line2[0],
                            size_needed, nullptr, nullptr);
      }

      LocalFree(wbuffer);

      line2 = row_gs("\n{0}", line2);
      return line1 + line2;
    }
  }
#endif

  static std::string errno_error(const int error_code, std::string_view func) {
    std::string line1 = io().s_error("").append(
      row_gs("`{0}` (error code: {1})", func, error_code));

    char buffer[_SIRIUS_LOG_BUF_SIZE];
    if (0 == utils_strerror(error_code, buffer, sizeof(buffer))) [[likely]] {
      std::string line2 = row_gs("\n{0}", buffer);
      return line1 + line2;
    } else {
      return line1;
    }
  }

  void ansi_enable(bool enable_out, bool enable_err) {
    ansi_enable_out_.store(enable_out, std::memory_order_relaxed);
    ansi_enable_err_.store(enable_err, std::memory_order_relaxed);
  }

 private:
  std::atomic<bool> ansi_enable_out_;
  std::atomic<bool> ansi_enable_err_;

  static std::string back_strip(std::string_view string) {
    auto pos = string.find_last_not_of("\r\n\t .");
    if (pos == std::string_view::npos) {
      return std::string {};
    }

    return std::string(string.substr(0, pos + 1));
  }

  static std::string s_gen(std::string_view prefix, std::string_view file,
                           int line, std::string_view color,
                           const std::atomic<bool> &ansi_enable,
                           std::string_view module) {
    if (ansi_enable.load(std::memory_order_relaxed)) {
      return std::string(color)
        .append(s_pre(prefix, module, file, line))
        .append(LOG_COLOR_NONE);
    }

    return s_pre(prefix, module, file, line);
  }
};
} // namespace Utils

#define IO_ERROR(fmt, ...) \
  (::Utils::Io::io() \
     .s_error(SIRIUS_FILE_NAME, __LINE__) \
     .append((::Utils::Io::row_gs(fmt, ##__VA_ARGS__))))

#define IO_WARNSP(fmt, ...) \
  (::Utils::Io::io().s_warn("").append( \
    (::Utils::Io::row_gs(fmt, ##__VA_ARGS__))))

#define IO_INFOSP(fmt, ...) \
  (::Utils::Io::io().s_info("").append( \
    (::Utils::Io::row_gs(fmt, ##__VA_ARGS__))))
