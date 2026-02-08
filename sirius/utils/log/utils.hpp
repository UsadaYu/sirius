#pragma once

#include "sirius/foundation/log.h"
#include "utils/namespace.hpp"

#define UTILS_LOG_KEY "utils_log"

namespace Utils {
namespace Log {
constexpr const char *KEY = UTILS_LOG_KEY;
constexpr const char *COMMON =
  " [Common " _SIRIUS_NAMESPACE " " UTILS_LOG_KEY "] ";
constexpr const char *NATIVE =
  " [Native " _SIRIUS_NAMESPACE " " UTILS_LOG_KEY "] ";
constexpr const char *DAEMON =
  " [Daemon " _SIRIUS_NAMESPACE " " UTILS_LOG_KEY "] ";
constexpr const char *DAEMON_ARG_KEY = UTILS_LOG_KEY "_daemon_arg";
constexpr const char *MUTEX_PROCESS_KEY = UTILS_LOG_KEY "_process";
constexpr const char *MUTEX_SHM_KEY = UTILS_LOG_KEY "_shm";

constexpr size_t LOG_BUF_SIZE = 4096;
constexpr size_t LOG_PATH_MAX = 4096;
constexpr size_t PROCESS_MAX = 128;
constexpr uint64_t PROCESS_FEED_GUARD_MS = 2000;
constexpr uint64_t PROCESS_GUARD_TIMEOUT_MS = 10000;
constexpr uint64_t SHM_SLOT_RESET_TIMEOUT_MS = 10000;
constexpr size_t SHM_CAPACITY =
  Utils::next_power_of_2(_SIRIUS_LOG_SHM_CAPACITY);
} // namespace Log
} // namespace Utils
