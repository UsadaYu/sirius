#pragma once

#include "utils/utils.h"

#ifdef __cplusplus
#  include <codecvt>
#endif

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

// --- UTILS_STRERROR_R ---
#undef UTILS_STRERROR_R

#if defined(_WIN32) || defined(_WIN64)
#  define UTILS_STRERROR_R(error_code, buf, size) \
    strerror_s(buf, size, error_code)
#else
#  define UTILS_STRERROR_R strerror_r
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
class Io {
 public:
#  if defined(_WIN32) || defined(_WIN64)
  static std::string win_last_error(const DWORD error_code,
                                    std::string function) {
    std::string line1 =
      std::format("Error `{0}` (error code: {1})", function, error_code);

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

      line2 = row(back_strip(line2));
      return line1 + line2;
    }
  }
#  endif

  static std::string errno_error(const int error_code,
                                 const std::string function) {
    std::string line1 =
      std::format("Error `{0}` (error code: {1})", function, error_code);

    char buffer[_SIRIUS_LOG_BUF_SIZE];
    if (0 == UTILS_STRERROR_R(error_code, buffer, sizeof(buffer))) [[likely]] {
      std::string line2 = row(back_strip(std::string(buffer)));
      return line1 + line2;
    } else {
      return line1;
    }
  }

  static std::string row(const std::string string) {
    std::string result = string;

    if (!result.empty()) {
      result = std::format("\n  |  {}", result);
    }

    return result;
  }

 private:
  static std::string back_strip(const std::string string) {
    std::string result = string;

    while (!result.empty() &&
           (result.back() == '\r' || result.back() == '\n' ||
            result.back() == '\t' || result.back() == ' ' ||
            result.back() == '.')) {
      result.pop_back();
    }

    return result;
  }
};
} // namespace Utils
#endif
