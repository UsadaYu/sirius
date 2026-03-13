#include "utils/decls.h"
#pragma once

#include "sirius/foundation/log.h"
#include "sirius/foundation/thread.h"
#include "utils/attributes.h"
#include "utils/io.h"

namespace sirius {
namespace utils {
inline void io_ln_fd(int fd, std::string_view msg) {
#if defined(_WIN32) || defined(_WIN64)
  auto imsg = std::format("{0}\n", msg);
  utils_write(fd, imsg.c_str(), imsg.size());
#else
  struct iovec iov[2];
  iov[0].iov_base = const_cast<char *>(msg.data());
  iov[0].iov_len = msg.size();
  iov[1].iov_base = const_cast<char *>("\n");
  iov[1].iov_len = 1;
  writev(fd, iov, 2);
#endif
}

// clang-format off
inline void io_msg_outln(std::string_view msg) { io_ln_fd(STDOUT_FILENO, msg); }
inline void io_msg_errln(std::string_view msg) { io_ln_fd(STDERR_FILENO, msg); }
inline void io_msg_out(std::string_view msg) { utils_write(STDOUT_FILENO, msg.data(), msg.size()); }
inline void io_msg_err(std::string_view msg) { utils_write(STDERR_FILENO, msg.data(), msg.size()); }
// clang-format on

// --- utils_strerror ---
#if defined(_WIN32) || defined(_WIN64)
inline int utils_strerror(int err_code, char *buffer, size_t buffer_size) {
  return strerror_s(buffer, buffer_size, err_code);
}
#else
namespace inner {
inline int utils_strerror(int ret, [[maybe_unused]] char *buffer) {
  return ret;
}

inline int utils_strerror(char *ret, char *buffer) {
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
} // namespace inner

inline int utils_strerror(int err_code, char *buffer, size_t buffer_size) {
  return inner::utils_strerror(strerror_r(err_code, buffer, buffer_size),
                               buffer);
}
#endif

class Io {
 public:
  static inline constexpr size_t kPrefixLength = 55;

 private:
  Io() = default;

  ~Io() = default;

 public:
  Io(const Io &) = delete;
  Io &operator=(const Io &) = delete;
  Io(Io &&) = delete;
  Io &operator=(Io &&) = delete;

  static Io &instance() {
    static Io instance;
    return instance;
  }

  template <typename... Args>
  static std::string row(std::string_view prefix,
                         std::format_string<Args...> fmt, Args &&...args) {
    std::string input =
      back_strip(std::format(fmt, std::forward<Args>(args)...));

    if (input.empty())
      return std::string {};

    size_t lines = 1;
    for (char c : input) {
      if (c == '\n') {
        ++lines;
      }
    }
    std::string result;
    result.reserve(input.size() + lines * prefix.size() + 4);

    const char *begin = input.c_str();
    const char *ptr = begin;
    const char *end = begin + input.size();
    while (ptr < end) {
      const char *line_start = ptr;
      while (ptr < end && *ptr != '\n') {
        ++ptr;
      }

      result.append(prefix);
      result.append(line_start, ptr - line_start);

      if (ptr < end) {
        result.push_back('\n');
        ++ptr;
      }
    }

    return result;
  }

  template <typename... Args>
  static std::string row_gs(std::format_string<Args...> fmt, Args &&...args) {
    return row(" > ", fmt, std::forward<Args>(args)...);
  }

  static std::string s_pre(std::string_view prefix, std::string_view module,
                           std::string_view file, int line = 0) {
    std::string inner;
    std::string result;
    inner.reserve(kPrefixLength + 16);
    result.reserve(kPrefixLength + 2);

    time_t raw_time;
    struct tm tm_info;
    std::time(&raw_time);
    utils_localtime_r(&raw_time, &tm_info);
    std::string f_str = back_strip(file);
    std::string f_pos =
      f_str.empty() ? "" : std::format("{0}:{1} ", f_str, line);
    inner = std::format("{0:5} [{1:02d}:{2:02d}:{3:02d} {4} {5}] {6}", prefix,
                        tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, module,
                        sirius_thread_id(), f_pos);
    result =
      std::vformat("{0:-<{1}}", std::make_format_args(inner, kPrefixLength));
    return result;
  }

  // clang-format off
  std::string s_error(std::string_view f, int l = 0, std::string_view m = _SIRIUS_LOG_MODULE_NAME) { return s_gen("Error", f, l, LOG_RED, err_ansi_enable_, m); }
  std::string s_warn (std::string_view f, int l = 0, std::string_view m = _SIRIUS_LOG_MODULE_NAME) { return s_gen("Warn", f, l, LOG_YELLOW, err_ansi_enable_, m); }
  std::string s_info (std::string_view f, int l = 0, std::string_view m = _SIRIUS_LOG_MODULE_NAME) { return s_gen("Info", f, l, LOG_GREEN, out_ansi_enable_, m); }
  std::string s_debug(std::string_view f, int l = 0, std::string_view m = _SIRIUS_LOG_MODULE_NAME) { return s_gen("Debug", f, l, LOG_COLOR_NONE, out_ansi_enable_, m); }
  // clang-format on

#if defined(_WIN32) || defined(_WIN64)
  static std::string win_err_str(const DWORD err_code) {
    wchar_t *wbuffer = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD ret = FormatMessageW(
      flags, nullptr, err_code, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
      reinterpret_cast<wchar_t *>(&wbuffer), 0, nullptr);
    if (ret == 0 || !wbuffer)
      return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wbuffer, (int)ret,
                                          nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0)
      return {};

