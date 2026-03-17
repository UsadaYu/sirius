#pragma once
/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include <mutex>

#include "sirius/foundation/thread.h"
#include "utils/attributes.h"
#include "utils/config.h"
#include "utils/fs.hpp"
#include "utils/io.h"

namespace sirius {
namespace utils {
namespace io {
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

/**
 * @implements Singleton pattern.
 */
class Fmt {
 public:
  static inline constexpr size_t kPrefixLength = 55;

 private:
  Fmt() = default;

  ~Fmt() = default;

 public:
  Fmt(const Fmt &) = delete;
  Fmt &operator=(const Fmt &) = delete;
  Fmt(Fmt &&) = delete;
  Fmt &operator=(Fmt &&) = delete;

  static Fmt &instance() {
    static Fmt instance;
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
  std::string s_error(std::string_view f, int l = 0, std::string_view m = _SIRIUS_LOG_MODULE_NAME) { return s_gen("Error", f, l, ANSI_RED, err_ansi_enable_, m); }
  std::string s_warn (std::string_view f, int l = 0, std::string_view m = _SIRIUS_LOG_MODULE_NAME) { return s_gen("Warn", f, l, ANSI_YELLOW, err_ansi_enable_, m); }
  std::string s_info (std::string_view f, int l = 0, std::string_view m = _SIRIUS_LOG_MODULE_NAME) { return s_gen("Info", f, l, ANSI_GREEN, out_ansi_enable_, m); }
  std::string s_debug(std::string_view f, int l = 0, std::string_view m = _SIRIUS_LOG_MODULE_NAME) { return s_gen("Debug", f, l, ANSI_NONE, out_ansi_enable_, m); }
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
    err_msg.resize(size_needed + 4);
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

  enum class AnsiState : int { kRemain, kEnable, kDisable };
  void ansi_enable(enum AnsiState out_state, enum AnsiState err_state) {
    auto ansi_switch = [](enum AnsiState state, std::atomic<bool> &enable) {
      switch (state) {
      case AnsiState::kEnable:
        enable.store(true, std::memory_order_relaxed);
        break;
      case AnsiState::kDisable:
        enable.store(false, std::memory_order_relaxed);
        break;
      default:
        break;
      }
    };

    ansi_switch(out_state, out_ansi_enable_);
    ansi_switch(err_state, err_ansi_enable_);
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
    std::string pre = std::format("`{0}` (err: {1})", fn_str, err_code);
    std::string err_msg = fn_ptr(err_code);
    if (!err_msg.empty()) {
      err_msg.insert(0, " -> ", sizeof(" -> ") - 1);
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
    std::string pre = std::format("`{0}` (err: {1})", fn_str, err_code);
    std::string err_msg = fn_ptr(err_code);
    if (!err_msg.empty()) {
      err_msg.insert(0, " -> ", sizeof(" -> ") - 1);
    }
    return pre.append(err_msg).append(usr_msg);
  }

  static std::string s_gen(std::string_view prefix, std::string_view file,
                           int line, std::string_view color,
                           const std::atomic<bool> &ansi_enable,
                           std::string_view module) {
    std::string result;
    if (ansi_enable.load(std::memory_order_relaxed)) {
      result.reserve(kPrefixLength + 16 + sizeof(ANSI_NONE));
      result.append(color)
        .append(s_pre(prefix, module, file, line))
        .append(ANSI_NONE);
    } else {
      result.reserve(kPrefixLength + 8);
      result = s_pre(prefix, module, file, line);
    }
    return result;
  }
};

enum class PutType : int {
  kIn = 0,
  kOut = 1,
  kErr = 2,
};

/**
 * @implements Singleton pattern.
 */
class Native {
 private:
  Native() = default;

  ~Native() {
    fd_close(fd_out_);
    fd_close(fd_err_);
  }

 public:
  Native(const Native &) = delete;
  Native &operator=(const Native &) = delete;
  Native(Native &&) = delete;
  Native &operator=(Native &&) = delete;

  static Native &instance() {
    static Native instance;
    return instance;
  }

  void log_write(int level, const void *buffer, size_t size) {
    auto lock = std::lock_guard(mutex_);
    int fd = level <= SIRIUS_LOG_LEVEL_WARN ? fd_err_ : fd_out_;
    utils_write(fd, buffer, size);
  }

  void out_write(const void *buffer, size_t size) {
    auto lock = std::lock_guard(mutex_);
    utils_write(fd_out_, buffer, size);
  }

  void err_write(const void *buffer, size_t size) {
    auto lock = std::lock_guard(mutex_);
    utils_write(fd_err_, buffer, size);
  }

  auto fs_configure(PutType put_type, const std::filesystem::path &path,
                    int flags, int mode, bool ansi_disable)
    -> std::expected<void, UTrace> {
    auto fn_standard = [&](int new_fd,
                           int &old_fd) -> std::expected<void, UTrace> {
      fd_close(old_fd);
      old_fd = new_fd;
      return {};
    };

    auto fn_file = [&](int &old_fd) -> std::expected<void, UTrace> {
      int new_fd = fs::fs_open_impl(path.string().c_str(), flags, mode);
      if (new_fd == -1) {
        const int errno_err = errno;
        auto es = Fmt::errno_err(errno_err, "fs_open_impl");
        return std::unexpected(UTrace(std::move(es)));
      }
      fd_close(old_fd);
      old_fd = new_fd;
      return {};
    };

#define E_PUT(fd_default, fd) \
  do { \
    auto ret = path.empty() ? fn_standard(fd_default, fd) : fn_file(fd); \
    if (ret.has_value()) { \
      E_ANSI(fd > 2 ? Fmt::AnsiState::kDisable \
                    : (ansi_disable ? Fmt::AnsiState::kDisable \
                                    : Fmt::AnsiState::kEnable), \
             Fmt::AnsiState::kRemain); \
      return {}; \
    } else { \
      utrace_return(ret); \
    } \
  } while (0)

    auto lock = std::lock_guard(mutex_);
    if (put_type == PutType::kOut) {
#define E_ANSI(out, err) Fmt::instance().ansi_enable(out, err)
      E_PUT(STDOUT_FILENO, fd_out_);
#undef E_ANSI
    } else if (put_type == PutType::kErr) {
#define E_ANSI(out, err) Fmt::instance().ansi_enable(err, out)
      E_PUT(STDOUT_FILENO, fd_err_);
#undef E_ANSI
    }
    return std::unexpected(UTrace("Invalid argument. `put_type`"));
#undef E_PUT
  }

#if defined(_WIN32) || defined(_WIN64)
#  define PRINTLN_IMPL(fd, msg) \
    do { \
      auto imsg = std::format("{0}\n", msg); \
      LOCK_EXPR; \
      utils_write(fd, imsg.c_str(), imsg.size()); \
    } while (0)
#else
#  define PRINTLN_IMPL(fd, msg) \
    do { \
      struct iovec iov[2]; \
      iov[0].iov_base = const_cast<char *>(msg.data()); \
      iov[0].iov_len = msg.size(); \
      iov[1].iov_base = const_cast<char *>("\n"); \
      iov[1].iov_len = 1; \
      LOCK_EXPR; \
      writev(fd, iov, 2); \
    } while (0)
#endif
  // clang-format off
#define LOCK_EXPR auto lock = std::lock_guard(mutex_)
  void println_out(std::string_view msg) { PRINTLN_IMPL(fd_out_, msg); }
  void println_err(std::string_view msg) { PRINTLN_IMPL(fd_err_, msg); }
  void print_out(std::string_view msg) { out_write(msg.data(), msg.size()); }
  void print_err(std::string_view msg) { err_write(msg.data(), msg.size()); }
#undef LOCK_EXPR
#define LOCK_EXPR (void)0
  static void fs_println(int fd, std::string_view msg) { PRINTLN_IMPL(fd, msg); }
  static void fs_print(int fd, std::string_view msg) { utils_write(fd, msg.data(), msg.size()); }
#undef LOCK_EXPR
#undef PRINTLN_IMPL
  // clang-format on

 private:
  std::mutex mutex_ {};
  int fd_out_ = STDOUT_FILENO;
  int fd_err_ = STDERR_FILENO;

  void fd_close(int &fd) {
    if (fd > 2) {
      (void)fs::fs_close_impl(fd);
      fd = -1;
    }
  }
};

// clang-format off
inline void println_out(std::string_view msg) { Native::instance().println_out(msg); }
inline void println_err(std::string_view msg) { Native::instance().println_err(msg); }
inline void print_out(std::string_view msg) { Native::instance().print_out(msg); }
inline void print_err(std::string_view msg) { Native::instance().print_err(msg); }
// clang-format on
} // namespace io
} // namespace utils
} // namespace sirius

#define log_error_str(fmt, ...) \
  (::sirius::utils::io::Fmt::instance() \
     .s_error(SIRIUS_FILE_NAME, __LINE__) \
     .append(::sirius::utils::io::Fmt::row_gs(fmt, ##__VA_ARGS__)))
#define log_warnsp_str(fmt, ...) \
  (::sirius::utils::io::Fmt::instance().s_warn("").append( \
    ::sirius::utils::io::Fmt::row_gs(fmt, ##__VA_ARGS__)))
#define io_infosp_str(fmt, ...) \
  (::sirius::utils::io::Fmt::instance().s_info("").append( \
    ::sirius::utils::io::Fmt::row_gs(fmt, ##__VA_ARGS__)))

#define logln_error(fmt, ...) \
  ::sirius::utils::io::println_err(log_error_str(fmt, ##__VA_ARGS__))
#define logln_warnsp(fmt, ...) \
  ::sirius::utils::io::println_err(log_warnsp_str(fmt, ##__VA_ARGS__))
#define logln_infosp(fmt, ...) \
  ::sirius::utils::io::println_out(io_infosp_str(fmt, ##__VA_ARGS__))
