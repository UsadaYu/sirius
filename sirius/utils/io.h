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

#ifdef __cplusplus
namespace Utils {
#  if defined(_WIN32) || defined(_WIN64)
inline int strerror_unify(int error_code, char *buffer, size_t buffer_size) {
  return strerror_s(buffer, buffer_size, error_code);
}
#  else
inline int T_strerror_unify(int ret, [[maybe_unused]] char *buffer) {
  return ret;
}

inline int T_strerror_unify(char *ret, char *buffer) {
  if (ret != buffer) {
    strncpy(buffer, ret, utils_string_length_check(ret, _SIRIUS_LOG_BUF_SIZE));
  }

  return 0;
}

inline int strerror_unify(int error_code, char *buffer, size_t buffer_size) {
  return T_strerror_unify(strerror_r(error_code, buffer, buffer_size), buffer);
}
#  endif

class Io {
 private:
  static inline constexpr int64_t kPrefixLength = 55;

  Io() : ansi_enable_out_(true), ansi_enable_err_(true) {}

  ~Io() = default;

 public:
  Io(const Io &) = delete;
  Io &operator=(const Io &) = delete;

  static Io &io() {
    static Io instance;

    return instance;
  }

  static std::string s_pre(const char *prefix, const char *module,
                           const std::string &file, int line = 0) {
    time_t raw_time;
    struct tm tm_info;
    time(&raw_time);
    UTILS_LOCALTIME_R(&raw_time, &tm_info);

    std::string f_str = back_strip(file);
    std::string f_l = f_str.empty() ? "" : std::format("{0}:{1}", f_str, line);

    std::string result = std::format(
      "{0:5} [{1:02d}:{2:02d}:{3:02d} {4} {5}] {6}", prefix, tm_info.tm_hour,
      tm_info.tm_min, tm_info.tm_sec, module, sirius_thread_id(), f_l);

    int64_t result_size = static_cast<int64_t>(result.size());
    if (result_size >= 0 && result_size < kPrefixLength) {
      result.append(kPrefixLength - result_size, '-');
    } else {
      result.append("\n  ");
    }

    result.append(" > ");

    return result;
  }

  std::string s_error(const std::string &f, int l = 0,
                      const char *m = _SIRIUS_LOG_MODULE_NAME) {
    return s_gen("Error", f, l, LOG_RED, ansi_enable_err_, m);
  }

  std::string s_warn(const std::string &f, int l = 0,
                     const char *m = _SIRIUS_LOG_MODULE_NAME) {
    return s_gen("Warn", f, l, LOG_YELLOW, ansi_enable_err_, m);
  }

  std::string s_info(const std::string &f, int l = 0,
                     const char *m = _SIRIUS_LOG_MODULE_NAME) {
    return s_gen("Info", f, l, LOG_GREEN, ansi_enable_out_, m);
  }

  std::string s_debug(const std::string &f, int l = 0,
                      const char *m = _SIRIUS_LOG_MODULE_NAME) {
    return s_gen("Debug", f, l, LOG_COLOR_NONE, ansi_enable_out_, m);
  }

 public:
#  if defined(_WIN32) || defined(_WIN64)
  static std::string win_last_error(const DWORD error_code,
                                    const std::string &function) {
    std::string line1 = std::format("{0} `{1}` (error code: {2})",
                                    io().s_error(""), function, error_code);

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
        line2.resize(size_needed);
        WideCharToMultiByte(CP_UTF8, 0, wbuffer, (int)ret, &line2[0],
                            size_needed, nullptr, nullptr);
      }

      LocalFree(wbuffer);

      line2 = row("{0}", back_strip(line2));
      return line1 + line2;
    }
  }
#  endif

  static std::string errno_error(const int error_code,
                                 const std::string &function) {
    std::string line1 = std::format("{0} `{1}` (error code: {2})",
                                    io().s_error(""), function, error_code);

    char buffer[_SIRIUS_LOG_BUF_SIZE];
    if (0 == strerror_unify(error_code, buffer, sizeof(buffer))) [[likely]] {
      std::string line2 = row("{0}", back_strip(std::string(buffer)));
      return line1 + line2;
    } else {
      return line1;
    }
  }

  template<typename... Args>
  static std::string row(std::format_string<Args...> fmt, Args &&...args) {
    std::string result = std::format(fmt, std::forward<Args>(args)...);

    if (!result.empty()) {
      return std::format("\n  > {}", result);
    }

    return "";
  }

  static std::string row() {
    return "";
  }

  void ansi_enable(bool enable_out, bool enable_err) {
    ansi_enable_out_.store(enable_out, std::memory_order_relaxed);
    ansi_enable_err_.store(enable_err, std::memory_order_relaxed);
  }

 private:
  std::atomic<bool> ansi_enable_out_;
  std::atomic<bool> ansi_enable_err_;

  static std::string back_strip(const std::string &string) {
    std::string result = string;

    while (!result.empty() &&
           (result.back() == '\r' || result.back() == '\n' ||
            result.back() == '\t' || result.back() == ' ' ||
            result.back() == '.')) {
      result.pop_back();
    }

    return result;
  }

  static std::string s_gen(const char *prefix, const std::string &file,
                           int line, const std::string &color,
                           const std::atomic<bool> &ansi_enable,
                           const char *module) {
    if (ansi_enable.load(std::memory_order_relaxed)) {
      return color + s_pre(prefix, module, file, line) + LOG_COLOR_NONE;
    }

    return s_pre(prefix, module, file, line);
  }
};
} // namespace Utils
#endif