    std::string err_msg;
    err_msg.resize(size_needed);
    int byte_count =
      WideCharToMultiByte(CP_UTF8, 0, wbuffer, static_cast<int>(ret),
                          &err_msg[0], size_needed, nullptr, nullptr);
    LocalFree(wbuffer);
    if (byte_count == 0)
      return {};
    return back_strip(err_msg);
  }
#endif

  static std::string errno_err_str(const int err_code) {
    char buffer[_SIRIUS_LOG_BUF_SIZE];
    if (0 == utils_strerror(err_code, buffer, sizeof(buffer)))
      return back_strip(std::format("{0}", buffer));
    return {};
  }

#if defined(_WIN32) || defined(_WIN64)
  static std::string win_err(const DWORD err_code, std::string_view fn_str) {
    return err_impl(err_code, fn_str, win_err_str);
  }

  template <typename... Args>
  static std::string win_err(const DWORD err_code, std::string_view fn_str,
                             std::format_string<Args...> fmt, Args &&...args) {
    return err_impl(err_code, fn_str, win_err_str, fmt,
                    std::forward<Args>(args)...);
  }
#endif

  static std::string errno_err(const int err_code, std::string_view fn_str) {
    return err_impl(err_code, fn_str, errno_err_str);
  }

  template <typename... Args>
  static std::string errno_err(const int err_code, std::string_view fn_str,
                               std::format_string<Args...> fmt,
                               Args &&...args) {
    return err_impl(err_code, fn_str, errno_err_str, fmt,
                    std::forward<Args>(args)...);
  }

  void ansi_enable(bool out_enable, bool err_enable) {
    out_ansi_enable_.store(out_enable, std::memory_order_relaxed);
    err_ansi_enable_.store(err_enable, std::memory_order_relaxed);
  }

 private:
  std::atomic<bool> out_ansi_enable_ = true;
  std::atomic<bool> err_ansi_enable_ = true;

  static std::string back_strip(std::string_view sv) {
    auto it = std::find_if(sv.rbegin(), sv.rend(), [](unsigned char ch) {
      return !std::isspace(ch) && ch != '\0';
    });
    return it == sv.rend() ? std::string {}
                           : std::string(sv.begin(), it.base());
  }

  template <typename T, typename FnPtr>
  static std::string err_impl(T err_code, std::string_view fn_str,
                              FnPtr fn_ptr) {
    std::string pre = std::format("`{0}` (error code: {1})", fn_str, err_code);
    std::string err_msg = fn_ptr(err_code);
    if (!err_msg.empty()) {
      err_msg.insert(err_msg.begin(), '\n');
    }
    return pre.append(err_msg);
  }

  template <typename T, typename FnPtr, typename... Args>
  static std::string err_impl(T err_code, std::string_view fn_str, FnPtr fn_ptr,
                              std::format_string<Args...> fmt, Args &&...args) {
    std::string usr_msg =
      back_strip(std::format(fmt, std::forward<Args>(args)...));
    if (!usr_msg.empty() && !usr_msg.starts_with('\n')) {
      usr_msg.insert(usr_msg.begin(), '\n');
    }
    std::string pre = std::format("`{0}` (error code: {1})", fn_str, err_code);
    std::string err_msg = fn_ptr(err_code);
    if (!err_msg.empty()) {
      err_msg.insert(err_msg.begin(), '\n');
    }
    return pre.append(err_msg).append(usr_msg);
  }

  static std::string s_gen(std::string_view prefix, std::string_view file,
                           int line, std::string_view color,
                           const std::atomic<bool> &ansi_enable,
                           std::string_view module) {
    std::string result;
    if (ansi_enable.load(std::memory_order_relaxed)) {
      result.reserve(kPrefixLength + 16 + sizeof(LOG_COLOR_NONE));
      result = std::string(color)
                 .append(s_pre(prefix, module, file, line))
                 .append(LOG_COLOR_NONE);
    } else {
      result.reserve(kPrefixLength + 8);
      result = s_pre(prefix, module, file, line);
    }
    return result;
  }
};
} // namespace utils
} // namespace sirius

#define io_str_error(fmt, ...) \
  (::sirius::utils::Io::instance() \
     .s_error(SIRIUS_FILE_NAME, __LINE__) \
     .append(::sirius::utils::Io::row_gs(fmt, ##__VA_ARGS__)))
#define io_str_warnsp(fmt, ...) \
  (::sirius::utils::Io::instance().s_warn("").append( \
    ::sirius::utils::Io::row_gs(fmt, ##__VA_ARGS__)))
#define io_str_infosp(fmt, ...) \
  (::sirius::utils::Io::instance().s_info("").append( \
    ::sirius::utils::Io::row_gs(fmt, ##__VA_ARGS__)))

#define io_ln_error(fmt, ...) \
  ::sirius::utils::io_msg_errln(io_str_error(fmt, ##__VA_ARGS__))
#define io_ln_warnsp(fmt, ...) \
  ::sirius::utils::io_msg_errln(io_str_warnsp(fmt, ##__VA_ARGS__))
#define io_ln_infosp(fmt, ...) \
  ::sirius::utils::io_msg_outln(io_str_infosp(fmt, ##__VA_ARGS__))
