#pragma once

#include "sirius/utils/log.h"
#include "utils/namespace.hpp"

#define UTILS_LOG_KEY "utils_log"

namespace Utils {
namespace Log {
constexpr const char *KEY = UTILS_LOG_KEY;
constexpr const char *COMMON =
  " [Common " sirius_namespace " " UTILS_LOG_KEY "] ";
constexpr const char *NATIVE =
  " [Native " sirius_namespace " " UTILS_LOG_KEY "] ";
constexpr const char *DAEMON =
  " [Daemon " sirius_namespace " " UTILS_LOG_KEY "] ";
constexpr const char *DAEMON_ARG_KEY = UTILS_LOG_KEY "_daemon_arg";
constexpr const char *MUTEX_PROCESS_KEY = UTILS_LOG_KEY "_process";
constexpr const char *MUTEX_SHM_KEY = UTILS_LOG_KEY "_shm";

constexpr uint64_t PROCESS_MAX = 128;
constexpr uint64_t PROCESS_FEED_GUARD_MS = 2000;
constexpr uint64_t PROCESS_GUARD_TIMEOUT_MS = 10000;
constexpr uint64_t SHM_SLOT_RESET_TIMEOUT_MS = 10000;
constexpr size_t SHM_CAPACITY = Utils::next_power_of_2(sirius_log_shm_capacity);

#if defined(_WIN32) || defined(_WIN64)
static inline void win_last_error(const char *function) {
  DWORD error_code = GetLastError();
  char e[sirius_log_buf_size] = {0};

  DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD size = FormatMessage(flags, nullptr, error_code,
                             MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), e,
                             sizeof(e) / sizeof(TCHAR), nullptr);
  if (size == 0) [[unlikely]] {
    UTILS_DPRINTF(STDERR_FILENO, log_level_str_error "%s%s: %lu\n", COMMON,
                  function, error_code);
  } else {
    UTILS_DPRINTF(STDERR_FILENO, log_level_str_error "%s%s: %lu. %s", COMMON,
                  function, error_code, e);
  }
}
#endif

static inline void errno_error(const char *function) {
  int error_code = errno;
  char e[sirius_log_buf_size];

  if (0 == UTILS_STRERROR_R(error_code, e, sizeof(e))) [[likely]] {
    UTILS_DPRINTF(STDERR_FILENO, log_level_str_error "%s%s: %d. %s", COMMON,
                  function, error_code, e);
  } else {
    UTILS_DPRINTF(STDERR_FILENO, log_level_str_error "%s%s: %d\n", COMMON,
                  function, error_code);
  }
}
} // namespace Log
} // namespace Utils
